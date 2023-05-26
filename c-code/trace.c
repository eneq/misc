/**
 * Optimization considerations
 *
 * - The set of FDs used for tracing sockets are linearly searched each time the
 *   poll returns. This is unfortunately unavoidable (afaik) but the set can
 *   be compressed so that unused sockets is at the end of the set and this
 *   reduces the loop size.
 * - Prioritize lookups for the closest connections, i.e. process as many 1-5
 *   hops distance traces first. This has the sideeffect that traces will take
 *   longer to complete which might have additional side effects with
 *   specific processing instances taking longer to complete.
 * - trace_end actually just inhibits the callback atm. Some lofgic should be
 *   added to trigger on end flag and proactively kill the connections and do
 *   cleanup.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/eventfd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <linux/errqueue.h>
#include <errno.h>
#include <stdbool.h>
#include <poll.h>
#include <pthread.h>
#include <float.h>

#include "mem.h"
#include "hardware.h"
#include "atomic.h"
#include "threadpool.h"
#include "trace.h"
#include "trace_opt.h"

// Macros for triggering the trace background thread
#define WAKE_PING(x) {if (x!=NULL) {uint64_t d=1; if(write(x->fds[0].fd, &d, sizeof(d)));}}
#define WAKE_PONG(x) {if (x!=NULL) {uint64_t d=0; if(read(x->fds[0].fd, &d, sizeof(d)));}}

#define STATUS_FAILED 0x80000000

// Trace status information
typedef enum
{
    eNO_STATUS= 0,
    eRUNNING,
    eFINISHED,

    // Fail status codes
    eFAILED_ADDR= STATUS_FAILED,
    eFAILED_SOCKET,
    eFAILED_BIND,
    eFAILED_CONNECT,
    eFAILED_OPTTTL,
    eFAILED_OPTRECVERR,
    eFAILED_SEND,
} trace_status_t;

/**
 * The trace service uses a context structure to maintain its internal
 * state. This structure also contains information about all the current trace
 * requests, this is done by preallocating a fixed number of tracking objects
 * for the traces.
 *
 * One is a set of pollfd elements thats used by the poll system call, for
 * each item in the pollfd array there is a matching worker description item
 * in the worker array (index matches).
 *
 * When a FD is triggered its detected and the corresponding worker entity is
 * used for generating the callback and associated data.
 */
typedef struct workers_s
{
    trace_t* trace;
    int free;
    int ttl;
    double ts;
} workers_t;

struct _trace_ctx_s
{
    trace_t* traces, *traces_tail;
    trace_t* queue, *queue_tail;

    tp_thread_t* thread;

    // Configuration data
    int udp_size;
    int udp_port;
    int pool_size;
    int timeout;
    int distance;
    int retries;
    char* address;

    struct pollfd* fds;
    workers_t* workers;
    int free; // First unallocated slot, this in turn has a ref to the next.

    int lock; // Atomic spin lock used for thread synchronization.
    char* data;
};

/**
 * Data entity for a specific trace request, this contains the address data
 * and needed data for the callback generation
 */
struct _trace_s
{
    struct _trace_s* next;
    trace_ctx_t* ctx;

    char* address; // Address, as addr is generated this is freed.
    struct sockaddr_storage addr;
    int ai_family;

    int max_ttl;
    int ttl; // Current ttl sent out, this is increased as we go.
    int responses;
    int endpoint; // Maximum ttl we got a valid response for

    trace_status_t status;
    trace_cb_t cb;
    void* user;

    bool end;
};


// Forward declarations
tp_code_t trace_thread(void* data);


/**
 * Initializes a trace context, this is later used when executing specific
 * traces.
 *
 * @returns trace_ctx_t* - Trace context
 */
trace_ctx_t* trace_init()
{
    trace_ctx_t* ctx= ALLOC(sizeof(*ctx));

    if (ctx!=NULL)
    {
        memset(ctx, 0, sizeof(*ctx));

        ctx->pool_size= TRACE_LOOKUP_I(TRACE_POOL_SIZE)+1;
        ctx->udp_size= TRACE_LOOKUP_I(TRACE_UDP_SIZE);
        ctx->udp_port= TRACE_LOOKUP_I(TRACE_UDP_PORT);
        ctx->timeout= TRACE_LOOKUP_I(TRACE_TIMEOUT);
        ctx->distance= TRACE_LOOKUP_I(TRACE_DISTANCE);
        ctx->retries= TRACE_LOOKUP_I(TRACE_RETRIES);
        char* t= TRACE_LOOKUP(TRACE_ADDRESS);

        ctx->workers= ALLOC(sizeof(workers_t)*ctx->pool_size);
        ctx->fds= ALLOC(sizeof(struct pollfd)*ctx->pool_size);
        ctx->address= ALLOC(strlen(t)+1);
        ctx->data= ALLOC(ctx->udp_size);
        if(ctx->address != NULL && ctx->data != NULL && ctx->workers != NULL && ctx->fds != NULL)
        {
            memset(ctx->workers, 0, sizeof(workers_t)*ctx->pool_size);
            memset(ctx->fds, 0, sizeof(struct pollfd)*ctx->pool_size);

            strcpy(ctx->address, t);

            // We use the same random data package for all UDP packets so we save
            // this data in the ctx and just reference it from the traces.
            generate_random(ctx->data, ctx->udp_size);

            // Create the background thread that monitors the sockets for activity
            ctx->thread= tp_request_thread(0, trace_thread, ctx);
            if (ctx->thread!=NULL)
            {
                // Prepare the space used for the sockets fd (poll/select)
                // [0] is used for internal abuse

                for (int idx= 0; idx<ctx->pool_size; idx++)
                {
                    ctx->workers[idx].free= idx+1;
                    ctx->fds[idx].fd= -1;
                    ctx->fds[idx].events= POLLERR;
                }

                //
                // The last workers free index should be -1 to indicate
                // there isn't an available worker. The loop above blindly
                // sets it to the next index value.
                //
                ctx->workers[ctx->pool_size-1].free= -1;

                ctx->free= 1;

                // FD[0] is used internally
                ctx->fds[0].fd= eventfd(0, EFD_NONBLOCK);
                ctx->fds[0].events= POLLIN|POLLPRI;
            }
            else
            {
                trace_release(ctx);
            }
        }
    }

    return ctx;
}

/**
 * Releases trace context, this will terminate the trace thread which will
 * free any running traces. If there is no thread (exception case) we will try
 * to free the data directly.
 *
 * @param trace_ctx_t* - Trace context
 */
void trace_release(trace_ctx_t* ctx)
{
    if (ctx!=NULL)
    {

        if (ctx->thread!=NULL)
        {
            tp_release_thread(ctx->thread);
            tp_wait_for_thread(ctx->thread);
        }
        else
        {
            while (ctx->traces!=NULL)
            {
                trace_t* tmp= ctx->traces;
                ctx->traces= tmp->next;
                FREE(tmp);
            }
        }

        FREE(ctx->address);
        FREE(ctx->workers);
        FREE(ctx->fds);
        FREE(ctx->data);
        FREE(ctx);
    }
}

/**
 * Generates a trace request and puts it on the trace thread queue, any entry
 * on this queue is processed in a FIFO order.
 *
 * @params trace_ctx_t* - Trace context
 * @params char* - Address
 * @params uint16_t - Max distance
 * @params trace_cb_t* - Callback pointer for trace updates
 * @params void* - User provided data for the trace, passed in callback
 * @return trace_t* - Assigned trace object
 */
trace_t* trace_start(trace_ctx_t* ctx, char* address,
                     uint16_t distance, trace_cb_t cb, void* data)
{
    trace_t* trace= NULL;

    if (ctx!=NULL && address!=NULL)
    {
        trace= ALLOC(sizeof(*trace));

        if (trace!=NULL)
        {
            memset(trace, 0, sizeof(*trace));

            trace->ctx= ctx;
            trace->ai_family= AF_INET;
            trace->cb= cb;
            trace->user= data;
            trace->max_ttl= distance;
            trace->ttl= 1; // TTL=0 is ignored
            trace->endpoint= 0;

            trace->address= ALLOC(strlen(address)+1);
            memcpy(trace->address, address, strlen(address)+1);

            // Lock ctx access and update shared list.
            ATOMIC_spin(&ctx->lock);
            if (ctx->traces!=NULL)
            {
                ctx->traces_tail->next= trace;
                ctx->traces_tail= trace;
            }
            else
            {
                ctx->traces= ctx->traces_tail= trace;
            }
            ATOMIC_release(&ctx->lock);

            // Ping the trace thread
            WAKE_PING(ctx);
        }
    }

    return trace;
}


/**
 * Terminates an active trace, this might take some time to actually release
 * all resources.
 *
 * @params trace_t* - Reference to trace
 */
void trace_end(trace_t* trace)
{
    if (trace==NULL)
    {
        return;
    }

    trace->end= true;
}


// Internal functions

/**
 * Returns the current timestamp in ms as a double
 * @return double - Current time in ms.
 */
double get_time (void)
{
    struct timeval tv;
    gettimeofday (&tv, NULL);

    return ((double) tv.tv_usec) / 1000.0 + (unsigned long) tv.tv_sec*1000.0;
}

/**
 * Sends a UDP probe with a requested TTL
 *
 * @params int* - Pointer to socket descriptor
 * @params trace_t* - Reference to trace request
 * @params int - TTL value
 */
void send_probe(int* fd, trace_t* trace)
{
    *fd= -1;

    // First time, lookup address
    if (trace->address!=NULL)
    {
        struct addrinfo hints, *ai, *addr= NULL;

        memset (&hints, 0, sizeof (hints));
        hints.ai_family= trace->ai_family;
        hints.ai_flags= AI_IDN;

        // Address lookup
        if (getaddrinfo (trace->address, NULL, &hints, &addr)!=0)
        {
            trace->status = eFAILED_ADDR;
            return;
        }

        for (ai= addr; ai; ai= ai->ai_next)
        {
            if (ai->ai_family == trace->ai_family)
            {
                break;
            }
        }

        // if we dont find it try what we got
        ai= (ai==NULL?addr:ai);

        if (sizeof(struct sockaddr_storage)<ai->ai_addrlen)
        {
            trace->status = eFAILED_ADDR;
            return;
        }

        memcpy(&trace->addr, ai->ai_addr, ai->ai_addrlen);
        freeaddrinfo(addr);

        FREE(trace->address);
        trace->address= NULL;
    }

    uint16_t* port= (trace->addr.ss_family==AF_INET?
                     &(((struct sockaddr_in*)&(trace->addr))->sin_port):
                     &(((struct sockaddr_in6*)&(trace->addr))->sin6_port));
    *port= htons((uint16_t)trace->ctx->udp_port+trace->ttl);

    *fd= socket (trace->addr.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (*fd<0)
    {
        trace->status = eFAILED_SOCKET;
        return;
    }

    /* Only relevant when we specify which device we want to bind the socket to
    if (bind (*fd, (struct sockaddr*)&trace->addr, sizeof (trace->addr)) < 0) {
        trace->status= eFAILED_BIND;
    return;
    }
    */

    // Socket setup
    if (trace->addr.ss_family==AF_INET)
    {
        int on= 1, op;

        // Disable fragmentation
        op= IP_PMTUDISC_DO;
        setsockopt(*fd, IPPROTO_IP, IP_MTU_DISCOVER, &op, sizeof(op));

        // Turn on timestamping
        //setsockopt (*fd, SOL_SOCKET, SO_TIMESTAMP, &on, sizeof (on));

        // Turn on recv ttl information
        setsockopt (*fd, SOL_IP, IP_RECVTTL, &on, sizeof (on));

        // Asynchronous
        fcntl (*fd, F_SETFL, O_NONBLOCK);

        // Set the ttl value on the socket
        setsockopt (*fd, SOL_IP, IP_TTL, &trace->ttl, sizeof (trace->ttl));

        // Activate IP Error information
        setsockopt (*fd, SOL_IP, IP_RECVERR, &on, sizeof (on));
    }

    if (connect(*fd, (struct sockaddr*)&trace->addr, sizeof(trace->addr)) < 0)
    {
        trace->status= eFAILED_CONNECT;
        return;
    }

    if (sendto(*fd, trace->ctx->data, sizeof(trace->ctx->data),
               0, (struct sockaddr*)&trace->addr, sizeof (trace->addr))<0)
    {
        trace->status= eFAILED_SEND;
        close(*fd);
        *fd= -1;
        return;
    }

    // Package sent
    return;
}

/**
 * Read the data from the socket and process it, we only read the information
 * on the IPCERR channel, this data is either a TTL expiration notice
 * (intermeddiate connections) or an unreachable host (reached the target)
 *
 * When these events occur we extract the addr information from the other
 * party and report it back as one of the intermeddiate connections. Which
 * connection this is is calculated from the worker information associated
 * with the socket.
 *
 * @params int - Socket
 * @params struct sockaddr_storage* - Address of opposite connection
 * @return bool - Success/Fail
 */
bool recv_reply (int fd, struct sockaddr_storage* addr)
{
    static char buf[1280];
    static char control[1024];
    struct sock_extended_err* ee= NULL;
    struct msghdr msg;
    struct sockaddr_storage from;
    struct iovec iov;

    if (addr==NULL)
    {
        return false;
    }

    // Prep receive struct
    memset (&msg, 0, sizeof (msg));
    msg.msg_name = &from;
    msg.msg_namelen = sizeof (from);
    msg.msg_control = control;
    msg.msg_controllen = sizeof (control);
    iov.iov_base = buf;
    iov.iov_len = sizeof (buf);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (recvmsg(fd, &msg, MSG_ERRQUEUE)<0)
    {
        return false;
    }

    struct cmsghdr* cm= NULL;
    for (cm= CMSG_FIRSTHDR(&msg); cm!=NULL; cm= CMSG_NXTHDR(&msg, cm))
    {
        void* ptr= CMSG_DATA(cm);

        switch (cm->cmsg_level)
        {
            case SOL_SOCKET:
            {
                if (cm->cmsg_type==SO_TIMESTAMP)
                {
                }
            }
            break;

            case SOL_IP:
            {
                switch (cm->cmsg_type)
                {
                    case IP_TTL:
                    {
                        break;
                    }
                    break;

                    case IP_RECVERR:
                    {
                        ee= (struct sock_extended_err*)ptr;

                        if (ee->ee_origin!=SO_EE_ORIGIN_ICMP || cm->cmsg_len==0)
                        {
                            ee= NULL;
                        }
                    }
                    break;

                    default:
                        break;
                }
            }
            break;

            default:
                break;
        }
    }

    if (ee==NULL)
    {
        return false;
    }

    switch (ee->ee_type)
    {
        case ICMP_TIME_EXCEEDED:
        {
            if (ee->ee_code==ICMP_EXC_TTL)
            {
                struct sockaddr_storage* term= (struct sockaddr_storage*)SO_EE_OFFENDER (ee);
                if (term!=NULL)
                {
                    if (term->ss_family==AF_INET)
                    {
                        memcpy(addr, term, sizeof(struct sockaddr_in));
                        return true;
                    }
                    else if (term->ss_family==AF_INET6)
                    {
                        memcpy(addr, term, sizeof(struct sockaddr_in6));
                        return true;
                    }
                }
            }
        }
        break;

        default:
            break;
    }

    return false;
}

/**
 * Trace cleanup
 *
 * @params trace_t* - This is a pointer to the trace context
 */
void trace_cleanup(trace_t* trace)
{
    if (trace->address!=NULL)
    {
        FREE(trace->address);
    }

    if (trace->cb!=NULL && !trace->end)
    {
        trace->cb(NULL, trace->user);
    }
    FREE(trace);
}

/**
 * Trace thread cleanup
 *
 * @params void* - This is a pointer to the trace context
 */
void thread_cleanup(void* data)
{
    trace_ctx_t* ctx= (trace_ctx_t*)data;

    for (int idx= 0; idx<ctx->pool_size; idx++)
    {
        if (ctx->fds[idx].fd>=0)
        {
            close(ctx->fds[idx].fd);
            ctx->fds[idx].fd= -1;

            // Move any that was removed from the queue back on it for proper
            // termination.
            trace_t* trace= ctx->workers[idx].trace;
            if (trace!=NULL && trace->next==NULL)
            {
                trace->next= ctx->queue;
                ctx->queue= trace;
            }
        }
    }

    while (ctx->queue!=NULL)
    {
        trace_t* tmp= ctx->queue;
        ctx->queue= ctx->queue->next;
        trace_cleanup(tmp);
    }

    while (ctx->traces!=NULL)
    {
        trace_t* tmp= ctx->traces;
        ctx->traces= ctx->traces->next;
        trace_cleanup(tmp);
    }
}

/**
 * Process internal management, this includes moving trace requests from the
 * incoming queue to the worker queue and also removing finished items from
 * the queues
 *
 * @params trace_ctx_t* - Trace context
 */
void process_internal(trace_ctx_t* ctx)
{
    // Unhook incoming traces and add to worker queue
    ATOMIC_spin(&ctx->lock);
    trace_t* traces= ctx->traces;
    trace_t* traces_tail= ctx->traces_tail;
    ctx->traces= ctx->traces_tail= NULL;
    ATOMIC_release(&ctx->lock);

    // New traces, queue them up.
    if (traces!=NULL && traces_tail!=NULL)
    {
        if (ctx->queue==NULL)
        {
            ctx->queue= traces;
            ctx->queue_tail= traces_tail;
        }
        else
        {
            ctx->queue_tail->next= traces;
            ctx->queue_tail= traces_tail;
        }
    }

    // Process worker queue until there are no more available sockets
    int attempts= 0;
    while (ctx->free>0 && ctx->queue!=NULL)
    {
        int slot= ctx->free;
        send_probe(&ctx->fds[slot].fd, ctx->queue);

        if (ctx->fds[slot].fd<0)
        {
            if (attempts++<ctx->retries)
            {
                continue;
            }
            else
            {
                // Logg system error
            }
        }

        ctx->free= ctx->workers[slot].free;
        ctx->workers[slot].free= -1;
        ctx->workers[slot].trace= ctx->queue;
        ctx->workers[slot].ttl= ctx->queue->ttl++;
        ctx->workers[slot].ts= get_time();

        // If we have reached the TTL roof move to next, the trace instance
        // will be handled when the probe responses has been recieved.
        if (ctx->queue->ttl>ctx->queue->max_ttl)
        {
            trace_t* tmp= ctx->queue;
            ctx->queue= ctx->queue->next;

            tmp->next= NULL;
        }
    }
}

/**
 * A specific socket has activated so we process it. If there are address data
 * then the callback is initiated and the socket released for the next worker.
 *
 * @params trace_ctx_t* - Trace context
 * @params int - Index in the worker table
 */
void process_socket(trace_ctx_t* ctx, int index)
{
    struct sockaddr_storage addr;
    trace_t* trace= ctx->workers[index].trace;

    bool res= recv_reply(ctx->fds[index].fd, &addr);
    trace->responses++;

    if (res)
    {
        trace_data_t data= {0};

        if (trace->endpoint<ctx->workers[index].ttl)
        {
            trace->endpoint= ctx->workers[index].ttl;
        }

        data.ctx= ctx;
        data.trace= trace;
        data.distance= ctx->workers[index].ttl;
        data.addr= &addr;

        trace->cb(&data, trace->user);
    }

    // Close the socket and mark it free
    close(ctx->fds[index].fd);
    ctx->fds[index].fd= -1;
    ctx->workers[index].free= ctx->free;
    ctx->free= index;

    // Check if we have all responses, if we do then we can just release it
    // since its already removed from the work queue when the last probe went
    // out but i we have an endpoint for the trace registered then we will
    // initiate one last callback (this is usually the target for the trace).
    if (trace->ttl>trace->max_ttl && trace->responses>=trace->max_ttl)
    {
        if (trace->endpoint<trace->max_ttl)
        {
            trace_data_t data= {0};
            data.ctx= ctx;
            data.trace= trace;
            data.distance= trace->endpoint+1;
            data.addr= &trace->addr;

            trace->cb(&data, trace->user);
        }
        trace_cleanup(trace);
    }
}

/**
 * Trace background thread, this is responsible for processing all jobs, it
 * usually waiting in poll until a socket has been activated. It can also be
 * woken by the special worker[0] fd which is an eventfd put in place to be
 * able to wake the tread for queing new requests.
 *
 * @params void* - trace_ctx_t* reference
 * @return tp_code_t - Return code for threadpool.
 */
tp_code_t trace_thread(void* data)
{
    trace_ctx_t* ctx= (trace_ctx_t*)data;

    pthread_cleanup_push(thread_cleanup, (void*)ctx);

    int timeout= -1;
    while (true)
    {
        poll(ctx->fds, ctx->pool_size, timeout);

        double closest= DBL_MAX;
        double current= get_time();

        for (int idx= 1; idx<ctx->pool_size; idx++)
        {
            if (ctx->fds[idx].revents!=0 ||
                (ctx->fds[idx].fd>0 && (current - ctx->workers[idx].ts) >= ctx->timeout))
            {

                process_socket(ctx, idx);
                ctx->fds[idx].revents= 0;
                continue;
            }

            if (ctx->fds[idx].fd>0 && ctx->workers[idx].ts<closest)
            {
                closest= ctx->workers[idx].ts;
            }
        }

        timeout= (int)(current-closest);

        if (ctx->fds[0].revents!=0)
        {
            WAKE_PONG(ctx);
            ctx->fds[0].revents= 0;
        }

        process_internal(ctx);
    }

    pthread_cleanup_pop(1);

    return TP_OK;
}

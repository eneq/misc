#include <stdint.h>
#include <stdbool.h>
#include <unbound.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>

#include "mem.h"
#include "lookup.h"
#include "str.h"

#define ARPA_INET  "in-addr.arpa"
#define ARPA_INET6 "ip6.arpa"

#define ADDR_BUF_LEN  128

#define DNS_RR_A      1        // IPV4 address
#define DNS_RR_AAAA   28       // IPV6 address
#define DNS_RR_CNAME  5        // Canonical name
#define DNS_RR_LOC    29       // Geo Locational information
#define DNS_RR_PTR    12       // PTR record (reverse dns)


struct _dns_s
{
    struct ub_result* result;

    char address[INET6_ADDRSTRLEN];
    dns_state_t state;
    int rr;
    int ubid;

    dns_cb_t func;
    void* user;
};


typedef struct _lookup_s
{
    struct ub_ctx* unbound;
    pthread_t worker;
    uint32_t refcnt;
} lookup_t;

static lookup_t* lookup_ctx= NULL;

static void ub_callback(void* user, int err, struct ub_result* result);
static void* lookup_worker(void* data);


void lookup_init()
{
    if (lookup_ctx==NULL)
    {
        lookup_ctx= ALLOC(sizeof(*lookup_ctx));
        lookup_ctx->unbound= ub_ctx_create();
        lookup_ctx->refcnt = 0;

        //ub_ctx_set_option(lookup_ctx->unbound, "verbosity:", "6");
        ub_ctx_resolvconf(lookup_ctx->unbound, NULL);
        ub_ctx_hosts(lookup_ctx->unbound, NULL);
        ub_ctx_set_option(lookup_ctx->unbound, "local-zone:", "10.in-addr.arpa. transparent");

        ub_ctx_async(lookup_ctx->unbound, true);

        pthread_create(&lookup_ctx->worker, NULL, lookup_worker, lookup_ctx);
    }

    //
    // Increment the singleton reference count.
    //
    ++(lookup_ctx->refcnt);
}


void lookup_terminate()
{
    if (lookup_ctx!=NULL)
    {
        //
        // Decrement the singleton reference count.
        //
        --(lookup_ctx->refcnt);

        //
        // If the reference count is zero we can shutdown the lookup
        // service.
        //
        if (lookup_ctx->refcnt == 0)
        {
            //
            // Shutdown the worker thread.
            //
            pthread_cancel(lookup_ctx->worker);
            pthread_join(lookup_ctx->worker, NULL);

            //
            // Free the context.
            //
            FREE(lookup_ctx);
            lookup_ctx= NULL;
        }
    }
}


static dns_t* lookup_internal(char* addr, int rr, dns_cb_t func, void* user)
{
    dns_t* ctx= ALLOC(sizeof(*ctx));
    if (ctx==NULL)
    {
        return NULL;
    }

    ctx->func= func;
    ctx->user= user;
    ctx->state= eDnsInProgress;
    ctx->rr= rr;
    ctx->ubid = 0;

    int ret= 0;
    if (func==NULL)
    {
        ret = ub_resolve(lookup_ctx->unbound, addr, ctx->rr, 1, &ctx->result);

        if (ret!=0 || !ctx->result->havedata)
        {
            ctx->state= eDnsFailed;
        }
        else
        {
            ctx->state= eDnsSuccess;
        }

    }
    else
    {
        ret = ub_resolve_async(lookup_ctx->unbound, addr, ctx->rr, 1,
                              (void*)ctx, ub_callback, &ctx->ubid);
    }

    if (ret!=0)
    {
        FREE(ctx);
        ctx= NULL;
    }

    return ctx;
}


dns_t* lookup(char* addr, dns_cb_t func, void* user)
{
    if (lookup_ctx==NULL)
    {
        return NULL;
    }

    return lookup_internal(addr, DNS_RR_A, func, user);
}


dns_t* reverse_lookup(char* addr, dns_cb_t func, void* user)
{
    if (lookup_ctx==NULL)
    {
        return NULL;
    }

    uint8_t buf[sizeof(struct in_addr)];
    char raddr[ADDR_BUF_LEN]= {0};

    if (inet_pton(AF_INET, addr, buf)==0)
    {
        return NULL;
    }

    // create a address reference for reverse lookup
    // (ip reversed+ARPA extension)
    int index= 0;
    index+= itoa(buf[3], &raddr[index], ADDR_BUF_LEN-index);
    raddr[index++]= '.';
    index+= itoa(buf[2], &raddr[index], ADDR_BUF_LEN-index);
    raddr[index++]= '.';
    index+= itoa(buf[1], &raddr[index], ADDR_BUF_LEN-index);
    raddr[index++]= '.';
    index+= itoa(buf[0], &raddr[index], ADDR_BUF_LEN-index);
    raddr[index++]= '.';
    strncpy(&raddr[index], ARPA_INET, ADDR_BUF_LEN-index);

    return lookup_internal(raddr, DNS_RR_PTR, func, user);
}

void lookup_cancel(dns_t *ctx)
{
    if ((ctx != NULL) && (lookup_ctx != NULL))
    {
        if (ctx->ubid != 0)
        {
            //
            // ub_cancel() will block if it is in the middle of delivering
            // the results to the callback.
            //
            // It will fail if the id is no longer valid (i.e. the query has completed)
            // or if a system error occured. In either case, we know the
            // callback will not execute and can consider the request as cancelled.
            //
            ub_cancel(lookup_ctx->unbound, ctx->ubid);
            ctx->ubid = 0;
            ctx->state = eDnsFailed;
        }
    }
}

dns_state_t get_state(dns_t* ctx)
{
    if (ctx!=NULL)
    {
        return ctx->state;
    }

    return eDnsBad;
}

// TODO: Multiple reply support
char* get_address(dns_t* ctx)
{
    if (ctx==NULL
        || ctx->state!=eDnsSuccess
        || ctx->result==NULL
        || !ctx->result->havedata)
    {
        return NULL;
    }

    char* ret= ctx->result->data[0];

    if (ctx->rr==DNS_RR_PTR)
    {
        for (int i=0; ctx->result->data[0][i]!='\0'; i++)
        {
            if (ctx->result->data[0][i]<' ')
            {
                ctx->result->data[0][i]= '.';
            }
        }

        ret++;
    }
    else {
        if (inet_ntop(AF_INET, ret, ctx->address, INET_ADDRSTRLEN)==NULL) {
            if (inet_ntop(AF_INET6, ret, ctx->address, INET6_ADDRSTRLEN)==NULL) {
                return NULL;
            }
        }

        ret= ctx->address;
    }

    return ret;
}

char* get_cname(dns_t* ctx)
{
    if (ctx==NULL
        || ctx->state!=eDnsSuccess
        || ctx->result==NULL)
    {
        return NULL;
    }

    return ctx->result->canonname;
}

void free_result(dns_t* ctx)
{
    if (ctx->func!=NULL && ctx->state==eDnsInProgress)
    {
        ctx->func(NULL, ctx->user);
    }

    if (ctx->result)
    {
        ub_resolve_free(ctx->result);
    }

    FREE(ctx);
}


static void ub_callback(void* user, int err, struct ub_result* result)
{
    dns_t* ctx= (dns_t*)user;

    //
    // Make sure we have a valid context.
    //
    if(ctx!=NULL)
    {
        ctx->result= result;
        ctx->state= (err!=0)?eDnsFailed:eDnsSuccess;

        if (ctx->func!=NULL)
        {
            //
            // The function is responsible for deleting the context
            //
            ctx->func(ctx, ctx->user);
        }
        else
        {
            free_result(ctx);
        }
    }
}

static void lookup_cleanup(void* data)
{
    lookup_t* ctx= (lookup_t*) data;

    if (ctx!=NULL && ctx->unbound!=NULL)
    {
        ub_ctx_delete(ctx->unbound);
    }
}

static void* lookup_worker(void* data)
{
    lookup_t* ctx= (lookup_t*) data;

    pthread_cleanup_push(lookup_cleanup, ctx);

    while (true)
    {
        if (ctx!=NULL)
        {
            int fd= ub_fd(ctx->unbound);
            fd_set set;

            FD_ZERO(&set);
            FD_SET(fd, &set);

            while (select(FD_SETSIZE, &set, NULL, NULL, NULL)>=0
                   && ub_process(ctx->unbound)==0);
        }
        else
        {
            return NULL;
        }
    }

    pthread_cleanup_pop(true);
    return NULL;
}



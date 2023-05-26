#ifndef __TRACE_H__
#define __TRACE_H__

#include <netinet/in.h>
#include <sys/socket.h>


typedef struct _trace_ctx_s trace_ctx_t;
typedef struct _trace_s trace_t;

typedef struct _trace_data_s
{
    trace_ctx_t* ctx;
    trace_t* trace;

    uint16_t distance;
    struct sockaddr_storage* addr;
} trace_data_t;

/**
 * Callback function for trace information
 *
 * @params trace_data_t* - Data element with information, NULL if trace ended.
 * @params void* - User provided data
 */
typedef void (*trace_cb_t)(trace_data_t* data, void* user);

/**
 * Initializes a trace context, this is later used when executing specific
 * traces.
 *
 * @returns trace_ctx_t* - Trace context
 */
trace_ctx_t* trace_init();

/**
 * Releases trace context, this will terminate any active trace (termination
 * callback will be executed just as when doing a trace_end)
 *
 * @param trace_ctx_t* - Trace context
 */
void trace_release(trace_ctx_t* ctx);

/**
 * Executes a trace
 *
 * @params trace_ctx_t* - Context
 * @params char* - Address
 * @params uint16_t - Max distance
 * @params trace_cb_t* - Callback pointer for trace updates
 * @params void* - User provided data for the trace, passed in callback
 * @returns trace_t* - Assigned trace object
 */
trace_t* trace_start(trace_ctx_t* ctx, char* address,
                     uint16_t distance, trace_cb_t cb, void* user);


/**
 * Terminates an active trace, this will release all allocated resources for
 * the trace. A final trace callback will be executed prior to termination.
 */
void trace_end(trace_t* trace);


#endif

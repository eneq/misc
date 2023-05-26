/**
 * This file contains Configuration Management items for the trace module,
 * this is used to generate default CM data as well as define internal
 * references.
 *
 */
#ifndef _TRACE_CM_H
#define _TRACE_CM_H

#define TRACE_DOMAIN            "trace"

#define TRACE_UDP_SIZE          40
#define TRACE_UDP_PORT          33434
#define TRACE_TIMEOUT           1000
#define TRACE_POOL_SIZE         100
#define TRACE_DISTANCE          10
#define TRACE_RETRIES           3
#define TRACE_ADDRESS           any

#define TRACE_UDP_SIZE_K        "udp size"
#define TRACE_UDP_PORT_K        "base port"
#define TRACE_TIMEOUT_K         "request timeout"
#define TRACE_POOL_SIZE_K       "request limit"
#define TRACE_DISTANCE_K        "hops limit"
#define TRACE_RETRIES_K         "retries"
#define TRACE_ADDRESS_K         "address"

// Undefine USE_CM to no rely on CM component
#define TRACE_USE_CM

#if defined(TRACE_USE_CM) && !defined(CM_MODE)
#include "cm.h"
#endif

// Lookup macro, requires the lookup key to be <k>_K and the default value
// macro to be <k> (as seen in this header file)
#ifdef TRACE_USE_CM
#define TRACE_QUOTE(k) #k
#define TRACE_LOOKUP(k)                                              \
    ({char* t= cm_lookup_value(NULL, TRACE_DOMAIN, k##_K); (t==NULL)?TRACE_QUOTE(k):t;})
#define TRACE_LOOKUP_I(k) \
    ({char* t= cm_lookup_value(NULL, TRACE_DOMAIN, k##_K); (t==NULL)?k:(atoi(t));})
#else
#define TRACE_LOOKUP(k) (#k)
#define TRACE_LOOKUP_I(k) (k)
#endif // TRACE_USE_CM
#endif // _TRACE_CM_H

#ifdef CM_MODE
#define TRACE_OPTION(x,c) CM_OPTION(TRACE_DOMAIN,x##_K,x,c)

TRACE_OPTION(TRACE_UDP_SIZE, \
             "# Size of trace package load")
TRACE_OPTION(TRACE_UDP_PORT, \
             "# Lowest port number for array of ports")
TRACE_OPTION(TRACE_POOL_SIZE, \
             "# Size of trace pool, number of parallell trace queries")
TRACE_OPTION(TRACE_TIMEOUT, \
             "# Query timeout in ms")
TRACE_OPTION(TRACE_DISTANCE, \
             "# Maximum trace distance")
TRACE_OPTION(TRACE_RETRIES, \
             "# Number of times we retry sending a trace probe")
TRACE_OPTION(TRACE_ADDRESS, \
             "# IP to send probe from or 'any'")
#endif


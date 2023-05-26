/**
 * This utility file provides reverse dns lookup functionality, this is in
 * most cases a simple wrapper component interacting with other DNS libraries
 * such as Unbound.
 */

#ifndef __LOOKUP_H__
#define __LOOKUP_H__

typedef enum
{
    eDnsBad,
    eDnsInitialized,
    eDnsFailed,
    eDnsSuccess,
    eDnsTimeout,
    eDnsInProgress,
} dns_state_t;

typedef struct _dns_s dns_t;
typedef void (*dns_cb_t)(dns_t* ctx, void* user);

/**
 * Initializes lookup functionality, this spawns a background thread that
 * manages lookup requests.
 *
 */
void lookup_init();

/**
 * Terminates and frees all resources
 *
 */
void lookup_terminate();

/**
 * Perform a lookup on the provided address, if the func argument is NULL the
 * method will run synchronous otherwise it returns directly and later invokes
 * the provided callback.
 *
 * @param addr          Address to do lookup on
 * @param func          Callback function for asynchronous mode
 * @param user          User provided data passed in callback
 *
 * @return DNS lookup context
 */
dns_t* lookup(char* addr, dns_cb_t func, void* user);

/**
 * Performs a reverse lookup on the provided address, if the func argument is
 * NULL the process will run in synchronous mode otherwise the method returns
 * directly and later invokes the callback function provided.
 *
 * @param addr          Address to do reverse lookup on.
 * @param func          Callback function for asynchronous mode
 * @param user          User provided data passed in callback
 *
 * @return DNS lookup context
 */
dns_t* reverse_lookup(char* addr, dns_cb_t func, void* user);


/**
 * @brief Cancel a pending lookup request.
 *
 * You are guaranteed that the user-specified callback will not
 * be called once this function has completed.
 *
 * If the results are being delivered while this function is called,
 * it will be blocked until the delivery has completed.
 *
 * @param ctx - The context of the pending request.
 */
void lookup_cancel(dns_t *ctx);

/**
 * Retrieves the state from the DNS lookup context
 *
 * @param ctx           DNS lookup context
 *
 * @return strate of lookup
 */
dns_state_t get_state(dns_t* ctx);

/**
 * Retrieves the address from a lookup (the results)
 *
 * @param ctx           DNS lookup context
 *
 * @return character string with address
 */
char* get_address(dns_t* ctx);

/**
 * Retrieves the CName from the DNS lookup.
 *
 * @param ctx           DNS lookup context
 *
 * @return character string with CName
 */
char* get_cname(dns_t* ctx);

/**
 * Frees all resources used in a DNS lookup, no callbacks will be generated
 *
 * @param ctx           DNS lookup context
 */
void free_result(dns_t* ctx);

#endif

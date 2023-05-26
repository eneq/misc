/**
 * This module provides a storage mechanism for generic data providing a key
 * as a lookup identifier. This key is simply a byte buffer of random length.
 *
 * This module in its design is generic but it was first intended for holding
 * NOTX protocol data that were waiting for validation and is designed to be
 * an efficient data store in a highly parallel environment while handling
 * potentially very large data amounts.
 */

#ifndef __STORE_H__
#define __STORE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Typedef for elem delete callback, this is executed for any element being
// deleted from the data store (key, key len, data).
typedef void (*store_delete_cb_t)(void*, void*);

//
// Typedef for elem find callback, this is executed when an element
// is found in the store. The arguments are the (key, data, user).
//
typedef void (*store_find_cb_t)(void*, void*, void*);

// Store context reference
typedef struct store_s store_t;

/**
 * Returns a singleton context, if it doesnt exist we create it.
 *
 * @param key_size      Size of key buffer in bytes
 * @param keybits       Number of bits used by the key each store noe.
 * @param lifespan      Maximum lifespan of a node before pruning
 *
 * @return Singleton reference
 */
store_t* store_singleton(size_t key_size, uint8_t keybits, unsigned int lifespan);


/**
 * Initializes the store, this allocates a context and initializes all
 * relevant data.
 *
 * @param keysize       Size of key buffer in bytes
 * @param keybits       Number of bits used by the key each store noe.
 * @param lifespan      Maximum lifespan of a node before pruning
 *
 * @return Store context
 */
store_t* store_init(size_t keysize, uint8_t keybits, unsigned int lifespan);

/**
 * @brief Get the store key size.
 *
 * @param ctx - Store context, if NULL uses singleton.
 *
 * @return - The store key size.
 */
size_t store_keysize(store_t *ctx);

/**
 * This prunes the store from deleted elements, duplicates and elements with
 * expired timespans.
 *
 * It also collapses tree paths if there are single nodes
 * in the nodes path. This needs to be executed periodically, normally this is
 * done by the store maintenance thread but can also manually be invoked if
 * desired.
 *
 * It should be noted that this enforces a writelock and will not return until
 * that writelock has been achieved and the store pruned.
 *
 * @param ctx           Store context, if NULL uses singleton
 */
void store_prune(store_t* ctx);


/**
 * Deletes the store, terminates thread and releases all resources, element
 * deletion performs as a normal delete scenario with potential callback.
 *
 * @param ctx           Store context, if NULL uses singleton
 */
void store_terminate(store_t* ctx);


/**
 * Adds an key-element to the data store
 *
 * @param ctx           Store context, if NULL uses singleton.
 * @param key           Key buffer, this + len specify the key for the data.
 * @param data          Data ptr for data associated with this key
 * @param del           Callbac function, executed on delete (can be NULL)
 *
 * @return Succes/Fail
 */
bool store_add(store_t* ctx, uint8_t* key, void* data, store_delete_cb_t del);


/**
 * Find an element with the specified key.
 *
 * You are only guaranteed the existence of the element for the duration of
 * the provided callback function. Although the return value indicates the element
 * was found, it does not guarantee it still exists as it may have been pruned
 * away since.
 *
 * @param ctx           Store context, if NULL uses singleton.
 * @param key           Key buffer, this + len specify the key for the data.
 * @param cb            Function to call if the element if found.
 * @param user          User-defined data passed back to the callback.
 *
 * @return True if the element was found.
 */
bool store_find(store_t* ctx, uint8_t* key, store_find_cb_t cb, void *user);


/**
 * Deletes an element with the specified key from the store, this only marks
 * the element for pruning. Callback function would be invoked if specified.
 *
 * @param ctx           Store context, if NULL uses singleton.
 * @param key           Key buffer, this + len specify the key for the data.
 *
 * @returns Succes/Fail
 */
bool store_delete(store_t* ctx, uint8_t* key);

#endif

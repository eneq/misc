#ifndef __LIST_H__
#define __LIST_H__

#include <stddef.h> // size_t

/**
 * External representation of the linked list
 */
typedef void list_handle_t;

/**
 * Callback function provided by client in destroyList() if they need to call
 * free() on the nodes before the node itself is freed.
 */
typedef void (*destructor_cb)(void* data);

/**
 * Creates an empty linked list and returns a handle to it
 *
 * @param destructor    Destructor callback used by clients who wants to free
 *                      Destroys the linked list pointed to by handle
 *
 * @return a list handle reference
 */
const list_handle_t* create_list(destructor_cb destructor);

/**
 * @param handle        Handle to linked list the user data for every
 *                      node. Can be NULL
 *
 */
void destroy_list(const list_handle_t* handle);

/**
 * Adds data to the head of the queue
 *
 * @param handle        Handle to the linked list
 * @param data          Arbitrary data that will be saved in the queue
 *
 * @return 0 if succeeded, -1 for error
 */
int add_to_head(const list_handle_t* handle, void* data);

/**
 * Adds data to the queue tail
 *
 * @param handle        Handle to the linked list
 * @param data          Arbitrary data that will be saved in the queue
 *
 * @return 0 if succeeded, -1 for error
 */
int add_to_tail(const list_handle_t* handle, void* data);

/**
 * Removes data from the head of the queue
 *
 * @param handle        Handle to the linked list
 *
 * @return The data
 */
void* remove_from_head(const list_handle_t* handle);

/**
 * Removes data from the queue tail
 *
 * @param handle        Handle to the linked list
 *
 * @return The data
 */
void* remove_from_tail(const list_handle_t* handle);

/**
 * Removes the data identified by key
 *
 * @param handle        Handle to linked list
 * @param ptr   Key identified in linked list
 *
 * @return 1 if deleted, 0 not deleted
 */
int remove_data_by_key(const list_handle_t* handle, const void* ptr);

/**
 * Retrieves the data identified by key
 *
 * @param handle        Handle to linked list
 * @param ptr           Key identified in linked list
 *
 * @return ptr to data if found, NULL otherwise
 */
void* get_data_by_key(const list_handle_t* handle, const void* ptr);

/**
 * Retrieves the data next to the item identified by key. Can be used to
 * iterate over the linked list.
 *
 * @param handle        Handle to linked list, NULL to fetch first element
 * @param ptr           Key in linked list
 *
 * @return ptr to data if found, NULL otherwise
 */
void* get_next(const list_handle_t* handle, const void* ptr);

/**
 * Returns the current size of the queue
 *
 * @param handle        The linked list handle
 *
 * @return The size of the queue
 */
size_t get_size(const list_handle_t* handle);


/**
 * Callback function used when iterating the list Please check the tests
 * (../test/testList/list_test.c) if it  is unclear how to use it.
 *
 * @param data          Data from the linked list
 * @param userdata      Userdata you want to compare with the data
 *
 * @return NULL if comparison failed, ptr to data otherwise
 */
typedef void* (*iterator_cb)(void* data, void* userdata);


/**
 * iterate function that iterates over all list elements
 * and executes the iteratorCB function.
 * If the callback function returns something != NULL
 * this is the value that will be returned.
 *
 * @param handle        Handle to linked list
 * @param userdata      Handle to userdata to compare with data
 * @param iterator_cb   Callback function
 *
 * @return Pointer to first non NULL data that is returned by the callback
 * function
 */
void* iterate(const list_handle_t* handle, void* userdata, iterator_cb);


/**
 * Enumeration function over the list content, 2nd argument is the last
 * returned or NULL for first element. If callback is provided this
 * enumeration will only occur for nodes that the callback returns NULL.
 *
 * Thus this enumeration can be used for each item by passing NULL as a
 * callback reference or if the callback returns NULL. To iterate over a
 * subset the callback can return !NULL for all items to be skipped.
 *
 * @param handle        Handle to linked list
 * @oaram data          Last returned element or NULL
 * @param iterator_cb   Callback function
 * @param userdata      Handle to userdata to compare with data
 *
 * @return Pointer to first non NULL data that is returned by the callback
 * function starting from the position of the 2nd argument
 */
void* enumerate(const list_handle_t* handle, void* data, iterator_cb, void* user);

#endif //LIST_H

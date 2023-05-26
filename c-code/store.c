/**
 * This is a generic data storage componet designed for large amount of data
 * and thread parallellism. It basically splits the key information to
 * segments of a specific bit length and then uses this to generate a tree of
 * nodes where each level covers <bitlength> of elements.
 *
 * Its similar to a btree but there is no consideration taken to balancing the
 * tree since the original implementation is for randomized keys.
 *
 * To handle synchronization issues and performance these rules are in place;
 * 1. We have a rwlock on the whole dataset
 * 2. wrlock is ONLY used by the pruning thread that runs rarely
 * 3. Deleting nodes simply marks them for pruning.
 * 4. Writing to deleted node saves data and resets prune
 * 5. Adding node DONT use wrlock, it uses GCC atomic functions to insert the
 *    element into the current tree.
 *
 * Since multiple threads can do inserts in parallel we are using atomic
 * functions, however this means that the pointers are valid but not that
 * what they point to is valid. As an example another thread could insert
 * an element at the position you are planning todo an insert in the
 * timespan between identifying the place and actually inserting the new
 * element thus making ordered data impossible without harder locks.
 *
 * Thus we dont do ordered data, data is sorted into smaller buckets thats
 * placed in a tree structure based on the key information. This method is
 * based on extracting a subset of a key (the key is divided into X number of
 * equal bits, as an example a 8 bit key could be divided into 4 2bit pairs).
 *
 * So with a 8bit key using 2bit pairs we would have a tree thats 4 levels
 * deep, instead of potentially comparing 256 items we are down to a maximum
 * of 16 (with a 1bit key down to 8). However when we reach a node without
 * childrens (ie competing keys) we terminate earlier.
 *
 * Its however crucial that the key is as random as possible, if this cannot
 * be guaranteed its suggested that the key is hashed using a hash algorithm
 * that favors uniqueness and randomness (such as Murmur)
 *
 * Since the data tree can become quite large pruning by traversing might
 * become way too costly and lock the read access for too long. Therefore
 * shortcuts need to be in place to handle these maintenance operations.
 *
 * - Timestamp pruning can be done by putting all elements on a flat list that
 *   has the oldest first (atomic functions)
 * - Deleted items are marked and also put onto a deleted items list.
 *
 * With this information pruning is a matter of traversing the timestamp list,
 * all nodes on the deleted items list is a linear search based of the parents
 * children list (simple linked list update).
 *
 * NOTE: When adding node to a leaf, ie converting the node to a leaf the
 * information in the node generates an item in the children list, in other
 * words the leaf data is moved to retain leaf status (although data
 * remains).
 *
 * A complicating factor is when a new leaf is added that has a smaller key
 * length, ie there are keys that are subsets of other keys. In those cases
 * there needs to be a leaf with the small key as well as a tree path through
 * another node that leads to the second leaf.
 */
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <threadpool.h>
#include <stdio.h>

#include "store.h"
#include "bitwise.h"
#include "mem.h"
#include "atomic.h"

#define MASK_TIME_LIST     0x00000001
#define MASK_DELETE_LIST   0x00000002
#define MASK_DEAD          0x00000004

#define NODEP_dlist(x) (x->mask&MASK_DELETE_LIST)
#define NODEP_olist(x) (x->mask&MASK_TIME_LIST)
#define NODEP_dead(x) (x->mask&MASK_DEAD)

/**
 * Data element node, this is the basic node for all elements in the data tree
 */
typedef struct _store_elem_s
{
    struct _store_elem_s* parent;
    struct _store_elem_s* next;
    struct _store_elem_s* childrens;

    // These linked lists are for maintenance only.
    struct _store_elem_s* olist;
    struct _store_elem_s* dlist;

    uint32_t id;
    uint32_t level;
    uint32_t mask;

    bool spin;

    // The following value is only used when there is no contention
    void* data;
    void* key;
    struct _store_elem_s* key_ref;
    time_t ts;
    store_delete_cb_t cb;
} store_elem_t;


/**
 * Context data
 */
struct store_s
{
    store_elem_t* data;
    store_elem_t* expired;

    tp_thread_t* thread;
    pthread_rwlock_t lock;

    size_t key_size;
    uint8_t bits;
    time_t life;
};


// Singleton reference
static store_t* g_singleton= NULL;

// Forward declarations
static store_elem_t* find_node(store_t* ctx, store_elem_t* start, uint8_t* key);
static void store_cleanup(void* data);
static tp_code_t store_thread(void* data);
static void store_unhook(store_elem_t* node);
static void store_release_node(store_elem_t* node);

/**
 * Returns a singleton context, if it doesnt exist we create it.
 * @param size_t - Size of key buffer in bytes
 * @param uint8_t - Number of bits used by the key each store noe.
 * @param unsigned int - Maximum lifespan of a node before pruning (seconds)
 * @returns store_t* - Singleton reference
 */
store_t* store_singleton(size_t keysize, uint8_t keybits, unsigned int lifespan)
{
    if (g_singleton==NULL)
    {
        g_singleton= store_init(keysize, keybits, lifespan);
    }

    return g_singleton;
}


/**
 * Initializes the store, this allocates a context and initializes all
 * relevant data.
 * @param size_t - Size of key buffer in bytes
 * @param uint8_t - Number of bits used by the key each store noe.
 * @param unsigned int - Maximum lifespan of a node before pruning (seconds)
 * @returns store_t* - Context
 */
store_t* store_init(size_t keysize, uint8_t keybits, unsigned int lifespan)
{
    store_t* ctx= NULL;
    store_elem_t* root= NULL;

    ctx= ALLOC(sizeof(*ctx));
    if (ctx==NULL)
    {
        return NULL;
    }

    root= ALLOC(sizeof(*root));
    if (root==NULL)
    {
        FREE(ctx);
        return NULL;
    }

    memset(ctx, 0, sizeof(*ctx));
    memset(root, 0, sizeof(*root));
    root->id= -1;
    root->level= -1;

    ctx->thread= tp_request_thread(0, store_thread, ctx);
    ctx->key_size= keysize;
    ctx->bits= keybits;
    ctx->life= lifespan;
    ctx->data= root;

    // Initialize pthread rwlock, writer always has preference. We need this to
    // avoid starvation and also the write lock is only used when pruning
    // which is rare compared to read.
    /*
        pthread_rwlockattr_t attr;
        pthread_rwlockattr_init (&attr);
        pthread_rwlockattr_setkind_np (&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
        pthread_rwlock_init(&ctx->lock, &attr);
    */
    pthread_rwlock_init(&ctx->lock, NULL);
    return ctx;
}


/**
 * Deletes the store, terminates thread and releases all resources, element
 * deletion performs as a normal delete scenario with potential callback.
 *
 * @param store_t* - Store context, if NULL uses singleton
 */
void store_terminate(store_t* ctx)
{
    //
    // Check for singleton usage.
    //
    if (ctx==NULL)
    {
        ctx = g_singleton;
        g_singleton = NULL;
    }

    //
    // In singleton case, the singleton may never have been
    // initialized.
    //
    if(ctx != NULL)
    {
        tp_release_thread(ctx->thread);
        tp_wait_for_thread(ctx->thread);

        if (ctx->data!=NULL)
        {
            FREE(ctx->data);
        }

        // Cleanup of store data is done in thread cleanup
        pthread_rwlock_destroy(&ctx->lock);
        FREE(ctx);
    }
}

size_t store_keysize(store_t *ctx)
{
    size_t retval = 0;

    if (ctx==NULL)
    {
        ctx= g_singleton;
    }

    retval = ctx->key_size;

    return retval;
}

bool store_add(store_t* ctx, uint8_t* key, void* data, store_delete_cb_t del)
{
    bool ret= false;

    if (ctx==NULL)
    {
        ctx= g_singleton;
    }

    // Prep element
    store_elem_t* el= ALLOC(sizeof(*el));
    if (el==NULL)
    {
        return false;
    }

    memset(el, 0, sizeof(*el));
    el->key= ALLOC(ctx->key_size);

    if (el->key==NULL)
    {
        FREE(el);
        return false;
    }

    el->data= data;
    el->cb= del;
    memcpy(el->key, key, ctx->key_size);
    el->key_ref= el;
    el->ts= time(NULL);
    el->mask|= MASK_TIME_LIST;

    // Find insert node
    pthread_rwlock_rdlock(&ctx->lock);
    while (!ret)
    {
        store_elem_t* node= find_node(ctx, NULL, key);
        store_elem_t* inject= NULL;

        // If find_node returns a leaf then it means the leafs key matches our key
        // to same depth in the stores data tree. If this happens then we need to
        // push the found key deeper down until the keys are different,
        // i.e. find_node returns a node with childrens.
        if (!node->childrens && node!=ctx->data)
        {
            // Key exists in store, ret fail
            if (node->key_ref->key!=NULL
                && memcmp(node->key_ref->key, key, ctx->key_size)==0)
            {
                break;
            }

            if (inject==NULL)
            {
                inject= ALLOC(sizeof(*inject));
                if (inject==NULL)
                {
                    break;
                }

                memset(inject, 0, sizeof(*inject));
            }

            inject->data= node->data;
            inject->cb= node->cb;
            inject->key_ref= node->key_ref;
            inject->ts= node->ts;
            inject->parent= node;
            inject->level= node->level+1;
            inject->id= (uint32_t)get_bits(inject->key_ref->key, inject->level*ctx->bits, ctx->bits);

            ATOMIC_spin(&node->spin);

            // If we suddenly got a child reiterate the process otherwise
            // inject the parent as a child
            if (!node->childrens)
            {
                node->childrens= inject;
                inject= NULL;
            }

            ATOMIC_release(&node->spin);
            continue;
        }

        // At this point we now that the node returned is the node our data
        // should be added to, lock it and check if we had a race duplicate.
        ATOMIC_spin(&node->spin);

        el->parent= node;
        el->level= node->level+1;
        el->id= get_bits(el->key_ref->key, el->level*ctx->bits, ctx->bits);

        store_elem_t* tmp= node->childrens;
        while (tmp && (tmp->id!=el->id || NODEP_dlist(tmp)))
        {
            tmp= tmp->next;
        }

        if (tmp)   // Duplicate
        {
            ATOMIC_release(&node->spin);
            continue;
        }

        el->next= node->childrens;
        node->childrens= el;
        ATOMIC_release(&node->spin);

        // Maintenance time ordered list, used for pruning.
        ATOMIC_store(&ctx->data->olist, &el, &el->olist);
        ret= true;
    }

    pthread_rwlock_unlock(&ctx->lock);

    return ret;
}


/**
 * Finds an element with the specified key and returns the data ptr or NULL.
 *
 * @param store_t* - Store context, if NULL uses singleton.
 * @param uint8_t* - Key buffer, this + len specify the key for the data.
 * @param size_t - Size of key buffer in bytes
 * @returns void* - Ptr to associated data
 */
bool store_find(store_t* ctx, uint8_t* key, store_find_cb_t cb, void *user)
{
    bool retval = false;

    //
    // Fallback to the singleton.
    //
    if (ctx == NULL)
    {
        ctx = g_singleton;
    }

    //
    // Lock the store.
    //
    pthread_rwlock_rdlock(&ctx->lock);

    store_elem_t* node= find_node(ctx, NULL, key);

    // If its not root
    if (node->key_ref != NULL &&
        node->key_ref->key!=NULL &&
        memcmp(node->key_ref->key, key, ctx->key_size)==0)
    {
        if(node->childrens == NULL)
        {
            //
            // Call the user-supplied callback if it is valid.
            //
            if(cb != NULL)
            {
                cb(key, node->data, user);
            }

            retval = true;
        }
    }

    //
    // Unlock the store.
    //
    pthread_rwlock_unlock(&ctx->lock);

    return retval;
}


/**
 * Deletes an element with the specified key from the store, this only marks
 * the element for pruning. Callback function would be invoked if specified.
 *
 * @param store_t* - Store context, if NULL uses singleton.
 * @param uint8_t* - Key buffer, this + len specify the key for the data.
 * @param size_t - Size of key buffer in bytes
 * @returns bool - Succes/Fail
 */
bool store_delete(store_t* ctx, uint8_t* key)
{
    bool res= false;

    if (ctx==NULL)
    {
        ctx= g_singleton;
    }

    if (key==NULL)
    {
        return false;
    }

    pthread_rwlock_rdlock(&ctx->lock);

    store_elem_t* node= find_node(ctx, NULL, key);

    // If we have childrens then store_find couldnt find it.
    if (node->parent!=NULL && node->childrens==NULL)
    {
        if (node->key_ref->key!=NULL && memcmp(key, node->key_ref->key, ctx->key_size)==0)
        {
            ATOMIC_spin(&node->spin);
            if (!NODEP_dlist(node))
            {
                node->mask|= MASK_DELETE_LIST;

                ATOMIC_store(&ctx->data->dlist, &node, &node->dlist);
            }
            ATOMIC_release(&node->spin);

            res= true;
        }
    }

    pthread_rwlock_unlock(&ctx->lock);

    return res;
}


/**
 * This prunes the store from deleted elements, duplicates and elements with
 * expired timespans. It also collapses tree paths if there are single nodes
 * in the nodes path. This needs to be executed periodically, normally this is
 * done by the store maintenance thread but can also manually be invoked if
 * desired.
 *
 * It should be noted that this enforces a writelock and will not return until
 * that writelock has been achieved and the store pruned.
 *
 * For efficiency we split this into two phases, first phase is unhooking the
 * deleted data from the store and the second phase is about freeing said
 * data. Any specified callbacks will be initiated during this second phase.
 *
 * @param store_t* - Store context, if NULL uses singleton
 */
void store_prune(store_t* ctx)
{
    if (ctx==NULL)
    {
        ctx= g_singleton;
    }

    pthread_rwlock_wrlock(&ctx->lock);

    store_elem_t* deleted= ctx->data->dlist;
    ctx->data->dlist= NULL;

    while (deleted!=NULL)
    {
        store_elem_t* node= deleted;
        deleted= deleted->dlist;

        // Unhook the node from the parent/sibling chain (keeping references)
        store_unhook(node);

        // If our unhook left the parent without any childrens then we add it
        // to the deletion list (exclude root node)
        if (!node->parent->childrens && node->parent->parent)
        {
            node->parent->mask|= MASK_DELETE_LIST;
            node->parent->cb= NULL;

            node->parent->dlist= deleted;
            deleted= node->parent;
        }

        if (!NODEP_olist(node))
        {
            store_release_node(node);
        }
        else
        {
            node->mask&= ~MASK_DELETE_LIST;
            node->mask|= MASK_DEAD;
        }
    }

    pthread_rwlock_unlock(&ctx->lock);
}

void store_release_node(store_elem_t* node)
{
    // Free key and initiate callback for leafs.
    if (node->childrens==NULL)
    {
        if (node->key_ref->key!=NULL && node->cb!=NULL)
        {
            node->cb(node->key_ref->key, node->data);
        }

        if (node->key_ref->key!=NULL)
        {
            FREE(node->key_ref->key);
            node->key_ref->key= NULL;
        }
    }
    FREE(node);
}

/**
 * Finds the node at the bottom of the traversal path specified by the key,
 * this might or might not match the key exactly depending on whether the key
 * exists in the tree.
 * @param store_t* - Store context, if NULL uses singleton.
 * @param store_elem_t* - Start node for the search
 * @param uint8_t* - Key buffer, this + len specify the key for the data.
 * @param size_t - Size of key buffer in bytes
 * @returns store_elem_t* - Ptr to closest node
 */
store_elem_t* find_node(store_t* ctx, store_elem_t* start, uint8_t* key)
{
    uint32_t index= 0;
    store_elem_t* node= ctx->data->childrens;
    store_elem_t* ret= ctx->data;

    // If we start at a specific node then calculate where
    // we are in the key bitstream
    if (start!=NULL && start->parent!=NULL)
    {
        store_elem_t* t= start;
        node= start;

        // Root is a dummy so all has min 1 parent.
        while (t->parent->parent!=NULL)
        {
            index+= ctx->bits;
            t= t->parent;
        }
    }

    while (node!=NULL && index<(ctx->key_size*8))
    {
        unsigned int id= get_bits(key, index, ctx->bits);

        while (node!=NULL && (node->id!=id || NODEP_dlist(node)))
        {
            node= node->next;
        }

        // Id isnt in list return parent
        if (node==NULL)
        {
            return ret;
        }

        // Id is in list and no children, this is as close as we get.
        if (node->id==id && node->childrens==NULL)
        {
            return node;
        }

        // Check childrens if they contain the next bit sequence
        ret= node;
        node= node->childrens;
        index+= ctx->bits;
    }

    // Should only happen when store is empty
    return ctx->data;
}


/**
 * Store background thread, this is responsible for termination cleanup and
 * pruning the data set at regular intervals. The maintenance is built on two
 * linked lists of data that is stored in parallell with the normal data
 * stores tree implementation. One of this lists is time ordered and used to
 * prune data base on time and the other list is a linked list of all deleted
 * items (or rather marked for deletion).
 *
 * @param void* - User data (context in this case).
 */
tp_code_t store_thread(void* data)
{
    store_t* ctx= (store_t*) data;
    time_t delta= 0;

    pthread_cleanup_push(store_cleanup, data);

    while (true)
    {
        sleep(ctx->life);

        delta= time(NULL);

        pthread_rwlock_rdlock(&ctx->lock);

        // We'll take the ordered list of new entries and put the reference
        // into the expired pointer, then well sleep for the lifespan of an
        // element at which time they should be deleted.

        store_elem_t* olist= NULL;
        store_elem_t* elnull= NULL;
        ATOMIC_store(&ctx->data->olist, &elnull, &olist);

        store_elem_t* expired= ctx->expired;
        ctx->expired= olist;

        // Delete all nodes that has expired.
        while (expired!=NULL)
        {
            store_elem_t* node= expired;
            expired= expired->olist;

            // store_delete will fail if the node is already deleted.
            node->olist= NULL;

            if (NODEP_dead(node))
            {
                store_release_node(node);
            }
            else
            {
                node->mask&= ~MASK_TIME_LIST;
                if (node->key_ref->key)
                {
                    store_delete(ctx, node->key_ref->key);
                }
            }
        }

        pthread_rwlock_unlock(&ctx->lock);

        // Clean tree will engage write lock
        store_prune(ctx);

        printf("Store pruned in %d seconds, sleeping for %d\n", (int)(time(NULL)-delta), (int)ctx->life);
    }

    pthread_cleanup_pop(true);
    return TP_OK;
}


/**
 * Cleanup function the the store thread, this releases all data associated
 * with the store, this should only occur when the overall software is
 * terminating. The behavior of additional store operations after this cleanup
 * is undefined.
 */
void store_cleanup(void* data)
{
    store_t* ctx= (store_t*) data;

    store_prune(ctx);

    pthread_rwlock_wrlock(&ctx->lock);

    store_elem_t* el= ctx->data;

    while (el!=NULL)
    {
        // Release from the bottom up
        if (el->childrens)
        {
            el= el->childrens;
            continue;
        }

        // If element has no parent then only root remains
        store_elem_t* parent= el->parent;
        if (!parent)
        {
            break;
        }

        // Unhook element from tree, element is always first in
        // the sibling list per design.
        parent->childrens= el->next;

        // We are a leaf release data and do cb if needed.
        if (!el->childrens)
        {
            if (el->key_ref->key!=NULL && el->cb!=NULL)
            {
                el->cb(el->key_ref->key, el->data);
            }

            FREE(el->key_ref->key);
            el->key_ref->key= NULL;
        }
        FREE(el);

        // Next sibling if it exists, otherwise parent.
        if (parent->childrens!=NULL)
        {
            el= parent->childrens;
        }
        else
        {
            el= parent;
        }
    }

    FREE(ctx->data);
    ctx->data= NULL;

    pthread_rwlock_unlock(&ctx->lock);
}


/**
 * Unhooks a node from its parents children list and updates the counters,
 * write lock need to be aquired before calling this function.
 *
 * @param store_elem_t* - Element to be removed from the parent
 */
void store_unhook(store_elem_t* node)
{
    store_elem_t* siblings= node->parent->childrens;
    if (node==siblings)
    {
        node->parent->childrens= node->next;
    }
    else
    {
        while (siblings!=NULL && siblings->next!=node)
        {
            siblings= siblings->next;
        }

        if (siblings!=NULL)
        {
            siblings->next= node->next;
        }
    }

    node->next= NULL;
}


void store_dump(store_t* ctx, store_elem_t* node)
{
    store_elem_t* parent= node->parent;

    printf("[store] ");
    while (parent!=NULL)
    {
        printf(" ");
        parent= parent->parent;
    }

    printf("[%d:%c, olist=%p, dlist=%p, id=%d]\n",
           node->childrens?'N':'L',
           node->level,
           (void*)node->olist,
           (void*)node->dlist,
           node->id);

    node= node->childrens;
    while (node!=NULL)
    {
        store_dump(ctx, node);
        node= node->next;
    }
}

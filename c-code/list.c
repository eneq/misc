#include "list.h"
#include "logger.h"
#include "mem.h"

/**
 * The linked list node containing the user data
 */
typedef struct node
{
    struct node* next;
    struct node* prev;
    void* data;
} node_t;

/**
 * A linked list containing pointers to first and last node
 */
typedef struct
{
    node_t* head;
    node_t* tail;
    int size;
    destructor_cb destructor;
} list_t;


/**
 *
 *
 * @param destructor
 *
 * @return
 */
const list_handle_t* create_list(destructor_cb destructor)
{
    list_t* list = ALLOC(sizeof(*list));
    if (NULL != list)
    {
        list->head = NULL;
        list->tail = NULL;
        list->size = 0;
        list->destructor = destructor;
    }
    return list;
}

/**
 *
 *
 * @param handle
 */
void destroy_list(const list_handle_t* handle)
{
    list_t* list = (list_t*) handle;

    if (list == NULL)
    {
        return;
    }

    node_t* head = list->head;
    node_t* curr = NULL;

    while (head != NULL)
    {
        curr= head;
        head= head->next;
        list->size--;

        if (NULL != list->destructor && curr->data)
        {
            list->destructor(curr->data);
        }

        FREE(curr);
    }

    if (list->size != 0)
    {
        LOG("BUG in list implementation: %d", list->size);
    }

    FREE(list);
}

/**
 *
 *
 * @param handle
 * @param data
 *
 * @return
 */
int add_to_head(const list_handle_t* handle, void* data)
{
    int res = -1;
    list_t* list = (list_t*) handle;

    if (NULL == list || NULL == data)
    {
        return res;
    }

    node_t* new_node = ALLOC(sizeof(*new_node));

    if (NULL != new_node)
    {
        new_node->data = data;
        new_node->next = list->head;
        new_node->prev = NULL;

        if (list->head != NULL)
        {
            list->head->prev = new_node;
        }
        list->head = new_node;
        list->size++;

        /**
         * If this was the first node to add,
         * point the tail to the same node
         **/
        if (NULL == list->tail)
        {
            list->tail = list->head;
        }

        res = 0;
    }

    return res;
}

/**
 *
 *
 * @param handle
 * @param data
 *
 * @return
 */
int add_to_tail(const list_handle_t* handle, void* data)
{

    int res = -1;
    list_t* list = (list_t*) handle;

    if (NULL == list || NULL == data)
    {
        return res;
    }

    node_t* new_node = ALLOC(sizeof(*new_node));

    if (NULL != new_node)
    {
        new_node->data = data;
        new_node->next = NULL;
        new_node->prev = list->tail;

        // Currently last node must point to the new last node
        if (list->tail != NULL)
        {
            list->tail->next = new_node;
        }
        list->tail = new_node;
        list->size++;

        /**
         * If this was the first node to add,
         * point the tail to the same node
         **/
        if (NULL == list->head)
        {
            list->head = list->tail;
        }

        res = 0;
    }
    return res;
}

static void remove_node_by_node(const list_handle_t* handle, node_t* node)
{
    list_t* list = (list_t*) handle;

    node_t* head= list->head;
    int remove= 0;

    if (head==node)
    {
        list->head= head->next;
        remove= 1;
    }
    else {
        while (head && head!=node) head= head->next;

        if (head)
        {
            head->prev->next= head->next;
            if (head->next)
            {
                head->next->prev= head->prev;
            }

            remove= 1;
        }
    }

    if (remove)
    {
        if (NULL != list->destructor)
        {
            list->destructor(head->data);
        }

        list->size--;
        FREE(head);

        /**
         * If this was the last node to delete
         * Modify tail accordingly
         */
        if (list->head==NULL || list->tail==NULL)
        {
            list->tail= list->head= NULL;
        }
    }
}

/**
 *
 *
 * @param handle
 * @param ptr
 * @param remove
 * @param pp_index
 *
 * @return
 */
static int get_node_by_key(const list_handle_t* handle, const void* ptr, int remove, node_t** pp_index)
{
    list_t* list = (list_t*) handle;

    if (NULL == list || NULL == ptr || pp_index == NULL || list->head==NULL)
    {
        return 0;
    }

    node_t* head = list->head;
    *pp_index = NULL;
    int found = 0;

    /* Head is a special case*/
    if (head->data == ptr)
    {
        found= 1;
        *pp_index = head;
    }
    else
    {
        /* General case */
        while (NULL != head)
        {
            if (head->data == ptr)
            {
                found = 1;
                *pp_index = head;
                break;
            }

            head= head->next;
        }
    }

    if (remove)
    {
        remove_node_by_node(handle, *pp_index);
    }

    return found;
}

/**
 *
 *
 * @param handle
 * @param ptr
 *
 * @return
 */
int remove_data_by_key(const list_handle_t* handle, const void* ptr)
{
    node_t* p_handle = NULL;

    if (NULL == handle || NULL == ptr)
    {
        return 0;
    }

    return get_node_by_key(handle, ptr, 1, &p_handle);
}

/**
 *
 *
 * @param handle
 * @param ptr
 *
 * @return
 */
void* get_data_by_key(const list_handle_t* handle, const void* ptr)
{
    node_t* p_node = NULL;
    void* res = NULL;

    if (NULL == handle || NULL == ptr)
    {
        return res;
    }

    get_node_by_key(handle, ptr, 0, &p_node);

    if (NULL != p_node)
    {
        res = p_node->data;
    }
    return res;
}

/**
 *
 *
 * @param handle
 * @param ptr
 *
 * @return
 */
void* get_next(const list_handle_t* handle, const void* ptr)
{
    list_t* list = (list_t*) handle;

    node_t* p_node = NULL;
    void* res = NULL;

    if (NULL == list)
    {
        return res;
    }

    if (NULL == ptr)
    {
        p_node = list->head;
    }
    else
    {
        get_node_by_key(handle, ptr, 0, &p_node);
        if (NULL != p_node)
        {
            p_node = p_node->next;
        }
    }

    if (NULL != p_node)
    {
        res = p_node->data;
    }
    return res;
}

/**
 *
 *
 * @param handle
 *
 * @return
 */
void* remove_from_head(const list_handle_t* handle)
{
    list_t* list = (list_t*) handle;

    if (NULL == list)
    {
        return NULL;
    }

    node_t* head = list->head;
    void* data = NULL;

    if (NULL != list->head)
    {
        data = head->data;
        list->head = list->head->next;
        FREE(head);
        list->size--;

        /**
         * If this was the last node to delete
         * Modify tail accordingly
         */
        if (list->head == NULL)
        {
            list->tail = NULL;
        }
    }
    return data;
}

/**
 *
 *
 * @param handle
 *
 * @return
 */
void* remove_from_tail(const list_handle_t* handle)
{
    list_t* list = (list_t*) handle;

    if (NULL == list)
    {
        return NULL;
    }

    node_t* tail = list->tail;
    void* data = NULL;

    if (NULL != list->tail)
    {
        data = tail->data;
        list->tail = list->tail->prev;
        FREE(tail);
        list->size--;

        /**
         * If this was the last node to delete
         * Modify head accordingly
         */
        if (list->tail == NULL)
        {
            list->head = NULL;
        }
    }
    return data;
}

/**
 *
 *
 * @param handle
 *
 * @return
 */
size_t get_size(const list_handle_t* handle)
{
    list_t* list = (list_t*)handle;
    if (NULL == list)
    {
        return 0;
    }

    return list->size;
}

/**
 *
 *
 * @param handle
 * @param userdata
 * @param callback
 *
 * @return
 */
void* iterate(const list_handle_t* handle, void* userdata, iterator_cb callback)
{
    void* res = NULL;
    list_t* list = (list_t*)handle;
    if (NULL == list)
    {
        return res;
    }

    node_t* iter = list->head;

    while (NULL == res && iter != NULL)
    {
        res = callback(iter->data, userdata);
        iter = iter->next;
    }

    return res;
}


/**
 * Enumeration function over the list content, 2nd argument is the last
 * returned or NULL for first element. If callback is provided this
 * enumeration will only occur for nodes that the callback returns !NULL.
 *
 * Thus this enumeration can be used for each item by passing NULL as a
 * callback reference or if the callback returns !NULL for all items.
 * To iterate over a subset the callback can return NULL for all items to be
 * skipped.
 *
 * @param handle        Handle to linked list
 * @oaram data          Last returned element or NULL
 * @param iterator_cb   Callback function
 * @param userdata      Handle to userdata to compare with data
 *
 * @return Pointer to first non NULL data that is returned by the callback
 * function starting from the position of the 2nd argument
 */
void* enumerate(const list_handle_t* handle, void* data, iterator_cb callback, void* user)
{
    void* res = NULL;
    list_t* list = (list_t*)handle;
    if (NULL == list)
    {
        return res;
    }

    node_t* iter = list->head;
    if (data!=NULL)
    {
        while (iter!=NULL && iter!=data) iter= iter->next;
        if (iter!=NULL)
        {
            iter= iter->next;
            res= iter;
        }
    }

    while (callback!=NULL && NULL == res && iter != NULL)
    {
        res = callback(iter->data, user);
        iter = iter->next;
    }

    return res;
}

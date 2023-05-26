#include <string.h> // memset
#include <assert.h>

#include "mem_mgr.h"
#include "logger.h"


/* Size of the total memory */
#define SIZE 4096

/* The memory space*/
static char mem[SIZE] = {0};

/* The boundary for every allocation*/
#define ALIGNMENT (4)

/* Size of the header struct*/
#define HEADERSIZE sizeof(mem_block)

/* Whether or not the memory area has run init*/
static int init = 0;

/* Lists of allocated/free nodes */
static mem_block* p_allocated = NULL;
static mem_block* p_free = NULL;

static void init_mem()
{
    init = 1;
    mem_block* p_tmp = (mem_block*)&mem[0];
    memset(p_tmp, 0x00, HEADERSIZE);
    p_tmp->data_len = SIZE - HEADERSIZE;
    p_free = (mem_block*)mem;
}

/**
 * Remove a pointer from a list
 * @param ppList - pointer to pointer to list
 * @param pData - Pointer to data to remove
 */
static int remove_from_list(mem_block** pp_list, mem_block* p_data)
{
    int removed = 0;

    if (NULL == pp_list || NULL == *pp_list)
    {
        LOG0("head is null, early exit\n");
        return 0;
    }

    /* Head is a special case*/
    if (*pp_list == p_data)
    {
        *pp_list = (*pp_list)->p_next;
        removed = 1;
    }

    /* General case */
    if (!removed)
    {
        mem_block* p_curr = (*pp_list)->p_next;
        mem_block* p_prev = *pp_list;
        while (NULL != p_curr && !removed)
        {
            if (p_curr == p_data)
            {
                p_prev->p_next = p_curr->p_next;
                removed = 1;
            }

            p_prev = p_curr;
            p_curr = p_curr->p_next;
        }
    }
    return removed;
}

static mem_block* split_mem_blk(mem_block* p_split, size_t size)
{
    // Adjust the size for alignment
    size += (size%ALIGNMENT) ? (ALIGNMENT-(size%ALIGNMENT)) : 0;
    size_t alloc_size = size + HEADERSIZE;

    mem_block* new_blk = (mem_block*)((char*)p_split + HEADERSIZE + p_split->data_len - alloc_size);
    p_split->data_len = p_split->data_len - alloc_size;

    new_blk->data_len = size;
    new_blk->state = ALLOCATED;

    return new_blk;
}

void* m_alloc(size_t size)
{
    void* result = NULL;

    if (0 == size)
    {
        LOG0("Trying to allocate zero bytes\n");
    }

    if (!init)
    {
        init_mem();
    }

    /**
     * To split at all, we need to have space for:
     * old header | ALIGNMENT bytes
     * new header | size
     * The old header is already present, so we dont need
     * to take that into account
     */
    size_t space_needed_to_split = size + HEADERSIZE + ALIGNMENT;

    mem_block* p_tmp = p_free;

    int split = 1;

    while (NULL != p_tmp)
    {
        /* Is this block big enough to split?*/
        if (space_needed_to_split < p_tmp->data_len)
        {
            break;
        }

        /* Not split-able but big enough to fit the data*/
        if (size < p_tmp->data_len)
        {
            split = 0;
            break;
        }
        p_tmp = p_tmp->p_next;
    }

    if (NULL != p_tmp)
    {
        mem_block* p_new = p_tmp;

        if (split)
        {
            p_new = split_mem_blk(p_tmp, size);
        }
        else
        {
            remove_from_list(&p_free, p_new);
            p_new->state = ALLOCATED;
        }

        p_new->p_prev = NULL;
        p_new->p_next = p_allocated;

        if (p_allocated != NULL)
        {
            p_allocated->p_prev = p_new;
        }

        p_allocated = p_new;
        result = (void*)((char*)p_new + HEADERSIZE);
    }

    return result;
}

/**
 * Merges blocks form "mergeFrom" and forward.
 *
 */
void merge_free_blk(mem_block* merge_from)
{
    mem_block* prev = merge_from;

    while (NULL != prev)
    {
        mem_block* cur = (mem_block*)((char*)prev + HEADERSIZE + prev->data_len);

        if (NULL != cur && (char*)cur < &mem[SIZE-1])
        {
            int merged = 0;
            if (FREE == prev->state && FREE == cur->state)
            {
                if (remove_from_list(&p_free, cur))
                {
                    prev->data_len += cur->data_len + HEADERSIZE;
                    merged = 1;
                }
            }
            /**
             * If merged, try with same prev and new current
             * If not merged, change prev variable
             */

            if (!merged)
            {
                prev = (mem_block*)((char*)prev + HEADERSIZE + prev->data_len);
            }
        }
        else
        {
            prev = NULL;
            cur = NULL;
        }

    }
}

void m_free(void* p)
{
    mem_block* p_block = (mem_block*)((char*)p - HEADERSIZE);

    if (!remove_from_list(&p_allocated, p_block))
    {
        LOG("Calling free on non allocated memory %p %p\n", p, (void*)p_block);
        assert(0);
        return;
    }

    p_block->state = FREE;
    p_block->p_next = p_free;

    if (p_free != NULL)
    {
        p_free->p_prev = p_block;
    }

    p_free = p_block;
    merge_free_blk((mem_block*)mem);
}

// Below is for debugging

mem_block* get_free_pointer()
{
    return p_free;
}

mem_block* get_allocated_pointer()
{
    return p_allocated;
}

void* get_mem_area()
{
    return mem;
}

size_t get_mem_size()
{
    return SIZE;
}

/**
 * This is a small memory manager built for handling very small memory
 * sizes. Its primarily designed to be used by a task running inside the
 * taskmanager where each task has a dedicated memory area (usually well below
 * 5k) and that still need some dynamic memory management.
 */


#ifndef __MEMMGR_H__
#define __MEMMGR_H__

/**
 * Allocates memory
 *
 * @param size - number of bytes to allocate
 *
 * @return - ptr to memory area if succeeds, NULL otherwise
 */
void* m_alloc(size_t size);

/**
 * Frees the memory allocated using m_alloc
 * @param ptr - The memory to free
 */
void m_free(void* ptr);


// Just debugging info so far
typedef enum
{
    FREE = 0x0,
    ALLOCATED = 0x1
} memstate;

typedef struct _mem_unit
{
    int data_len;
    memstate state;
    struct _mem_unit* p_prev, *p_next;
} mem_block;

mem_block* get_free_pointer();
mem_block* get_allocated_pointer();
void* get_mem_area();
size_t get_mem_size();
void merge_free_blk(mem_block* merge_from);

#endif

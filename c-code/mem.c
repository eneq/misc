#include <stdlib.h> //malloc
#include <string.h> //strncpy
#include <pthread.h>
#include <stdint.h>
#include "mem.h"
#include "logger.h"

#define FILE_LEN (15)
typedef struct node
{
    void* address;
    struct node* next;
    struct node* prev;
    char file[FILE_LEN + 1];
    unsigned int line;
    unsigned int size;
    uint32_t pattern;
} node_t;

static node_t* head = NULL;
static int count;
static pthread_mutex_t mtx= PTHREAD_MUTEX_INITIALIZER;

#define LOCK() {pthread_mutex_lock(&mtx);}
#define RELEASE() {pthread_mutex_unlock(&mtx);}

void* alloc_mem(size_t size, const char* file, unsigned int line)
{
    void* ptr = malloc(size + sizeof(node_t));
    node_t* p = ptr;

    if (p==NULL)
    {
        return NULL;
    }

    LOCK();

    memset(p, 0x00, size + sizeof(node_t));
    if (head!=NULL)
    {
        head->prev= p;
    }
    p->next = head;
    p->address = (char*)p + sizeof(node_t);
    strncpy(p->file, file, FILE_LEN);
    p->file[FILE_LEN] = '\0';
    p->size = size;
    p->line = line;
    p->pattern= 0x0aa00aa0;
    head = p;

    p = p->address;
    count++;

    RELEASE();

    return p;
}

void* realloc_mem( void* cur, size_t nsize, size_t osize )
{

    void* tmp = NULL;

    if ( cur )
    {
        if ( nsize >= osize )
        {
            tmp = ALLOC( nsize + 1 );
            if ( tmp )
            {
                strcpy( tmp, cur );
            }
            else
            {
                // NULL will be returned
            }
        }
        else
        {
            // NULL will be returned
        }
        FREE( cur );
    }
    else
    {
        if ( osize == 0 )
        {
            tmp = ALLOC( nsize );
        }
        else
        {
            // NULL will be returned
        }
    }

    return ( tmp );
}

void free_mem(void* ptr, const char* file, unsigned int line)
{
    node_t* p = (node_t*) ((char*)ptr - sizeof(node_t));
    int found = 0;

    if (ptr==NULL || head==NULL)
    {
        return;
    }

    LOCK();

    if (p->pattern==0x0bb00bb0)
    {
        printf("Mem: Freeing freed memory @ %s:%u\n", file, line);
        printf("%s:%u %u bytes %p\n", p->file, p->line, p->size, p->address);
    }
    else if (p->pattern!=0x0aa00aa0)
    {
        printf("Mem: Memory corrupted, detected @ %s:%u", file, line);
        printf("%s:%u %u bytes %p\n", p->file, p->line, p->size, p->address);
        return;
    }
    else
    {
        p->pattern= 0x0bb00bb0;

        node_t* prev = p->prev;

        /* Special case */
        if (head->address == ptr)
        {
            head = head->next;
            found = 1;

            if (head!=NULL)
            {
                head->prev= NULL;
            }
        }

        if (prev!=NULL)
        {
            prev->next = p->next;
        }

        if (p->next!=NULL)
        {
            p->next->prev= p->prev;
        }

        found = 1;

        memset(p->address, 0, p->size);

        free(p);
        count--;
    }

    RELEASE();

    if (!found)
    {
        SLOG(LOG_DEBUG, "Trying to free invalid pointer %p", (void*)p);
    }
}

void printlist()
{
    LOCK();

    node_t* tmp = head;

    SLOG0(LOG_DEBUG, "\n=======BEGIN=====");
    SLOG(LOG_DEBUG, "count = %d", count);
    while (tmp)
    {
        SLOG(LOG_DEBUG, "%s [%u] %u bytes %p", tmp->file, tmp->line, tmp->size, tmp->address);
        tmp = tmp->next;
    }
    SLOG0(LOG_DEBUG, "=======END=======\n");

    RELEASE();
}

int get_number_of_allocs()
{
    return count;
}

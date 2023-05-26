#ifndef __ALLOCATE_H__
#define __ALLOCATE_H__

#include <stddef.h>
#include <stdlib.h>

void* alloc_mem(size_t size, const char* file, unsigned int line);
void* realloc_mem( void* cur, size_t nsize, size_t osize );
void free_mem(void* ptr, const char* file, unsigned int line);
void printlist();
int get_number_of_allocs();

// TODO: Remove tracking of memory if desirec

#define DEBUG

#ifdef DEBUG
#define ALLOC(x) alloc_mem(x, __FILE__, __LINE__)
#define REALLOC(cur,x,y) realloc_mem(cur, x, y)
#define FREE(x) {if(x!=NULL) free_mem(x, __FILE__, __LINE__); x= NULL; }

#else
#define ALLOC(x) malloc(x)
#define FREE(x) free(x)
#define REALLOC(cur,x,y) realloc(cur,x)

#endif

#endif /*ALLOCATE_H*/


#ifndef __MY_MALLOC_H__
#define __MY_MALLOC_H__

#include <stddef.h>
#include <stdalign.h>
#include <sys/mman.h> // mmap

void *my_malloc(size_t size);
void *my_calloc(size_t nmemb, size_t size);
void *my_realloc(void *ptr, size_t size);
void my_free(void *ptr);

#endif
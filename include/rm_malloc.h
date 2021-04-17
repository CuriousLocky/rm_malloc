#ifndef RM_MALLOC_H
#define RM_MALLOC_H

#include <stddef.h>

void *rm_malloc(size_t size);
void rm_free(void *ptr);
void *rm_realloc(void *ptr, size_t new_size);

#endif
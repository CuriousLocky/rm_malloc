#include "rm_malloc.h"

__attribute__((visibility("default")))
void *malloc(size_t size){
    // write(1, "malloc\n", sizeof("malloc"));
    return rm_malloc(size);
}

__attribute__((visibility("default")))
void free(void *ptr){
    // write(1, "free\n", sizeof("free"));
    rm_free(ptr);
}

__attribute__((visibility("default")))
void *realloc(void *ptr, size_t size){
    return rm_realloc(ptr, size);
}

__attribute__((visibility("default")))
void* calloc(size_t num, size_t size){
    // write(1, "calloc\n", sizeof("calloc"));
    size_t total_size = num*size;
    void* result = rm_malloc(total_size);
    char *result_c = result;
    if(result!=NULL){
        for(size_t i = 0; i < total_size; i++){
            result_c[i] = 0;
        }
        // memset(result, 0, total_size);
    }
    return result;
}

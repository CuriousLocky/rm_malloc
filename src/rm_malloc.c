#include <stdio.h>
#include <stdlib.h>
#include "rm_malloc.h"
#include "datastructure_bitmap.h"
#include "datastructure_payload.h"
#include "mempool.h"

extern tls uint16_t thread_id;
__attribute__((visibility("default")))
void *rm_malloc(size_t ori_size){
    if(ori_size==0){return NULL;}
    size_t size = align(ori_size, 16);
    uint64_t *victim_block = find_bitmap_victim(size);
    if(victim_block == NULL){
        victim_block = create_payload_block(size);
    }
    return victim_block+1;
}

__attribute__((visibility("default")))
void *malloc(size_t size) __attribute__((weak, alias("rm_malloc")));

// uint64_t *coalesce(uint64_t *payload){
//     uint64_t *forward_payload_tail = payload-1;
//     uint64_t *next_payload_head = GET_PAYLOAD_TAIL(payload, GET_CONTENT(payload));
//     if(!IS_ALLOC(forward_payload_tail)){

//     }
// }
__attribute__((visibility("default")))
void rm_free(void *ptr){
    if(ptr==NULL){return;}
    // printf("into rm_free\n");
    if((uint64_t)ptr&15!=0){
        perror("not aligned\n");
        exit(-1);
    }
    uint64_t *payload = ((uint64_t*)ptr) - 1;
    if(!IS_ALLOC(payload)){
        perror("double free\n");
        exit(-1);
    }
    size_t size = GET_CONTENT(payload);
    // printf("size = %ld\n", size);
    //payload = coalesce(payload);
    SET_PAYLOAD_TAIL_FREE(payload, size);
    add_bitmap_block(payload, size);
}

__attribute__((visibility("default")))
void free(void *ptr) __attribute__((weak, alias("rm_free")));

size_t rm_get_size(void *ptr){
    if(ptr==NULL){return 0;}
    uint64_t *block = (uint64_t*)ptr - 1;
    return GET_CONTENT(block);
}
__attribute__((visibility("default")))
void *rm_realloc(void* ptr, size_t new_size){
    if(ptr==NULL){return rm_malloc(new_size);}
    if(new_size==0){
        rm_free(ptr);
        return NULL;
    }
    size_t old_size = rm_get_size(ptr);
    if(old_size >= new_size){return ptr;}
    rm_free(ptr);
    return rm_malloc(new_size);
}

__attribute__((visibility("default")))
void *realloc(void *ptr, size_t size) __attribute__((weak, alias("rm_realloc")));
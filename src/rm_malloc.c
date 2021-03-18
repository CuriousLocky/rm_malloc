#include <stdio.h>
#include <stdlib.h>
#include "rm_malloc.h"
#include "datastructure.h"
#include "datastructure_bitmap.h"
#include "datastructure_payload.h"
#include "mempool.h"

extern thread_local uint16_t thread_id;

void invalid_free(char *msg){
    printf("invalid free: %s\n", msg);
    exit(-1);
}

void *rm_malloc(size_t size){
    if(size==0){return NULL;}
    size = align(size, 16);
    uint64_t *victim_block = find_victim(size);
    if(victim_block == NULL){
        victim_block = create_payload_block(size);
    }
    PACK_PAYLOAD_HEAD(victim_block, 1, thread_id, size);
    PACK_PAYLOAD_TAIL(victim_block, 1, size);
    return victim_block+1;
}

uint64_t *coalesce(uint64_t *payload){
    uint64_t *forward_payload_tail = payload-1;
    uint64_t *next_payload_head = GET_PAYLOAD_TAIL(payload, GET_CONTENT(payload));
    if(!IS_ALLOC(forward_payload_tail)){

    }
}

void rm_free(void *ptr){
    if(ptr==NULL){return;}
    if((uint64_t)ptr&15!=0){
        invalid_free("ptr not allocated by rm_malloc");
    }
    uint64_t *payload = (uint64_t*)ptr - 1;
    if(!IS_ALLOC(payload)){
        invalid_free("duplicated free");
    }
    size_t size = GET_CONTENT(payload);
    //payload = coalesce(payload);
    SET_PAYLOAD_TAIL_FREE(payload, size);
    add_bitmap_block(payload, size);
}
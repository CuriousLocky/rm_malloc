#include <stdbool.h>
#include <stdio.h>
#include "mempool.h"
#include "datastructure_bitmap.h"
#include "datastructure_payload.h"
#include "rm_threads.h"

tls void *payload_pool = NULL;
tls size_t payload_pool_size = 0;
#define MIN_PAYLOAD_BLOCK_SIZE 32

extern tls uint16_t thread_id;

void payload_init(size_t payload_size){
    #ifdef __NOISY_DEBUG
    write(1, "into payload_init\n", sizeof("into payload_init"));
    #endif
    size_t new_pool_size_with_ht = align(payload_size+16, PAYLOAD_CHUNK_SIZE);
    uint64_t *new_payload_pool = (uint64_t*)payload_chunk_req(payload_size);
    // PACK_PAYLOAD_TAIL(new_payload_pool, 1, 8);
    // payload_pool = (char*)new_payload_pool + 24;
    // payload_pool_size = payload_size - 24;
    payload_pool = new_payload_pool + 1;
    payload_pool_size = new_pool_size_with_ht - 16;
}

void *create_payload_block(size_t size){
    #ifdef __NOISY_DEBUG
    write(1, "into create_payload_block\n", sizeof("into create_payload_block"));
    #endif
    size_t payload_size = size+16;
    if(payload_pool_size<payload_size){
        add_bitmap_block((uint64_t*)payload_pool, payload_pool_size-16);
        payload_init(payload_size);
    }
    if(payload_pool_size-payload_size<MIN_PAYLOAD_BLOCK_SIZE){
        payload_size = payload_pool_size;
    }
    void *result = payload_pool;
    PACK_PAYLOAD_HEAD(result, 1, 0x1e, payload_size-16);
    PACK_PAYLOAD_TAIL(result, 1, payload_size-16);
    payload_pool_size -= payload_size;
    payload_pool = (char*)payload_pool + payload_size;
    return result;
}
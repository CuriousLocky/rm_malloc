#include <stdbool.h>
#include <stdio.h>
#include "mempool.h"
#include "datastructure_bitmap.h"
#include "datastructure_payload.h"
#include <threads.h>

thread_local void *payload_pool = NULL;
thread_local size_t payload_pool_size = 0;
#define MIN_PAYLOAD_BLOCK_SIZE 32

extern thread_local uint16_t thread_id;

void payload_init(size_t payload_size){
    #ifdef __NOISY_DEBUG
    write(1, "into payload_init\n", sizeof("into payload_init"));
    #endif
    size_t new_pool_size = align(payload_size+32, PAYLOAD_CHUNK_SIZE) - 32;
    uint64_t *new_payload_pool = (uint64_t*)payload_chunk_req(payload_size);
    PACK_PAYLOAD(new_payload_pool, ID_MASK, 1, new_pool_size);
    payload_pool = new_payload_pool + 3;
    payload_pool_size = new_pool_size;
}

void *create_payload_block(size_t size){
    #ifdef __NOISY_DEBUG
    write(1, "into create_payload_block\n", sizeof("into create_payload_block"));
    #endif
    size_t buddy_size = BUDDIFY(size);
    size_t payload_size = buddy_size + 16;
    if(payload_pool_size<payload_size){
        buddify_add(payload_pool, payload_pool_size-16);
        payload_init(payload_size);
    }
    void *result = payload_pool;
    PACK_PAYLOAD(result, thread_id, 1, buddy_size);
    payload_pool_size -= payload_size;
    payload_pool = (char*)payload_pool + payload_size;
    // a dummy header to prevent coalescing into payload_pool
    PACK_PAYLOAD_HEAD(payload_pool, 1, ID_MASK, 0);

    return result;
}
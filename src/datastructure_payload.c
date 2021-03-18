#include <threads.h>
#include <stdbool.h>
#include "mempool.h"
#include "datastructure.h"
#include "datastructure_payload.h"

thread_local void *payload_pool = NULL;
thread_local size_t payload_pool_size = 0;
#define MIN_PAYLOAD_BLOCK_SIZE 32

void payload_init(size_t payload_size){
    payload_size = align(payload_size+24, PAYLOAD_CHUNK_SIZE);
    uint64_t *new_payload_pool = (uint64_t*)payload_chunk_req(payload_size);
    PACK_PAYLOAD_TAIL(new_payload_pool, 1, 8);
    payload_pool = (char*)new_payload_pool + 24;
    payload_pool_size = payload_size;
}

void *create_payload_block(size_t size){
    size_t payload_size = size+16;
    if(payload_pool_size<payload_size){
        add_block((uint64_t*)payload_pool, payload_pool_size-16);
        payload_init(payload_size);
    }
    if(payload_pool_size-payload_size<MIN_PAYLOAD_BLOCK_SIZE){
        payload_size = payload_pool_size;
    }
    void *result = payload_pool;
    payload_pool_size -= payload_size;
    payload_pool = (char*)payload_pool + payload_size;
    return result;
}
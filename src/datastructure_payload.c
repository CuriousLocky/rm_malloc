#include <stdbool.h>
#include <stdio.h>
#include "mempool.h"
#include "datastructure.h"
#include "datastructure_payload.h"
#include "rm_threads.h"

tls void *payload_pool = NULL;
tls size_t payload_pool_size = 0;
#define MIN_PAYLOAD_BLOCK_SIZE 32

void payload_init(size_t payload_size){
    #ifdef __NOISY_DEBUG
    write(1, "into payload_init\n", sizeof("into payload_init"));
    #endif
    payload_size = align(payload_size+24, PAYLOAD_CHUNK_SIZE);
    uint64_t *new_payload_pool = (uint64_t*)payload_chunk_req(payload_size);
    PACK_PAYLOAD_TAIL(new_payload_pool, 1, 8);
    payload_pool = (char*)new_payload_pool + 24;
    payload_pool_size = payload_size;
}

void *create_payload_block(size_t size){
    #ifdef __NOISY_DEBUG
    write(1, "into create_payload_block\n", sizeof("into create_payload_block"));
    #endif
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
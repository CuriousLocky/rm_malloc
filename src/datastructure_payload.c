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

/*initiate a new payload pool according to the requested size. total wasted space is 32 bytes, in which 16
bytes are the chunk header and tails and 16 bytes are for alignment requirement*/
void payload_init(size_t payload_size){
    #ifdef __NOISY_DEBUG
    write(1, "into payload_init\n", sizeof("into payload_init"));
    #endif
    size_t new_pool_size = align(payload_size+32, PAYLOAD_CHUNK_SIZE) - 32;
    uint64_t *new_payload_pool = (uint64_t*)payload_chunk_req(payload_size);
    PACK_PAYLOAD(new_payload_pool, ID_MASK, 1, new_pool_size);
    PACK_PAYLOAD(new_payload_pool+1, ID_MASK, 1, 16);
    payload_pool = new_payload_pool + 3;
    payload_pool_size = new_pool_size;
}

/*cut the rounded size from payload_pool, if the size exceed payload pool size, request a new payload pool
from the mempool and repeat, the remain size will be added to the table*/
void *create_payload_block(size_t size){
    #ifdef __NOISY_DEBUG
    write(1, "into create_payload_block\n", sizeof("into create_payload_block"));
    #endif
    size_t payload_size = GET_ROUNDED(size);
    if(payload_pool_size<payload_size){
        add_bitmap_block(payload_pool, payload_pool_size);
        payload_init(payload_size);
    }
    if(payload_pool_size - payload_size <= MIN_PAYLOAD_BLOCK_SIZE){
        payload_size = payload_pool_size;
    }
    void *result = payload_pool;
    PACK_PAYLOAD(result, thread_id, 1, payload_size);
    payload_pool_size -= payload_size;
    payload_pool = (char*)payload_pool + payload_size;
    // a dummy header to prevent coalescing into payload_pool
    PACK_PAYLOAD_HEAD(payload_pool, 1, ID_MASK, 0);

    return result;
}
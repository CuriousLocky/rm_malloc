#include <stdbool.h>
#include <stdio.h>
#include "mempool.h"
#include "datastructure_bitmap.h"
#include "datastructure_payload.h"
#include <threads.h>

thread_local void *payload_pool = NULL;
thread_local size_t payload_pool_size = 0;
#define MIN_PAYLOAD_BLOCK_SIZE 64

extern thread_local uint16_t thread_id;

/*initiate a new payload pool according to the requested size. total wasted space is 128 bytes*/
void payload_init(size_t block_size){
    size_t new_pool_size = align(block_size + 128, PAYLOAD_CHUNK_SIZE);
    uint64_t *new_payload_pool = (uint64_t*)payload_chunk_req(block_size);
    uint64_t *chunk_head_guard = new_payload_pool;
    // uint64_t *chunk_tail_guard = new_payload_pool + new_pool_size/8 - 64/8;
    PACK_BLOCK_HEAD(chunk_head_guard, 1, ID_MASK, 64);
    // PACK_BLOCK_HEAD(chunk_tail_guard, 1, ID_MASK, 64);
    payload_pool = new_payload_pool + 64/8;
    payload_pool_size = new_pool_size - 128;
}

/*cut the rounded size from payload_pool, if the size exceed payload pool size, request a new payload pool
from the mempool and repeat, the remain size will be added to the table. The request size can not exceed 
PAYLOAD_CHUNK_SIZE*/
void *create_payload_block(size_t size){
    size_t block_size = GET_ROUNDED(size);
    if(block_size >= PAYLOAD_CHUNK_SIZE){
        // huge block is given
        // 16 bytes are for alignment requirement, no need to align again since rounded size
        block_size = align(size + 32, PAYLOAD_CHUNK_SIZE);
        uint64_t *block_head = payload_chunk_req(block_size);
        PACK_BLOCK_HEAD(block_head, 1, ID_MASK, block_size);
        return block_head;
    }    
    if(payload_pool_size<block_size){
        add_bitmap_block(payload_pool, payload_pool_size);
        payload_init(block_size);
    }
    // if(payload_pool_size - block_size <= MIN_PAYLOAD_BLOCK_SIZE){
    //     block_size = payload_pool_size;
    // }
    void *result = payload_pool;
    PACK_PAYLOAD(result, thread_id, 1, block_size);
    payload_pool_size -= block_size;
    payload_pool = (char*)payload_pool + block_size;
    // a dummy header to prevent coalescing into payload_pool
    PACK_BLOCK_HEAD(payload_pool, 1, ID_MASK, 0);

    return result;
}
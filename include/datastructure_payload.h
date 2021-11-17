#ifndef DATASTRUCTURE_PAYLOAD_H
#define DATASTRUCTURE_PAYLOAD_H

#include <inttypes.h>
#include <stddef.h>
#include <x86intrin.h>

#define FREE_MASK       0xfffffffffffffffeUL
#define ALLOC_MASK      (~FREE_MASK)

// a mask for getting the size/pointer from header/tail
#define CONT_MASK       0x0000fffffffffffcUL

// a mask for getting the id, also marks the impossible thread_id
#define ID_MASK         0x000000000000ffffUL

/*returns the block's size*/
static inline uint64_t GET_CONTENT(uint64_t *block){
    return (*block)&CONT_MASK;
}

/*returns the header of the block stored in the tail*/
static inline uint64_t *GET_BLOCK_HEAD(uint64_t *block_tail){
    return (uint64_t*)GET_CONTENT(block_tail);
}

/*returns the address of the tail according to the size*/
static inline uint64_t *GET_BLOCK_TAIL(uint64_t *block_head, size_t size){
    return (uint64_t*)((char*)block_head + size - 8);
}

/*store the required information into a block's tail*/
static inline void PACK_BLOCK_TAIL(uint64_t *block_tail, int alloc, int thread_id, uint64_t *block_head){
    // uint64_t *block_tail = GET_BLOCK_TAIL(block_head, size);
    *block_tail = (uint64_t)alloc | ((uint64_t)thread_id << 48) | (uint64_t)block_head;
}

/*store the required information into a block's header*/
static inline void PACK_BLOCK_HEAD(uint64_t *block_head, int alloc, uint16_t thread_id, uint64_t content){
    *block_head = (uint64_t)alloc  | ((uint64_t)thread_id << 48) | content;
}

/*store the necessary information into both the block's header and tail*/
static inline void PACK_PAYLOAD(uint64_t *block_head, uint16_t thread_id, int alloc, size_t size){
    PACK_BLOCK_HEAD(block_head, alloc, thread_id, size);
    uint64_t *block_tail = GET_BLOCK_TAIL(block_head, size);
    PACK_BLOCK_TAIL(block_tail, alloc, thread_id, block_head);
}

static inline uint64_t *GET_BEHIND_HEAD(uint64_t *block_head, uint64_t size){
    uint64_t *tail = GET_BLOCK_TAIL(block_head, size);
    return tail + 1;
}

static inline uint64_t *GET_PAYLOAD(uint64_t *head){
    return head + 2;
}

/*returns whether a block is allocated or free*/
static inline int IS_ALLOC(uint64_t *block_head){
    return (*block_head)&1;
}

/*returns the id of a block, accept either header or tail as input*/
static inline int GET_ID(uint64_t *block){
    return _bextr_u64(*block, 48, 16);
}

/*create a new block with pre-zeroed block segment*/
void *create_payload_block(size_t size);

#endif
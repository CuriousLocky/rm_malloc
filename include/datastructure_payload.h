#ifndef DATASTRUCTURE_PAYLOAD_H
#define DATASTRUCTURE_PAYLOAD_H

#include <inttypes.h>
#include <stddef.h>
#include <x86intrin.h>

#define FREE_MASK       0xfffffffffffffffeUL
#define ALLOC_MASK      (~FREE_MASK)
#define CONT_MASK       0x0000fffffffffffeUL

#define ID_MASK         0x000000000000ffffUL
#define MAX_THREAD_NUM  0x8000

/*returns the block's size*/
static inline uint64_t GET_CONTENT(uint64_t *payload){
    return (*payload)&CONT_MASK;
}

/*returns the header of the block stored in the tail*/
static inline uint64_t *GET_PAYLOAD_HEAD(uint64_t *payload_tail){
    return (uint64_t*)GET_CONTENT(payload_tail);
}

/*returns the address of the tail according to the size*/
static inline uint64_t *GET_PAYLOAD_TAIL(uint64_t *payload_head, size_t size){
    return (uint64_t*)((char*)payload_head + size - 8);
}

/*store the required information into a block's tail*/
static inline void PACK_PAYLOAD_TAIL(uint64_t *payload_tail, int alloc, int thread_id, uint64_t *payload_head){
    // uint64_t *payload_tail = GET_PAYLOAD_TAIL(payload_head, size);
    *payload_tail = (uint64_t)alloc | ((uint64_t)thread_id << 48) | (uint64_t)payload_head;
}

/*store the required information into a block's header*/
static inline void PACK_PAYLOAD_HEAD(uint64_t *payload_head, int alloc, uint16_t thread_id, uint64_t content){
    *payload_head = (uint64_t)alloc  | ((uint64_t)thread_id << 48) | content;
}

/*store the necessary information into both the block's header and tail*/
static inline void PACK_PAYLOAD(uint64_t *payload_head, uint16_t thread_id, int alloc, size_t size){
    PACK_PAYLOAD_HEAD(payload_head, alloc, thread_id, size);
    uint64_t *payload_tail = GET_PAYLOAD_TAIL(payload_head, size);
    PACK_PAYLOAD_TAIL(payload_tail, alloc, thread_id, payload_head);
}

/*returns whether a block is allocated or free*/
static inline int IS_ALLOC(uint64_t *payload_head){
    return (*payload_head)&1;
}

/*returns the id of a block, accept either header or tail as input*/
static inline int GET_ID(uint64_t *payload){
    return _bextr_u64(*payload, 48, 16);
}

/*create a new block with pre-zeroed payload segment*/
void *create_payload_block(size_t size);

#endif
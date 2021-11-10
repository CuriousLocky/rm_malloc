#ifndef DATASTRUCTURE_PAYLOAD_H
#define DATASTRUCTURE_PAYLOAD_H

#include <inttypes.h>
#include <stddef.h>

#define FREE_MASK       0xfffffffffffffffeUL
#define ALLOC_MASK      (~FREE_MASK)
#define CONT_MASK       0x0000fffffffffffeUL

#define ID_MASK         0x000000000000ffffUL
#define MAX_THREAD_NUM  0x8000


static inline uint64_t GET_CONTENT(uint64_t *payload){
    return (*payload)&CONT_MASK;
}
static inline uint64_t *GET_PAYLOAD_HEAD(uint64_t *payload_tail){
    return (uint64_t*)GET_CONTENT(payload_tail);
}
static inline uint64_t *GET_PAYLOAD_TAIL(uint64_t *payload_head, size_t size){
    return (uint64_t*)((char*)payload_head + size + 8);
}
static inline void SET_PAYLOAD_TAIL_FREE(uint64_t *payload_head, size_t size){
    uint64_t *payload_tail = GET_PAYLOAD_TAIL(payload_head, size);
    *payload_tail &= FREE_MASK;
}
static inline void SET_PAYLOAD_TAIL_ALLOC(uint64_t *payload_head, size_t size){
    uint64_t *payload_tail = GET_PAYLOAD_TAIL(payload_head, size);
    *payload_tail |= ALLOC_MASK;
}
static inline void PACK_PAYLOAD_TAIL(uint64_t *payload_head, int alloc, size_t size){
    uint64_t *payload_tail = GET_PAYLOAD_TAIL(payload_head, size);
    *payload_tail = (uint64_t)alloc | (uint64_t)payload_head;
}
static inline void PACK_PAYLOAD_HEAD(uint64_t *payload_head, int alloc, uint16_t thread_id, uint64_t content){
    *payload_head = (uint64_t)alloc  | ((uint64_t)thread_id << 48) | content;
}
static inline int IS_ALLOC(uint64_t *payload_head){
    return (*payload_head)&1;
}
static inline int GET_ID(uint64_t *payload_head){
    return ((*payload_head)>>48)&ID_MASK;
}

void *create_payload_block(size_t size);

#endif
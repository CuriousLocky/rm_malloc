#ifndef DATASTRUCTURE_PAYLOAD_H
#define DATASTRUCTURE_PAYLOAD_H

#include <inttypes.h>
#include <string.h>

#define PAYLOAD_SIZE_STEP 16
#define FREE_MASK   0x7fffffffffffffffUL
#define ALLOC_MASK  0x8000000000000000UL
#define CONT_MASK   0x0000ffffffffffffUL

inline uint64_t *GET_PAYLOAD_TAIL(uint64_t *payload_head, size_t size){
    return (uint64_t*)((char*)payload_head + size + 8);
}
inline void SET_PAYLOAD_TAIL_FREE(uint64_t *payload_head, size_t size){
    uint64_t *payload_tail = GET_PAYLOAD_TAIL(payload_head, size);
    *payload_tail &= FREE_MASK;
}
inline void SET_PAYLOAD_TAIL_ALLOC(uint64_t *payload_head, size_t size){
    uint64_t *payload_tail = GET_PAYLOAD_TAIL(payload_head, size);
    *payload_tail |= ALLOC_MASK;
}
inline void PACK_PAYLOAD_TAIL(uint64_t *payload_head, int alloc, size_t size){
    uint64_t *payload_tail = GET_PAYLOAD_TAIL(payload_head, size);
    *payload_tail = ((uint64_t)alloc << 63) | size;
}
inline void PACK_PAYLOAD_HEAD(uint64_t *payload_head, int alloc, uint16_t thread_id, uint64_t content){
    *payload_head = ((uint64_t)alloc << 63) | ((uint64_t)thread_id << 48) | content;
}
inline uint64_t GET_CONTENT(uint64_t *payload_head){
    return (*payload_head)&CONT_MASK;
}
inline int IS_ALLOC(uint64_t *payload_head){
    return (*payload_head)>>63;
}

void *create_payload_block(size_t size);

#endif
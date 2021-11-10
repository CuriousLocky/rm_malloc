#ifndef __RM_MALLOC_DATASTRUCTURE_BITMAP_H
#define __RM_MALLOC_DATASTRUCTURE_BITMAP_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include <threads.h>

#define LOCAL_TABLE_NUMBER      2
#define SHARED_TABLE_NUMBER     4

// A non-blocking stack is used for storing inactive threadinfo for reusage
typedef union{
    __uint128_t block_16b;
    struct{
        void *ptr;
        uint64_t id;
    }block_struct;
}NonBlockingStackBlock;

/*each bit of index is for indicating whether entries[i]==NULL*/
typedef union{
    uint64_t index;
    // entries[0] is never used
    uint64_t *entries[64];
} LocalTable;

/*works like LocalTable but shared, so entries organized by non-blocking stacks*/
typedef struct{
    uint64_t index;
    NonBlockingStackBlock entryStacks[63];
} SharedTable;

typedef struct ThreadInfo{
    #ifdef __RACE_TEST
    int active;
    #endif
    int16_t thread_id;
    LocalTable tables[LOCAL_TABLE_NUMBER];
    struct ThreadInfo *next;
    void *payload_pool;
    size_t payload_pool_size;
    uint64_t *big_block;
} ThreadInfo;

inline uint64_t *GET_NEXT_BLOCK(uint64_t *block){
    return (uint64_t*)(*(block+1));
}
inline void SET_NEXT_BLOCK(uint64_t *block, uint64_t *next){
    *(block+1) = (uint64_t)next;
}
inline uint64_t *GET_PREV_BLOCK(uint64_t *block){
    return (uint64_t*)(*(block+2));
}
inline void SET_PREV_BLOCK(uint64_t *block, uint64_t *prev){
    *(block+2) = (uint64_t)prev;
}
inline int GET_LOCAL_TABLE_SHIFT_DIG(int table_level){
    return table_level? 10 : 4;
}
inline uint64_t GET_LOCAL_TABLE_STEP(int table_level){
    return 1UL << GET_LOCAL_TABLE_SHIFT_DIG(table_level);
}
inline int GET_LOCAL_TABLE_LEVEL(uint64_t size){
    return size < (64*16)? 0 : 1;
}
inline int GET_LOCAL_TABLE_SLOT(uint64_t size, int table_level){
    int level_0_slot = (size >> 4)&63;
    int level_1_slot = (size >> 10)&63;
    return table_level? level_1_slot : level_0_slot;
}

/*returns the number of trailing zeros of an uint64_t*/
inline int trailing0s(uint64_t x){
    if(x==0){return 64;}
    #ifdef __GNUC__
        return __builtin_ctzll(x);
    #else
        int result = 0;
        while(!(x&1)){
            x = x >> 1;
            result ++;
        }
        return result;
    #endif
}

/*returns the number of leading zeros of an uint64_t*/
inline int leading0s(uint64_t x){
    if(x==0){return 64;}
    #ifdef __GNUC__
        return __builtin_clzll(x);
    #else
        int result = 0;
        while(!(x&(1<<63))){
            x = x << 1;
            result ++;
        }
        return result;
    #endif
}

inline void set_threadInfo_next(ThreadInfo *head, ThreadInfo *next){
    head->next = next;
}

inline void *get_threadInfo_next(ThreadInfo *threadInfo){
    return threadInfo->next;
}

/*returns the top of a nonblocking stack*/
#define pop_nonblocking_stack(stack, get_next)                                                              \
({                                                                                                          \
    NonBlockingStackBlock old_block;                                                                        \
    NonBlockingStackBlock new_block;                                                                        \
    do{                                                                                                     \
        old_block = stack;                                                                                  \
        if(old_block.block_struct.ptr == NULL){                                                             \
            break;                                                                                          \
        }                                                                                                   \
        new_block.block_struct.ptr = get_next(old_block.block_struct.ptr);                                  \
        new_block.block_struct.id = old_block.block_struct.id+1;                                            \
    }while(!__sync_bool_compare_and_swap(&(stack.block_16b), old_block.block_16b, new_block.block_16b));    \
    old_block.block_struct.ptr;                                                                             \
})

/*a pseudo function to push a ptr to a nonblocking stack*/
#define push_nonblocking_stack(pushed_ptr, stack, set_next){                                                \
    NonBlockingStackBlock new_block;                                                                        \
    NonBlockingStackBlock old_block;                                                                        \
    new_block.block_struct.ptr = pushed_ptr;                                                                \
    do{                                                                                                     \
        old_block = stack;                                                                                  \
        set_next(pushed_ptr, old_block.block_struct.ptr);                                                   \
        new_block.block_struct.id = old_block.block_struct.id+1;                                            \
    }while(!__sync_bool_compare_and_swap(&(stack.block_16b), old_block.block_16b, new_block.block_16b));    \
}

/*initialize the bitmap structure, should be called once per thread*/
void thread_bitmap_init();

/*find a victim block that can hold size bytes, return NULL if no block
in both the global freelist and thread local freelist satisfies*/
uint64_t *find_bitmap_victim(size_t size);

/*insert a block into freelist. will insert to global freelist if not
in use, thread local freelist otherwise.*/
void add_bitmap_block(uint64_t *block, size_t size);

/*try to coalesce with forward and next block, returns the block to add*/
uint64_t *coalesce(uint64_t *payload);

#endif
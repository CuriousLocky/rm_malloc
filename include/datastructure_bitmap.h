#ifndef __RM_MALLOC_DATASTRUCTURE_BITMAP_H
#define __RM_MALLOC_DATASTRUCTURE_BITMAP_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#include <threads.h>

static inline int trailing0s(uint64_t x);
static inline int leading0s(uint64_t x);


/*Align "size" to "alignment", alignment should be 2^n.*/
inline size_t align(size_t size, size_t alignment){
    return (((size)+(alignment-1)) & (~(alignment-1)));
}

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
    LocalTable table;
    struct ThreadInfo *next;
    void *payload_pool;
    size_t payload_pool_size;
    uint64_t *big_block;
} ThreadInfo;

static inline uint64_t *GET_NEXT_BLOCK(uint64_t *block){
    return (uint64_t*)(*(block+1));
}
static inline void SET_NEXT_BLOCK(uint64_t *block, uint64_t *next){
    *(block+1) = (uint64_t)next;
}
static inline uint64_t *GET_PREV_BLOCK(uint64_t *block){
    return (uint64_t*)(*(block+2));
}
static inline void SET_PREV_BLOCK(uint64_t *block, uint64_t *prev){
    *(block+2) = (uint64_t)prev;
}
static inline int GET_SLOT(uint64_t size){
    return trailing0s(size >> 4);
    // return __builtin_popcountl(size);
}

// // align a size to a format of 2^n-16
// static inline uint64_t BUDDIFY(uint64_t size){
//     int leading_0_num = leading0s(size);
//     return ((~(INT64_MIN>>leading_0_num))<<1) & (-16L);
// }

// get a mask for index querying
static inline uint64_t GET_MASK(uint64_t size){
    // int leading_0_num = leading0s(size-16);
    // return (INT64_MIN>>leading_0_num)>>3;
    int leading_0_num = leading0s(size);
    return (INT64_MIN>>leading_0_num)>>4;
}

// get a split block size
static inline uint64_t GET_SPLIT_SIZE(uint64_t size){
    return (size >> 1) & (-16L);
}

// static inline bool IS_BUDDY_SIZE(uint64_t size){
//     return __builtin_popcountl(size+16)==1;
// }

// #define ROUND_DEGREE 2
// return a rounded size for allocation 
static inline uint64_t GET_ROUNDED(uint64_t size){
    // uint64_t round_step = 1UL<<(63-ROUND_DEGREE-leading0s(size));
    // round_step = round_step>32? round_step : 32;
    // uint64_t partial_round = align((size&(round_step-1)), round_step);
    // return (size&(~(round_step-1))) + partial_round;
    uint64_t round_step = (~(INT64_MIN>>leading0s(size)))+1;
    return align(size, round_step);
}

/*returns the number of trailing zeros of an uint64_t*/
static inline int trailing0s(uint64_t x){
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

/*returns the number of leading zeros of an uint64_t, does not take 0 input*/
static inline int leading0s(uint64_t x){
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

static inline void set_threadInfo_next(ThreadInfo *head, ThreadInfo *next){
    head->next = next;
}

static inline void *get_threadInfo_next(ThreadInfo *threadInfo){
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

// /*transfer a non-buddy block to (many) buddy blocks and add*/
// void *buddify_add(uint64_t *block, size_t size);

#endif
#ifndef __RM_MALLOC_DATASTRUCTURE_BITMAP_H
#define __RM_MALLOC_DATASTRUCTURE_BITMAP_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

/*each bit of index is for indicating whether entries[i]==NULL*/
typedef struct{
    uint64_t index;
    void *entries[64];
} Table;

typedef struct ThreadInfo{
    bool active;
    int16_t thread_id;
    Table *level_0_table;
    struct ThreadInfo *next;
    // mtx_t thread_lock;
    // pthread_spinlock_t thread_lock;
    pthread_mutex_t thread_lock;
    void *payload_pool;
    size_t payload_pool_size;
} ThreadInfo;

inline uint64_t *GET_NEXT_BLOCK(uint64_t *block){
    return (uint64_t*)(*block);
}
inline void *SET_NEXT_BLOCK(uint64_t *block, uint64_t *next){
    *block = (uint64_t)next;
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

/*initialize the bitmap structure, should be called once per thread*/
void thread_bitmap_init();

/*find a victim block that can hold size bytes, return NULL if no block
in both the global freelist and thread local freelist satisfies*/
uint64_t *find_bitmap_victim(size_t size);

/*insert a block into freelist. will insert to global freelist if not
in use, thread local freelist otherwise.*/
void add_bitmap_block(uint64_t *block, size_t size);

#endif
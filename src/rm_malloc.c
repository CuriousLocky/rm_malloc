#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "rm_malloc.h"
#include "datastructure_bitmap.h"
#include "datastructure_payload.h"
#include "mempool.h"

extern thread_local uint16_t thread_id;
extern thread_local ThreadInfo *local_thread_info;

typedef struct{
    int prezeroed_flag; // to indicate whether the block is pre-zeroed
    uint64_t *ptr;  // the result of malloc
}MallocResult;

#ifdef SANITY_TEST
#define TEST_THREAD_LIMIT 0x18
__attribute__((optimize(0)))
void check_valid_header(uint64_t *block){
    uint64_t size = GET_CONTENT(block);
    uint64_t id = GET_ID(block);
    if(id == ID_MASK){
        return;
    }
    bool size_valid = (size & 31)==0;
    bool id_valid = id<=TEST_THREAD_LIMIT;
    if(!(size_valid && id_valid)){
        raise(SIGABRT);
    }
}

__attribute__((optimize(0)))
void check_valid_tail(uint64_t *tail){
    if(GET_ID(tail)!=thread_id){
        return;
    }
    uint64_t *head = GET_PAYLOAD_HEAD(tail);
    uint64_t *real_tail = GET_PAYLOAD_TAIL(head, GET_CONTENT(head));
    if(tail != real_tail){
        raise(SIGABRT);
    }
}

__attribute__((optimize(0)))
void check_front_behind_sanity(uint64_t *block){
    uint64_t *front_tail = block - 1;
    uint64_t *front_head = GET_PAYLOAD_HEAD(front_tail);
    uint64_t *behind_head = GET_PAYLOAD_TAIL(block, GET_CONTENT(block)) + 1;
    check_valid_header(front_head);
    check_valid_header(behind_head);
    check_valid_tail(front_tail);
}
#endif

static inline MallocResult __rm_malloc(size_t ori_size){
    MallocResult result;
    size_t size = align(ori_size + 16, 32);
    uint64_t *victim_block = find_bitmap_victim(size);
    if(victim_block == NULL){
        result.prezeroed_flag = 1;
        result.ptr = create_payload_block(size);
    }else{
        result.prezeroed_flag = 0;
        result.ptr = victim_block;
    }
    #ifdef SANITY_TEST
        check_front_behind_sanity(result.ptr);
    #endif
    result.ptr ++;
    return result;
}

__attribute__((visibility("default")))
void *rm_malloc(size_t ori_size){
    if(ori_size==0){return NULL;}
    return __rm_malloc(ori_size).ptr;
}

__attribute__((visibility("default")))
void *malloc(size_t size) __attribute__((weak, alias("rm_malloc")));

uint64_t *problem_ptr;
__attribute__((visibility("default")))
void rm_free(void *ptr){
    if(ptr==NULL){return;}
    if((uint64_t)ptr&15!=0){
        perror("not aligned\n");
        exit(-1);
    }
    uint64_t *payload = ((uint64_t*)ptr) - 1;
    if(!IS_ALLOC(payload)){
        problem_ptr = payload;
        perror("double free\n");
        raise(SIGABRT);
        exit(-1);
    }
    int block_id = GET_ID(payload);
    if(block_id != thread_id){
        if(block_id == ID_MASK){
            // huge block
            uint64_t size = GET_CONTENT(payload);
            payload_chunk_rel(payload-1, size);
            return;
        }
        remote_free(payload, block_id);
        return;
    }
    #ifdef SANITY_TEST
        if(__builtin_popcountl(GET_CONTENT(payload))!=1){
            raise(SIGABRT);
        }
        check_front_behind_sanity(payload);
    #endif
    uint64_t *block_to_add = coalesce(payload);
    size_t size = GET_CONTENT(block_to_add);
    PACK_PAYLOAD(block_to_add, thread_id, 0, size);
    add_block_LocalTable(block_to_add, size);

    uint64_t current_debt_stack_size = local_thread_info->debt_stack_size;
    __atomic_fetch_sub(&(local_thread_info->debt_stack_size), current_debt_stack_size, __ATOMIC_RELAXED);
    for(int i = 0; i < current_debt_stack_size; i++){
        uint64_t *debt_block = pop_nonblocking_stack(local_thread_info->debt_stack, GET_NEXT_BLOCK);
        uint64_t *block_to_add = coalesce(debt_block);
        size_t size = GET_CONTENT(block_to_add);
        PACK_PAYLOAD(block_to_add, thread_id, 0, size);
        add_block_LocalTable(block_to_add, size);
    }
}

__attribute__((visibility("default")))
void free(void *ptr) __attribute__((weak, alias("rm_free")));

size_t rm_get_size(void *ptr){
    if(ptr==NULL){return 0;}
    uint64_t *block = (uint64_t*)ptr - 1;
    return GET_CONTENT(block);
}
__attribute__((visibility("default")))
void *rm_realloc(void* ptr, size_t new_size){
    if(ptr==NULL){return rm_malloc(new_size);}
    if(new_size==0){
        rm_free(ptr);
        return NULL;
    }
    size_t old_size = rm_get_size(ptr);
    if(old_size >= new_size){return ptr;}
    rm_free(ptr);
    return rm_malloc(new_size);
}

__attribute__((visibility("default")))
void *realloc(void *ptr, size_t size) __attribute__((weak, alias("rm_realloc")));

void* rm_calloc(size_t num, size_t size){
    size_t total_size = num*size;
    if(total_size == 0) {return NULL;}
    MallocResult resultPack = __rm_malloc(total_size);
    uint64_t *resultPtr = resultPack.ptr;
    if(!resultPack.prezeroed_flag){
        memset(resultPtr, 0, total_size);
    }
    return resultPtr;
}

__attribute__((visibility("default")))
void *calloc(size_t num, size_t size) __attribute__((weak, alias("rm_calloc")));
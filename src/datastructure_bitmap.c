#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include <threads.h>

#include "datastructure_bitmap.h"
#include "datastructure_payload.h"
#include "mempool.h"

#define THREADINFO_PER_CHUNK (META_CHUNK_SIZE/sizeof(ThreadInfo))

static LocalTable *table_pool = NULL;
static ThreadInfo *threadInfo_pool = NULL;
static volatile uint16_t threadInfo_pool_usage = 0;
static volatile uint8_t threadInfo_pool_init_flag = 0;
static volatile uint16_t threadInfo_count = 0;

volatile NonBlockingStackBlock inactive_threadInfo_stack;

// to store blocks with size >= BIG_BLOCK_MIN, <= PAYLOAD_CHUNK_SIZE
volatile NonBlockingStackBlock big_block_stack = {.block_16b=0};

thread_local ThreadInfo *local_thread_info = NULL;
thread_local LocalTable *local_table;

extern thread_local void *payload_pool;
extern thread_local size_t payload_pool_size;

thread_local uint16_t thread_id;

#define THREADINFO_ARRAY_SIZE 4096
ThreadInfo *threadInfo_array[THREADINFO_ARRAY_SIZE];

/*go through the threadInfo_list to find an inactive one*/
ThreadInfo *find_inactive_threadInfo(){
    // pop the current inactive threadinfo from the stack
    return pop_nonblocking_stack(inactive_threadInfo_stack, get_threadInfo_next);
}

/*initialize the threadInfo_pool*/
void *threadInfo_pool_init(){
    uint8_t init_id = __atomic_fetch_add(&threadInfo_pool_init_flag, 1, __ATOMIC_RELAXED);
    if(init_id == 0){
        threadInfo_pool = meta_chunk_req(META_CHUNK_SIZE);
        threadInfo_pool_usage = 0;
    }else{
        while(threadInfo_pool==NULL){
            ;
        }
    }
    threadInfo_pool_init_flag = 0;
}

/*create a new threadInfo for this thread*/
ThreadInfo *create_new_threadInfo(){
    uint16_t id = __atomic_fetch_add(&threadInfo_count, 1, __ATOMIC_RELAXED);

    uint16_t offset;
    ThreadInfo *threadInfo_pool_snapshot;
        /*
        #XXX:
        The following operation is safe as long as threadInfo_pool is not updated between getting the snapshot and increasing
        offset. This is extremly rare but possible. 
        */    
    do{
        if(threadInfo_pool_usage == THREADINFO_PER_CHUNK){
            threadInfo_pool = NULL;
            threadInfo_pool_init();
        }else{
            while(threadInfo_pool_usage > THREADINFO_PER_CHUNK){
                ;
            }            
        }
        threadInfo_pool_snapshot = threadInfo_pool;
        offset = __atomic_fetch_add(&threadInfo_pool_usage, 1, __ATOMIC_RELAXED);
    }while(offset >= THREADINFO_PER_CHUNK);
    
    ThreadInfo *new_threadInfo = &threadInfo_pool_snapshot[offset];

    new_threadInfo->next = NULL;
    new_threadInfo->thread_id = id;
    new_threadInfo->payload_pool = NULL;
    new_threadInfo->payload_pool_size = 0;

    #ifdef __RACE_TEST
    new_threadInfo->active = 0;
    #endif

    threadInfo_array[id] = new_threadInfo;

    return new_threadInfo;
}

pthread_key_t inactive_key;

/*to set the threadinfo as inactive so it can be reused*/
void set_threadInfo_inactive(void *arg){
    local_thread_info->payload_pool = payload_pool;
    local_thread_info->payload_pool_size = payload_pool_size;

    #ifdef __RACE_TEST
    __sync_fetch_and_sub(&(local_thread_info->active), 1);
    #endif

    // push current threadinfo to the non-blocking stack
    push_nonblocking_stack(local_thread_info, inactive_threadInfo_stack, set_threadInfo_next);

    local_thread_info = NULL;

    thread_id = THREADINFO_ARRAY_SIZE;
}

/*
#XXX:
The init function is registered as a constructor for the main thread. However, the order of constructors not providing
a priority is random, thus it is possible that other constructors in the user application are executed brfore this.
*/

__attribute__ ((constructor)) void thread_bitmap_init(){
    if(threadInfo_pool == NULL){
        threadInfo_pool_init();
    }
    
    ThreadInfo *inactive_threadInfo = find_inactive_threadInfo();

    if(inactive_threadInfo == NULL){
        inactive_threadInfo = create_new_threadInfo();
    }
    local_thread_info = inactive_threadInfo;
    local_table = &(inactive_threadInfo->table);
    thread_id = inactive_threadInfo->thread_id;
    payload_pool = inactive_threadInfo->payload_pool;
    payload_pool_size = inactive_threadInfo->payload_pool_size;

    #ifdef __RACE_TEST
    if(__sync_fetch_and_add(&(local_thread_info->active), 1) > 0){
        #include <signal.h>
        raise(SIGABRT);
    }
    #endif

    pthread_key_create(&inactive_key, set_threadInfo_inactive);
    pthread_setspecific(inactive_key, (void*)0x8353);

    return;
}

/*remove a table's head, and properly deal with next and prev for all blocks involved*/
static inline void remove_table_head(uint64_t *block, int slot){
    uint64_t *new_list_head = GET_NEXT_BLOCK(block);
    local_table->entries[slot] = new_list_head;
    if(new_list_head==NULL){
        local_table->index &= ~(1UL<<slot);
    }else{
        SET_PREV_BLOCK(new_list_head, NULL);
    }
}

// look for a victim block in tables and local big block pool, requested size < (3/4)PAYLOAD_CHUNK_SIZE-16
// the returned block is not zeroed
uint64_t *find_bitmap_victim(size_t ori_size){
    uint64_t req_size = GET_ROUNDED(ori_size);
    uint64_t mask = GET_MASK(req_size);
    int slot = trailing0s(local_table->index & mask);
    uint64_t *result = NULL;
    if(slot < 64){
        result = local_table->entries[slot];
        uint64_t result_size = GET_SLOT_SIZE(slot);
        remove_table_head(result, slot);
        PACK_PAYLOAD(result, thread_id, 1, req_size);
        uint64_t *new_block = GET_PAYLOAD_TAIL(result, req_size) + 1;
        uint64_t new_block_size = result_size - req_size;
        add_bitmap_block(new_block, new_block_size);
    }
    return result;
}

// add a block to the LocalTable indexed as table_level, requires the block to be packed in advance
void add_block_LocalTable(uint64_t *block, uint64_t size){
    int slot = GET_SLOT(size);
    SET_PREV_BLOCK(block, NULL);
    uint64_t *old_head = local_table->entries[slot];
    SET_NEXT_BLOCK(block, old_head);
    local_table->entries[slot] = block;
    if(old_head == NULL){
        local_table->index |= 1UL << slot;
    }else{
        SET_PREV_BLOCK(old_head, block);
    }
}

// add a block according to its size, requires the block to be packed in advance, does not accept NULL
void add_bitmap_block(uint64_t *block, size_t size){
    uint64_t remain_size = size;
    uint64_t *new_block = block;
    while(remain_size != 0){
        uint64_t new_block_size = __blsi_u64(remain_size);
        PACK_PAYLOAD(new_block, thread_id, 0, new_block_size);
        add_block_LocalTable(new_block, new_block_size);
        remain_size -= new_block_size;
        new_block += new_block_size/8;
    }
}

// remove a block from table structure
static inline void remove_block(uint64_t *block, int slot){
    uint64_t *prev = GET_PREV_BLOCK(block);
    uint64_t *next = GET_NEXT_BLOCK(block);
    if(prev != NULL){
        SET_NEXT_BLOCK(prev, next);
        if(next != NULL){
            SET_PREV_BLOCK(next, prev);
        }
    }else{
        // is a table head
        remove_table_head(block, slot);
    }
}

// try to merge the block in the front and back, returns the merge result to add into the table
uint64_t *coalesce(uint64_t *payload){
    uint64_t size = GET_CONTENT(payload);
    uint64_t *front_tail = payload-1;
    
    uint64_t *block_to_add = payload;
    uint64_t block_size = size;
    
    // test shows check merging behind first saves 1/3 memory usage, potentially related to the way
    // blocks are splitted
    uint64_t *behind_head = GET_PAYLOAD_TAIL(payload, size) + 1;
    uint64_t estimated_behind_head = ((uint64_t)thread_id<<48)|block_size;
    bool merge_behind = (*behind_head)==estimated_behind_head;
    if(merge_behind){
        int behind_slot = GET_SLOT(block_size);
        remove_block(behind_head, behind_slot);
        block_size = block_size << 1;
    }

    uint64_t *estimated_front_head = front_tail - block_size/8 + 1;
    uint64_t estimated_front_tail = ((uint64_t)thread_id<<48)|(uint64_t)estimated_front_head;
    bool merge_front = estimated_front_tail == *front_tail;
    if(merge_front){
        uint64_t *front_head = estimated_front_head;
        int front_slot = GET_SLOT(block_size);
        remove_block(front_head, front_slot);
        block_to_add = front_head;
        block_size = block_size << 1;
    }

    if(merge_front || merge_behind){
        // complete packing done in free()
        *block_to_add = block_size;
    }
    return block_to_add;
}

// attach a block to the debt stack in the owner's threadinfo
void remote_free(uint64_t *remote_block, int block_id){
    ThreadInfo *target_threadInfo = threadInfo_array[block_id];
    push_nonblocking_stack(remote_block, target_threadInfo->debt_stack, SET_NEXT_BLOCK);
    __atomic_fetch_add(&(target_threadInfo->debt_stack_size), 1, __ATOMIC_RELAXED);
}

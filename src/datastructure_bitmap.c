#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include <threads.h>

#include "datastructure_bitmap.h"
#include "datastructure_payload.h"
#include "mempool.h"

#define TABLE_PER_META_CHUNK (META_CHUNK_SIZE/sizeof(LocalTable))
#define THREADINFO_PER_CHUNK (META_CHUNK_SIZE/sizeof(ThreadInfo))

// rm_lock_t threadInfo_pool_lock = RM_LOCK_INITIALIZER;

static LocalTable *table_pool = NULL;
static ThreadInfo *threadInfo_pool = NULL;
static volatile uint16_t threadInfo_pool_usage = 0;
static volatile uint8_t threadInfo_pool_init_flag = 0;
static volatile uint16_t threadInfo_count = 0;

volatile NonBlockingStackBlock inactive_threadInfo_stack;

// to store blocks with size >= BIG_BLOCK_MIN, <= PAYLOAD_CHUNK_SIZE
volatile NonBlockingStackBlock big_block_stack = {.block_16b=0};

thread_local uint64_t *local_big_block = NULL;

//the default values are for initialization in the first call in init functions
static int table_meta_pool_usage = TABLE_PER_META_CHUNK;

thread_local ThreadInfo *local_thread_info = NULL;
// thread_local LocalTable *local_table[LOCAL_TABLE_NUMBER];
thread_local LocalTable *local_table;

extern thread_local void *payload_pool;
extern thread_local size_t payload_pool_size;

thread_local uint16_t thread_id;

static inline void add_block_LocalTable(uint64_t *block, int slot);

/*go through the threadInfo_list to find an inactive one*/
ThreadInfo *find_inactive_threadInfo(){
    // pop the current inactive threadinfo from the stack
    return pop_nonblocking_stack(inactive_threadInfo_stack, get_threadInfo_next);
}

/*initialize the threadInfo_pool with sufficient space so that it will never need to expand*/
void *threadInfo_pool_init(){
    // #define THREADINFO_POOL_SIZE    align(MAX_THREAD_NUM*sizeof(ThreadInfo), META_CHUNK_SIZE)
    uint8_t init_id = __atomic_fetch_add(&threadInfo_pool_init_flag, 1, __ATOMIC_RELAXED);
    // init_id is used to create a spinlock so that threadInfo_pool is init only once
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

/*create a new threadInfo for this thread, with local_level_0_table initialized*/
ThreadInfo *create_new_threadInfo(){
    #ifdef __NOISY_DEBUG
    write(1, "create_new_threadInfo\n", sizeof("create_new_threadInfo"));
    #endif
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
    // threadInfo_pool ++;
    // rm_unlock(&threadInfo_pool_lock);

    new_threadInfo->next = NULL;
    new_threadInfo->thread_id = id;
    new_threadInfo->payload_pool = NULL;
    new_threadInfo->payload_pool_size = 0;

    #ifdef __RACE_TEST
    new_threadInfo->active = 0;
    #endif

    return new_threadInfo;
}

pthread_key_t inactive_key;

/*
#FIXME:
The execution order of destructors registered by pthread_key_create() is random, and will be executed before a free() call
releasing the memory requested during pthread_setspecific(). The former is a potention trouble maker but the later is surely
causing crashing when there are rapid thread creating and destroying. 
*/

/*to set the threadinfo as inactive so it can be reused*/
void set_threadInfo_inactive(void *arg){
    #ifdef __NOISY_DEBUG
    write(1, "cleaning up\n", sizeof("cleaning up"));
    #endif
    local_thread_info->payload_pool = payload_pool;
    local_thread_info->payload_pool_size = payload_pool_size;
    local_thread_info->big_block = local_big_block;

    #ifdef __RACE_TEST
    __sync_fetch_and_sub(&(local_thread_info->active), 1);
    #endif

    // push current threadinfo to the non-blocking stack
    push_nonblocking_stack(local_thread_info, inactive_threadInfo_stack, set_threadInfo_next);

    local_thread_info = NULL;

}

/*
#XXX:
The init function is registered as a constructor for the main thread. However, the order of constructors not providing
a priority is random, thus it is possible that other constructors in the user application are executed brfore this.
*/

__attribute__ ((constructor)) void thread_bitmap_init(){
    #ifdef __NOISY_DEBUG
    write(1, "thread_bitmap_init\n", sizeof("thread_bitmap_init"));
    #endif
    if(threadInfo_pool == NULL){
        threadInfo_pool_init();
    }
    
    ThreadInfo *inactive_threadInfo = find_inactive_threadInfo();
    #ifdef __NOISY_DEBUG
    write(1, "find_inactive_threadInfo returned\n", sizeof("find_inactive_threadInfo returned"));
    #endif

    if(inactive_threadInfo == NULL){
        inactive_threadInfo = create_new_threadInfo();
    }
    local_thread_info = inactive_threadInfo;
    local_table = &(inactive_threadInfo->table);
    thread_id = inactive_threadInfo->thread_id;
    payload_pool = inactive_threadInfo->payload_pool;
    payload_pool_size = inactive_threadInfo->payload_pool_size;
    local_big_block = inactive_threadInfo->big_block;

    #ifdef __RACE_TEST
    if(__sync_fetch_and_add(&(local_thread_info->active), 1) > 0){
        #include <signal.h>
        raise(SIGABRT);
    }
    #endif

    pthread_key_create(&inactive_key, set_threadInfo_inactive);
    pthread_setspecific(inactive_key, (void*)0x8353);
    // write(1, "exit thread_bitmap_init\n", sizeof("exit thread_bitmap_init"));

    return;
}

static inline void remove_table_head(uint64_t *block, int slot){
    uint64_t *new_list_head = GET_NEXT_BLOCK(block);
    local_table->entries[slot] = new_list_head;
    if(new_list_head==NULL){
        local_table->index &= ~(1UL<<slot);
    }else{
        SET_PREV_BLOCK(new_list_head, NULL);
    }
}

// uint64_t *utilize_big_block(size_t req_size){
//     if(local_big_block == NULL){
//         local_big_block = pop_nonblocking_stack(big_block_stack, GET_NEXT_BLOCK);
//         if(local_big_block == NULL){
//             return NULL;
//         }
//     }
//     uint64_t big_block_size = GET_CONTENT(local_big_block);
//     if(req_size > big_block_size){
//         return NULL;
//     }
//     uint64_t *result = local_big_block;
//     uint64_t result_size = big_block_size;
//     if((req_size*2) >= big_block_size){
//         local_big_block = NULL;
//     }else{
//         uint64_t diff_block_size = big_block_size - req_size - 16;
//         if(diff_block_size >= BIG_BLOCK_MIN){
//             // donor system
//             uint64_t *donator = result;
//             uint64_t new_size = diff_block_size;
//             PACK_PAYLOAD(donator, thread_id, 0, new_size);
//             result = GET_PAYLOAD_TAIL(donator, new_size) + 1;
//             result_size = req_size;
//         }else{
//             local_big_block = NULL;
//             result_size = req_size;
//             uint64_t *new_block = GET_PAYLOAD_TAIL(result, req_size) + 1;
//             PACK_PAYLOAD(new_block, thread_id, 0, diff_block_size);
//             add_bitmap_block(new_block, diff_block_size);
//         }
//     }
//     PACK_PAYLOAD(result, thread_id, 1, result_size);
//     return result;
// }

// split a block, add the latter one into table and return the size of splited blocks
static inline uint64_t split_block(uint64_t *block, uint64_t size){
    uint64_t new_size = GET_SPLIT_SIZE(size);
    uint64_t *new_block = GET_PAYLOAD_TAIL(block, new_size) + 1;
    PACK_PAYLOAD(new_block, thread_id, 0, new_size);
    add_bitmap_block(new_block, new_size);
    return new_size;
}

// look for a victim block in tables and local big block pool, requested size < (3/4)PAYLOAD_CHUNK_SIZE-16
// the returned block is not zeroed
uint64_t *find_bitmap_victim(size_t ori_size){
    #ifdef __NOISY_DEBUG
    write(1, "find_bitmap_victim\n", sizeof("find_bitmap_victim"));
    #endif
    // if(ori_size >= BIG_BLOCK_MIN){
    //     return utilize_big_block(ori_size);
    // }
    uint64_t buddy_size = BUDDIFY(ori_size);
    uint64_t mask = GET_MASK(buddy_size);
    int slot = trailing0s(local_table->index & mask);
    uint64_t *result = NULL;
    if(slot < 64){
        result = local_table->entries[slot];
        remove_table_head(result, slot);
        uint64_t result_size = GET_CONTENT(result);
        while(GET_SPLIT_SIZE(result_size) > buddy_size){
            result_size = split_block(result, result_size);
        }
        PACK_PAYLOAD(result, thread_id, 1, result_size);
    }
    return result;
}

// add a block to the LocalTable indexed as table_level, requires the block to be packed in advance
static inline void add_block_LocalTable(uint64_t *block, int slot){
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
    #ifdef __NOISY_DEBUG
    write(1, "add_bitmap_block\n", sizeof("add_bitmap_block"));
    #endif
    // if(size >= BIG_BLOCK_MIN){
    //     // change the id of big blocks to avoid coalescing
    //     PACK_PAYLOAD(block, ID_MASK, 0, size);
    //     push_nonblocking_stack(block, big_block_stack, SET_NEXT_BLOCK);
    //     return;
    // }
    // int table_level = GET_LOCAL_TABLE_LEVEL(size);
    // add_block_LocalTable(block, size, table_level);
    int slot = GET_SLOT(size);
    add_block_LocalTable(block, slot);
}

static inline void remove_block(uint64_t *block, int slot){
    // remove a block from table structure
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

uint64_t *coalesce(uint64_t *payload){
    return payload;
//     uint64_t size = GET_CONTENT(payload);
//     uint64_t *front_tail = payload-1;
//     uint64_t *front_head = GET_PAYLOAD_HEAD(front_tail);
//     uint64_t *behind_head = GET_PAYLOAD_TAIL(payload, size) + 1;
//     // check ALLOC first to avoid segfault on chunk edges
//     bool merge_front = (!IS_ALLOC(front_tail)) && (GET_ID(front_tail)==thread_id);
//     bool merge_behind = (!IS_ALLOC(behind_head)) && (GET_ID(behind_head)==thread_id);
//     uint64_t *block_to_add = payload;
//     if(merge_front && merge_behind){
//         uint64_t front_size = GET_CONTENT(front_head);
//         uint64_t behind_size = GET_CONTENT(behind_head);
//         int front_level = GET_LOCAL_TABLE_LEVEL(front_size);
//         int behind_level = GET_LOCAL_TABLE_LEVEL(behind_size);
//         int front_slot = GET_LOCAL_TABLE_SLOT(front_size, front_level);
//         int behind_slot = GET_LOCAL_TABLE_SLOT(behind_size, behind_level);
//         uint64_t front_step = GET_LOCAL_TABLE_STEP(front_level);
//         uint64_t new_block_size = front_size + behind_size + size + 32;
//         uint64_t size_diff = new_block_size ^ front_size;
//         PACK_PAYLOAD(front_head, thread_id, 0, new_block_size);
//         remove_block(behind_head, behind_level, behind_slot);
//         if(size_diff < front_step){
//             // no change to the table structure
//             // nothing to add to table
//             block_to_add = NULL;
//         }else{
//             // remove front block from table structure
//             remove_block(front_head, front_level, front_slot);
//             block_to_add = front_head;
//         }
//     }else if(merge_front){
//         uint64_t front_size = GET_CONTENT(front_head);
//         int front_level = GET_LOCAL_TABLE_LEVEL(front_size);
//         int front_slot = GET_LOCAL_TABLE_SLOT(front_size, front_level);
//         uint64_t front_step = GET_LOCAL_TABLE_STEP(front_level);
//         uint64_t new_block_size = front_size + size + 16;
//         uint64_t size_diff = new_block_size ^ front_size;
//         PACK_PAYLOAD(front_head, thread_id, 0, new_block_size);
//         if(size_diff < front_step){
//             // no change to the table structure
//             // nothing to add to table
//             block_to_add = NULL;
//         }else{
//             // remove front block from table structure
//             remove_block(front_head, front_level, front_slot);
//             block_to_add = front_head;
//         }
//     }else if(merge_behind){
//         // block_to_add is still payload
//         uint64_t behind_size = GET_CONTENT(behind_head);
//         int behind_level = GET_LOCAL_TABLE_LEVEL(behind_size);
//         int behind_slot = GET_LOCAL_TABLE_SLOT(behind_size, behind_level);
//         uint64_t new_block_size = behind_size + size + 16;
//         PACK_PAYLOAD(payload, thread_id, 0, new_block_size);
//         // can't avoid table structure change
//         remove_block(behind_head, behind_level, behind_slot);
//     }
//     return block_to_add;
}

void *buddify_add(uint64_t *block, size_t size){
    uint64_t total_size = size + 16;
    uint64_t remain_size = total_size;
    uint64_t *new_block = block;
    while(remain_size != 0){
        uint64_t new_block_total_size = 1 << trailing0s(remain_size);
        uint64_t new_block_size = new_block_total_size - 16;
        PACK_PAYLOAD(new_block, thread_id, 0, new_block_size);
        add_bitmap_block(new_block, new_block_size);
        remain_size -= new_block_total_size;
        new_block += new_block_total_size/8;
    }
}
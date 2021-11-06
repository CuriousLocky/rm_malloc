#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "rm_threads.h"

#include "datastructure_bitmap.h"
#include "datastructure_payload.h"
#include "mempool.h"

#define TABLE_PER_META_CHUNK (META_CHUNK_SIZE/sizeof(LocalTable))
#define THREADINFO_PER_CHUNK (META_CHUNK_SIZE/sizeof(ThreadInfo))

// rm_lock_t threadInfo_pool_lock = RM_LOCK_INITIALIZER;
rm_lock_t table_pool_lock = RM_LOCK_INITIALIZER;

static LocalTable *table_pool = NULL;
static ThreadInfo *threadInfo_pool = NULL;
static volatile uint16_t threadInfo_pool_usage = 0;
static volatile uint8_t threadInfo_pool_init_flag = 0;

volatile NonBlockingStackBlock inactive_threadInfo_stack;

//the default values are for initialization in the first call in init functions
static int table_meta_pool_usage = TABLE_PER_META_CHUNK;
// static int threadInfo_meta_pool_usage = THREADINFO_PER_CHUNK;

static uint64_t *giant_root = NULL;

tls ThreadInfo *local_thread_info = NULL;
tls LocalTable *local_level_0_table = NULL;
tls LocalTable *local_level_0_table_big = NULL;

extern tls void *payload_pool;
extern tls size_t payload_pool_size;

tls uint16_t thread_id;

// void print_table(LocalTable *table){
//     printf("\nindex = %lx\n", table->index);
//     for(int i = 0; i < 8; i++){
//         for(int j = 0; j < 8; j++){
//             printf("%p\t", table->entries[8*i+j]);
//         }
//         printf("\n");
//     }
// }

/*create a new table without any contents*/
LocalTable *create_new_table(){
    #ifdef __NOISY_DEBUG
    write(1, "create_new_table\n", sizeof("create_new_table"));
    #endif
    rm_lock(&table_pool_lock);
    if(table_meta_pool_usage == TABLE_PER_META_CHUNK){
        table_pool = meta_chunk_req(META_CHUNK_SIZE);
        table_meta_pool_usage = 0;
    }
    table_meta_pool_usage ++;
    LocalTable *result = table_pool;
    table_pool ++;

    result->index = 0;
    // memset(local_level_0_table->entries, 0, sizeof(local_level_0_table->entries));
    //meta info will always use fresh memory space, already zeroed
    rm_unlock(&table_pool_lock);
    return result;
}

/*go through the threadInfo_list to find an inactive one*/
ThreadInfo *find_inactive_threadInfo(){
    // pop the current inactive threadinfo from the stack
    NonBlockingStackBlock old_block;
    NonBlockingStackBlock new_block;
    do{
        old_block = inactive_threadInfo_stack;
        if(old_block.block_struct.ptr == NULL){
            return NULL;
        }
        new_block.block_struct.ptr = ((ThreadInfo*)(old_block.block_struct.ptr))->next;
        new_block.block_struct.id = old_block.block_struct.id+1;
    }while(!__sync_bool_compare_and_swap(&(inactive_threadInfo_stack.block_16b), old_block.block_16b, new_block.block_16b));
    return old_block.block_struct.ptr;
}

/*initialize the threadInfo_pool with sufficient space so that it will never need to expand*/
void *threadInfo_pool_init(){
    #define THREADINFO_POOL_SIZE    align(MAX_THREAD_NUM*sizeof(ThreadInfo), META_CHUNK_SIZE)
    uint8_t init_id = __atomic_fetch_add(&threadInfo_pool_init_flag, 1, __ATOMIC_RELAXED);
    // init_id is used to create a spinlock so that threadInfo_pool is init only once
    if(init_id == 0){
        threadInfo_pool = meta_chunk_req(THREADINFO_POOL_SIZE);
    }else{
        while(threadInfo_pool==NULL){
            ;
        }
    }
}

/*create a new threadInfo for this thread, with local_level_0_table initialized*/
ThreadInfo *create_new_threadInfo(){
    #ifdef __NOISY_DEBUG
    write(1, "create_new_threadInfo\n", sizeof("create_new_threadInfo"));
    #endif
    // rm_lock(&threadInfo_pool_lock);
    // if(threadInfo_meta_pool_usage == THREADINFO_PER_CHUNK){
    //     threadInfo_pool = meta_chunk_req(META_CHUNK_SIZE);
    //     threadInfo_meta_pool_usage = 0;
    // }
    // threadInfo_meta_pool_usage ++;
    uint16_t id = __atomic_fetch_add(&threadInfo_pool_usage, 1, __ATOMIC_RELAXED);
    ThreadInfo *new_threadInfo = &threadInfo_pool[id];
    // threadInfo_pool ++;
    // rm_unlock(&threadInfo_pool_lock);

    new_threadInfo->next = NULL;
    new_threadInfo->thread_id = id;
    new_threadInfo->level_0_table = create_new_table();
    new_threadInfo->level_0_table_big = create_new_table();
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

    #ifdef __RACE_TEST
    __sync_fetch_and_sub(&(local_thread_info->active), 1);
    #endif

    // push current threadinfo to the non-blocking stack
    NonBlockingStackBlock new_block;
    NonBlockingStackBlock old_block;
    new_block.block_struct.ptr = local_thread_info;
    do{
        old_block = inactive_threadInfo_stack;
        local_thread_info->next = old_block.block_struct.ptr;
        new_block.block_struct.id = old_block.block_struct.id+1;
    }while(!__sync_bool_compare_and_swap(&(inactive_threadInfo_stack.block_16b), old_block.block_16b, new_block.block_16b));

    local_thread_info = NULL;
    local_level_0_table = NULL;
    local_level_0_table_big = NULL;

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
    local_level_0_table = inactive_threadInfo->level_0_table;
    local_level_0_table_big = inactive_threadInfo->level_0_table_big;
    thread_id = inactive_threadInfo->thread_id;
    payload_pool = inactive_threadInfo->payload_pool;
    payload_pool_size = inactive_threadInfo->payload_pool_size;

    #ifdef __RACE_TEST
    if(__sync_fetch_and_add(&(local_thread_info->active), 1) > 0){
        exit(-1);
    }
    #endif

    pthread_key_create(&inactive_key, set_threadInfo_inactive);
    pthread_setspecific(inactive_key, (void*)0x8353);
    // write(1, "exit thread_bitmap_init\n", sizeof("exit thread_bitmap_init"));

    return;
}

uint64_t *find_bitmap_victim(size_t ori_size){
    #ifdef __NOISY_DEBUG
    write(1, "find_bitmap_victim\n", sizeof("find_bitmap_victim"));
    #endif

    size_t size = ori_size - 16;
    uint64_t level_0_offset = (size>>10)&63;
    uint64_t level_1_offset = (size>>4)&63;
    uint64_t level_0_index_mask = ~((1UL<<level_0_offset)-1);
    uint64_t level_1_index_mask = ~((1UL<<level_1_offset)-1);

    uint64_t *result = NULL;
    size_t result_size = 0;

    #ifdef __NOISY_DEBUG
    write(1, "finished global table checking\n", sizeof("finished global table checking"));
    #endif
    // check local table

    // check small table
    if(level_0_offset == 0){
        LocalTable *level_1_table = local_level_0_table;
        uint64_t *table_slot_addr = NULL;
        int level_1_slot = trailing0s((level_1_table->index)&level_1_index_mask);
        if(level_1_slot < 64){
            result = level_1_table->entries[level_1_slot];
            table_slot_addr = (uint64_t*)&(level_1_table->entries[level_1_slot]);
            result_size = level_1_slot<<4;
            uint64_t *new_list_head = GET_NEXT_BLOCK(result);
            level_1_table->entries[level_1_slot] = new_list_head;
            if(new_list_head==NULL){
                level_1_table->index &= ~(1UL<<level_1_slot);
            }else{
                SET_PREV_BLOCK(new_list_head, NULL);
            }
            goto FIND_BITMAP_VICTIM_EXIT;
        }
    }
    // check large table
    uint64_t *table_slot_addr = NULL;
    int level_0_slot = trailing0s((local_level_0_table_big->index)&level_0_index_mask);
    if(level_0_slot < 64){
        LocalTable *level_1_table = local_level_0_table_big->entries[level_0_slot];
        int level_1_slot = trailing0s((level_1_table->index)&level_1_index_mask);
        if(level_1_slot < 64){
            result = level_1_table->entries[level_1_slot];
            table_slot_addr = (uint64_t*)&(level_1_table->entries[level_1_slot]);
            result_size = (((uint64_t)level_0_slot<<6)+level_1_slot)<<4;
            uint64_t *new_list_head = GET_NEXT_BLOCK(result);
            level_1_table->entries[level_1_slot] = new_list_head;
            if(new_list_head==NULL){
                level_1_table->index &= ~(1UL<<level_1_slot);
                if(level_1_table->index == 0){
                    local_level_0_table_big->index &= ~(1UL<<level_0_slot);
                }
            }else{
                SET_PREV_BLOCK(new_list_head, NULL);
            }
        }
    }

    #ifdef __NOISY_DEBUG
    write(1, "finished local table checking\n", sizeof("finished local table checking"));
    #endif
FIND_BITMAP_VICTIM_EXIT:
    if(result != NULL){
        // printf("result_size = %ld\n", result_size);
        size_t size_diff = result_size - size;
        // size_diff should be tested and replaced with a less aggressive(larger) number
        if(size_diff > 16){
            result_size = size;
            uint64_t *new_block = GET_PAYLOAD_TAIL(result, ori_size) + 1;
            size_t new_block_size = size_diff - 16;
            add_bitmap_block(new_block, new_block_size);
        }
        PACK_PAYLOAD_HEAD(result, 1, 0x1f, result_size + 16);
        PACK_PAYLOAD_TAIL(result, 1, result_size+16);
    }
    return result;
}

void add_bitmap_block(uint64_t *block, size_t size){
    #ifdef __NOISY_DEBUG
    write(1, "add_bitmap_block\n", sizeof("add_bitmap_block"));
    #endif
    if(block==NULL){
        #ifdef __NOISY_DEBUG
        write(1, "add_bitmap_block received null, returnning\n", sizeof("add_bitmap_block received null, returnning"));
        #endif
        return;
    }
    size -= 16;
    uint64_t level_0_offset = (size>>10)&63;
    uint64_t level_1_offset = (size>>4)&63;

    // add to small list
    if(level_0_offset == 0){
        LocalTable *level_1_table = local_level_0_table;
        uint64_t *old_list_head = level_1_table->entries[level_1_offset];
        SET_NEXT_BLOCK(block, old_list_head);
        level_1_table->entries[level_1_offset] = block;
        if(old_list_head == NULL){
            level_1_table->index |= 1UL<<level_1_offset;
        }else{
            SET_PREV_BLOCK(old_list_head, block);
        }
        return;
    }

    // add to large list
    LocalTable *level_0_table = local_level_0_table_big;
    if(level_0_table->entries[level_0_offset]==NULL){
        level_0_table->entries[level_0_offset] = create_new_table();
        level_0_table->index |= 1UL<<level_0_offset;
    }
    LocalTable *level_1_table = level_0_table->entries[level_0_offset];
    // printf("level 1 table is %p\n", level_1_table);
    uint64_t *old_list_head = level_1_table->entries[level_1_offset];
    SET_NEXT_BLOCK(block, level_1_table->entries[level_1_offset]);
    level_1_table->entries[level_1_offset] = block;
    if(old_list_head == NULL){
        level_1_table->index |= 1UL<<level_1_offset;
    }else{
        SET_PREV_BLOCK(old_list_head, block);
    }
}


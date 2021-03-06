#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "rm_threads.h"

#include "datastructure_bitmap.h"
#include "datastructure_payload.h"
#include "mempool.h"

#define TABLE_PER_META_CHUNK (META_CHUNK_SIZE/sizeof(Table))
#define THREADINFO_PER_CHUNK (META_CHUNK_SIZE/sizeof(ThreadInfo))

rm_lock_t threadInfo_pool_lock = RM_LOCK_INITIALIZER;
// static bool threadInfo_pool_lock_init_flag = 0;
rm_lock_t table_pool_lock = RM_LOCK_INITIALIZER;
// static bool table_pool_lock_init_flag = 0;

static Table *table_pool = NULL;
static ThreadInfo *threadInfo_pool = NULL;
static ThreadInfo *threadInfo_list_head = NULL;
static ThreadInfo *threadInfo_list_tail = NULL;
static uint16_t threadInfo_list_size = 0;

//the default values are for initialization in the first call in init functions
static int table_meta_pool_usage = TABLE_PER_META_CHUNK;
static int threadInfo_meta_pool_usage = THREADINFO_PER_CHUNK;

static rm_once_t global_level_0_table_flag = RM_ONCE_INITIALIZER;
static Table *global_level_0_table = NULL;
static rm_lock_t global_lock = RM_LOCK_INITIALIZER;
static uint64_t *giant_root = NULL;

tls ThreadInfo *local_thread_info = NULL;
tls Table *local_level_0_table = NULL;

extern tls void *payload_pool;
extern tls size_t payload_pool_size;

tls uint16_t thread_id;

// void print_table(Table *table){
//     printf("\nindex = %lx\n", table->index);
//     for(int i = 0; i < 8; i++){
//         for(int j = 0; j < 8; j++){
//             printf("%p\t", table->entries[8*i+j]);
//         }
//         printf("\n");
//     }
// }

/*create a new table without any contents*/
Table *create_new_table(){
    #ifdef __NOISY_DEBUG
    write(1, "create_new_table\n", sizeof("create_new_table"));
    #endif
    rm_lock(&table_pool_lock);
    if(table_meta_pool_usage == TABLE_PER_META_CHUNK){
        table_pool = meta_chunk_req();
        table_meta_pool_usage = 0;
    }
    table_meta_pool_usage ++;
    Table *result = table_pool;
    table_pool ++;

    result->index = 0;
    // memset(local_level_0_table->entries, 0, sizeof(local_level_0_table->entries));
    //meta info will always use fresh memory space, already zeroed
    rm_unlock(&table_pool_lock);
    return result;
}

/*go through the threadInfo_list to find an inactive one*/
ThreadInfo *find_inactive_threadInfo(){
    if(threadInfo_list_head==NULL){return NULL;}
    ThreadInfo *walker = threadInfo_list_head;
    while(walker!=NULL){
        if(walker->active == false){
            return walker;
        }
        walker = walker->next;
    }
    return walker;
}

/*create a new threadInfo for this thread, with local_level_0_table initialized*/
ThreadInfo *create_new_threadInfo(){
    #ifdef __NOISY_DEBUG
    write(1, "create_new_threadInfo\n", sizeof("create_new_threadInfo"));
    #endif
    if(threadInfo_meta_pool_usage == THREADINFO_PER_CHUNK){
        // printf("requesting meta_chunk\n");
        threadInfo_pool = meta_chunk_req();
        threadInfo_meta_pool_usage = 0;
    }
    threadInfo_meta_pool_usage ++;
    ThreadInfo *new_threadInfo = threadInfo_pool;
    threadInfo_pool ++;

    new_threadInfo->active = true;
    new_threadInfo->next = NULL;
    new_threadInfo->thread_id = threadInfo_list_size;
    new_threadInfo->level_0_table = create_new_table();
    new_threadInfo->payload_pool = NULL;
    new_threadInfo->payload_pool_size = 0;
    rm_lock_init(&(new_threadInfo->thread_lock));
    threadInfo_list_size++;

    if(threadInfo_list_head == NULL){
        threadInfo_list_head = new_threadInfo;
    }

    if(threadInfo_list_tail != NULL){
        threadInfo_list_tail->next = new_threadInfo;
    }
    threadInfo_list_tail = new_threadInfo;

    return new_threadInfo;
}

pthread_key_t inactive_key;
/*to set the threadinfo as inactive so it can be reused*/
void set_threadInfo_inactive(void *arg){
    #ifdef __NOISY_DEBUG
    write(1, "cleaning up\n", sizeof("cleaning up"));
    #endif
    local_thread_info->active = false;
    local_thread_info->payload_pool = payload_pool;
    local_thread_info->payload_pool_size = payload_pool_size;
}

void global_level_0_table_init(){
    global_level_0_table = create_new_table();
}

void thread_bitmap_init(){
    #ifdef __NOISY_DEBUG
    write(1, "thread_bitmap_init\n", sizeof("thread_bitmap_init"));
    #endif
    // pthread_cleanup_push(set_threadInfo_inactive, NULL);    // not a safe implementation, but no idea how to improve
    rm_lock(&threadInfo_pool_lock);
    rm_callonce(&global_level_0_table_flag, global_level_0_table_init);
    ThreadInfo *inactive_threadInfo = find_inactive_threadInfo();
    #ifdef __NOISY_DEBUG
    write(1, "find_inactive_threadInfo returned\n", sizeof("find_inactive_threadInfo returned"));
    #endif

    if(inactive_threadInfo == NULL){
        inactive_threadInfo = create_new_threadInfo();
    }else{
        inactive_threadInfo->active = true;
    }
    local_thread_info = inactive_threadInfo;
    local_level_0_table = inactive_threadInfo->level_0_table;
    thread_id = inactive_threadInfo->thread_id;
    payload_pool = inactive_threadInfo->payload_pool;
    payload_pool_size = inactive_threadInfo->payload_pool_size;
    
    // if(inactive_threadInfo != NULL){
    //     inactive_threadInfo->active = true;
        
    // }else{
    //     local_thread_info = create_new_threadInfo();
    //     // printf("threadInfo created\n");
    //     local_level_0_table = create_new_table();
    //     // printf("level 0 table created\n");
    //     local_thread_info->level_0_table = local_level_0_table;
    //     thread_id = local_thread_info->thread_id;
    // }
    rm_unlock(&threadInfo_pool_lock);
    pthread_key_create(&inactive_key, set_threadInfo_inactive);
    pthread_setspecific(inactive_key, (void*)0x8353);
    // write(1, "exit thread_bitmap_init\n", sizeof("exit thread_bitmap_init"));
    // print_table(global_level_0_table);
    // print_table(local_level_0_table);
    return;
    // pthread_cleanup_pop(0);
}

uint64_t *find_bitmap_victim(size_t ori_size){
    #ifdef __NOISY_DEBUG
    write(1, "find_bitmap_victim\n", sizeof("find_bitmap_victim"));
    #endif
    if(local_level_0_table==NULL){
        thread_bitmap_init();
        #ifdef __NOISY_DEBUG
        write(1, "thread_bitmap_init returned\n", sizeof("thread_bitmap_init returned"));
        #endif
    }
    size_t size = ori_size - 16;
    int level_0_offset = (size>>10)&63;
    int level_1_offset = (size>>4)&63;
    uint64_t level_0_index_mask = ~((((uint64_t)1)<<level_0_offset)-1);
    uint64_t level_1_index_mask = ~((((uint64_t)1)<<level_1_offset)-1);
    // printf("level 1 index mask is %lx\n", level_1_index_mask);
    uint64_t *result = NULL;
    size_t result_size = 0;
    // check global table
    // printf("checking global table\n");
    if(rm_trylock(&global_lock)==RM_LOCKED){
        int level_0_slot = trailing0s((global_level_0_table->index)&level_0_index_mask);
        if(level_0_slot < 64){
            // printf("level_0_slot = %d\n", level_0_slot);
            Table *level_1_table = global_level_0_table->entries[level_0_slot];
            int level_1_slot = trailing0s((level_1_table->index)&level_1_index_mask);
            if(level_1_slot < 64){
                result = level_1_table->entries[level_1_slot];
                result_size = (((uint64_t)level_0_slot<<6)+level_1_slot)<<4;
                level_1_table->entries[level_1_slot] = GET_NEXT_BLOCK(result);
                if(GET_NEXT_BLOCK(result)==NULL){
                    level_1_table->index &= ~((uint64_t)1<<level_1_slot);
                    if(level_1_table->index == 0){
                        global_level_0_table->index &= ~((uint64_t)1<<level_0_slot);
                    }
                }
            }
        }
        rm_unlock(&global_lock);
    }
    #ifdef __NOISY_DEBUG
    write(1, "finished global table checking\n", sizeof("finished global table checking"));
    #endif
    // check local table
    if(result == NULL){
        // mtx_lock(&(local_thread_info->thread_lock));
        rm_lock(&(local_thread_info->thread_lock));
        int level_0_slot = trailing0s((local_level_0_table->index)&level_0_index_mask);
        if(level_0_slot < 64){
            Table *level_1_table = local_level_0_table->entries[level_0_slot];
            int level_1_slot = trailing0s((level_1_table->index)&level_1_index_mask);
            if(level_1_slot < 64){
                result = level_1_table->entries[level_1_slot];
                result_size = (((uint64_t)level_0_slot<<6)+level_1_slot)<<4;
                level_1_table->entries[level_1_slot] = GET_NEXT_BLOCK(result);
                if(GET_NEXT_BLOCK(result)==NULL){
                    level_1_table->index &= ~((uint64_t)1<<level_1_slot);
                    if(level_1_table->index == 0){
                        local_level_0_table->index &= ~((uint64_t)1<<level_0_slot);
                    }
                }
            }
        }
        // mtx_unlock(&(local_thread_info->thread_lock));
        rm_unlock(&(local_thread_info->thread_lock));
    }
    #ifdef __NOISY_DEBUG
    write(1, "finished local table checking\n", sizeof("finished local table checking"));
    #endif
    if(result != NULL){
        // printf("result_size = %ld\n", result_size);
        size_t size_diff = result_size - size;
        // size_diff should be tested and replaced with a less aggressive(larger) number
        if(size_diff > 16){
            uint64_t *new_block = GET_PAYLOAD_TAIL(result, ori_size) + 1;
            size_t new_block_size = size_diff - 16;
            add_bitmap_block(new_block, new_block_size);
        }
    }
    // printf("returned result\n");
    // print_table(global_level_0_table);
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
    if(local_level_0_table==NULL){
        thread_bitmap_init();
    }
    size -= 16;
    int level_0_offset = (size>>10)&63;
    // printf("level_0_offset = %d\n", level_0_offset);
    int level_1_offset = (size>>4)&63;
    // printf("level_1_offset = %d\n", level_1_offset);
    rm_lock_t *lock = &(local_thread_info->thread_lock);
    Table *level_0_table = local_level_0_table;
    if(rm_trylock(&global_lock)==RM_LOCKED){
        lock = &global_lock;
        level_0_table = global_level_0_table;
    }else{
        // mtx_lock(lock);
        rm_lock(lock);
    }
    level_0_table->index |= (uint64_t)1<<level_0_offset;
    if(level_0_table->entries[level_0_offset]==NULL){
        level_0_table->entries[level_0_offset] = create_new_table();
    }
    Table *level_1_table = level_0_table->entries[level_0_offset];
    // printf("level 1 table is %p\n", level_1_table);
    level_1_table->index |= (uint64_t)1<<level_1_offset;
    SET_NEXT_BLOCK(block, level_1_table->entries[level_1_offset]);
    level_1_table->entries[level_1_offset] = block;
    // mtx_unlock(lock);
    rm_unlock(lock);
    // print_table(global_level_0_table);
    // print_table(global_level_0_table->entries[0]);
}


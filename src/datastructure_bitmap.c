#include <threads.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "datastructure_bitmap.h"
#include "mempool.h"

#define TABLE_PER_META_CHUNK (META_CHUNK_SIZE/sizeof(Table))
#define THREADINFO_PER_CHUNK (META_CHUNK_SIZE/sizeof(ThreadInfo))

static mtx_t meta_pool_lock;
static Table *table_meta_pool;
static ThreadInfo *threadInfo_meta_pool;
static ThreadInfo *threadInfo_list_head = NULL;
static ThreadInfo *threadInfo_list_tail = NULL;
static uint16_t threadInfo_list_size = 0;

//the default values are for initialization in the first call in init functions
static int table_meta_pool_usage = TABLE_PER_META_CHUNK;
static int threadInfo_meta_pool_usage = THREADINFO_PER_CHUNK;

static Table *global_level_0_table;
static mtx_t global_lock;
static uint64_t *giant_root = NULL;

thread_local ThreadInfo *local_thread_info;
thread_local Table *local_level_0_table;

thread_local uint16_t thread_id;

void print_table(Table *table){
    printf("\nindex = %lx\n", table->index);
    for(int i = 0; i < 8; i++){
        for(int j = 0; j < 8; j++){
            printf("%p\t", table->entries[8*i+j]);
        }
        printf("\n");
    }
}

/*create a new table without any contents*/
Table *create_new_table(){
    if(table_meta_pool_usage == TABLE_PER_META_CHUNK){
        table_meta_pool = meta_chunk_req();
        table_meta_pool_usage = 0;
    }
    table_meta_pool_usage ++;
    Table *result = table_meta_pool;
    table_meta_pool ++;

    result->index = 0;
    //memset(local_level_0_table->entries, 0, sizeof(local_level_0_table->entries));
    //meta info will always use fresh memory space, already zeroed
    return result;
}

/*go through the threadInfo_list to find an inactive one*/
ThreadInfo *find_inactive_threadInfo(){
    if(threadInfo_list_head==NULL){return NULL;}
    ThreadInfo *walker = threadInfo_list_head;
    while(walker->active==1 && walker->next!=NULL){
        walker = walker->next;
    }
    if(walker->next == NULL){
        return NULL;
    }
    return walker;
}

/*create a new threadInfo for this thread, with local_level_0_table uninitialized*/
ThreadInfo *create_new_threadInfo(){
    if(threadInfo_meta_pool_usage == THREADINFO_PER_CHUNK){
        printf("requesting meta_chunk\n");
        threadInfo_meta_pool = meta_chunk_req();
        threadInfo_meta_pool_usage = 0;
    }
    threadInfo_meta_pool_usage ++;
    ThreadInfo *new_threadInfo = threadInfo_meta_pool;
    threadInfo_meta_pool ++;

    new_threadInfo->active = 1;
    new_threadInfo->next = NULL;
    new_threadInfo->thread_id = threadInfo_list_size;
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

/*to set the threadinfo as inactive so it can be reused*/
static tss_t thread_set_inactive_key;
void set_threadInfo_inactive(void *arg){
    local_thread_info->active = false;
}

/*To initialize the global freelist*/
static once_flag global_freelist_init_flag;
void global_freelist_init(){
    global_level_0_table = create_new_table();
    printf("global_freelist_init finished\n");
}

thread_local bool thread_bitmap_init_flag = false;
void thread_bitmap_init(){
    printf("into thread_bitmap_init\n");
    tss_create(&thread_set_inactive_key, &set_threadInfo_inactive);
    tss_set(thread_set_inactive_key, (void*)0x8353);
    mtx_lock(&meta_pool_lock);
    call_once(&global_freelist_init_flag, &global_freelist_init);
    ThreadInfo *inactive_threadInfo = find_inactive_threadInfo();
    printf("find_inactive_threadInfo returned, %p\n", inactive_threadInfo);
    if(inactive_threadInfo != NULL){
        inactive_threadInfo->active = true;
        local_thread_info = inactive_threadInfo;
        local_level_0_table = inactive_threadInfo->level_0_table;
        thread_id = inactive_threadInfo->thread_id;
    }else{
        local_thread_info = create_new_threadInfo();
        printf("threadInfo created\n");
        local_level_0_table = create_new_table();
        printf("level 0 table created\n");
        local_thread_info->level_0_table = local_level_0_table;
        thread_id = local_thread_info->thread_id;
    }
    mtx_unlock(&meta_pool_lock);
    printf("thread_bitmap_init returned\n");
    print_table(global_level_0_table);
    print_table(local_level_0_table);
}

uint64_t *find_bitmap_victim(size_t size){
    printf("into find_bitmap_victim\n");
    if(!thread_bitmap_init_flag){
        thread_bitmap_init();
    }
    size -= 16;
    int level_0_offset = (size>>10)&63;
    int level_1_offset = (size>>4)&63;
    uint64_t level_0_index_mask = ~((((uint64_t)1)<<level_0_offset)-1);
    uint64_t level_1_index_mask = ~((((uint64_t)1)<<level_1_offset)-1);
    uint64_t *result = NULL;
    size_t result_size = 0;
    // check global table
    printf("checking global table\n");
    if(mtx_trylock(&global_lock)==thrd_success){
        int level_0_slot = trailing0s((global_level_0_table->index)&level_0_index_mask);
        if(level_0_slot < 64){
            printf("level_0_slot = %d\n", level_0_slot);
            Table *level_1_table = global_level_0_table->entries[level_0_slot];
            int level_1_slot = trailing0s((level_1_table->index)&level_1_index_mask);
            if(level_1_slot < 64){
                result = level_1_table->entries[level_1_slot];
                result_size = ((level_0_slot<<6)+level_1_slot)<<4;
                level_1_table->entries[level_1_slot] = GET_NEXT_BLOCK(result);
                if(GET_NEXT_BLOCK(result)==NULL){
                    level_1_table->index &= ~((uint64_t)1<<level_1_slot);
                    if(level_1_table->index == 0){
                        global_level_0_table->index &= ~((uint64_t)1<<level_0_slot);
                    }
                }
            }
        }
        mtx_unlock(&global_lock);
    }
    printf("finished global table checking\n");
    // check local table
    if(result == NULL){
        mtx_lock(&(local_thread_info->thread_lock));
        int level_0_slot = trailing0s((local_level_0_table->index)&level_0_index_mask);
        if(level_0_slot < 64){
            Table *level_1_table = local_level_0_table->entries[level_0_slot];
            int level_1_slot = trailing0s((level_1_table->index)&level_1_index_mask);
            if(level_1_slot < 64){
                result = level_1_table->entries[level_1_slot];
                result_size = ((level_0_slot<<6)+level_1_slot)<<4;
                level_1_table->entries[level_1_slot] = GET_NEXT_BLOCK(result);
                if(GET_NEXT_BLOCK(result)==NULL){
                    level_1_table->index &= ~((uint64_t)1<<level_1_slot);
                    if(level_1_table->index == 0){
                        local_level_0_table->index &= ~((uint64_t)1<<level_0_slot);
                    }
                }
            }
        }
        mtx_unlock(&(local_thread_info->thread_lock));
    }
    if(result != NULL){
        size_t size_diff = result_size - size;
        if(size_diff > 16){
            uint64_t *new_block = (uint64_t*)((char*)result + size + 16);
            size_t new_block_size = size_diff - 16;
            add_bitmap_block(new_block, new_block_size);
        }
    }
    return result;
}

void add_bitmap_block(uint64_t *block, size_t size){
    if(!thread_bitmap_init_flag){
        thread_bitmap_init();
    }
    size -= 16;
    int level_0_offset = (size>>10)&63;
    int level_1_offset = (size>>4)&63;
    mtx_t *lock = &(local_thread_info->thread_lock);
    Table *level_0_table = local_level_0_table;
    if(mtx_trylock(&global_lock)==thrd_success){
        lock = &global_lock;
        level_0_table = global_level_0_table;
    }else{
        mtx_lock(lock);
    }
    level_0_table->index |= (uint64_t)1<<level_0_offset;
    if(level_0_table->entries[level_1_offset]==NULL){
        level_0_table->entries[level_1_offset] = create_new_table();
    }
    Table *level_1_table = level_0_table->entries[level_0_offset];
    level_0_table->index |= (uint64_t)1<<level_1_offset;
    SET_NEXT_BLOCK(block, level_1_table->entries[level_1_offset]);
    level_1_table->entries[level_1_offset] = block;
    mtx_unlock(lock);
}


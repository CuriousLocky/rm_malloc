#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "system_macros.h"

#include "datastructure_bitmap.h"
#include "mempool.h"

#define TABLE_PER_META_CHUNK (META_CHUNK_SIZE/sizeof(Table))
#define THREADINFO_PER_CHUNK (META_CHUNK_SIZE/sizeof(ThreadInfo))

// static pthread_spinlock_t threadInfo_pool_lock;
pthread_mutex_t threadInfo_pool_lock = PTHREAD_MUTEX_INITIALIZER;
static bool threadInfo_pool_lock_init_flag = 0;
// static pthread_spinlock_t table_pool_lock;
pthread_mutex_t table_pool_lock = PTHREAD_MUTEX_INITIALIZER;
static bool table_pool_lock_init_flag = 0;
// static mtx_t threadInfo_pool_lock;
// static mtx_t table_pool_lock;
static Table *table_pool;
static ThreadInfo *threadInfo_pool;
static ThreadInfo *threadInfo_list_head = NULL;
static ThreadInfo *threadInfo_list_tail = NULL;
static uint16_t threadInfo_list_size = 0;

//the default values are for initialization in the first call in init functions
static int table_meta_pool_usage = TABLE_PER_META_CHUNK;
static int threadInfo_meta_pool_usage = THREADINFO_PER_CHUNK;

static Table *global_level_0_table;
// static mtx_t global_lock;
// static pthread_spinlock_t global_lock;
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
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
    // mtx_lock(&table_pool_lock);
    // pthread_spin_lock(&table_pool_lock);
    pthread_mutex_lock(&table_pool_lock);
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

    // printf("table_pool is %p\n", table_pool);
    // printf("table_meta_usage is %d\n", table_meta_pool_usage);
    // mtx_unlock(&table_pool_lock);
    // pthread_spin_unlock(&table_pool_lock);
    pthread_mutex_unlock(&table_pool_lock);
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

/*create a new threadInfo for this thread, with local_level_0_table uninitialized*/
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
    // mtx_init(&(new_threadInfo->thread_lock), mtx_plain);
    pthread_mutex_init(&(new_threadInfo->thread_lock), PTHREAD_PROCESS_PRIVATE);
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
static pthread_key_t thread_set_inactive_key;
void set_threadInfo_inactive(void *arg){
    #ifdef __NOISY_DEBUG
    write(1, "cleaning up\n", sizeof("cleaning up"));
    #endif
    local_thread_info->active = false;
    local_thread_info->payload_pool = payload_pool;
    local_thread_info->payload_pool_size = payload_pool_size;
}

/*To initialize the global freelist*/
// pthread_once_t global_freelist_init_flag = PTHREAD_ONCE_INIT;
// void global_freelist_init(){
//     global_level_0_table = create_new_table();
//     // printf("global_freelist_init finished\n");
// }

/*To initialize the meta pool lock*/
// pthread_once_t meta_pool_lock_init_flag = PTHREAD_ONCE_INIT;
// void meta_pool_lock_init(){
//     // mtx_init(&threadInfo_pool_lock, mtx_plain);
//     #ifdef __NOISY_DEBUG
//     write(1, "meta_pool_lock_init\n", sizeof("meta_pool_lock_init"));
//     #endif
//     pthread_spin_init(&threadInfo_pool_lock, PTHREAD_PROCESS_PRIVATE);
//     pthread_spin_init(&table_pool_lock, PTHREAD_PROCESS_PRIVATE);
// }

void thread_bitmap_init(){
    #ifdef __NOISY_DEBUG
    write(1, "thread_bitmap_init\n", sizeof("thread_bitmap_init"));
    #endif
    // pthread_once(&meta_pool_lock_init_flag, &meta_pool_lock_init);
    // if(threadInfo_pool_lock_init_flag==0){
    //     threadInfo_pool_lock_init_flag = 1;
    //     pthread_spin_init(&threadInfo_pool_lock, PTHREAD_PROCESS_PRIVATE);
    //     pthread_spin_init(&table_pool_lock, PTHREAD_PROCESS_PRIVATE);
    //     write(1, "du\n", sizeof("du"));
    // }
    pthread_key_create(&thread_set_inactive_key, set_threadInfo_inactive);
    write(1, "du\n", sizeof("du"));
    // pthread_setspecific(thread_set_inactive_key, (void*)0x8353);
    // mtx_lock(&threadInfo_pool_lock);
    // pthread_spin_lock(&threadInfo_pool_lock);
    pthread_mutex_lock(&threadInfo_pool_lock);
    if(global_level_0_table==NULL){
        global_level_0_table = create_new_table();
    }
    // pthread_once(&global_freelist_init_flag, &global_freelist_init);
    ThreadInfo *inactive_threadInfo = find_inactive_threadInfo();
    #ifdef __NOISY_DEBUG
    write(1, "find_inactive_threadInfo returned\n", sizeof("find_inactive_threadInfo returned"));
    #endif
    
    if(inactive_threadInfo != NULL){
        inactive_threadInfo->active = true;
        local_thread_info = inactive_threadInfo;
        local_level_0_table = inactive_threadInfo->level_0_table;
        thread_id = inactive_threadInfo->thread_id;
        payload_pool = inactive_threadInfo->payload_pool;
        payload_pool_size = inactive_threadInfo->payload_pool_size;
    }else{
        local_thread_info = create_new_threadInfo();
        // printf("threadInfo created\n");
        local_level_0_table = create_new_table();
        // printf("level 0 table created\n");
        local_thread_info->level_0_table = local_level_0_table;
        thread_id = local_thread_info->thread_id;
    }
    // mtx_unlock(&threadInfo_pool_lock);
    // pthread_spin_unlock(&threadInfo_pool_lock);
    pthread_mutex_unlock(&threadInfo_pool_lock);
    // write(1, "exit thread_bitmap_init\n", sizeof("exit thread_bitmap_init"));
    // print_table(global_level_0_table);
    // print_table(local_level_0_table);
}

uint64_t *find_bitmap_victim(size_t size){
    #ifdef __NOISY_DEBUG
    write(1, "find_bitmap_victim\n", sizeof("find_bitmap_victim"));
    #endif
    if(local_level_0_table==NULL){
        thread_bitmap_init();
        #ifdef __NOISY_DEBUG
        write(1, "thread_bitmap_init returned\n", sizeof("thread_bitmap_init returned"));
        #endif
    }
    size -= 16;
    int level_0_offset = (size>>10)&63;
    int level_1_offset = (size>>4)&63;
    uint64_t level_0_index_mask = ~((((uint64_t)1)<<level_0_offset)-1);
    uint64_t level_1_index_mask = ~((((uint64_t)1)<<level_1_offset)-1);
    // printf("level 1 index mask is %lx\n", level_1_index_mask);
    uint64_t *result = NULL;
    size_t result_size = 0;
    // check global table
    // printf("checking global table\n");
    // if(mtx_trylock(&global_lock)==thrd_success){
    // if(pthread_spin_trylock(&global_lock)==0){
    if(pthread_mutex_trylock(&global_lock)==0){
        int level_0_slot = trailing0s((global_level_0_table->index)&level_0_index_mask);
        if(level_0_slot < 64){
            // printf("level_0_slot = %d\n", level_0_slot);
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
        // mtx_unlock(&global_lock);
        pthread_mutex_unlock(&global_lock);
    }
    #ifdef __NOISY_DEBUG
    write(1, "finished global table checking\n", sizeof("finished global table checking"));
    #endif
    // check local table
    if(result == NULL){
        // mtx_lock(&(local_thread_info->thread_lock));
        pthread_mutex_lock(&(local_thread_info->thread_lock));
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
        // mtx_unlock(&(local_thread_info->thread_lock));
        pthread_mutex_unlock(&(local_thread_info->thread_lock));
    }
    #ifdef __NOISY_DEBUG
    write(1, "finished local table checking\n", sizeof("finished local table checking"));
    #endif
    if(result != NULL){
        // printf("result_size = %ld\n", result_size);
        size_t size_diff = result_size - size;
        if(size_diff > 16){
            uint64_t *new_block = (uint64_t*)((char*)result + size + 32);
            size_t new_block_size = size_diff - 16;
            add_bitmap_block(new_block, new_block_size);
        }
    }
    // printf("returned result\n");
    // print_table(global_level_0_table);
    // if(global_level_0_table->entries[0]!=NULL){
    //     print_table(global_level_0_table->entries[0]);
    // }
    return result;
}

void add_bitmap_block(uint64_t *block, size_t size){
    #ifdef __NOISY_DEBUG
    write(1, "add_bitmap_block\n", sizeof("add_bitmap_block"));
    #endif
    if(block==NULL){return;}
    if(local_level_0_table==NULL){
        thread_bitmap_init();
    }
    size -= 16;
    int level_0_offset = (size>>10)&63;
    // printf("level_0_offset = %d\n", level_0_offset);
    int level_1_offset = (size>>4)&63;
    // printf("level_1_offset = %d\n", level_1_offset);
    // mtx_t *lock = &(local_thread_info->thread_lock);
    // pthread_spinlock_t *lock = &(local_thread_info->thread_lock);
    pthread_mutex_t *lock = &(local_thread_info->thread_lock);
    Table *level_0_table = local_level_0_table;
    // if(mtx_trylock(&global_lock)==thrd_success){
    if(pthread_mutex_trylock(&global_lock)==0){
        lock = &global_lock;
        level_0_table = global_level_0_table;
    }else{
        // mtx_lock(lock);
        pthread_mutex_lock(lock);
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
    pthread_mutex_unlock(lock);
    // print_table(global_level_0_table);
    // print_table(global_level_0_table->entries[0]);
}


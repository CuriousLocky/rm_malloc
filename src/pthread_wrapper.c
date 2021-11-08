#include "pthread_wrapper.h"
#include <threads.h>
#include "datastructure_bitmap.h"
#include "rm_malloc.h"

#define __USE_GNU
#include <dlfcn.h>

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

typedef int (*pthread_create_fpt)(pthread_t *, const pthread_attr_t *, void *(*)(void*), void *);

static pthread_create_fpt thread_create = NULL;
volatile uint8_t thread_create_init_flag = 0;

typedef struct Task{
    void *(*routine)(void *);
    void *arg;
}Task;

void find_thread_create(){
    char *error;
    dlerror();  // Clear any existing error

    thread_create = (pthread_create_fpt)dlsym(RTLD_NEXT, "pthread_create");
    if ((error = dlerror()) != NULL) {
        fprintf(stderr, "%s\n", error);
        exit(EXIT_FAILURE);
    }
}

/*
#XXX: 
This is a dummy function to execute thread_bitmap_init() before the real task. It will malloc from
the parent thread and free in the child thread, resulting an inevitable cross-thread free. It's only
one block but the consiquence is hard to track since the threadInfo is reused. 
*/

void *wrapped_task(void *task){
    thread_bitmap_init();
    Task* task_content = task;
    void *(*routine)(void *) = task_content->routine;
    void *arg = task_content->arg;
    rm_free(task);
    routine(arg);
}

int rm_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg){
    if(thread_create == NULL){
        uint8_t init_flag = __atomic_fetch_add(&thread_create_init_flag, 1, __ATOMIC_RELAXED);
        if(init_flag == 0){
            find_thread_create();
        }else{
            do{
                ;
            }while(thread_create==NULL);
        }
    }

    Task *task = rm_malloc(sizeof(Task));
    task->routine = start_routine;
    task->arg = arg;
    
    return thread_create(thread, attr, wrapped_task, task);
}

__attribute__((visibility("default")))
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg)
                   __attribute__((weak, alias("rm_pthread_create")));
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include <dlfcn.h>

#define _GNU_SOURCE
#include <getopt.h>

#define PSEUDOSEED 92320736

int minSize = 10;
int maxSize = 1000;
int pairNum = 50000;
int arraySize = 1000;
int term = 0;
uint64_t allocNum = 0;
int threadNum = 1;
int randomSeed = (435300 + 1251900); // a random number chosen
// pthread_mutex_t allocNumLock = PTHREAD_MUTEX_INITIALIZER;
int sleepTime = 30;
char *mallocLibPath = NULL;
pthread_spinlock_t allocNumLock;

void *(*my_malloc)(size_t) = malloc;
void (*my_free)(void*) = free;
int (*my_pthread_create)(pthread_t*, const pthread_attr_t*, 
    void *(*)(void *), void*) = pthread_create;

enum args{
    ARG_MINSIZE, ARG_MAXSIZE, ARG_THREAD, ARG_RANDOM, ARG_ARRAY, ARG_PAIR, ARG_SLEEP, ARG_LIB
};

struct option longopts[] = {
    {"min",     required_argument,  NULL, ARG_MINSIZE},
    {"max",     required_argument,  NULL, ARG_MAXSIZE},
    {"thread",  required_argument,  NULL, ARG_THREAD},
    {"random",  required_argument,  NULL, ARG_RANDOM},
    {"array",   required_argument,  NULL, ARG_ARRAY},
    {"pair",    required_argument,  NULL, ARG_PAIR},
    {"sleep",   required_argument,  NULL, ARG_SLEEP},
    {"lib",     required_argument,  NULL, ARG_LIB}
};

uint gen_pseudo_rand(uint input){
    input ^= input<<13;
    input ^= input>>17;
    input ^= input<<5;
    return input;
}

void *run_thread(void *arg){
    char **pointerArr = arg;
    uint64_t allocNum_thread;
    uint randomNum = rand();
    for(allocNum_thread = 0; allocNum_thread < pairNum; allocNum_thread++){
        if(term){break;}
        randomNum = gen_pseudo_rand(randomNum);
        int operationPos = randomNum%arraySize;
        my_free(pointerArr[operationPos]);
        randomNum = gen_pseudo_rand(randomNum);
        int allocSize = minSize + (randomNum%(maxSize-minSize));
        pointerArr[operationPos] = my_malloc(allocSize);
    }
    __sync_add_and_fetch(&allocNum, allocNum_thread);
    // pthread_spin_lock(&allocNumLock);
    // allocNum += allocNum_thread;
    // pthread_spin_unlock(&allocNumLock);
    if(!term){
        pthread_t newThread;
        my_pthread_create(&newThread, NULL, run_thread, pointerArr);
        pthread_detach(newThread);
    }
    return NULL;
}

int main(int argc, char** argv){
    int arg_opt;
    while((arg_opt = getopt_long_only(argc, argv, "h", longopts, NULL))!=-1){
        switch (arg_opt){
            case ARG_MINSIZE:
                minSize = atoi(optarg); break;
            case ARG_MAXSIZE:
                maxSize = atoi(optarg); break;
            case ARG_THREAD:
                threadNum = atoi(optarg); break;
            case ARG_RANDOM:
                randomSeed = atoi(optarg); break;
            case ARG_ARRAY:
                arraySize = atoi(optarg); break;
            case ARG_PAIR:
                pairNum = atoi(optarg); break;
            case ARG_SLEEP:
                sleepTime = atoi(optarg); break;
            case ARG_LIB:{
                    printf("path is %s\n", optarg);
                    void *lib = dlopen(optarg, RTLD_NOW);
                    printf("lib is %p\n", lib);
                    my_malloc = dlsym(lib, "malloc");
                    my_free = dlsym(lib, "free");
                    my_pthread_create = dlsym(lib, "pthread_create");
                } break;
            default:
                printf("unsupported flag\n");
                exit(0);
        }
    }
    printf("random seed =\t%d\n", randomSeed);
    printf("pair number =\t%d\n", pairNum);
    printf("array size =\t%d\n", arraySize);
    printf("thread number =\t%d\n", threadNum);
    pthread_spin_init(&allocNumLock, PTHREAD_PROCESS_PRIVATE);
    srand(randomSeed);
    // pthread_t threadPool[threadNum];
    void ***arrayPool = my_malloc(sizeof(void**)*threadNum);
    for(int i = 0; i < threadNum; i++){
        arrayPool[i] = my_malloc(sizeof(void*)*arraySize);
        for(int j = 0; j < arraySize; j++){
            int size = minSize + (rand()%(maxSize-minSize));
            arrayPool[i][j] = my_malloc(size);
        }
    }
    for(int i = 0; i < threadNum; i++){
        pthread_t thread;
        my_pthread_create(&thread, NULL, run_thread, arrayPool[i]);
        pthread_detach(thread);
    }
    sleep(sleepTime);
    term = 1;
    sleep(5);
    // for(int i = 0; i < threadNum; i++){
    //     pthread_join(threadPool[i], NULL);
    // }
    printf("allocNum =\t%ld\n", allocNum);
}
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/time.h>

#include <dlfcn.h>

#define _GNU_SOURCE
#include <getopt.h>

int arraySize = 20000;
int turnNum = 10;
int threadNum = 1;
int writeNum = 20000;
bool passive = false;

enum args{
    ARG_THREAD, ARG_ARRAY, ARG_PASSIVE, ARG_TURN
};

struct option longopts[] = {
    {"thread",  required_argument,  NULL, ARG_THREAD},
    {"array",   required_argument,  NULL, ARG_ARRAY},
    {"passive", no_argument,        NULL, ARG_PASSIVE},
    {"turn",    required_argument,  NULL, ARG_TURN},
    {NULL,      0,                  NULL, 0     }
};

void parse_arg(int argc, char** argv){
    int arg_opt;
    while((arg_opt = getopt_long_only(argc, argv, "h", longopts, NULL))!=-1){
        switch (arg_opt){
            case ARG_THREAD:
                threadNum = atoi(optarg); break;
            case ARG_ARRAY:
                arraySize = atoi(optarg); break;
            case ARG_PASSIVE:
                passive = true; break;
            case ARG_TURN:
                turnNum = atoi(optarg); break;
            default:
                printf("unsupported flag\n");
                exit(0);
        }
    }
}

void *run_thread(void *arg){
    int **array = arg;
    char **pointerArr = arg;
    if(passive){
        for(int i = 0; i < arraySize; i++){
            free(array[i]);
        }
    }
    for(int i = 0; i < arraySize; i++){
        array[i] = malloc(8);
    }
    for(int write = 0; write < writeNum; write++){
        for(int i = 0; i < arraySize; i++){
            memset(array[i], 0xc, 8);
        }
    }
    for(int i = 0; i < arraySize; i++){
        free(array[i]);
    }
    return NULL;
}

int main(int argc, char** argv){
    parse_arg(argc, argv);
    printf("array size =\t%d\n", arraySize);
    printf("thread number =\t%d\n", threadNum);
    printf("turn number =\t%d\n", turnNum);
    char *modeString = passive? "passive" : "active";
    printf("test mode =\t%s\n", modeString);
	struct timeval start;
	gettimeofday(&start, NULL);
    void ***arrayPool = malloc(sizeof(int**)*threadNum);
    for(int turn = 0; turn < turnNum; turn++){
        for(int i = 0; i < threadNum; i++){
            arrayPool[i] = malloc(sizeof(int*)*arraySize);
            if(passive){
                for(int j = 0; j < arraySize; j++){
                    arrayPool[i][j] = malloc(8);
                }
            }
        }
        pthread_t threadPool[threadNum];
        for(int i = 0; i < threadNum; i++){
            pthread_create(&(threadPool[i]), NULL, run_thread, arrayPool[i]);
        }
        for(int i = 0; i < threadNum; i++){
            pthread_join(threadPool[i], NULL);
            free(arrayPool[i]);
        }
    }
    struct timeval now;
    gettimeofday(&now, NULL);
	double time = (now.tv_sec - start.tv_sec) + ((double)now.tv_usec-start.tv_usec)/1000000;
    printf("time consumption =\t%.3f\n", time);
}
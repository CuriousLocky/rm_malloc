#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <getopt.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#define MAXLINE 1024
#define STD_TURN 100000

int test_traces_size = 0;
int test_all_flag = 0;
int test_thread = 1;
int test_turn = 1000;
int request_size = 64;

bool verbose = false;

bool recordFlag = false;
char recordFile[128] = "memory_record.csv";
char testFile[128];

bool test_all = true;

bool term = false;

enum args{
    ARG_THREADNUM, ARG_LIB, ARG_TURN, ARG_RECORD, ARG_RECORDFILE, 
    ARG_VERBOSE, ARG_SEED
};

struct option opts[] = {
    {"thread",      required_argument,  NULL,   ARG_THREADNUM},
    {"turn",        required_argument,  NULL,   ARG_TURN},
    {"record",      no_argument,        NULL,   ARG_RECORD},
    {"out",         required_argument,  NULL,   ARG_RECORDFILE},
    {"verbose",     no_argument,        NULL,   ARG_VERBOSE},
    {"size",        required_argument,  NULL,   ARG_SEED},
    {NULL,          0,                  NULL,   0}
};

void parse_arg(int argc, char **argv){
    int arg_opt;
    while((arg_opt = getopt_long_only(argc, argv, "", opts, NULL))!=-1){
        switch (arg_opt){
            case ARG_THREADNUM:{
                test_thread = atoi(optarg); 
            }break;
            case ARG_TURN:{
                test_turn = atoi(optarg);
            }break;
            case ARG_RECORD:{
                recordFlag = true;
            }break;
            case ARG_RECORDFILE:{
                strcpy(recordFile, optarg);
            }break;
            case ARG_VERBOSE:{
                verbose = true;
            }break;
            case ARG_SEED:{
                request_size = atoi(optarg);
            }break;
            default:
                printf("unsupported flag\n");
                exit(0);
        }
    }
    printf("turn =\t%d\n", test_turn);
    printf("size =\t%d\n", request_size);
    printf("thread =\t%d\n", test_thread);
}

void *run_thread(void *arg){
    int thread_id = *(int*)arg;
    if(verbose){
        printf("thread %d created\n", thread_id);
    }
    
    for(int i = 0; i < test_turn; i++){
        int **array = malloc(STD_TURN * sizeof(int*));
        for(int j = 0; j < STD_TURN; j++){
            array[j] = malloc(request_size);
            memset(array[j], 0xc, request_size);   
        }
        for(int j = 0; j < STD_TURN; j++){
            free(array[j]);
        }
        free(array);
    }
    if(verbose){
        printf("thread %d completed\n", thread_id);
    }
}

void *record_mem(void *arg){
    // char *procFilePath = arg;
    FILE *recordFile = fopen(arg, "w");
    struct timeval start;
    gettimeofday(&start, NULL);
    fprintf(recordFile, "timestamp,rss_in_bytes\n");
    while(!term){
        FILE *procFile = fopen("/proc/self/status", "r");
        if(procFile==NULL){pthread_exit(NULL);}
        long memoryUsage = 0;
        char lineBuffer[1024];
        char *targetLine = NULL;
        while(fgets(lineBuffer, 1024, procFile)){
            // printf("%s\n", lineBuffer);
            targetLine = strstr(lineBuffer, "VmRSS");
            if(targetLine){
                sscanf(targetLine, "VmRSS: %ld kB", &memoryUsage);
                break;
            }
        }
        fclose(procFile);
        struct timeval now;
        gettimeofday(&now, NULL);
        double time = (now.tv_sec - start.tv_sec) + ((double)now.tv_usec-start.tv_usec)/1000000;
        fprintf(recordFile, "%.3f,%ld\n", time, memoryUsage*1024);
        usleep(1000*100);
    }
    fclose(recordFile);
}

int main(int argc, char **argv){
    parse_arg(argc, argv);
    pthread_t recorder;
    if(recordFlag){
        printf("memory consumption started\n");
        pthread_create(&recorder, NULL, record_mem, recordFile);
    }
    struct timeval start;
    printf("timing started\n");
    gettimeofday(&start, NULL);
    pthread_t thread_pool[test_thread];

    for(int i = 0; i < test_thread; i++){
        int *thread_id = malloc(sizeof(int));
        *thread_id = i;
        pthread_create(&(thread_pool[i]), NULL, run_thread, thread_id);
    }
    for(int i = 0; i < test_thread; i++){
        pthread_join(thread_pool[i], NULL);
    }
    struct timeval end;
    gettimeofday(&end, NULL);
    double time = (end.tv_sec - start.tv_sec) + ((double)end.tv_usec-start.tv_usec)/1000000;
    printf("running time =\t%.3f\n", time);
    if(recordFlag){
        term = true;
        pthread_join(recorder, NULL);
    }
    
}
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <getopt.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#define MAXLINE 1024

char *trace_folder = ".././sing_traces";

int test_traces_size = 0;
int test_all_flag = 0;
int test_thread = 1;
int test_turn = 1;

bool verbose = false;

bool recordFlag = false;
char recordFile[128] = "memory_record.csv";
char testFile[128];

bool test_all = true;

bool timeFlag = false;

bool term = false;

void *(*my_malloc)(size_t) = malloc;
void (*my_free)(void*) = free;
int (*my_pthread_create)(pthread_t*, const pthread_attr_t*, 
    void *(*)(void *), void*) = pthread_create;

enum args{
    ARG_THREADNUM, ARG_LIB, ARG_TRACE, ARG_TURN, ARG_RECORD, 
    ARG_RECORDFILE, ARG_VERBOSE, ARG_TIME
};

struct option opts[] = {
    {"thread",      required_argument,  NULL,   ARG_THREADNUM},
    {"lib",         required_argument,  NULL,   ARG_LIB},
    {"trace",       required_argument,  NULL,   ARG_TRACE},
    {"turn",        required_argument,  NULL,   ARG_TURN},
    {"record",      no_argument,        NULL,   ARG_RECORD},
    {"out",         required_argument,  NULL,   ARG_RECORDFILE},
    {"verbose",     no_argument,        NULL,   ARG_VERBOSE},
    {"time",        no_argument,        NULL,   ARG_TIME},
    {NULL,          0,                  NULL,   0}
};

void parse_arg(int argc, char **argv){
    int arg_opt;
    while((arg_opt = getopt_long_only(argc, argv, "", opts, NULL))!=-1){
        switch (arg_opt){
            case ARG_THREADNUM:{
                test_thread = atoi(optarg); 
            }break;
            case ARG_LIB:{
                printf("path is %s\n", optarg);
                void *lib = dlopen(optarg, RTLD_NOW);
                if(lib == NULL){
                    printf("failed to load library, %s\n", dlerror());
                    exit(0);
                }else{
                    printf("successfully loaded library, address = %p\n", lib);
                }
                my_malloc = dlsym(lib, "malloc");
                my_free = dlsym(lib, "free");
                my_pthread_create = dlsym(lib, "pthread_create");
                if(my_pthread_create == NULL){
                    // allow unmodified pthread_create
                    my_pthread_create = pthread_create;
                }
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
            case ARG_TRACE:{
                test_all = false;
                strcpy(testFile, optarg);
            }break;
            case ARG_TIME:{
                timeFlag = true;
            }break;
            default:
                printf("unsupported flag\n");
                exit(0);
        }
    }
}

void test_single(char* file_name, int thread_id){
    printf("thread %d, testing %s\n", thread_id, file_name);
    FILE* trace = fopen(file_name, "r");
    if(trace == NULL){
        printf("can't open %s\n", file_name);
        exit(0);
    }
    char line_buffer[MAXLINE];
    if(fgets(line_buffer, MAXLINE, trace)==NULL){
        printf("cannot read trace file\n");
        exit(0);
    }
    int pointer_pool_size = atoi(line_buffer);
    if(pointer_pool_size <= 0){
        printf("Cannot get pool size\n");
        exit(0);
    }
    char* pointer_pool[pointer_pool_size];
    for(int i = 0; i < pointer_pool_size; i++)
        pointer_pool[i] = NULL;
    char* pointer_end[pointer_pool_size];
    int line_number = 1;
    while(fgets(line_buffer, MAXLINE, trace)!=NULL){
        line_number ++;
        if(verbose){
            printf("thread %d, running line %d\n", thread_id, line_number);
        }
        char op = 'b';
        int pos = -1;
        size_t request_size = -1;
        sscanf(line_buffer, "%c %d %ld", &op, &pos, &request_size);
        if(pos<0 || (request_size<0&&op!='f')){
            printf("Broken trace file\n");
            exit(0);
        }
        if(op == 'a'){
            char* pointer = my_malloc(request_size);
            if((u_int64_t)pointer%16 != 0){
                printf("not aligned to 16\n");
                raise(SIGABRT);
                exit(0);
            }
            if(pointer==NULL && request_size!=0){
                printf("returned NULL");
                raise(SIGABRT);
                exit(0);
            }
            char *pointer_end_tmp = pointer + request_size;
            for(int i = 0; i < pointer_pool_size; i++){
                if(pointer_pool[i]!=NULL && 
                    ((pointer>=pointer_pool[i] && pointer<pointer_end[i])||
                        (pointer_end_tmp>=pointer_pool[i] && pointer_end_tmp<pointer_end[i]) ||
                        (pointer_pool[i]>=pointer && pointer_pool[i]<pointer_end_tmp) ||
                        (pointer_end[i]>=pointer && pointer_end[i]<pointer_end_tmp))){
                    printf("result overlapped, result is %p, overlapped with pointer %d\n", pointer, i);
                    raise(SIGABRT);
                    exit(0);
                }
            }
            pointer_pool[pos] = pointer;
            pointer_end[pos] = pointer + request_size;
            memset(pointer_pool[pos], 0xc, request_size);
        }else if(op == 'f'){
            my_free(pointer_pool[pos]);
            pointer_pool[pos] = NULL;
        }else{
            printf("Broken trace file\n");
            exit(0);
        }
        if(verbose){
            printf("thread %d, line %d completed\n", thread_id, line_number);
        }
    }
    printf("thread %d, test %s completed\n", thread_id, file_name);
    fclose(trace);    
}

void *run_thread(void *arg){
    int thread_id = *(int*)arg;
    printf("thread %d created\n", thread_id);
    free(arg);
    for(int i = 0; i < test_turn; i++){
        if(test_all){
            chdir(trace_folder);
            DIR *d = opendir(trace_folder);
            struct dirent *dir;
            if(d){
                while((dir = readdir(d)) != NULL){
                    char* temp = strstr(dir->d_name, ".rep");
                    if(temp!=NULL && strlen(temp)==4){
                        test_single(dir->d_name, thread_id);
                    }
                }
                closedir(d);
            }
        }else{
            test_single(testFile, thread_id);
        }
    }
    printf("thread %d test completed\n", thread_id);
    return NULL;
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

char *remove_bin(char *bin_path){
    int len = strlen(bin_path);
    for(int i = len-1; i >= 0; i--){
        if(bin_path[i] == '/'){
            bin_path[i] = '\0';
            break;
        }
    }
    printf("processed path is %s\n", bin_path);
    return bin_path;
}

int main(int argc, char **argv){
    parse_arg(argc, argv);
    chdir(remove_bin(realpath(argv[0], NULL)));
    pthread_t recorder;
    if(recordFlag){
        pthread_create(&recorder, NULL, record_mem, recordFile);
    }
    struct timeval start;
    if(timeFlag){
        gettimeofday(&start, NULL);
    }
    pthread_t thread_pool[test_thread];

    for(int i = 0; i < test_thread; i++){
        int *thread_id = malloc(sizeof(int));
        *thread_id = i;
        my_pthread_create(&(thread_pool[i]), NULL, run_thread, thread_id);
    }
    for(int i = 0; i < test_thread; i++){
        pthread_join(thread_pool[i], NULL);
    }
    if(timeFlag){
        struct timeval end;
        gettimeofday(&end, NULL);
        double time = (end.tv_sec - start.tv_sec) + ((double)end.tv_usec-start.tv_usec)/1000000;
        printf("running time =\t%.3f\n", time);
    }
    if(recordFlag){
        term = true;
        pthread_join(recorder, NULL);
    }
    
}
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#define PSEUDOSEED 92320736

int minSize = 10;
int maxSize = 1000;
int pairNum = 50000;
int arraySize = 1000;
int term = 0;
uint64_t allocNum = 0;
// pthread_mutex_t allocNumLock = PTHREAD_MUTEX_INITIALIZER;
pthread_spinlock_t allocNumLock;

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
        free(pointerArr[operationPos]);
        randomNum = gen_pseudo_rand(randomNum);
        int allocSize = minSize + (randomNum%(maxSize-minSize));
        pointerArr[operationPos] = malloc(allocSize);
    }
    pthread_spin_lock(&allocNumLock);
    allocNum += allocNum_thread;
    pthread_spin_unlock(&allocNumLock);
    if(!term){
        pthread_t newThread;
        pthread_create(&newThread, NULL, run_thread, pointerArr);
        pthread_detach(newThread);
    }
    return NULL;
}

int main(int argc, char** argv){
    int threadNum = 1;
    int randomSeed = (435300 + 1251900); // a random number chosen
    int sleepTime = 30;
    for(int i = 1; i < argc; i++){
        if(argv[i][0]=='-'){
			char* argPos = strstr(argv[i], "=");
			if(argPos==NULL){
				printf("invalid flag\n");
				exit(-1);
			}
			argPos++;
			if(strstr(argv[i], "min")){
                minSize = atoi(argPos);
                if(minSize < 0){
                    printf("invalid min size\n");
                    exit(0);
                }
			}else if(strstr(argv[i], "max")){
                maxSize = atoi(argPos);
                if(maxSize < minSize){
                    printf("invalid max size\n");
                    exit(0);
                }
            }else if(strstr(argv[i], "thread")){
                threadNum = atoi(argPos);
                if(threadNum < 1){
                    printf("invalid thread number\n");
                    exit(0);
                }
            }else if(strstr(argv[i], "random")){
                randomSeed = atoi(argPos);
                if(randomSeed < 1){
                    printf("using time as random seed\n");
                    randomSeed = time(NULL);
                }
            }else if(strstr(argv[i], "array")){
                arraySize = atoi(argPos);
                if(arraySize < 1){
                    printf("invalid array size\n");
                    exit(0);
                }
            }else if(strstr(argv[i], "pair")){
                pairNum = atoi(argPos);
                if(pairNum < 1){
                    printf("invalid malloc-free pair number\n");
                    exit(0);
                }
            }else if(strstr(argv[i], "sleep")){
                sleepTime = atoi(argPos);
                if(sleepTime < 1){
                    printf("invalid sleep time\n");
                    exit(0);
                }
            }else{
				printf("unsupported flag\n");
				exit(-1);
			}
		}
    }
    printf("random seed =\t%d\n", randomSeed);
    printf("pair number =\t%d\n", pairNum);
    printf("array size =\t%d\n", arraySize);
    printf("thread number =\t%d\n", threadNum);
    pthread_spin_init(&allocNumLock, PTHREAD_PROCESS_PRIVATE);
    srand(randomSeed);
    // pthread_t threadPool[threadNum];
    void ***arrayPool = malloc(sizeof(void**)*threadNum);
    for(int i = 0; i < threadNum; i++){
        arrayPool[i] = malloc(sizeof(void*)*arraySize);
        for(int j = 0; j < arraySize; j++){
            int size = minSize + (rand()%(maxSize-minSize));
            arrayPool[i][j] = malloc(size);
        }
    }
    for(int i = 0; i < threadNum; i++){
        pthread_t thread;
        pthread_create(&thread, NULL, run_thread, arrayPool[i]);
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
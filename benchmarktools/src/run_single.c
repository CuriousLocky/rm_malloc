// #include "dmem_malloc.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>

#define MAXLINE 256
#define STEP 1

#if defined(WIN32) || defined(_WIN32) 
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR_S "\\"
#else 
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR_S "/"
#endif 

char** test_traces;
char* working_dir;
int test_traces_size = 0;
int test_all_flag = 0;
int test_thread = 1;
int test_turn = 1;
int test_set = 1;
int argc_s = 0;
int recordFlag = 0;
// FILE *recordFile = NULL;
// FILE *procFile = NULL;

void test_single(char* file_name, int thread_id){
	printf("thread %d, testing %s\n", thread_id, file_name);
	FILE* trace = fopen(file_name, "r");
	if(trace == NULL){
		// perror("Cannot open trace file");
		printf("file name is %s\n", file_name);
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
		// printf("thread %d, line %d\n", thread_id, line_number);
		char op = 'b';
		int pos = -1;
		size_t request_size = -1;

		sscanf(line_buffer, "%c %d %ld", &op, &pos, &request_size);
		if(pos<0 || (request_size<0&&op!='f')){
			printf("Broken trace file\n");
			exit(0);
		}
		if(op == 'a'){
			char* pointer = malloc(request_size*STEP);
			if((u_int64_t)pointer%16 != 0){
				printf("not aligned to 16\n");
				exit(0);
			}
			if(pointer==NULL && request_size!=0){
				printf("returned NULL, line %d\n", line_number);
				exit(0);
			}
			char *pointer_end_tmp = pointer + request_size*STEP;
			for(int i = 0; i < pointer_pool_size; i++){
				if(pointer_pool[i]!=NULL && 
					((pointer>=pointer_pool[i] && pointer<pointer_end[i])||
						pointer_end_tmp>=pointer_pool[i] && pointer_end_tmp<pointer_end[i])){
					printf("result overlapped\n");
					printf("i = %d\n", i);
					exit(0);
				}
			}
			pointer_pool[pos] = pointer;
			pointer_end[pos] = pointer + request_size*STEP;
			memset(pointer_pool[pos], 0xc, request_size*STEP);
		}else if(op == 'f'){
			free(pointer_pool[pos]);
			pointer_pool[pos] = NULL;
		}else{
			printf("Broken trace file\n");
			exit(0);
		}
		printf("thread %d, line %d completed\n", thread_id, line_number);
	}
	// printf("thread %d, test %s completed\n", thread_id, file_name);
	fclose(trace);	
}

void* run_thread(void* arg){
	int thread_id = *((int*)arg);
	printf("thread id = %ld\n", pthread_self());
	free(arg);
	
	for(int set_num = 0; set_num < test_set; set_num++){
		char trace_dir[128];
		strcpy(trace_dir, working_dir);
		strcat(trace_dir, "sing_traces");
		strcat(trace_dir, PATH_SEPARATOR_S);		
		if(test_all_flag){
			DIR *d;
			struct dirent *dir;
			d = opendir(trace_dir);
			if(d){
				while((dir = readdir(d)) != NULL){
					char* temp = strstr(dir->d_name, ".rep");
					if(temp!=NULL && strlen(temp)==4){
						char name_buffer[128];
						strcpy(name_buffer, trace_dir);
						strcat(name_buffer, dir->d_name);
						test_single(name_buffer, thread_id);
					}
				}
				closedir(d);
			}
			printf("All test completed\n");
		}else{
			for(int i = 0; i < test_traces_size; i++){
				char name_buffer[128];
				strcpy(name_buffer, trace_dir);
				strcat(trace_dir, test_traces[i]);
				test_single(trace_dir, thread_id);
			}			
		}
	}

	return NULL;
}

void *record_mem(void *arg){
	// char *procFilePath = arg;
	FILE *recordFile = arg;
	struct timeval start;
	gettimeofday(&start, NULL);
	fprintf(recordFile, "timestamp,rss_in_bytes\n");
	while(recordFlag){
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
}

int gen_working_dir(char* bin_path){
	int i = 0;
	char* last_divider_loc = bin_path;
	while(bin_path[i] != '\0'){
		if(bin_path[i] == PATH_SEPARATOR){
			last_divider_loc = bin_path + i;
		}
		i++;
		if(i > MAXLINE){
			return 0;
		}
	}
	*(last_divider_loc+1) = '\0';
	return 1;
}

void* run_main(void* arg){
	char** argv = arg;
	if(argc_s <= 1){
		write(1, "testing malloc\n", sizeof("testing malloc\n")-1);
		malloc(sizeof(int));
		return 0;
	}
	char* working_dir_temp = malloc(MAXLINE);
	// strcpy(working_dir_temp, argv[0]);
	realpath(argv[0], working_dir_temp);
	gen_working_dir(working_dir_temp);
	working_dir = working_dir_temp;
	int start = 1;
	recordFlag = 0;
	// chdir("traces/");
	setbuf(stdout, NULL);
	test_traces = malloc(sizeof(char*)*argc_s);
	for(int i = 1; i < argc_s; i++){
		if(argv[i][0]=='-'){
			char* numpos = strstr(argv[i], "=");
			if(numpos==NULL){
				printf("invalid flag\n");
				exit(-1);
			}
			numpos++;
			if(strstr(argv[i], "thread")){
				int test_thread_temp = atoi(numpos);
				if(test_thread_temp<=0){
					printf("invalid thread num\n");
					exit(-1);
				}
				test_thread = test_thread_temp;
			}else if(strstr(argv[i], "set")){
				int test_set_temp = atoi(numpos);
				if(test_set_temp<=0){
					printf("invalid set num\n");
					exit(-1);
				}
				test_set = test_set_temp;
			}else if(strstr(argv[i], "turn")){
				int test_turn_temp = atoi(numpos);
				if(test_turn_temp<=0){
					printf("invalid turn num\n");
					exit(-1);
				}
				test_turn = test_turn_temp;
			}else if(strstr(argv[i], "start")){
				start = atoi(numpos);
			}else if(strstr(argv[i], "record")){
				recordFlag = atoi(numpos);
			}else{
				printf("unsupported flag\n");
				exit(-1);
			}
		}else if(strcmp(argv[i], "all")==0){
			test_all_flag = 1;
		}else{
			test_traces[test_traces_size++] = argv[i];
		}
	}
	if(test_traces_size<=0 && test_all_flag==0){
		printf("usage: %s (-thread=?) {all/trace file name}\n", argv[0]);
		return 0;
	}
	if(!start){
		printf("input Y to start:\n");
		char inputChar = 'N';
		do{
			inputChar = getchar();
		}while(inputChar!='Y');
	}
	pthread_t recordThread;
	FILE *recordFile;
	if(recordFlag){
		printf("recording memory usage\n");
		recordFile = fopen("memory_usage.csv", "w");
		pthread_create(&recordThread, NULL, record_mem, recordFile);
	}
	pthread_t thread_pool[test_thread];
	printf("test_thread = %d\n", test_thread);
	for(int i = 0; i < test_traces_size; i++){
		printf("test_trace = %s\n", test_traces[i]);
	}
	for(int turn = 0; turn < test_turn; turn++){
		for(int i = 0; i < test_thread; i++){
			int* temp_thread_id = malloc(sizeof(int));
			*temp_thread_id = i;
			pthread_create(&thread_pool[i], NULL, run_thread, temp_thread_id);
		}
		for(int i = 0; i < test_thread; i++){
			pthread_join(thread_pool[i], NULL);
		}		
	}
	if(recordFlag){
		recordFlag = 0;
		pthread_join(recordThread, NULL);
		fclose(recordFile);
	}
}

int main(int argc, char** argv){
	// pthread_t sub_main;
	argc_s = argc;
	// char *test_char_p = malloc(sizeof(char)*16);
	// *test_char_p = argv[0][0];
	// pthread_create(&sub_main, NULL, run_main, argv);
	// pthread_detach(sub_main);
	// free(test_char_p);
	// printf("detached!\n");
	// pthread_exit(NULL);
	run_main(argv);
}
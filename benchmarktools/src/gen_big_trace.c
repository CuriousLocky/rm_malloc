#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define UNINI 'u'
#define ALLOC 'a'
#define DEALC 'd'

#define RANDOM 'r'
#define MAXMEMUSAGE (1024UL*1024*128)

void gen_random(FILE *outputFile, int traceLength, unsigned long max_usage){
    int randomSize = traceLength;
    char memSlot[traceLength];
    for(int i = 0; i < traceLength; i++){memSlot[i]=UNINI;}
    srand(time(NULL));
    unsigned long averageAllocSize = max_usage/traceLength;
    while(randomSize){
        int targetSlot = rand()%randomSize;
        int slotWalker = 0;
        for(;slotWalker<traceLength;slotWalker++){
            if(memSlot[slotWalker]==DEALC){continue;}
            if(targetSlot==0){break;}
            targetSlot--;
        }
        switch(memSlot[slotWalker]){
            case UNINI:{
                // unsigned long allocSize = (MAXMEMUSAGE-memUsage)*2/randomSize;
                fprintf(outputFile, "a %d %ld\n", slotWalker, rand()%averageAllocSize);
                // memUsage -= allocSize;
                memSlot[slotWalker] = ALLOC;
                break;
            }
            case ALLOC:{
                fprintf(outputFile, "f %d\n", slotWalker);
                memSlot[slotWalker] = DEALC;
                randomSize--;
                break;
            }
            default:{
                perror("internal error!");
                exit(-1);
            }
        }
    }
}

int main(int argc, char** argv){
    if(argc!=4 || !strcmp(argv[1], "-h")){
        printf("usage: %s (-mode={mode}) (-length={length}) (-memory={memory})\n", argv[0]);
        exit(0);
    }
    unsigned long max_usage = 1024*1024*2;
    char *fileName = "trace.rep";
    char workMod = RANDOM;
    int traceLength = 1024;
    for(int i = 1; i < argc; i++){
        if(argv[i][0]=='-'){
			char* argPos = strstr(argv[i], "=");
			if(argPos==NULL){
				printf("invalid flag\n");
				exit(-1);
			}
			argPos++;
			if(strstr(argv[i], "mode")){
                if(!strcmp(argPos, "random")){
                    workMod = RANDOM;
                }else{
                    printf("unsupported working mode\n");
                    exit(-1);
                }
			}else if(strstr(argv[i], "length")){
                traceLength = atoi(argPos);
                if(traceLength < 1){
                    printf("invalid trace length\n");
                    exit(-1);
                }
            }else if(strstr(argv[i], "memory")){
                max_usage = 1024*1024*atoi(argPos);
                if(max_usage < 1){
                    printf("invalid trace length\n");
                    exit(-1);
                }
            }else{
				printf("unsupported flag\n");
				exit(-1);
			}
		}else if(strstr(argv[i], ".rep")){
			fileName = argv[i];
		}
    }
    FILE *outputFile = fopen(fileName, "w");
    fprintf(outputFile, "%d\n", traceLength);
    switch(workMod){
        case RANDOM:
            gen_random(outputFile, traceLength, max_usage);
            break;
        default:
            break;
    }
    gen_random(outputFile, traceLength, max_usage);
    fclose(outputFile);
}
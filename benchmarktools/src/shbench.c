#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define UNINI 'u'
#define ALLOC 'a'
#define DEALC 'd'

int main(int argc, char **argv){
    if(argc < 4){
        printf("usage: %s {min size of block} {max size of block} {number of pairs}\n", argv[0]);
        exit(0);
    }
    int minSize = atoi(argv[1]);
    if(minSize < 1){
        printf("invalid min block size\n");
        exit(0);
    }
    int maxSize = atoi(argv[2]);
    if(maxSize < minSize){
        printf("invalid max block size\n");
        exit(0);
    }
    int slotNum = atoi(argv[2]);
    if(slotNum < 1){
        printf("invalid pair number\n");
        exit(0);
    }
    FILE *outputFile = fopen("shbench.rep", "w");
    fprintf(outputFile, "%d\n", slotNum);
    int randomSize = slotNum;
    char memSlot[slotNum];
    for(int i = 0; i < slotNum; i++){memSlot[i]=UNINI;}
    srand(time(NULL));
    while(randomSize){
        int targetSlot = rand()%randomSize;
        int slotWalker = 0;
        for(;slotWalker<slotNum;slotWalker++){
            if(memSlot[slotWalker]==DEALC){continue;}
            if(targetSlot==0){break;}
            targetSlot--;
        }
        switch(memSlot[slotWalker]){
            case UNINI:{
                // unsigned long allocSize = (MAXMEMUSAGE-memUsage)*2/randomSize;
                int step = rand()%(maxSize-minSize);
                fprintf(outputFile, "a %d %d\n", slotWalker, step+minSize);
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
    fclose(outputFile);
}
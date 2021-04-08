#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv){
    if(argc < 3){
        printf("usage: %s {size of block} {number of pairs}\n", argv[0]);
        exit(0);
    }
    int blockSize = atoi(argv[1]);
    if(blockSize < 1){
        printf("invalid block size\n");
        exit(0);
    }
    int pairNum = atoi(argv[2]);
    if(pairNum < 1){
        printf("invalid pair number\n");
        exit(0);
    }
    FILE *outputFile = fopen("threadTest.rep", "w");
    fprintf(outputFile, "%d\n", pairNum);
    for(int i = 0; i < pairNum; i++){
        fprintf(outputFile, "a %d %d\n", i, blockSize);
    }
    for(int i = 0; i < pairNum; i++){
        fprintf(outputFile, "f %d\n", i);
    }
    fclose(outputFile);
}
#include "mempool.h"

/*Edit these functions for different memory system backend*/

#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

void *payload_chunk_req(size_t size){
    size = align(size, PAYLOAD_CHUNK_SIZE);
    void *result = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(result == MAP_FAILED){
        perror("chunk request failed: mmap failed\n");
        exit(EXIT_FAILURE);
    }
    return result;
}

void payload_chunk_rel(void *ptr, size_t size){
    size = align(size, PAYLOAD_CHUNK_SIZE);
    int result = munmap(ptr, size);
    if(result != 0){
        perror("chunk release failed: munmap failed\n");
        exit(EXIT_FAILURE);
    }
}

void *meta_chunk_req(size_t size){
    void *result = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(result == MAP_FAILED){
        perror("meta request failed: mmap failed\n");
        exit(EXIT_FAILURE);
    }
    return result;
}

void *meta_chunk_rel(void *ptr, size_t size){
    int result = munmap(ptr, size);
    if(result !=0){
        perror("meta release failed: munmap failed\n");
        exit(EXIT_FAILURE);
    }
}
#include <stdio.h>
#include <stdlib.h>

size_t align(size_t size, size_t alignment){
    return (((size)+(alignment-1)) & (~(alignment-1)));
}

int main(){
    printf("%ld\n", align(16, 16));
}
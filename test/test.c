#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include <threads.h>

#include "rm_malloc.h"

int main(){
    rm_free(rm_malloc(sizeof(24)));
    return 0;
}

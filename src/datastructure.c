#include "datastructure.h"
#include "datastructure_bitmap.h"

void add_block(uint64_t *block, size_t size){
    return add_bitmap_block(block, size);
}

uint64_t *find_victim(size_t size){
    return find_bitmap_victim(size);
}
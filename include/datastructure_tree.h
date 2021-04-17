#ifndef DATASTRUCTURE_TREE_H
#define DATASTRUCTURE_TREE_H

#include <inttypes.h>
#include <stddef.h>

/*find a victim block that can hold size bytes, return NULL if no block
in the tree satisfies*/
uint64_t *find_tree_victim(size_t size);

/*insert a block into tree*/
void add_bitmap_block(uint64_t *block, size_t size);

#endif
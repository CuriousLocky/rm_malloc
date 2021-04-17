#ifndef DATASTRUCTURE_H
#define DATASTRUCTURE_H

#include <inttypes.h>
#include <stddef.h>

void add_block(uint64_t *block, size_t size);

uint64_t *find_victim(size_t size);

#endif
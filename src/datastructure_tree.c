
#include "datastructure_tree.h"
#include "rm_threads.h"

rm_lock_t tree_lock = RM_UNLOCKED;

uint64_t *root;

uint64_t *find_tree_victim(size_t size){
    return NULL;
}

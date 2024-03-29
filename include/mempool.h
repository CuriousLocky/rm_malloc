#ifndef __RM_MALLOC_MEMPOOL_H
#define __RM_MALLOC_MEMPOOL_H

#include <stddef.h>

/*The size of each memory chunk in bytes. Payload will be stored here and will 
be released when possible. Required to be 2^n.*/
#define PAYLOAD_CHUNK_SIZE (64*1024*1024)

/*The size of each metadata chunk in bytes. This is used to hold thread_Info and
local tables. Required to be 2^n and greater than a single ThreadInfo (1056)*/
#define META_CHUNK_SIZE (4*1024*1024)

/*Request "size" bytes of payload memory chunk. The returned chunk should 
have a size with multiply of PAYLOAD_CHUNK_SIZE and be aligned with 16*/
void *payload_chunk_req(size_t size);

/*Release a memory chunk*/
void payload_chunk_rel(void *ptr, size_t size);

/*Request a metadata chunk*/
void *meta_chunk_req(size_t size);

/*Release a metadata chunk*/
void *meta_chunk_rel(void *ptr, size_t size);

#endif
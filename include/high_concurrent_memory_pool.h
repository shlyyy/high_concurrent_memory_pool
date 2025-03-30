#ifndef __HIGH_CONCURRENT_MEMORY_POOL_H__
#define __HIGH_CONCURRENT_MEMORY_POOL_H__

#include "central_cache.h"
#include "common.h"
#include "page_cache.h"
#include "thread_cache.h"

void* hc_malloc(size_t size);
void hc_free(void* ptr);

#endif  // __HIGH_CONCURRENT_MEMORY_POOL_H__
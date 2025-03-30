#ifndef __HIGH_CONCURRENT_MEMORY_POOL_THREAD_CACHE_H__
#define __HIGH_CONCURRENT_MEMORY_POOL_THREAD_CACHE_H__
#include "common.h"

class ThreadCache
{
public:
  void *allocate(size_t size);

  void deallocate(void *ptr, size_t size);

  // 从中心缓存获取一定数量的对象到线程缓存
  void *fetch_from_central_cache(size_t index, size_t size);

private:
  FreeList free_list_[N_FREE_LIST];
};

ThreadCache *GetThreadCache();

#endif // __HIGH_CONCURRENT_MEMORY_POOL_THREAD_CACHE_H__
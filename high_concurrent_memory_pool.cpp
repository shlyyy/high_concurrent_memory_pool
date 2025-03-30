#include "high_concurrent_memory_pool.h"

void* hc_malloc(size_t size) {
  if (size > MAX_BYTES) {
    // 大内存申请，走 page cache
    size_t aligned_size = AlignMap::align_upwards(size);  // 按页面对齐
    size_t num_pages = aligned_size >> kPageShift;        // 占用的页数

    // 从 page cache 中获取一定数量的页，需要上锁
    PageCache* page_cache = PageCache::GetInstance();
    page_cache->page_cache_lock_.lock();
    Span* span = page_cache->new_span(num_pages);
    page_cache->page_cache_lock_.unlock();
    void* ptr = reinterpret_cast<void*>(span->page_id_ << kPageShift);
    return ptr;
  } else {
    return GetThreadCache()->allocate(size);
  }
}

void hc_free(void* ptr) {
  // 根据地址获取对应的 span
  PageCache* page_cache = PageCache::GetInstance();
  Span* span = page_cache->get_span_by_address(ptr);
  size_t size = span->obj_size_;

  if (size > MAX_BYTES) {
    // 大内存释放，走 page cache
    page_cache->page_cache_lock_.lock();
    page_cache->release_span_to_page_cache(span);
    page_cache->page_cache_lock_.unlock();
  } else {
    // 小内存释放，走 thread cache
    GetThreadCache()->deallocate(ptr, size);
  }
}
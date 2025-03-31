#ifndef __HIGH_CONCURRENT_MEMORY_POOL_PAGE_CACHE_H__
#define __HIGH_CONCURRENT_MEMORY_POOL_PAGE_CACHE_H__
#include "common.h"
#include "object_pool.h"

class PageCache {
 public:
  // 单例模式
  static PageCache* GetInstance();

  // 从 page cache 中获取一个包含 page_count 个 page 的 span
  Span* new_span(size_t page_count);

  std::mutex page_cache_lock_;
  static PageCache page_cache_instance_;

  // 通过地址获取页号，进而获取 span
  Span* get_span_by_address(void* ptr);

  // 将 central cache 中的 span 归还给 page cache
  void release_span_to_page_cache(Span* span);

 private:
  PageCache() = default;
  PageCache(const PageCache&) = delete;
  PageCache& operator=(const PageCache&) = delete;

 private:
  SpanList span_lists_[N_PAGES_BUCKET];
  std::unordered_map<size_t, Span*> page_id_span_map_;

  // span 对象的申请和释放都是在 page cache 中进行的
  ObjectPool<Span> span_pool_;
};

#endif  // __HIGH_CONCURRENT_MEMORY_POOL_PAGE_CACHE_H__
#ifndef __HIGH_CONCURRENT_MEMORY_POOL_CENTRAL_CACHE_H__
#define __HIGH_CONCURRENT_MEMORY_POOL_CENTRAL_CACHE_H__
#include "common.h"

class CentralCache {
 public:
  // 单例模式
  static CentralCache* GetInstance();

  // 从中心缓存获取一定数量的对象给 thread cache
  size_t fetch_range_objs(void*& start, void*& end, size_t size, size_t n);

  // 从span_list中获取一个span
  Span* get_one_span(SpanList& span_list, size_t size);

  // start 指向的链表归还给 central cache，链表中的对象大小为 size
  // thread cache 中自由链表归还的并不一定连续，可能在多个span中
  void release_list_to_spans(void* start, size_t size);

 private:
  CentralCache() = default;
  CentralCache(const CentralCache&) = delete;
  CentralCache& operator=(const CentralCache&) = delete;

 private:
  SpanList span_lists_[N_FREE_LIST];
  static CentralCache central_cache_instance_;
};

#endif  // __HIGH_CONCURRENT_MEMORY_POOL_CENTRAL_CACHE_H__
#include "thread_cache.h"

#include "central_cache.h"

// 使用C++11 thread_local + unique_ptr实现
thread_local std::unique_ptr<ThreadCache> tls_thread_cache;

// 线程退出时自动清理的保险措施
thread_local struct ThreadCacheCleanup {
  ~ThreadCacheCleanup() {
    if (tls_thread_cache) {
      // 可以在这里将未释放的内存归还给中央缓存
      tls_thread_cache.reset();
    }
  }
} tls_cleanup;

ThreadCache* GetThreadCache() {
  if (!tls_thread_cache) {
    tls_thread_cache = std::make_unique<ThreadCache>();
  }
  return tls_thread_cache.get();
}

void* ThreadCache::allocate(size_t size) {
  size = AlignMap::align_upwards(size);
  size_t index = AlignMap::hash_bucket_index(size);
  if (free_list_[index].empty()) {
    return fetch_from_central_cache(index, size);
  }
  return free_list_[index].pop_front();
}

void ThreadCache::deallocate(void* ptr, size_t size) {
  assert(ptr);
  assert(size <= MAX_BYTES);

  // 释放的内存插入到 thread cache 对应桶的自由链表中
  size_t index = AlignMap::hash_bucket_index(size);
  free_list_[index].push_front(ptr);

  // 如果自由链表中的对象数量超过一定阈值，将多余的对象归还给 central cache
  // 当自由链表长度大于一次批量申请的内存时，就从自由链表中还一段list给 central
  // cache
  FreeList& free_list = free_list_[index];
  if (free_list.size() > free_list.max_size()) {
    void* start = nullptr;
    void* end = nullptr;
    // 释放一定数量的对象: free_list.size() - free_list.max_size();
    // size_t num_objects = free_list.size() - free_list.max_size();
    size_t num_objects = free_list.max_size();
    free_list.pop_range(start, end, num_objects);

    // 将 start 到 end 之间的 size 大小的 num_objects 个对象归还给 central cache
    // 自由链表中的节点间不一定是连续的，所以需要一个个的释放
    // 无需传递 end ，因为end的next指针是nullptr
    // 需传递对齐后的size，告知central cache去哪个桶中找
    CentralCache::GetInstance()->release_list_to_spans(start, size);
  }
}

void* ThreadCache::fetch_from_central_cache(size_t index, size_t size) {
  // 压缩到 [2, 512] 个对象
  size_t num_objects = AlignMap::calculate_num_objects(size);

  // 慢开始
  num_objects = (std::min)(free_list_[index].max_size(), num_objects);
  if (free_list_[index].max_size() == num_objects) {
    // 如果使用 max_size_ ，说明 max_size_ 比较小，可以加速增长
    free_list_[index].max_size() += 1;
  }

  // 从中心缓存获取一定数量的对象
  void* start = nullptr;
  void* end = nullptr;
  size_t actual_num = CentralCache::GetInstance()->fetch_range_objs(
      start, end, size, num_objects);
  assert(actual_num > 0);

  // 如果申请到对象是一个，直接返回
  if (actual_num == 1) {
    assert(start == end);
    return start;
  } else {
    // 如果申请到的对象不止一个，将多余的对象加入到自由链表中
    free_list_[index].push_range(get_next_obj(start), end, actual_num - 1);
    return start;
  }
}
#include "central_cache.h"

#include "page_cache.h"

CentralCache CentralCache::central_cache_instance_;

CentralCache* CentralCache::GetInstance() { return &central_cache_instance_; }

Span* CentralCache::get_one_span(SpanList& span_list, size_t size) {
  // TODO: Implement this function
  // 1. 先在 list 中寻找非空 Span, 如果找到就返回
  Span* span = span_list.begin();
  while (span != span_list.end()) {
    if (span->free_list_ != nullptr) {
      return span;
    }
    span = span->next_;
  }

  // 把 central cache 的桶解锁，这样如果有其他线程释放内存，不会阻塞
  span_list.bucket_lock_.unlock();

  // 2. 如果 list 中没有非空 Span, 就从 page cache 中获取一个 Span
  PageCache::GetInstance()->page_cache_lock_.lock();
  span =
      PageCache::GetInstance()->new_span(AlignMap::calculate_num_pages(size));
  span->is_used_ = true;
  span->obj_size_ = size;
  PageCache::GetInstance()->page_cache_lock_.unlock();

  // 其它线程不会访问到这个 span，所以不需要加锁
  // 最后挂入到 span_list 中需要加锁

  // 3. 把新的 Span 插入到 list 中
  char* start = (char*)(span->page_id_ * SYSTEM_PAGE_SIZE);
  char* end = start + span->n_pages_ * SYSTEM_PAGE_SIZE;

  // 把大块内存切分成小块内存放在自由链表中，尾插法
  span->free_list_ = start;
  start += size;
  void* tail = span->free_list_;
  int i = 1;
  while (start < end) {
    ++i;
    get_next_obj(tail) = start;
    tail = start;
    start += size;
  }

  // 链表结束
  get_next_obj(tail) = nullptr;

  // 切分好 span 以后，把span挂到桶里，需要加锁
  span_list.bucket_lock_.lock();
  span_list.push_front(span);

  return span;
}

// 从中心缓存中申请 n 个大小为 size 的对象
// size 是对齐后的大小:
// thread_cache.allocate() -> thread_cache.fetch_from_central_cache() ->
// CentralCache.fetch_range_objs()
size_t CentralCache::fetch_range_objs(void*& start, void*& end, size_t size,
                                      size_t n) {
  //   size = AlignMap::align_upwards(size);
  size_t index = AlignMap::hash_bucket_index(size);

  SpanList& span_list = span_lists_[index];
  // 上锁
  span_list.bucket_lock_.lock();

  // 获取一个非空的span
  Span* span = get_one_span(span_list, size);
  assert(span);
  assert(span->free_list_);

  // 从 span 中获取 n 个对象，如果不够 n 个，就尽可能多的获取
  start = span->free_list_;
  end = start;

  size_t i = 0;
  size_t actual_num = 1;
  while (i < n - 1 && get_next_obj(end) != nullptr) {
    end = get_next_obj(end);
    ++i;
    ++actual_num;
  }
  span->free_list_ = get_next_obj(end);
  get_next_obj(end) = nullptr;

  // span 的小片内存分配给 thread cache，对应的 use_count_ 增加
  span->use_count_ += actual_num;

  // 解锁
  span_list.bucket_lock_.unlock();

  return actual_num;
}

// 把一段内存归还给 central cache
void CentralCache::release_list_to_spans(void* start, size_t size) {
  size_t index = AlignMap::hash_bucket_index(size);
  SpanList& span_list = span_lists_[index];

  span_list.bucket_lock_.lock();

  void* current = start;
  while (current) {
    void* next = get_next_obj(current);
    Span* span = PageCache::GetInstance()->get_span_by_address(current);

    // 把内存放回 span 的 free_list 中
    get_next_obj(current) = span->free_list_;
    span->free_list_ = current;
    --span->use_count_;

    // 如果 use_count_ 为 0，就把 span 归还给 page cache
    // page cache 会尝试做前后页的合并
    if (span->use_count_ == 0) {
      // 从 central cache 中移除 span
      span_list.erase(span);

      span->free_list_ = nullptr;
      span->next_ = nullptr;
      span->prev_ = nullptr;

      span_list.bucket_lock_.unlock();

      // 把 span 归还给 page cache
      PageCache* page_cache = PageCache::GetInstance();
      page_cache->page_cache_lock_.lock();
      page_cache->release_span_to_page_cache(span);
      page_cache->page_cache_lock_.unlock();

      span_list.bucket_lock_.lock();
    }

    current = next;
  }

  span_list.bucket_lock_.unlock();
}
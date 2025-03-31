#include "page_cache.h"

PageCache PageCache::page_cache_instance_;

PageCache* PageCache::GetInstance() { return &page_cache_instance_; }

Span* PageCache::new_span(size_t page_count) {
  assert(page_count > 0);

  // 大于 128 个页的 span 直接从系统中获取
  if (page_count > N_PAGES_BUCKET - 1) {
    // Span* span = new Span;
    Span* span = span_pool_.New();
    void* ptr = system_alloc(page_count);
    span->page_id_ = reinterpret_cast<size_t>(ptr) >> kPageShift;
    span->n_pages_ = page_count;

    // 记录 page_id_ 和 span 的映射关系
    page_id_span_map_[span->page_id_] = span;
    page_id_span_map_[span->page_id_ + span->n_pages_ - 1] = span;
    return span;
  }

  // 检查 page_count 对应的桶里有没有 span，如果有就直接返回
  if (!span_lists_[page_count].empty()) {
    Span* span = span_lists_[page_count].pop_front();

    // 建立页号和 span 的映射关系
    for (size_t i = 0; i < span->n_pages_; ++i) {
      page_id_span_map_[span->page_id_ + i] = span;
    }
    return span;
  }

  // 没有找到，就从后面的桶里查找并进行拆分
  for (size_t i = page_count + 1; i < N_PAGES_BUCKET; ++i) {
    if (!span_lists_[i].empty()) {
      // 大块 span
      Span* span = span_lists_[i].pop_front();
      // 切分下来的小块 span 作为返回值，无需插入到 span_lists_ 中
      // Span* split = new Span;
      Span* split = span_pool_.New();
      split->page_id_ = span->page_id_;
      split->n_pages_ = page_count;

      // 切分之后的剩余页面应该放到新的桶中，插入到 span_lists_ 中
      span->page_id_ += page_count;
      span->n_pages_ -= page_count;
      span_lists_[span->n_pages_].push_front(span);

      // page cache 中的映射关系，方便 page cache 进行回收
      page_id_span_map_[span->page_id_] = span;
      page_id_span_map_[span->page_id_ + span->n_pages_ - 1] = span;

      // 返回切分下来的小块 span，方便 central cache
      // 回收小块内存时查找对应的span
      for (size_t i = 0; i < page_count; ++i) {
        page_id_span_map_[split->page_id_ + i] = split;
      }

      return split;
    }
  }

  // 说明已经没有足够大的 span 可以切分了，只能从系统中获取
  // 从系统中申请 128 页的 span
  // Span* system_allocated_span = new Span;
  Span* system_allocated_span = span_pool_.New();
  void* ptr = system_alloc(N_PAGES_BUCKET - 1);  // 128 个页
  system_allocated_span->page_id_ =
      reinterpret_cast<size_t>(ptr) / SYSTEM_PAGE_SIZE;
  system_allocated_span->n_pages_ = N_PAGES_BUCKET - 1;

  // 新页插入到 span_lists_ 中
  span_lists_[system_allocated_span->n_pages_].push_front(
      system_allocated_span);

  // 递归给第二次 span 拆分调用
  return new_span(page_count);
}

Span* PageCache::get_span_by_address(void* ptr) {
  size_t page_id = reinterpret_cast<size_t>(ptr) / SYSTEM_PAGE_SIZE;
  auto it = page_id_span_map_.find(page_id);
  if (it == page_id_span_map_.end()) {
    assert(false);
    return nullptr;
  }
  return it->second;
}

void PageCache::release_span_to_page_cache(Span* span) {
  // 大于 128 个页的 span 直接释放
  if (span->n_pages_ > N_PAGES_BUCKET - 1) {
    void* ptr = reinterpret_cast<void*>(span->page_id_ << kPageShift);
    system_dealloc(ptr, span->n_pages_);

    // 释放 span 对象
    // delete span;
    span_pool_.Delete(span);
    return;
  }

  // 对 span 前后的页尝试进行合并，缓解内存碎片问题
  while (1) {
    size_t prev_page_id = span->page_id_ - 1;
    auto prev_it = page_id_span_map_.find(prev_page_id);

    // 前面没有页了，无法合并
    if (prev_it == page_id_span_map_.end()) {
      break;
    }

    // 前面的页是被占用的，无法合并
    Span* prev_span = prev_it->second;
    if (prev_span->is_used_ == true) {
      break;
    }

    // 合并超过 128 个页的 span，直接释放，不合并
    if (prev_span->n_pages_ + span->n_pages_ > N_PAGES_BUCKET - 1) {
      break;
    }

    // 合并前后两个 span
    span->page_id_ = prev_span->page_id_;
    span->n_pages_ += prev_span->n_pages_;

    // 从 span_lists_ 中移除
    span_lists_[prev_span->n_pages_].erase(prev_span);
    // delete prev_span;
    span_pool_.Delete(prev_span);
  }

  // 向后合并
  while (1) {
    size_t next_page_id = span->page_id_ + span->n_pages_;
    auto next_it = page_id_span_map_.find(next_page_id);

    // 后面没有页了，无法合并
    if (next_it == page_id_span_map_.end()) {
      break;
    }

    // 后面的页是被占用的，无法合并
    Span* next_span = next_it->second;
    if (next_span->is_used_ == true) {
      break;
    }

    // 合并超过 128 个页的 span，直接释放，不合并
    if (next_span->n_pages_ + span->n_pages_ > N_PAGES_BUCKET - 1) {
      break;
    }

    // 合并前后两个 span
    span->n_pages_ += next_span->n_pages_;

    // 从 span_lists_ 中移除
    span_lists_[next_span->n_pages_].erase(next_span);
    // delete next_span;
    span_pool_.Delete(next_span);
  }

  // 将合并后的 span 插入到新的桶中，插入到 span_lists_ 中
  span_lists_[span->n_pages_].push_front(span);
  span->is_used_ = false;
  page_id_span_map_[span->page_id_] = span;
  page_id_span_map_[span->page_id_ + span->n_pages_ - 1] = span;
}
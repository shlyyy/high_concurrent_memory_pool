#ifndef __HIGH_CONCURRENT_MEMORY_POOL_COMMON_H__
#define __HIGH_CONCURRENT_MEMORY_POOL_COMMON_H__
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>  // Windows API
#else
#include <sys/mman.h>  // Linux mmap
#include <unistd.h>    // sysconf
#endif

// 小于等于 256KB 去 thread cache 申请，否则去 page cache 或者系统堆申请
const size_t MAX_BYTES = 256 * 1024;

// thread_cache 和 central cache 自由链表哈希桶的个数
const size_t N_FREE_LIST = 208;

// page cache 的桶的个数
const size_t N_PAGES_BUCKET = 129;

// 操作系统的页大小
extern const size_t SYSTEM_PAGE_SIZE;
extern const size_t kPageShift;

void *&get_next_obj(void *obj);

class FreeList {
 public:
  void push_front(void *obj);
  void *pop_front();

  bool empty() const;

  size_t &max_size();
  size_t size() const;

  // 将一段包含 n 个对象的内存插入到自由链表中
  void push_range(void *start, void *end, size_t n);

  // 从自由链表中取出一段包含 n 个对象的内存，放到 start 和 end 中
  void pop_range(void *&start, void *&end, size_t n);

 private:
  void *free_list_ = nullptr;
  size_t max_size_ = 1;  // 用于向 central cache 申请内存时的慢启动
  size_t size_ =
      0;  // 当前链表中的对象数量，用于判断是否需要向 central cache 归还内存
};

// size的对齐映射规则
class AlignMap {
 public:
  static size_t align_upwards(size_t size, size_t align);

  static size_t align_upwards(size_t size);

  static size_t hash_bucket_index(size_t size, size_t align_shift);

  static size_t hash_bucket_index(size_t size);

  // thread cache 一次从 central cache 中获取多少个对象
  static size_t calculate_num_objects(size_t size);

  // central cache 一次从 page cache 中获取多少个页
  static size_t calculate_num_pages(size_t size);
};

// 管理多个连续页的大块内存跨度结构
class Span {
 public:
  size_t page_id_ = 0;  // 大块内存的起始页号
  size_t n_pages_ = 0;  // 大块内存跨度的页数

  // Span 结构的双向链表
  Span *next_ = nullptr;
  Span *prev_ = nullptr;

  size_t use_count_ = 0;  // 切分好的小块内存，已分配给 thread cache 的计数
  size_t obj_size_ = 0;   // 切分好的小块内存的对象大小, 用于释放内存时计算 span
  void *free_list_ = nullptr;  // 切分好的小块内存的空闲链表

  bool is_used_ = false;  // 用于标记是否被使用
};

// 带头双向循环链表
class SpanList {
 public:
  SpanList();
  Span *begin();
  Span *end();

  bool empty() const;

  void insert(Span *pos, Span *span);
  void erase(Span *pos);

  void push_front(Span *span);
  Span *pop_front();

  std::mutex bucket_lock_;

 private:
  Span *head_ = nullptr;
};

#endif  // __HIGH_CONCURRENT_MEMORY_POOL_COMMON_H__
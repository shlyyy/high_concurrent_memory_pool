#include "common.h"

const size_t SYSTEM_PAGE_SIZE = []() {
#if defined(_WIN32)
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  return si.dwPageSize;
#else
  return sysconf(_SC_PAGESIZE);
#endif
}();

const size_t kPageShift = []() {
  std::cout << "[SYSTEM_PAGE_SIZE]: " << SYSTEM_PAGE_SIZE << std::endl;
  size_t page_size = SYSTEM_PAGE_SIZE;
  size_t shift = 0;
  while (page_size >>= 1) {
    shift++;
  }

  std::cout << "[kPageShift]: " << shift << std::endl;
  return shift;
}();

void*& get_next_obj(void* obj) { return *static_cast<void**>(obj); }

/**
 * FreeList
 */

void FreeList::push_front(void* obj) {
  assert(obj != nullptr);

  get_next_obj(obj) = free_list_;
  free_list_ = obj;

  ++size_;
}

void* FreeList::pop_front() {
  assert(free_list_);

  void* obj = free_list_;
  free_list_ = get_next_obj(free_list_);

  --size_;
  return obj;
}

bool FreeList::empty() const { return free_list_ == nullptr; }

size_t& FreeList::max_size() { return max_size_; }

size_t FreeList::size() const { return size_; }

void FreeList::push_range(void* start, void* end, size_t n) {
  assert(start != nullptr);
  assert(end != nullptr);
  assert(n > 0);

  get_next_obj(end) = free_list_;
  free_list_ = start;

  size_ += n;
}

void FreeList::pop_range(void*& start, void*& end, size_t n) {
  assert(n <= size_);

  start = free_list_;
  end = start;
  for (size_t i = 0; i < n - 1; i++) {
    end = get_next_obj(end);
  }

  free_list_ = get_next_obj(end);
  get_next_obj(end) = nullptr;
  size_ -= n;
}

/**
 * AlignMap:
 * 对齐映射规则的目的是为了减少内存碎片，在是实现上使用基于哈希桶的定长内存池，为了减少桶的个数，我们使用对齐策略
 */

size_t AlignMap::align_upwards(size_t size, size_t align) {
  return (size + align - 1) & ~(align - 1);
}

// 针对不同大小的内存块采用不同的对齐策略，整体控制在最多10%左右的内碎⽚浪费
// [1, 128]                      8byte对⻬          freelist[0, 16)
// [128 + 1, 1024]               16byte对⻬         freelist[16, 72)
// [1024 + 1, 8 * 1024]          128byte对⻬        freelist[72, 128)
// [8 * 1024 + 1, 64 * 1024]     1024byte对⻬       freelist[128, 184)
// [64 * 1024 + 1, 256 * 1024]   8 * 1024byte对⻬   freelist[184, 208)
// 上面分级对齐以后，每个区间的大小是固定的，对齐以后的种类数也是固定的
// 因此区间中桶的数量也是固定的，可以计算出每个区间中桶的数量
// 在[1, 128]区间中，每个桶的大小是8，桶的数量是16
// 在[128 + 1, 1024]区间中，每个桶的大小是16，128/16=8, 1024/16=64,
// 该区间中桶的数量是64-8=56，索引号从16开始，到16+56=72结束
size_t AlignMap::align_upwards(size_t size) {
  if (size <= 128) {
    return align_upwards(size, 8);
  } else if (size <= 1024) {
    return align_upwards(size, 16);
  } else if (size <= 8 * 1024) {
    return align_upwards(size, 128);
  } else if (size <= 64 * 1024) {
    return align_upwards(size, 1024);
  } else if (size <= 256 * 1024) {
    return align_upwards(size, 8 * 1024);
  } else {
    // 大内存，按照系统页对齐
    // 256KB < size <= 128*SYSTEM_PAGE_SIZE 走 page cache
    // size > 128*SYSTEM_PAGE_SIZE 走 system alloc
    return align_upwards(size, SYSTEM_PAGE_SIZE);
  }
}

size_t AlignMap::hash_bucket_index(size_t size, size_t align_shift) {
  return ((size + (1ULL << align_shift) - 1) >> align_shift) - 1;
}

size_t AlignMap::hash_bucket_index(size_t size) {
  static int group_array[] = {16, 56, 56, 56};
  if (size <= 128) {
    return hash_bucket_index(size, 3);
  } else if (size <= 1024) {
    return hash_bucket_index(size - 128, 4) + group_array[0];
  } else if (size <= 8 * 1024) {
    return hash_bucket_index(size - 1024, 7) + group_array[0] + group_array[1];
  } else if (size <= 64 * 1024) {
    return hash_bucket_index(size - 8 * 1024, 10) + group_array[0] +
           group_array[1] + group_array[2];
  } else if (size <= 256 * 1024) {
    return hash_bucket_index(size - 64 * 1024, 13) + group_array[0] +
           group_array[1] + group_array[2] + group_array[3];
  } else {
    assert(false);
    return -1;
  }
}

size_t AlignMap::calculate_num_objects(size_t size) {
  if (size == 0) {
    return 0;
  }

  // [2, 512] 一次批量移动多少个对象的上限
  // 小对象一次批量上限高，每次从中心缓存获取的对象数量多
  // 大对象一次批量上限低，每次从中心缓存获取的对象数量少
  size_t num_objects = MAX_BYTES / size;
  if (num_objects < 2) {
    num_objects = 2;
  }
  if (num_objects > 512) {
    num_objects = 512;
  }
  return num_objects;
}

// 一次向系统中获取几个页
size_t AlignMap::calculate_num_pages(size_t size) {
  // 一次从 page cache 中获取多少页
  // 获取对象的上限
  size_t num_objects = calculate_num_objects(size);
  // 总字节数
  size_t total_bytes = num_objects * size;
  // 总页数
  size_t num_pages = total_bytes / SYSTEM_PAGE_SIZE;
  if (total_bytes % SYSTEM_PAGE_SIZE != 0) {
    num_pages++;
  }
  return num_pages;
}

/**
 * SpanList
 */

SpanList::SpanList() {
  head_ = new Span;
  head_->next_ = head_;
  head_->prev_ = head_;
}

Span* SpanList::begin() { return head_->next_; }
Span* SpanList::end() { return head_; }

bool SpanList::empty() const { return head_->next_ == head_; }

void SpanList::insert(Span* pos, Span* span) {
  assert(pos != nullptr);
  assert(span != nullptr);

  span->next_ = pos;
  span->prev_ = pos->prev_;
  pos->prev_->next_ = span;
  pos->prev_ = span;
}

void SpanList::erase(Span* pos) {
  assert(pos != nullptr);
  assert(pos != head_);

  // 将span从链表中移除，该span会还给下一层的page cache
  pos->next_->prev_ = pos->prev_;
  pos->prev_->next_ = pos->next_;
}

void SpanList::push_front(Span* span) { insert(head_->next_, span); }

Span* SpanList::pop_front() {
  assert(!empty());

  // Span* span = head_->next_;
  // span->next_->prev_ = head_;
  // head_->next_ = span->next_;
  Span* span = head_->next_;
  erase(span);
  return span;
}

// 按页数分配内存
void* system_alloc(size_t page_count) {
  size_t length = page_count << kPageShift;
#ifdef _WIN32
  // MEM_COMMIT: 提交物理内存
  // PAGE_READWRITE: 内存可读可写
  void* ptr =
      VirtualAlloc(nullptr, length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
  // PROT_READ | PROT_WRITE: 内存可读可写
  // MAP_PRIVATE | MAP_ANONYMOUS: 私有匿名映射，不与任何文件关联
  void* ptr = mmap(nullptr, length, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return ptr;
}

// 释放由 system_alloc 分配的内存
void system_dealloc(void* ptr, size_t page_count) {
  if (ptr == nullptr) return;
#ifdef _WIN32
  if (VirtualFree(ptr, 0, MEM_RELEASE) == 0) {
    throw std::runtime_error(std::string("Memory deallocation failed: ") +
                             std::to_string(GetLastError()));
  }
#else
  size_t length = page_count << kPageShift;

  if (munmap(ptr, length) == -1) {
    throw std::runtime_error(std::string("Memory deallocation failed: ") +
                             strerror(errno));
  }
#endif
}
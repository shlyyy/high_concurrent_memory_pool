#ifndef __HIGH_CONCURRENT_MEMORY_POOL_OBJECT_POOL_H__
#define __HIGH_CONCURRENT_MEMORY_POOL_OBJECT_POOL_H__
#include "common.h"

template <class T>
class ObjectPool {
 public:
  typedef T value_type;
  typedef T* pointer;
  typedef T& reference;
  typedef const T* const_pointer;
  typedef const T& const_reference;
  typedef size_t size_type;

  pointer New() {
    pointer obj = nullptr;
    if (free_list_) {
      void* next = get_next_obj(free_list_);
      obj = reinterpret_cast<pointer>(free_list_);
      free_list_ = next;
    } else {
      size_type obj_size =
          sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*);
      if (remain_size_ < obj_size) {
        void* block = system_alloc(current_block_size_);
        memory_blocks_.push_back(std::make_pair(block, current_block_size_));
        current_ = static_cast<char*>(block);
        remain_size_ = current_block_size_ << kPageShift;

        // 每次1.5倍增长，直到最大页面数
        if (current_block_size_ < max_block_size_) {
          current_block_size_ = current_block_size_ * 3 / 2;
        } else {
          current_block_size_ = max_block_size_;
        }
      }
      obj = reinterpret_cast<pointer>(current_);
      current_ += obj_size;
      remain_size_ -= obj_size;
    }
    // placement new
    new (obj) T();

    return obj;
  }

  void Delete(pointer obj) {
    obj->~T();
    get_next_obj(obj) = free_list_;
    free_list_ = obj;
  }

  ObjectPool() : current_(nullptr), remain_size_(0), free_list_(nullptr) {
    // 初始化页面大小是对象页面对齐数，以后每次1.5倍增长，直到最大页面数
    initial_block_size_ = AlignMap::align_upwards(sizeof(T), SYSTEM_PAGE_SIZE);
    current_block_size_ = initial_block_size_;
    max_block_size_ = 1024;
  }

  ~ObjectPool() {
    for (auto& block : memory_blocks_) {
      system_dealloc(block.first, block.second);
    }
  }

 private:
  char* current_;
  size_type remain_size_;
  void* free_list_;
  std::vector<std::pair<void*, size_type>> memory_blocks_;

  // 慢启动分配策略 以页面为单位
  size_type initial_block_size_;
  size_type current_block_size_;
  size_type max_block_size_;
};

#endif  // __HIGH_CONCURRENT_MEMORY_POOL_OBJECT_POOL_H__
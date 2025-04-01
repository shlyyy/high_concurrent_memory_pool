#ifndef __HIGH_CONCURRENT_MEMORY_POOL_PAGE_MAP_H__
#define __HIGH_CONCURRENT_MEMORY_POOL_PAGE_MAP_H__
#include "common.h"

// Single-level array
template <int BITS>  // (32 - kPageShift) or (64 - kPageShift)
class TCMalloc_PageMap1 {
 private:
  static const int LENGTH = 1 << BITS;
  void** array_;

 public:
  typedef uintptr_t Number;

  explicit TCMalloc_PageMap1() {
    // 数组长度(bytes)：指针大小 * 2^BITS
    size_t size = sizeof(Number) << BITS;
    size_t alignSize = AlignMap::align_upwards(size, 1 << kPageShift);
    array_ = (void**)system_alloc(alignSize >> kPageShift);
    memset(array_, 0, sizeof(Number) << BITS);
  }

  // Return the current value for KEY.  Returns NULL if not yet set,
  // or if k is out of range.
  void* get(Number k) const {
    // k: [0, 2^BITS-1]
    // assume BITS is 3, k is 8, 8 >> 3 = 1, so the array index is overflow
    if ((k >> BITS) > 0) {
      return nullptr;
    }
    return array_[k];
  }

  // REQUIRES "k" is in range "[0,2^BITS-1]".
  // REQUIRES "k" has been ensured before.
  // Sets the value 'v' for key 'k'.
  void set(Number k, void* v) { array_[k] = v; }
};

// Two-level radix tree
template <size_t BITS>  // 页号所占位数：(32 - kPageShift) or (48 - kPageShift)
class TCMalloc_PageMap2 {
 private:
  // Put 32 entries in the root and (2^BITS)/32 entries in each leaf.
  static const size_t ROOT_BITS = 5;
  static const size_t ROOT_LENGTH = 1 << ROOT_BITS;  // 32 first-level nodes

  static const size_t LEAF_BITS = BITS - ROOT_BITS;  // for 32(4K): 19 - 5 = 14
  static const size_t LEAF_LENGTH = 1
                                    << LEAF_BITS;  // each leaf has 2^14 entries

  // Leaf node
  struct Leaf {
    void* values[LEAF_LENGTH];
  };

  Leaf* root_[ROOT_LENGTH];     // Pointers to 32 child nodes
  void* (*allocator_)(size_t);  // Memory allocator

 public:
  typedef uintptr_t Number;

  explicit TCMalloc_PageMap2() {
    // init first-level nodes
    memset(root_, 0, sizeof(root_));

    // Allocate enough to keep track of all possible pages
    Ensure(0, 1 << BITS);
  }

  void* get(Number k) const {
    const Number i1 = k >> LEAF_BITS;
    const Number i2 = k & (LEAF_LENGTH - 1);
    if ((k >> BITS) > 0 || root_[i1] == nullptr) {
      return nullptr;
    }
    return root_[i1]->values[i2];
  }

  void set(Number k, void* v) {
    const Number i1 = k >> LEAF_BITS;
    const Number i2 = k & (LEAF_LENGTH - 1);
    assert(i1 < ROOT_LENGTH);
    root_[i1]->values[i2] = v;
  }

  bool Ensure(Number start, size_t n) {
    for (Number key = start; key <= start + n - 1;) {
      const Number i1 = key >> LEAF_BITS;

      // Check for overflow
      if (i1 >= ROOT_LENGTH) return false;

      // Make 2nd level node if necessary
      if (root_[i1] == NULL) {
        static ObjectPool<Leaf> leafPool;
        Leaf* leaf = (Leaf*)leafPool.New();

        memset(leaf, 0, sizeof(*leaf));
        root_[i1] = leaf;
      }

      // Advance key past whatever is covered by this leaf node
      key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
    }
    return true;
  }
};

// Three-level radix tree
template <int BITS>
class TCMalloc_PageMap3 {
 private:
  // How many bits should we consume at each interior level
  static const int INTERIOR_BITS = (BITS + 2) / 3;  // Round-up
  static const int INTERIOR_LENGTH = 1 << INTERIOR_BITS;

  // How many bits should we consume at leaf level
  static const int LEAF_BITS = BITS - 2 * INTERIOR_BITS;
  static const int LEAF_LENGTH = 1 << LEAF_BITS;

  // Interior node
  struct Node {
    Node* ptrs[INTERIOR_LENGTH];
  };

  // Leaf node
  struct Leaf {
    void* values[LEAF_LENGTH];
  };

  Node* root_;                  // Root of radix tree
  void* (*allocator_)(size_t);  // Memory allocator

  Node* NewNode() {
    Node* result = reinterpret_cast<Node*>((*allocator_)(sizeof(Node)));
    if (result != NULL) {
      memset(result, 0, sizeof(*result));
    }
    return result;
  }

 public:
  typedef uintptr_t Number;

  explicit TCMalloc_PageMap3(void* (*allocator)(size_t)) {
    allocator_ = allocator;
    root_ = NewNode();
  }

  void* get(Number k) const {
    const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
    const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
    const Number i3 = k & (LEAF_LENGTH - 1);
    if ((k >> BITS) > 0 || root_->ptrs[i1] == NULL ||
        root_->ptrs[i1]->ptrs[i2] == NULL) {
      return NULL;
    }
    return reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3];
  }

  void set(Number k, void* v) {
    assert(k >> BITS == 0);
    const Number i1 = k >> (LEAF_BITS + INTERIOR_BITS);
    const Number i2 = (k >> LEAF_BITS) & (INTERIOR_LENGTH - 1);
    const Number i3 = k & (LEAF_LENGTH - 1);
    reinterpret_cast<Leaf*>(root_->ptrs[i1]->ptrs[i2])->values[i3] = v;
  }

  bool Ensure(Number start, size_t n) {
    for (Number key = start; key <= start + n - 1;) {
      const Number i1 = key >> (LEAF_BITS + INTERIOR_BITS);
      const Number i2 = (key >> LEAF_BITS) & (INTERIOR_LENGTH - 1);

      // Check for overflow
      if (i1 >= INTERIOR_LENGTH || i2 >= INTERIOR_LENGTH) return false;

      // Make 2nd level node if necessary
      if (root_->ptrs[i1] == NULL) {
        Node* n = NewNode();
        if (n == NULL) return false;
        root_->ptrs[i1] = n;
      }

      // Make leaf node if necessary
      if (root_->ptrs[i1]->ptrs[i2] == NULL) {
        Leaf* leaf = reinterpret_cast<Leaf*>((*allocator_)(sizeof(Leaf)));
        if (leaf == NULL) return false;
        memset(leaf, 0, sizeof(*leaf));
        root_->ptrs[i1]->ptrs[i2] = reinterpret_cast<Node*>(leaf);
      }

      // Advance key past whatever is covered by this leaf node
      key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
    }
    return true;
  }

  void PreallocateMoreMemory() {}
};

// 两级页表实现
template <size_t BITS>
class PageMap {
 private:
  // 固定叶节点位数为15位（参考TCMalloc）
  static constexpr size_t kLeafBits = 15;
  static constexpr size_t kLeafLength = 1 << kLeafBits;

  // 根节点位数
  static constexpr size_t kRootBits = BITS - kLeafBits;
  static constexpr size_t kRootLength = 1 << kRootBits;

  // 叶节点结构
  struct Leaf {
    void* values[kLeafLength];

    Leaf() { memset(values, 0, sizeof(values)); }
  };

  Leaf* root_[kRootLength];  // 根节点数组

 public:
  typedef uintptr_t Number;

  PageMap() { memset(root_, 0, sizeof(root_)); }

  // 获取页面对应的指针
  void* get(Number k) const {
    if (k >> BITS) return nullptr;  // 超出范围

    const Number i1 = k >> kLeafBits;
    const Number i2 = k & (kLeafLength - 1);

    if (i1 >= kRootLength || root_[i1] == nullptr) {
      return nullptr;
    }
    return root_[i1]->values[i2];
  }

  // 设置页面对应的指针（自动分配所需内存）
  void set(Number k, void* v) {
    assert((k >> BITS) == 0);

    const Number i1 = k >> kLeafBits;
    const Number i2 = k & (kLeafLength - 1);

    if (root_[i1] == nullptr) {
      /*static ObjectPool<Leaf> leafPool;
      Leaf* leaf = (Leaf*)leafPool.New();
      root_[i1] = leaf;*/

      static Fixed256KBlockPool leafPool;
      Leaf* leaf = (Leaf*)leafPool.New();
      memset(leaf, 0, sizeof(Leaf));
      root_[i1] = leaf;
    }

    root_[i1]->values[i2] = v;
  }
};

#endif  // __HIGH_CONCURRENT_MEMORY_POOL_PAGE_MAP_H__
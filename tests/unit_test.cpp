#include "high_concurrent_memory_pool.h"

struct TreeNode {
  int _val;
  TreeNode* _left;
  TreeNode* _right;

  TreeNode() : _val(100), _left(nullptr), _right(nullptr) {}
};

void TestObjectPool() {
  // 申请释放的轮次
  const size_t Rounds = 5;

  // 每轮申请释放多少次
  const size_t N = 100000;

  std::cout << "test vector<TreeNode*> vs ObjectPool<TreeNode>" << std::endl;
  std::vector<TreeNode*> v1;
  v1.reserve(N);

  // std::cout << "vector<TreeNode*> begin" << std::endl;
  auto start1 = std::chrono::high_resolution_clock::now();
  for (size_t j = 0; j < Rounds; ++j) {
    for (int i = 0; i < N; ++i) {
      v1.push_back(new TreeNode);
    }
    for (int i = 0; i < N; ++i) {
      delete v1[i];
    }
    v1.clear();
  }
  auto end1 = std::chrono::high_resolution_clock::now();
  // std::cout << "vector<TreeNode*> end" << std::endl;

  std::vector<TreeNode*> v2;
  v2.reserve(N);

  ObjectPool<TreeNode> TNPool;

  // std::cout << "ObjectPool<TreeNode> begin" << std::endl;
  auto start2 = std::chrono::high_resolution_clock::now();
  for (size_t j = 0; j < Rounds; ++j) {
    for (int i = 0; i < N; ++i) {
      v2.push_back(TNPool.New());
    }
    for (int i = 0; i < N; ++i) {
      TNPool.Delete(v2[i]);
    }
    v2.clear();
  }
  auto end2 = std::chrono::high_resolution_clock::now();
  // std::cout << "ObjectPool<TreeNode> end" << std::endl;

  std::cout << "vector<TreeNode*> time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end1 -
                                                                     start1)
                   .count()
            << "ms" << std::endl;
  std::cout << "ObjectPool<TreeNode> time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end2 -
                                                                     start2)
                   .count()
            << "ms" << std::endl;
}

void BenchmarkMalloc1(size_t ntimes, size_t nworks, size_t rounds) {
  std::vector<std::thread> vthread(nworks);
  std::atomic<size_t> malloc_costtime = 0;
  std::atomic<size_t> free_costtime = 0;

  for (size_t k = 0; k < nworks; ++k) {
    vthread[k] = std::thread([&, k]() {
      std::vector<void*> v;
      v.reserve(ntimes);

      for (size_t j = 0; j < rounds; ++j) {
        auto begin1 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < ntimes; i++) {
          // v.push_back(malloc(16));
          v.push_back(malloc((16 + i) % 8192 + 1));
        }
        auto end1 = std::chrono::high_resolution_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(
            end1 - begin1);

        auto begin2 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < ntimes; i++) {
          free(v[i]);
        }
        auto end2 = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(
            end2 - begin2);
        v.clear();

        malloc_costtime += duration1.count();
        free_costtime += duration2.count();
      }
    });
  }

  for (auto& t : vthread) {
    t.join();
  }

  printf("%zu个线程并发执行%zu轮次，每轮次malloc %zu次: 花费：%zu ms\n", nworks,
         rounds, ntimes, malloc_costtime.load());

  printf("%zu个线程并发执行%zu轮次，每轮次free %zu次: 花费：%zu ms\n", nworks,
         rounds, ntimes, free_costtime.load());

  printf("%zu个线程并发malloc&free %zu次，总计花费：%zu ms\n", nworks,
         nworks * rounds * ntimes,
         malloc_costtime.load() + free_costtime.load());
}

void BenchmarkConcurrentMalloc1(size_t ntimes, size_t nworks, size_t rounds) {
  std::vector<std::thread> vthread(nworks);
  std::atomic<size_t> malloc_costtime = 0;
  std::atomic<size_t> free_costtime = 0;

  for (size_t k = 0; k < nworks; ++k) {
    vthread[k] = std::thread([&]() {
      std::vector<void*> v;
      v.reserve(ntimes);

      for (size_t j = 0; j < rounds; ++j) {
        auto begin1 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < ntimes; i++) {
          // v.push_back(hc_malloc(16));
          v.push_back(hc_malloc((16 + i) % 8192 + 1));
        }
        auto end1 = std::chrono::high_resolution_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(
            end1 - begin1);

        auto begin2 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < ntimes; i++) {
          hc_free(v[i]);
        }
        auto end2 = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(
            end2 - begin2);
        v.clear();

        malloc_costtime += duration1.count();
        free_costtime += duration2.count();
      }
    });
  }

  for (auto& t : vthread) {
    t.join();
  }

  printf(
      "%zu个线程并发执行%zu轮次，每轮次concurrent alloc %zu次: 花费：%zu ms\n",
      nworks, rounds, ntimes, malloc_costtime.load());

  printf(
      "%zu个线程并发执行%zu轮次，每轮次concurrent dealloc %zu次: 花费：%zu "
      "ms\n",
      nworks, rounds, ntimes, free_costtime.load());

  printf("%zu个线程并发concurrent alloc&dealloc %zu次，总计花费：%zu ms\n",
         nworks, nworks * rounds * ntimes,
         malloc_costtime.load() + free_costtime.load());
}

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <thread>
#include <vector>

// 符合现实场景的内存大小分布生成器
size_t generate_realistic_size(std::mt19937& gen) {
  // 使用指数分布模拟现实中的内存分配模式（小内存更频繁）
  std::exponential_distribution<double> exp_dist(1.0 / 1024);  // 平均1024字节

  // 限制最小16字节（避免过小），最大1GB（避免过大）
  size_t size = static_cast<size_t>(exp_dist(gen)) + 16;
  return std::min(size, static_cast<size_t>(1UL << 30));  // 上限1GB
}

void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds) {
  std::vector<std::thread> vthread(nworks);
  std::atomic<size_t> malloc_costtime = 0;
  std::atomic<size_t> free_costtime = 0;
  std::atomic<size_t> total_allocated = 0;

  for (size_t k = 0; k < nworks; ++k) {
    vthread[k] = std::thread([&]() {
      std::vector<void*> v;
      v.reserve(ntimes);
      std::random_device rd;
      std::mt19937 gen(rd());

      for (size_t j = 0; j < rounds; ++j) {
        auto begin1 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < ntimes; i++) {
          size_t size = generate_realistic_size(gen);
          void* ptr = malloc(size);
          if (ptr) {
            v.push_back(ptr);
            total_allocated += size;
          }
        }
        auto end1 = std::chrono::high_resolution_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(
            end1 - begin1);

        auto begin2 = std::chrono::high_resolution_clock::now();
        for (void* ptr : v) {
          free(ptr);
        }
        auto end2 = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(
            end2 - begin2);
        v.clear();

        malloc_costtime += duration1.count();
        free_costtime += duration2.count();
      }
    });
  }

  for (auto& t : vthread) {
    t.join();
  }

  printf("%zu threads x %zu rounds x %zu alloc/free ops\n", nworks, rounds,
         ntimes);
  printf("Total allocated: %.2f MB\n",
         total_allocated.load() / (1024.0 * 1024.0));
  printf(
      "Average allocation size: %.2f bytes\n",
      total_allocated.load() / static_cast<double>(nworks * rounds * ntimes));
  printf("malloc time: %zu ms (%.3f us per op)\n", malloc_costtime.load(),
         malloc_costtime.load() * 1000.0 / (nworks * rounds * ntimes));
  printf("free time: %zu ms (%.3f us per op)\n", free_costtime.load(),
         free_costtime.load() * 1000.0 / (nworks * rounds * ntimes));
  printf("total time: %zu ms\n", malloc_costtime.load() + free_costtime.load());
}

// 同样的修改应用于BenchmarkConcurrentMalloc
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds) {
  std::vector<std::thread> vthread(nworks);
  std::atomic<size_t> malloc_costtime = 0;
  std::atomic<size_t> free_costtime = 0;
  std::atomic<size_t> total_allocated = 0;

  for (size_t k = 0; k < nworks; ++k) {
    vthread[k] = std::thread([&]() {
      std::vector<void*> v;
      v.reserve(ntimes);
      std::random_device rd;
      std::mt19937 gen(rd());

      for (size_t j = 0; j < rounds; ++j) {
        auto begin1 = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < ntimes; i++) {
          size_t size = generate_realistic_size(gen);
          void* ptr = hc_malloc(size);
          if (ptr) {
            v.push_back(ptr);
            total_allocated += size;
          }
        }
        auto end1 = std::chrono::high_resolution_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(
            end1 - begin1);

        auto begin2 = std::chrono::high_resolution_clock::now();
        for (void* ptr : v) {
          hc_free(ptr);
        }
        auto end2 = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(
            end2 - begin2);
        v.clear();

        malloc_costtime += duration1.count();
        free_costtime += duration2.count();
      }
    });
  }

  for (auto& t : vthread) {
    t.join();
  }

  printf("%zu threads x %zu rounds x %zu alloc/free ops (concurrent)\n", nworks,
         rounds, ntimes);
  printf("Total allocated: %.2f MB\n",
         total_allocated.load() / (1024.0 * 1024.0));
  printf(
      "Average allocation size: %.2f bytes\n",
      total_allocated.load() / static_cast<double>(nworks * rounds * ntimes));
  printf("malloc time: %zu ms (%.3f us per op)\n", malloc_costtime.load(),
         malloc_costtime.load() * 1000.0 / (nworks * rounds * ntimes));
  printf("free time: %zu ms (%.3f us per op)\n", free_costtime.load(),
         free_costtime.load() * 1000.0 / (nworks * rounds * ntimes));
  printf("total time: %zu ms\n", malloc_costtime.load() + free_costtime.load());
}

int main2() {
  TestObjectPool();
  return 0;
}

int main() {
  size_t system_page_size = 0;
#if defined(_WIN32)
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  system_page_size = si.dwPageSize;
#else
  system_page_size = sysconf(_SC_PAGESIZE);
#endif
  std::cout << "System page size: " << system_page_size << std::endl;

  size_t n = 100000;
  std::cout << "=========================================================="
            << std::endl;
  BenchmarkConcurrentMalloc(n, 4, 10);
  std::cout << std::endl << std::endl;

  BenchmarkMalloc(n, 4, 10);
  std::cout << "=========================================================="
            << std::endl;

  return 0;
}

int main1() {
  // 打印当前工作路径
  std::cout << "Current work path: " << get_current_dir_name() << std::endl;
  std::cout << __cplusplus << std::endl;

  // 打印 页大小
  std::cout << "System page size: " << SYSTEM_PAGE_SIZE << std::endl;
  std::cout << "kPageShift: " << kPageShift << std::endl;
  return 0;
}

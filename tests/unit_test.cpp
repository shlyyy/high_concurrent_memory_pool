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

void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds) {
  std::vector<std::thread> vthread(nworks);
  std::atomic<size_t> malloc_costtime = 0;
  std::atomic<size_t> free_costtime = 0;

  for (size_t k = 0; k < nworks; ++k) {
    vthread[k] = std::thread([&, k]() {
      std::vector<void*> v;
      v.reserve(ntimes);

      for (size_t j = 0; j < rounds; ++j) {
        size_t begin1 = clock();
        for (size_t i = 0; i < ntimes; i++) {
          v.push_back(malloc(16));
          // v.push_back(malloc((16 + i) % 8192 + 1));
        }
        size_t end1 = clock();

        size_t begin2 = clock();
        for (size_t i = 0; i < ntimes; i++) {
          free(v[i]);
        }
        size_t end2 = clock();
        v.clear();

        malloc_costtime += (end1 - begin1);
        free_costtime += (end2 - begin2);
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

// 单轮次申请释放次数 线程数 轮次
void BenchmarkConcurrentMalloc(size_t ntimes, size_t nworks, size_t rounds) {
  std::vector<std::thread> vthread(nworks);
  std::atomic<size_t> malloc_costtime = 0;
  std::atomic<size_t> free_costtime = 0;

  for (size_t k = 0; k < nworks; ++k) {
    vthread[k] = std::thread([&]() {
      std::vector<void*> v;
      v.reserve(ntimes);

      for (size_t j = 0; j < rounds; ++j) {
        size_t begin1 = clock();
        for (size_t i = 0; i < ntimes; i++) {
          v.push_back(hc_malloc(16));
          // v.push_back(ConcurrentAlloc((16 + i) % 8192 + 1));
        }
        size_t end1 = clock();

        size_t begin2 = clock();
        for (size_t i = 0; i < ntimes; i++) {
          hc_free(v[i]);
        }
        size_t end2 = clock();
        v.clear();

        malloc_costtime += (end1 - begin1);
        free_costtime += (end2 - begin2);
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

int main() {
  TestObjectPool();
  return 0;
}

int main2() {
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

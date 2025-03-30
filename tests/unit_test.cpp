#include "high_concurrent_memory_pool.h"

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

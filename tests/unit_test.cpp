#include "common.h"
#include "thread_cache.h"

/*
void BenchmarkMalloc(size_t ntimes, size_t nworks, size_t rounds) {
  std::vector[std::thread](std::thread) vthread(nworks);
  std::atomic<size_t> malloc_costtime = 0;
  std::atomic<size_t> free_costtime = 0;
  for (size_t k = 0; k < nworks; ++k) {
    vthread[k] = std::thread(&, k {
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
        for (size_t i = 0; i < ntimes; i++)
        {
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
  printf("%u个线程并发执⾏%u轮次，每轮次malloc %u次: 花费：%u ms\n", nworks,
         rounds, ntimes, malloc_costtime);

  printf("%u个线程并发执⾏%u轮次，每轮次free %u次: 花费：%u ms\n", nworks,
         rounds, ntimes, free_costtime);

  printf("%u个线程并发malloc&free %u次，总计花费：%u ms\n", nworks,
         nworks * rounds * ntimes, malloc_costtime + free_costtime);
}
*/

int main() {
  // 打印 页大小
  std::cout << "System page size: " << SYSTEM_PAGE_SIZE << std::endl;
  std::cout << "kPageShift: " << kPageShift << std::endl;

  // 调试 内存池

  // ThreadCache* tc = GetThreadCache();
  // void* ptr1 = tc->allocate(17);
  // void* ptr2 = tc->allocate(8);
  // void* ptr3 = tc->allocate(1);
  // void* ptr4 = tc->allocate(1025);
  // void* ptr5 = tc->allocate(1024 * 64 + 1);
  // void* ptr6 = tc->allocate(1024 * 256);

  return 0;
}
#include <vector>

#include "flowpool.h"
// #include "priority_thread_pool.h"
// #include "thread_pool.hpp"

int main() {
  Flowpool pool(1);

  std::vector<int> v = {1, 2, 3, 4, 5};

  for (auto &x: v) {
    std::cout << x << ' ';
  }
  std::cout << std::endl;

  std::shared_ptr<std::atomic<bool>> flag;

  for (int i = 0; i < v.size(); ++i) {
    flag = pool.push_task([&v, i]() {
      v[i] += 3;
    });
  }

  pool.push_task(1, [&v]() {
    v[v.size() - 1] *= 3;
  }, flag);

  while (!flag->load()) {
    std::cout << "yo" << std::endl;
  }
  std::cout << "dawg" << std::endl;

  pool.wait_for_tasks();

  for (auto &x: v) {
    std::cout << x << ' ';
  }
  std::cout << std::endl;
}

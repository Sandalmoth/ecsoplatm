#include <vector>

#include "flowpool.h"
// #include "priority_thread_pool.h"
// #include "thread_pool.hpp"

int main() {
  Flowpool pool;

  std::vector<int> v = {1, 2, 3, 4, 5};

  for (auto &x: v) {
    std::cout << x << ' ';
  }
  std::cout << std::endl;


  for (int i = 0; i < v.size(); ++i) {
    pool.push_task([&v, i]() {
      v[i] += 3;
    });
  }

  pool.wait_for_tasks();

  for (auto &x: v) {
    std::cout << x << ' ';
  }
  std::cout << std::endl;
}

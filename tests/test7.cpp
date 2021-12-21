#include <iostream>
#include <chrono>

using namespace std::chrono_literals;

#include "flowpool.h"


int main() {
  Flowpool pool;

  int a = 1;

  auto flag = pool.push_task([&]() {
    std::this_thread::sleep_for(2000ms);
    ++a;
  });

  flag = pool.push_task([&]() {
    a *= a;
  }, flag);

  pool.wait_for_tasks();

  // should be 4 if we are correcly waiting
  // for the sleeping task before doing the second one
  std::cout << a << std::endl;
}

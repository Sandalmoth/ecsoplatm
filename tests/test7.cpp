#include <iostream>
#include <chrono>

using namespace std::chrono_literals;

#include "flowpool.h"


int main() {
  Flowpool pool;

  int a = 1;
  int b = 2;

  auto flag = pool.push_task([&]() {
    std::this_thread::sleep_for(2000ms);
    ++a;
  });

  pool.push_task([&]() {
    --b;
  });

  flag = pool.push_task([&]() {
    a *= a;
  }, flag);

  flag = pool.push_task([&]() {
    --a;
  }, flag);

  pool.wait_for_tasks();

  // should be 3 1 if we are correcly waiting
  // for the sleeping task before doing the second one
  std::cout << a << ' ' << b << std::endl;
}

#include <iostream>


#include "flowpool.h"


int main() {
  Flowpool pool;

  int a = 0;

  pool.push_task([&]() {
    ++a;
  });
  pool.wait_for_tasks();

  std::cout << a << std::endl;
}

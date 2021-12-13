#include <iostream>

#include "interval_map.h"

int main() {
  IntervalMap<int> im;

  im.set(1, 3, 5);
  std::cout << im << std::endl;
  im.set(2, 8, 2);
  std::cout << im << std::endl;
  im.set(6, 7, 1);
  std::cout << im << std::endl;
  im.set(-3, 33, 0);
  std::cout << im << std::endl;
}

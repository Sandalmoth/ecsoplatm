#include <iostream>

#include "interval_map.h"

int main() {
  IntervalMap<int> im;

  im.set(1, 3, 1);
  std::cout << im << std::endl;
  im.set(2, 8, 2);
  std::cout << im << std::endl;
  im.set(6, 7, 3);
  std::cout << im << std::endl;
  im.set(-3, 33, 4);
  std::cout << im << std::endl;
  im.set(-11, 15, 5);
  std::cout << im << std::endl;
  im.set(12, 17, 6);
  std::cout << im << std::endl;
  im.set(12, 17, 7);
  std::cout << im << std::endl;

  for (auto x: im.get(-99, 99)) {
    std::cout << x << ' ';
  } std::cout << std::endl;
  for (auto x: im.get(-11, 12)) {
    std::cout << x << ' ';
  } std::cout << std::endl;
  for (auto x: im.get(12, 17)) {
    std::cout << x << ' ';
  } std::cout << std::endl;
  for (auto x: im.get(17, 33)) {
    std::cout << x << ' ';
  } std::cout << std::endl;
}

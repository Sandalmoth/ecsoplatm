
#include <iostream>

#include "thread_pool.hpp"

#include "../src/ecsoplatm.h"

struct foo {
  void operator()(float* a, float* b) { (*a) *= (*b); }
};

int main() {
  thread_pool pool;
  ecs::Component<float> c;
  c.data.push_back(std::make_pair(1, 2.0));
  c.data.push_back(std::make_pair(3, 4.0));
  c.data.push_back(std::make_pair(5, 6.0));

  for (int i = 0; i < 6; ++i) {
    auto x = c[i];
    if (x) {
      std::cout << i << '\t' << (*x) << std::endl;
    } else {
      std::cout << i << " not present" << std::endl;
    }
  }

  ecs::Component<float> c2;
  c2.data.push_back(std::make_pair(1, 3.0));
  c2.data.push_back(std::make_pair(5, 7.0));

  ecs::apply<foo>(c, c2);

  for (int i = 0; i < 6; ++i) {
    auto x = c[i];
    if (x) {
      std::cout << i << '\t' << (*x) << std::endl;
    } else {
      std::cout << i << " not present" << std::endl;
    }
  }

}

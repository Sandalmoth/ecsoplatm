
#include <iostream>
#include <chrono>
#include <string>
#include <cmath>

#include "thread_pool.hpp"

#include "../src/ecsoplatm.h"

class Timer {
public:
  Timer(std::string name_) {
    start_time = std::chrono::high_resolution_clock::now();
    name = name_;
  }
  ~Timer() {
    auto now = std::chrono::high_resolution_clock::now();
    std::cout << name << ": \t" << std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count() << " us" << std::endl;
  }
private:
  std::string name;
  std::chrono::high_resolution_clock::time_point start_time;
};

struct foo {
  void operator()(float& a) {
    a *= sin(a) + cos(a)/a;
    if (a < 0)
      a = -a;
    for (int i = 0; i < 100; ++i) {
      a *= 1000;
      while (a > 10.0) {
        a = sqrt(a);
      }
    }
  }
};
struct bar {
  void operator()(float &a, float &b) {
    if (a < 0)
      a = -a;
    for (int i = 0; i < 100; ++i) {
      a *= 1000;
      while (a > 10.0) {
        a = sqrt(a);
      }
    }
    a *= b;
  }
};

void zoom(float &a) {
  a *= sin(a) + cos(a) / a;
}

int main() {
  thread_pool pool;
  // pool.sleep_duration = 10;
  ecs::Component<float> c;
  ecs::Component<float> c2;

  for (int i = 0; i < 10000; ++i) {
    if (i % 3 != 0)
      c.data.push_back(std::make_pair(i, static_cast<float>(i + 1)*0.1));
    if (i % 5 != 0)
      c2.data.push_back(std::make_pair(i, static_cast<float>(i + 35)));
  }

  {
    Timer t("single component");
    ecs::apply<foo>(c);
  }

  {
    Timer t("single component mt");
    ecs::apply<foo>(pool, c);
    pool.wait_for_tasks();
  }

  {
    Timer t("single component mt function pointer");
    ecs::apply(&zoom, pool, c);
    pool.wait_for_tasks();
  }

  {
    Timer t("two components");
    ecs::apply<bar>(c, c2);
  }

  {
    Timer t("two components mt");
    ecs::apply<bar>(pool, c, c2);
    pool.wait_for_tasks();
  }

  for (int i = 0; i < 10; ++i) {
    if (c[i] != nullptr)
      std::cout << *c[i] << std::endl;
  }

}

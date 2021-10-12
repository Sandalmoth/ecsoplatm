#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

#include "thread_pool.hpp"


class Timer {
public:
  Timer(std::string name_) {
    start_time = std::chrono::high_resolution_clock::now();
    name = name_;
  }
  ~Timer() {
    auto now = std::chrono::high_resolution_clock::now();
    std::cout << name << ": \t"
              << std::chrono::duration_cast<std::chrono::microseconds>(
                     now - start_time)
                     .count()
              << " us" << std::endl;
  }

private:
  std::string name;
  std::chrono::high_resolution_clock::time_point start_time;
};


constexpr int N = 100000;


double foo(double x) {
  return sqrt(x + sin(x) + cos(x))/23.4;
  // return x + 1;
}

double bar(double& x) {
  x = sqrt(x + sin(x) + cos(x)) / 23.4;
  // x += 1;
}


int main() {
  thread_pool pool;
  pool.sleep_duration = 10;

  std::vector<double> v;

  for (int i = 0; i < N; ++i) {
    v.push_back(i);
  }

  {
    Timer t("linear");
    for (auto & x: v) {
      x = foo(x);
    }
  }

  {
    Timer t("linear in place");
    for (auto &x : v) {
      foo(x);
    }
  }

  {
    Timer t("threaded");
    pool.parallelize_loop(0, 100, [&v](const uint32_t &a, const uint32_t &b) {
      for (uint32_t i = a; i < b; i++)
        v[i] = foo(v[i]);
    });
  }

  {
    Timer t("threaded in place");
    pool.parallelize_loop(0, 100, [&v](const uint32_t &a, const uint32_t &b) {
      for (uint32_t i = a; i < b; i++)
        foo(v[i]);
    });
  }

  for (int i = 0; i < 10; ++i) {
    std::cout << i << ' ';
  }
  std::cout << std::endl << v[N - 1] << std::endl;
  }

#include <iostream>
#include <chrono>
#include <string>
#include <cmath>
#include <cassert>

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
    auto delta = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();
    std::cout << name << ": \t" << delta << " us" << std::endl;
  }
private:
  std::string name;
  std::chrono::high_resolution_clock::time_point start_time;
};


const int N = 10000;


// intentionally kinda awkward bogus maths
// just to make it a semi slow operation

void f1(double &x) {
  x *= sin(x);
  if (x < 0)
    x = -x;
  x = sqrt(x);
}

void f2(double &x, double &y) {
  double z = x*y;
  x = sin(z);
  y = cos(z);
  if (x < 0)
    x = -x;
  if (y < 0)
    y = -y;
  x = sqrt(x*y);
  y = sqrt(x + y);
}


int main() {

  thread_pool pool;

  ecs::Component<double> st_1, st_2, mt_1, mt_2;
  for (int i = 0; i < N; ++i) {
    if (i % 3 != 0) {
      st_1.data.push_back(std::make_pair(i, static_cast<double>(i + 1)*0.1));
      mt_1.data.push_back(std::make_pair(i, static_cast<double>(i + 1)*0.1));
    }
    if (i % 5 != 0) {
      st_2.data.push_back(std::make_pair(i, i + 35));
      mt_2.data.push_back(std::make_pair(i, i + 35));
    }
  }

  {
    Timer t("single-thread\t1 component");
    ecs::apply(f1, st_1);
  }
  {
    Timer t("multi-thread\t1 component");
    ecs::apply(f1, mt_1, pool);
    pool.wait_for_tasks();
  }
  {
    Timer t("single-thread\t2 components");
    ecs::apply(f2, st_1, st_2);
  }
  {
    Timer t("multi-thread\t2 components");
    ecs::apply(f2, mt_1, mt_2, pool);
    pool.wait_for_tasks();
  }

  for (int i = 0; i < st_1.data.size(); ++i) {
    // std::cout << st_1.data[i].second << ' ' << mt_1.data[i].second << std::endl;
    assert (st_1.data[i] == mt_1.data[i]);
  }
  for (int i = 0; i < st_2.data.size(); ++i) {
    // std::cout << st_2.data[i].second << ' ' << mt_2.data[i].second << std::endl;
    assert (st_2.data[i] == mt_2.data[i]);
  }
}

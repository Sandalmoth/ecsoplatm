#include <iostream>
#include <chrono>
#include <string>
#include <cmath>
#include <cassert>

#include "thread_pool.hpp"

#define ECSOPLATM_IMPLEMENTATION
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

  std::cout << "--- Test 3 ---" << std::endl;

  thread_pool pool;

  ecs::Component<double> c1, c2;
  for (int i = 0; i < N; ++i) {
    auto id = ecs::get_id();
    bool used_id = false;
    // this construction is a bit awkward
    // normally you woudn't get an id if you didn't know you had to use it
    if (i % 3 != 0) {
      c1.data.push_back(std::make_pair(id, static_cast<double>(i + 1)*0.1));
      used_id = true;
    }
    if (i % 5 != 0) {
      c2.data.push_back(std::make_pair(id, static_cast<double>(i + 1) * 0.1));
      used_id = true;
    }
    if (!used_id) {
      ecs::return_id(id);
    }
  }

  {
    Timer t("Big creation job");
    c1.update(pool);
    c2.update(pool);
    pool.wait_for_tasks();
  }

  for (int i = 1; i < c1.data.size(); ++i) {
    assert (c1.data[i - 1] < c1.data[i]); // still ordered, and has no duplicates
  }
  for (int i = 1; i < c2.data.size(); ++i) {
    assert(c2.data[i - 1] < c2.data[i]); // still ordered, and has no duplicates
  }
}

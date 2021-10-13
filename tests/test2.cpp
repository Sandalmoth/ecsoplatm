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

  for (int i = 0; i < N; i += 7) {
    st_1.destroy(i);
    st_2.destroy(i);
    mt_1.destroy(i);
    mt_2.destroy(i);
  }

  for (int i = 0; i < N; i += 15) {
    st_1.create(i, 999.999);
    st_2.create(i, 999.999);
    mt_1.create(i, 999.999);
    mt_2.create(i, 999.999);
  }

  {
    Timer t("update single threaded");
    // pool.push_task([&st_1](){ st_1.update(); });
    st_1.update();
    st_2.update();
  }

  {
    Timer t("update multi threaded");
    // pool.push_task([&st_1](){ st_1.update(); });
    mt_1.update(pool);
    mt_2.update(pool);
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

#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>
#include <iostream>

#include "thread_pool.hpp"

const uint32_t BLOCK_SIZE = 1000;

namespace ecs {

  template <typename T>
  struct Component {
    std::vector<std::pair<uint32_t, T>> data;

    T* operator[](uint32_t key) {
      auto it = std::lower_bound(
        data.begin(), data.end(), key,
        [](const std::pair<uint32_t, T>& a, uint32_t b) {
          return a.first < b;
      });

      if (key == it->first) {
        return &(it->second);
      }
      else {
        return nullptr;
      }
    }

  };

  template <typename F, typename T>
  void apply_impl(
                  typename std::vector<std::pair<uint32_t, T>>::iterator first,
                  typename std::vector<std::pair<uint32_t, T>>::iterator last
                  ) {
    F f;
    while (first != last) {
      f(first->second);
      ++first;
    }
  }

  template <typename F, typename T>
  void apply(thread_pool & pool, Component<T> &a){
    // pool.parallelize_loop(
    //   0, a.data.size(),
    //   [&a, &f](uint32_t x, uint32_t y) {
    //     for (uint32_t i = x; i < y; ++i)
    //       f(a.data[i].second);
    //   },
    //   (a.data.size()/BLOCK_SIZE) + 1
    // );
    int n = a.data.size()/BLOCK_SIZE;
    // pool.push_task(apply_impl<F, T>, a.data.begin(), a.data.end());
    // pool.push_task(
    //     [](typename std::vector<std::pair<uint32_t, T>>::iterator first,
    //        typename std::vector<std::pair<uint32_t, T>>::iterator last) {
    //       F f;
    //       while (first != last) {
    //         f(first->second);
    //         ++first;
    //       }
    //     },
    //     a.data.begin(), a.data.end()
                   // );

    auto walk = a.data.begin();
    F f;
    for (int i = 1; i < n; ++i) {
      pool.push_task(
          [&f](typename std::vector<std::pair<uint32_t, T>>::iterator first,
             typename std::vector<std::pair<uint32_t, T>>::iterator last) {
            while (first != last) {
              f(first->second);
              ++first;
            }
          },
          walk, walk + BLOCK_SIZE);
      walk += BLOCK_SIZE;
    }
  }

  template <typename F, typename T>
  void apply(Component<T> &a) {
    F f;
    auto it_a = a.data.begin();
    while (it_a != a.data.end()) {
      f(it_a->second);
      ++it_a;
    }
  }

  template <typename T>
  void apply_impl(void (*f)(T &),
                  typename std::vector<std::pair<uint32_t, T>>::iterator first,
                  typename std::vector<std::pair<uint32_t, T>>::iterator last
                  ) {
    while (first != last) {
      f(first->second);
      ++first;
    }
  }

  template <typename T>
  void apply(void (*f)(T &), thread_pool &pool,
             Component<T> &a){
    // auto it_a = a.data.begin();
    // while (it_a != a.data.end()) {
    //   f(it_a->second);
    //   ++it_a;
    // }
    auto walk = a.data.begin();
    int n = a.data.size() / BLOCK_SIZE;
    std::cout << n << std::endl;
    std::cout << a.data.size() << std::endl;
    std::cout << (n - 1) * BLOCK_SIZE << std::endl;
    for (int i = 1; i < n; ++i) {
      pool.push_task(
          [f](typename std::vector<std::pair<uint32_t, T>>::iterator first,
              typename std::vector<std::pair<uint32_t, T>>::iterator last) {
            while (first != last) {
              f(first->second);
              ++first;
            }
          },
          walk, walk + BLOCK_SIZE);
      walk += BLOCK_SIZE;
    }
  }

  template <typename F, typename T, typename U>
  void apply(Component<T>& a, Component<U>& b) {
    F f;
    auto it_a = a.data.begin();
    auto it_b = b.data.begin();
    while ((it_a != a.data.end()) && (it_b != b.data.end())) {
      if (it_a->first == it_b->first) {
        f(it_a->second, it_b->second);
        ++it_a;
        ++it_b;
      } else if (it_a->first < it_b->first) {
        ++it_a;
      } else {
        ++it_b;
      }
    }
  }

  template <typename F, typename T, typename U, typename V>
  void apply(Component<T>& a, Component<U>& b, Component<V>& c) {
    F f;
    auto it_a = a.data.begin();
    auto it_b = b.data.begin();
    auto it_c = c.data.begin();
    while ((it_a != a.data.end()) && (it_b != b.data.end()) && (it_c != c.data.end())) {
      if ((it_a->first == it_b->first) && (it_a->first == it_c->first)) {
        f(it_a->second, it_b->second, it_c->second);
        ++it_a;
        ++it_b;
        ++it_c;
      } else if ((it_a->first < it_b->first) or (it_a->first < it_c->first)) {
        ++it_a;
      } else if ((it_b->first < it_a->first) or (it_b->first < it_c->first)) {
        ++it_b;
      } else if ((it_c->first < it_a->first) or (it_c->first < it_b->first)) {
        ++it_c;
      }
    }
  }

};


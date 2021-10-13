#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include <iostream>

#include "thread_pool.hpp"

// chunk arrays into jobs of this size
// when running multithreaded versions
const uint32_t BLOCK_SIZE = 256;

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


  template <typename T>
    void apply(void (*f)(T &), Component<T> &a) {
    auto it_a = a.data.begin();
    while (it_a != a.data.end()) {
      f(it_a->second);
      ++it_a;
    }
  }


  template <typename T>
  void apply(void (*f)(T &),
             Component<T> &a, thread_pool &pool){

    auto walk = a.data.begin();
    int n = a.data.size() / BLOCK_SIZE;
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
    pool.push_task(
                   [f](typename std::vector<std::pair<uint32_t, T>>::iterator first,
                       typename std::vector<std::pair<uint32_t, T>>::iterator last) {
                     while (first != last) {
                       f(first->second);
                       ++first;
                     }
                   },
                   walk, a.data.end());
  }


  template <typename T, typename U>
    void apply(void (*f)(T &, U &), Component<T>& a, Component<U>& b) {
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


  template <typename T, typename U>
    void apply(void (*f)(T &, U &), Component<T>& a, Component<U>& b, thread_pool &pool) {
    // we will split work into n + 1 groups
    int n = a.data.size() / BLOCK_SIZE;
    int a_step = a.data.size()/n;
    int b_step = b.data.size()/n;

    // first, decide on what entity index breakpoints that entails
    // first one is always 0, and last one is always based on last elements
    std::vector<int> breakpoints;
    breakpoints.reserve(n);
    for (int i = 1; i < n; ++i) {
      breakpoints.push_back((a.data[i*a_step].first + b.data[i*b_step].first) >> 1);
    }

    // now start a job between each breakpoint
    auto it_a = a.data.begin();
    auto it_b = b.data.begin();

    for (auto breakpoint: breakpoints) {
      // find an iterator to the entity with id = breakpoint in each list
      auto it_a_break = std::lower_bound(
                                         a.data.begin(), a.data.end(), breakpoint,
                                         [](const std::pair<uint32_t, T>& a, uint32_t b) {
                                           return a.first < b;
                                         });
      auto it_b_break = std::lower_bound(
                                         b.data.begin(), b.data.end(), breakpoint,
                                         [](const std::pair<uint32_t, T>& a, uint32_t b) {
                                           return a.first < b;
                                         });

      pool.push_task(
                     [f](
                         typename std::vector<std::pair<uint32_t, T>>::iterator a_first,
                         typename std::vector<std::pair<uint32_t, T>>::iterator a_last,
                         typename std::vector<std::pair<uint32_t, T>>::iterator b_first,
                         typename std::vector<std::pair<uint32_t, T>>::iterator b_last
                         ) {
                       while ((a_first != a_last) && (b_first != b_last)) {
                         if (a_first->first == b_first->first) {
                           f(a_first->second, b_first->second);
                           ++a_first;
                           ++b_first;
                         } else if (a_first->first < b_first->first) {
                           ++a_first;
                         } else {
                           ++b_first;
                         }
                       }
                     },
                     it_a, it_a_break, it_b, it_b_break);

      // we will reuse the breakpoint as the next starting point
      it_a = it_a_break;
      it_b = it_b_break;
    }

    pool.push_task(
                    [f](
                        typename std::vector<std::pair<uint32_t, T>>::iterator a_first,
                        typename std::vector<std::pair<uint32_t, T>>::iterator a_last,
                        typename std::vector<std::pair<uint32_t, T>>::iterator b_first,
                        typename std::vector<std::pair<uint32_t, T>>::iterator b_last
                        ) {
                      while ((a_first != a_last) && (b_first != b_last)) {
                        if (a_first->first == b_first->first) {
                          f(a_first->second, b_first->second);
                          ++a_first;
                          ++b_first;
                        } else if (a_first->first < b_first->first) {
                          ++a_first;
                        } else {
                          ++b_first;
                        }
                      }
                    },
                    it_a, a.data.end(), it_b, b.data.end());

  }


} // end namespace ecs

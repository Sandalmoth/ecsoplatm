#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>
#include <iostream>

#include "thread_pool.hpp"

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

  template <typename F, typename T, typename U>
  void apply(Component<T>& a, Component<U>& b) {
    F f;
    auto it_a = a.data.begin();
    auto it_b = b.data.begin();
    while ((it_a != a.data.end()) && (it_b != b.data.end())) {
      if (it_a->first == it_b->first) {
        f(&(it_a->second), &(it_b->second));
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
        f(&(it_a->second), &(it_b->second, &(it_c->second)));
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


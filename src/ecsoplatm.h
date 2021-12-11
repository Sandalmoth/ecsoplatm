#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include <iostream>

#include "thread_pool.hpp"

// chunk arrays into jobs of this size
// when running multithreaded versions
const uint32_t BLOCK_SIZE = 1024;

namespace ecs {

  template <typename T>
  struct Component {
    // a component is a sorted vector of entity id - value pairs
    // since preserving the sorting is critical (and slow)
    // all creation and destruction is deferred to an update step
    // that can be ran at the end of the gameloop
    std::vector<std::pair<uint32_t, T>> data;
    std::vector<std::pair<uint32_t, T>> create_queue;
    std::vector<typename std::vector<std::pair<uint32_t, T>>::iterator> destroy_queue;


    T* operator[](uint32_t key) {
      auto it = std::lower_bound(
                                  data.begin(), data.end(), key,
                                  [](const std::pair<uint32_t, T>& a, uint32_t b) {
                                    return a.first < b;
                                  });

      if (it == data.end()) {
        return nullptr;
      }
      if (key == it->first) {
        return &(it->second);
      }
      else {
        return nullptr;
      }
    }


    void create(uint32_t entity, T value) {
      create_queue.push_back(std::make_pair(entity, value));
    }


    void destroy(uint32_t entity) {
      // we may as well find right away what entity will be destroyed
      // since all modifications are guaranteed (if used correctly...)
      // to happen after actually using this iterator
      auto it = std::lower_bound(
                                 data.begin(), data.end(), entity,
                                 [](const std::pair<uint32_t, T>& a, uint32_t b) {
                                   return a.first < b;
                                 });
      if (it != data.end()) {
        destroy_queue.push_back(it);
      }
    }


    void update() {
      // execute deferred destruction
      // first, the destroy queue needs to be sorted
      std::sort(destroy_queue.begin(), destroy_queue.end(), [](auto a, auto b) {
        return a->first > b->first;
      });
      // then they can be destroyed in reverse order
      auto last = --data.end();
      for (auto it: destroy_queue) {
        // swap to last, then erase (for speed!)
        std::iter_swap(it, last);
        data.erase(last--);
      }
      destroy_queue.clear();
      // execute deferred creation
      for (auto &ev: create_queue) {
        // FIXME? it's not great that this fails silently
        // if the entity already exists
        data.push_back(std::move(ev));
      }
      create_queue.clear();
      // sort by entity id
      std::sort(data.begin(), data.end(),
                [](const std::pair<uint32_t, T> &a,
                   const std::pair<uint32_t, T> &b) {
                  return a.first < b.first;
                });
    }


    void update(thread_pool& pool) {
      // version that delegates to a thread pool
      pool.push_task([this](){ update(); });
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
    int n = a.data.size() / BLOCK_SIZE + 1;
    if (n == 0)
      return; // no work to do
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
    int n = (a.data.size() + b.data.size())/BLOCK_SIZE/2 + 1;
    if (n == 0)
      return; // no work to do
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
                                         [](const std::pair<uint32_t, U>& a, uint32_t b) {
                                           return a.first < b;
                                         });

      pool.push_task(
                     [f](
                         typename std::vector<std::pair<uint32_t, T>>::iterator a_first,
                         typename std::vector<std::pair<uint32_t, T>>::iterator a_last,
                         typename std::vector<std::pair<uint32_t, U>>::iterator b_first,
                         typename std::vector<std::pair<uint32_t, U>>::iterator b_last
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
                        typename std::vector<std::pair<uint32_t, U>>::iterator b_first,
                        typename std::vector<std::pair<uint32_t, U>>::iterator b_last
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


  template <typename T, typename U, typename V>
  void apply(void (*f)(T &, U &, V &), Component<T>& a, Component<U>& b, Component<V>& c) {
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


  // IDEA
  // create a manager class
  // move id counting into it
  // register function pointers
  // for destroy calls (maybe create too?)
  // so you can easily destroy an entire entity
  // however
  // that is not possible without an interface class for components
  // as we cannot call the destroy member function with runtime polymorphism
  // while it's not too hard to implement
  // it may as well be a normal function in the main program
  // hardcoded to delete from all the relevant components

  class Manager {
  public:
    Manager();
    uint32_t get_id();
    void return_id(uint32_t);
  private:
    uint32_t max_unused_id;
    std::vector<uint32_t> unused_ids;
  };

#ifdef ECSOPLATM_IMPLEMENTATION

  Manager::Manager() {
    max_unused_id = 0;
  }

  uint32_t Manager::get_id() {
    uint32_t id = max_unused_id;
    if (!unused_ids.empty()) {
      id = unused_ids.back();
      unused_ids.pop_back();
    } else {
      ++max_unused_id;
    }
    return id;
  }

  void Manager::return_id(uint32_t id) {
    unused_ids.push_back(id);
  }

#endif // ECSOPLATM_MPLEMENTATION

} // end namespace ecs



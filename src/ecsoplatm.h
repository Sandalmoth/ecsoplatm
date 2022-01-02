#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>


namespace ecs { class Flowpool; }
inline std::ostream &operator<<(std::ostream &, ecs::Flowpool &);


namespace ecs {

  const int BLOCK_SIZE = 256;
  const int CACHE_BITS = 4;
  const int CACHE_SIZE = 0x1 << CACHE_BITS;


  template <typename T>
  struct IntervalMap {

    /*
     * This is a very simple struct for storing non-overlapping intervals
     * So we can store a T in some interval [4, 5] = t1
     * And then store another T in like [2, 4] = t2
     * Which creates [2, 4] = t2, [5] = t1
     * And obviously we can do lookups with an interavl as well
     */

    void set(int first, int last, T value) {
      // first put our key into the vector
      auto it = std::lower_bound(
          data.begin(), data.end(), first,
          [](const std::tuple<int, int, T> &a, const int &b) {
            return std::get<0>(a) < b;
          });
      it = data.insert(it, std::make_tuple(first, last, value));
      // then, determine if we need to shorten the previous interval
      auto prev = it;
      while (prev != data.begin()) {
        --prev;
        if (std::get<1>(*prev) > first) {
          if (std::get<1>(*prev) > last) {
            // if we're inserting in the middle, we need to create a new entry
            int tmp = std::get<1>(*prev);
            std::get<1>(*prev) = first;
            ++it;
            data.insert(it, std::make_tuple(last, tmp, std::get<2>(*prev)));
            return;
          } else {
            // otherwise we just shorten the prior
            std::get<1>(*prev) = first;
          }
        } else {
          break;
        }
      }
      // then, determine the number of following intervals that were overwritten
      auto next = it;
      ++next;
      while (next != data.end()) {
        if (std::get<1>(*next) <= last) {
          // we've entirely overwritten this one
          next = data.erase(next);
          --next;
        } else if (std::get<0>(*next) < last) {
          std::get<0>(*next) = last;
        }
        ++next;
      }
    }

    std::vector<T> get(int first, int last) {
      std::vector<T> result;
      for (auto &[f, l, v]: data) {
        if (!(last <= f) & !(first >= l)) {
          result.push_back(v);
        }
      }
      return result;
    }

    std::vector<std::tuple<int, int, T>> data;
  };


  class Flowpool {

    /*
    * This thread pool is essentially a rewrite and modified version of
    * SandSnip3r's fork ( https://github.com/SandSnip3r/thread-pool ) of
    * Barak Shashany's thread_pool.hpp library (
    * https://github.com/bshoshany/thread-pool ).
    *
    * It also implements a waiting system where a pushed task can be required
    * to wait for some prior task to finish before entering the work queue.
    * This enables us to queue tasks that will sequentially modify the same data.
    */

  public:

    enum TaskStatus {
      WAITING,
      IN_PROGRESS,
      DONE,
      NUM_TASK_STATUS
    };

    Flowpool() {
      n_threads = std::thread::hardware_concurrency();
      create_threads();
    }

    Flowpool(int n_threads_)
      : n_threads(n_threads_) {
      create_threads();
    }

    ~Flowpool()
    {
      wait_for_tasks();
      running = false;
      destroy_threads();
    }


    void wait_for_tasks() {
      std::unique_lock<std::mutex> lock(tasks_mutex);
      tasks_done_condition.wait(lock, [&] {
        return (n_tasks == 0);
      });

      total_tasks = 0;
      tasks.clear();
      flags.clear();
      conditions.clear();
    }

    template <typename F>
    int push_task(const F &task,
                  std::vector<int> conds) {
      int id;
      {
        std::scoped_lock lock(tasks_mutex);
        tasks.push_back(task);
        conditions.push_back(std::move(conds));
        flags.push_back(WAITING);
        ++n_tasks;
        id = total_tasks++;
      }
      task_available_condition.notify_one();
      return id;
    }

    template <typename F,
              std::convertible_to<int>... C>
    int push_task(const F &task, C... conds) {
      std::vector<int> vconds {conds...};
      return push_task(task, vconds);
    }


  private:

    void create_threads() {
      threads = std::make_unique<std::thread[]>(n_threads);
      for (int i = 0; i < n_threads; ++i) {
        threads[i] = std::thread(&Flowpool::worker, this);
      }
    }

    void destroy_threads()
    {
      task_available_condition.notify_all();
      for (int i = 0; i < n_threads; i++)
        {
          threads[i].join();
        }
    }


    void worker() {
      while (running) {

        std::unique_lock<std::mutex> lock(tasks_mutex);

        task_available_condition.wait(lock, [&]{
          return !tasks.empty() || !running;
        });

        if (running) {
          // grab the first task from tasks that is doable
          // a task is doable if conds[task].size == 0
          // or if all flags[conds[task]] == true
          // possible speedup: don't start from 0
          int task_id = 0;
          while (task_id < total_tasks) {
            if (flags[task_id] == WAITING) {
              // it's a waiting task, see if it can run
              bool doable = true;
              for (auto cond: conditions[task_id]) {
                // possible speedup: short circuit loop
                doable = doable && (flags[cond] == DONE);
              }
              if (doable) {
                auto task = std::move(tasks[task_id]);
                flags[task_id] = IN_PROGRESS;

                lock.unlock();
                task();
                lock.lock();

                flags[task_id] = DONE;
                --n_tasks;
                break;
              }
            }
            ++task_id;
          }

          lock.unlock();
          tasks_done_condition.notify_one();
        }
      }
    }

    friend std::ostream &::operator<<(std::ostream &, ecs::Flowpool &);

    int n_threads;
    std::unique_ptr<std::thread[]> threads;

    std::mutex tasks_mutex; // locks tasks, n_tasks, flags, and conditions

    std::condition_variable task_available_condition;
    std::condition_variable tasks_done_condition;

    std::atomic<bool> running {true};
    int n_tasks {0}; // total number of waiting, queued, and running tasks
    int total_tasks {0}; // total number of tasks queued since last wait

    std::vector<char> flags;
    std::vector<std::function<void()>> tasks;
    std::vector<std::vector<int>> conditions; // indices into flags
  };


  struct ComponentInterface {
    virtual void update() = 0;
    virtual bool exists(uint32_t) = 0;
    void destroy(uint32_t id) { destroy_queue.push_back(id); }

    std::vector<uint32_t> destroy_queue;
    IntervalMap<int> waiting_flags;
  };


  template <typename T>
  struct Component : ComponentInterface {

    T *operator[](uint32_t key) {

      int64_t hashed = key * 0xf9b25d65 >> 8; // see arXiv:2001.05304
      hashed = hashed & (CACHE_SIZE - 1);

      if (key == cache[hashed].first) {
        return cache[hashed].second;
      }

      auto it = std::lower_bound(data.begin(), data.end(), key,
                                 [](const std::pair<uint32_t, T> &a,
                                    uint32_t b) { return a.first < b; });

      cache[hashed].first = key;
      cache[hashed].second = nullptr;

      if (it == data.end()) {
        return nullptr;
      }
      if (key == it->first) {
        cache[hashed].second = &(it->second);
        return &(it->second);
      } else {
        return nullptr;
      }
    }

    void create(uint32_t entity, T value) {
      create_queue.push_back(std::make_pair(entity, value));
    }

    bool exists(uint32_t id) {
      return nullptr != this->operator[](id);
    }

    void update() {
      // update may invalidate the cache, so erase it
      cache.fill(std::make_pair(0, nullptr));
      // execute deferred destruction
      // first, the destroy queue needs to be sorted
      std::sort(destroy_queue.begin(), destroy_queue.end(), std::greater<>());
      // also, remove duplicate erases
      destroy_queue.erase(std::unique(destroy_queue.begin(), destroy_queue.end()),
                          destroy_queue.end());
      // then they can be destroyed in reverse order
      auto last = --data.end();
      for (auto i: destroy_queue) {
        // find the position of the element we're erasing
        // (still guaranteeed to be in reverse order, since the vector is sorted)
        auto it = std::lower_bound(data.begin(), data.end(), i,
                                   [](const std::pair<uint32_t, T> &a,
                                      uint32_t b) { return a.first < b; });
        // swap to last, then erase (for speed!)
        std::swap(data[it - data.begin()], (*last));
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

    std::vector<std::pair<uint32_t, T>> data;
    std::vector<std::pair<uint32_t, T>> create_queue;
    std::array<std::pair<uint32_t, T *>, CACHE_SIZE> cache;

  };


  struct Manager {
    uint32_t max_unused_id {1}; // 0 is no entity by definition
    Flowpool pool;
    std::vector<ComponentInterface *> components;
    std::vector<std::string> component_names; // used for debug features
    std::vector<uint32_t> unused_ids;

    Manager() {}
    Manager(int n_threads)
      : pool(n_threads) {}

    uint32_t get_id() {
      uint32_t id = max_unused_id;
      if (!unused_ids.empty()) {
        id = unused_ids.back();
        unused_ids.pop_back();
      } else {
        ++max_unused_id;
      }
      return id;
    }

    void return_id(uint32_t id) { unused_ids.push_back(id); }

    template <typename T> void enlist(Component<T> *component) {
      components.push_back(component);
      component_names.push_back("UNKNOWN");
    }

    template <typename T> void enlist(Component<T> *component, std::string name) {
      components.push_back(component);
      component_names.push_back(name);
    }

    void debug_print_entity_components(uint32_t id) {
      std::cout << id << " : ";
      // print what components an entity has
      for (size_t i = 0; i < components.size(); ++i) {
        if (components[i]->exists(id)) {
          std::cout << component_names[i] << ' ';
        }
      }
      std::cout << std::endl;
    }

    void update() {
      for (auto c : components) {
        c->update();
      }
    }

    void destroy(uint32_t id) {
      for (auto c : components) {
        c->destroy(id);
      }
    }

    void wait() {
      pool.wait_for_tasks();
      for (auto c : components) {
        c->waiting_flags.data.clear();
      }
    }

    template <typename A>
    void apply(void (*f)(A &), Component<A> &a) {
      if (a.data.size() == 0)
        return; // no work to do

      int i = 0;
      while (static_cast<size_t>(i) < a.data.size()) {
        int j = std::min(a.data.size(), static_cast<size_t>(i + BLOCK_SIZE));
        auto wait = a.waiting_flags.get(i, j);
        auto flag = pool.push_task([f,
                                  first = a.data.begin() + i,
                                  last = a.data.begin() + j]() {
          auto it = first;
          while (it != last) {
            f(it->second);
            ++it;
          }
        }, wait);
        a.waiting_flags.set(i, j, flag);
        i += BLOCK_SIZE;
      }
    }

    template <typename A> void apply(void (*f)(A &, void *), Component<A> &a, void *payload) {
      // a version that passes along an arbitrary void pointer
      // could be used to access some shared data (in an unprotected manner)
      // since the may be accessed in parallel, it is probably unwise to modify it
      if (a.data.size() == 0)
        return; // no work to do

      int i = 0;
      while (static_cast<size_t>(i) < a.data.size()) {
        int j = std::min(a.data.size(), static_cast<size_t>(i + BLOCK_SIZE));
        auto wait = a.waiting_flags.get(i, j);
        auto flag = pool.push_task([f, payload,
                                    first = a.data.begin() +
                                    i, last = a.data.begin() + j]() {
              auto it = first;
              while (it != last) {
                f(it->second, payload);
                ++it;
              }
            },
            wait);
        a.waiting_flags.set(i, j, flag);
        i += BLOCK_SIZE;
      }
    }

    template <typename A, typename B>
    void apply(void (*f)(A &, B &), Component<A> &a, Component<B> &b) {
      if ((a.data.size() == 0) || (b.data.size() == 0))
        return;

      int n = (a.data.size() + b.data.size())/BLOCK_SIZE/2;
      n = std::max(n, 1);
      int a_step = a.data.size()/n;
      int b_step = b.data.size()/n;

      std::vector<int> breaks;
      breaks.reserve(n);
      for (int i = 1; i < n; ++i) {
        breaks.push_back((a.data[i*a_step].first + b.data[i*b_step].first) / 2);
      }

      auto it_a = a.data.begin();
      auto it_b = b.data.begin();

      for (auto breakpoint: breaks) {
        // find an iterator to the entity with id = breakpoint in each list
        auto it_a_break =
            std::lower_bound(a.data.begin(), a.data.end(), breakpoint,
                             [](const std::pair<uint32_t, A> &a, uint32_t b) {
                               return a.first < b;
                             });
        auto it_b_break =
            std::lower_bound(b.data.begin(), b.data.end(), breakpoint,
                             [](const std::pair<uint32_t, B> &a, uint32_t b) {
                               return a.first < b;
                             });

        auto wait_a = a.waiting_flags.get(it_a - a.data.begin(),
                                          it_a_break - a.data.begin());
        auto wait_b = b.waiting_flags.get(it_b - b.data.begin(),
                                          it_b_break - b.data.begin());
        wait_a.insert(wait_a.end(), wait_b.begin(), wait_b.end());

        auto flag = pool.push_task(
            [f,
             afirst = it_a, alast = it_a_break,
             bfirst = it_b, blast = it_b_break]() {
              auto it_a = afirst;
              auto it_b = bfirst;
              while ((it_a != alast) && (it_b != blast)) {
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
            }, wait_a);

        a.waiting_flags.set(it_a - a.data.begin(),
                            it_a_break - a.data.begin(),
                            flag);
        b.waiting_flags.set(it_b - b.data.begin(),
                            it_b_break - b.data.begin(),
                            flag);

        it_a = it_a_break;
        it_b = it_b_break;
      }

      auto wait_a = a.waiting_flags.get(it_a - a.data.begin(),
                                        a.data.size());
      auto wait_b = b.waiting_flags.get(it_b - b.data.begin(),
                                        b.data.size());
      wait_a.insert(wait_a.end(), wait_b.begin(), wait_b.end());

      auto flag = pool.push_task([f, afirst = it_a, alast = a.data.end(),
                                bfirst = it_b, blast = b.data.end()]() {
        auto it_a = afirst;
        auto it_b = bfirst;
        while ((it_a != alast) && (it_b != blast)) {
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
      }, wait_a);

      a.waiting_flags.set(it_a - a.data.begin(),
                          a.data.size(),
                          flag);
      b.waiting_flags.set(it_b - b.data.begin(),
                          b.data.size(),
                          flag);

    }

    template <typename A, typename B>
    void apply(void (*f)(A &, B &, void *), Component<A> &a, Component<B> &b, void *payload) {
      if ((a.data.size() == 0) || (b.data.size() == 0))
        return;

      int n = (a.data.size() + b.data.size())/BLOCK_SIZE/2;
      n = std::max(n, 1);
      int a_step = a.data.size()/n;
      int b_step = b.data.size()/n;

      std::vector<int> breaks;
      breaks.reserve(n);
      for (int i = 1; i < n; ++i) {
        breaks.push_back((a.data[i*a_step].first + b.data[i*b_step].first) / 2);
      }

      auto it_a = a.data.begin();
      auto it_b = b.data.begin();

      for (auto breakpoint: breaks) {
        // find an iterator to the entity with id = breakpoint in each list
        auto it_a_break =
            std::lower_bound(a.data.begin(), a.data.end(), breakpoint,
                             [](const std::pair<uint32_t, A> &a, uint32_t b) {
                               return a.first < b;
                             });
        auto it_b_break =
            std::lower_bound(b.data.begin(), b.data.end(), breakpoint,
                             [](const std::pair<uint32_t, B> &a, uint32_t b) {
                               return a.first < b;
                             });

        auto wait_a = a.waiting_flags.get(it_a - a.data.begin(),
                                          it_a_break - a.data.begin());
        auto wait_b = b.waiting_flags.get(it_b - b.data.begin(),
                                          it_b_break - b.data.begin());
        wait_a.insert(wait_a.end(), wait_b.begin(), wait_b.end());

        auto flag = pool.push_task(
                                   [f, payload,
             afirst = it_a, alast = it_a_break,
             bfirst = it_b, blast = it_b_break]() {
              auto it_a = afirst;
              auto it_b = bfirst;
              while ((it_a != alast) && (it_b != blast)) {
                if (it_a->first == it_b->first) {
                  f(it_a->second, it_b->second, payload);
                  ++it_a;
                  ++it_b;
                } else if (it_a->first < it_b->first) {
                  ++it_a;
                } else {
                  ++it_b;
                }
              }
            }, wait_a);

        a.waiting_flags.set(it_a - a.data.begin(),
                            it_a_break - a.data.begin(),
                            flag);
        b.waiting_flags.set(it_b - b.data.begin(),
                            it_b_break - b.data.begin(),
                            flag);

        it_a = it_a_break;
        it_b = it_b_break;
      }

      auto wait_a = a.waiting_flags.get(it_a - a.data.begin(),
                                        a.data.size());
      auto wait_b = b.waiting_flags.get(it_b - b.data.begin(),
                                        b.data.size());
      wait_a.insert(wait_a.end(), wait_b.begin(), wait_b.end());

      auto flag = pool.push_task([f, payload,
                                  afirst = it_a, alast = a.data.end(),
                                  bfirst = it_b, blast = b.data.end()]() {
        auto it_a = afirst;
        auto it_b = bfirst;
        while ((it_a != alast) && (it_b != blast)) {
          if (it_a->first == it_b->first) {
            f(it_a->second, it_b->second, payload);
            ++it_a;
            ++it_b;
          } else if (it_a->first < it_b->first) {
            ++it_a;
          } else {
            ++it_b;
          }
        }
      }, wait_a);

      a.waiting_flags.set(it_a - a.data.begin(),
                          a.data.size(),
                          flag);
      b.waiting_flags.set(it_b - b.data.begin(),
                          b.data.size(),
                          flag);

    }

    template <typename A, typename B, typename C>
    void apply(void (*f)(A &, B &, C &), Component<A> &a, Component<B> &b, Component<C> &c) {
      if ((a.data.size() == 0) || (b.data.size() == 0) || (c.data.size() == 0))
        return;

      int n = (a.data.size() + b.data.size() + c.data.size())/BLOCK_SIZE/3;
      n = std::max(n, 1);
      int a_step = a.data.size()/n;
      int b_step = b.data.size()/n;
      int c_step = c.data.size()/n;

      std::vector<int> breaks;
      breaks.reserve(n);
      for (int i = 1; i < n; ++i) {
        breaks.push_back((a.data[i*a_step].first +
                          b.data[i*b_step].first +
                          c.data[i*c_step].first) / 3);
      }

      auto it_a = a.data.begin();
      auto it_b = b.data.begin();
      auto it_c = c.data.begin();

      for (auto breakpoint: breaks) {
        // find an iterator to the entity with id = breakpoint in each list
        auto it_a_break =
            std::lower_bound(a.data.begin(), a.data.end(), breakpoint,
                             [](const std::pair<uint32_t, A> &a, uint32_t b) {
                               return a.first < b;
                             });
        auto it_b_break =
            std::lower_bound(b.data.begin(), b.data.end(), breakpoint,
                             [](const std::pair<uint32_t, B> &a, uint32_t b) {
                               return a.first < b;
                             });
        auto it_c_break =
            std::lower_bound(c.data.begin(), c.data.end(), breakpoint,
                             [](const std::pair<uint32_t, C> &a, uint32_t b) {
                               return a.first < b;
                             });

        auto wait_a = a.waiting_flags.get(it_a - a.data.begin(),
                                          it_a_break - a.data.begin());
        auto wait_b = b.waiting_flags.get(it_b - b.data.begin(),
                                          it_b_break - b.data.begin());
        auto wait_c = c.waiting_flags.get(it_c - c.data.begin(),
                                          it_c_break - c.data.begin());
        wait_a.insert(wait_a.end(), wait_b.begin(), wait_b.end());
        wait_a.insert(wait_a.end(), wait_c.begin(), wait_c.end());

        // the idea here is to increment whichever iterator points to the lowest id
        // and if they're all equal, then we apply the function, and increase all
        auto flag = pool.push_task(
            [f,
             afirst = it_a, alast = it_a_break,
             bfirst = it_b, blast = it_b_break,
             cfirst = it_c, clast = it_c_break]() {
              auto it_a = afirst;
              auto it_b = bfirst;
              auto it_c = cfirst;
              while ((it_a != alast) && (it_b != blast) && (it_c != clast)) {
                if ((it_a->first == it_b->first) && (it_a->first == it_c->first)) {
                  f(it_a->second, it_b->second, it_c->second);
                  ++it_a;
                  ++it_b;
                  ++it_c;
                } else if ((it_a->first < it_b->first) ||
                           (it_a->first < it_c->first)) {
                  ++it_a;
                } else if ((it_b->first < it_a->first) ||
                           (it_b->first < it_c->first)) {
                  ++it_b;
                } else if ((it_c->first < it_a->first) ||
                           (it_c->first < it_b->first)) {
                  ++it_c;
                }
              }
            }, wait_a);

        a.waiting_flags.set(it_a - a.data.begin(),
                            it_a_break - a.data.begin(),
                            flag);
        b.waiting_flags.set(it_b - b.data.begin(),
                            it_b_break - b.data.begin(),
                            flag);
        c.waiting_flags.set(it_c - c.data.begin(),
                            it_c_break - c.data.begin(),
                            flag);

        it_a = it_a_break;
        it_b = it_b_break;
        it_c = it_c_break;
      }

      auto wait_a = a.waiting_flags.get(it_a - a.data.begin(),
                                        a.data.size());
      auto wait_b = b.waiting_flags.get(it_b - b.data.begin(),
                                        b.data.size());
      auto wait_c = c.waiting_flags.get(it_c - c.data.begin(),
                                        c.data.size());

      wait_a.insert(wait_a.end(), wait_b.begin(), wait_b.end());
      wait_a.insert(wait_a.end(), wait_c.begin(), wait_c.end());

      auto flag = pool.push_task(
          [f,
           afirst = it_a, alast = a.data.end(),
           bfirst = it_b, blast = b.data.end(),
           cfirst = it_c, clast = c.data.end()]() {
            auto it_a = afirst;
            auto it_b = bfirst;
            auto it_c = cfirst;
            while ((it_a != alast) && (it_b != blast) && (it_c != clast)) {
              if ((it_a->first == it_b->first) && (it_a->first == it_c->first)) {
                f(it_a->second, it_b->second, it_c->second);
                ++it_a;
                ++it_b;
                ++it_c;
              } else if ((it_a->first < it_b->first) ||
                          (it_a->first < it_c->first)) {
                ++it_a;
              } else if ((it_b->first < it_a->first) ||
                          (it_b->first < it_c->first)) {
                ++it_b;
              } else if ((it_c->first < it_a->first) ||
                          (it_c->first < it_b->first)) {
                ++it_c;
              }
            }
          }, wait_a);

      a.waiting_flags.set(it_a - a.data.begin(),
                          a.data.size(),
                          flag);
      b.waiting_flags.set(it_b - b.data.begin(),
                          b.data.size(),
                          flag);
      c.waiting_flags.set(it_c - c.data.begin(),
                          c.data.size(),
                          flag);
    }

    template <typename A, typename B, typename C>
    void apply(void (*f)(A &, B &, C &, void *),
               Component<A> &a, Component<B> &b, Component<C> &c,
               void *payload) {
      if ((a.data.size() == 0) || (b.data.size() == 0) || (c.data.size() == 0))
        return;

      int n = (a.data.size() + b.data.size() + c.data.size())/BLOCK_SIZE/3;
      n = std::max(n, 1);
      int a_step = a.data.size()/n;
      int b_step = b.data.size()/n;
      int c_step = c.data.size()/n;

      std::vector<int> breaks;
      breaks.reserve(n);
      for (int i = 1; i < n; ++i) {
        breaks.push_back((a.data[i*a_step].first +
                          b.data[i*b_step].first +
                          c.data[i*c_step].first) / 3);
      }

      auto it_a = a.data.begin();
      auto it_b = b.data.begin();
      auto it_c = c.data.begin();

      for (auto breakpoint: breaks) {
        // find an iterator to the entity with id = breakpoint in each list
        auto it_a_break =
            std::lower_bound(a.data.begin(), a.data.end(), breakpoint,
                             [](const std::pair<uint32_t, A> &a, uint32_t b) {
                               return a.first < b;
                             });
        auto it_b_break =
            std::lower_bound(b.data.begin(), b.data.end(), breakpoint,
                             [](const std::pair<uint32_t, B> &a, uint32_t b) {
                               return a.first < b;
                             });
        auto it_c_break =
            std::lower_bound(c.data.begin(), c.data.end(), breakpoint,
                             [](const std::pair<uint32_t, C> &a, uint32_t b) {
                               return a.first < b;
                             });

        auto wait_a = a.waiting_flags.get(it_a - a.data.begin(),
                                          it_a_break - a.data.begin());
        auto wait_b = b.waiting_flags.get(it_b - b.data.begin(),
                                          it_b_break - b.data.begin());
        auto wait_c = c.waiting_flags.get(it_c - c.data.begin(),
                                          it_c_break - c.data.begin());
        wait_a.insert(wait_a.end(), wait_b.begin(), wait_b.end());
        wait_a.insert(wait_a.end(), wait_c.begin(), wait_c.end());

        // the idea here is to increment whichever iterator points to the lowest id
        // and if they're all equal, then we apply the function, and increase all
        auto flag = pool.push_task(
                                   [f, payload,
             afirst = it_a, alast = it_a_break,
             bfirst = it_b, blast = it_b_break,
             cfirst = it_c, clast = it_c_break]() {
              auto it_a = afirst;
              auto it_b = bfirst;
              auto it_c = cfirst;
              while ((it_a != alast) && (it_b != blast) && (it_c != clast)) {
                if ((it_a->first == it_b->first) && (it_a->first == it_c->first)) {
                  f(it_a->second, it_b->second, it_c->second, payload);
                  ++it_a;
                  ++it_b;
                  ++it_c;
                } else if ((it_a->first < it_b->first) ||
                           (it_a->first < it_c->first)) {
                  ++it_a;
                } else if ((it_b->first < it_a->first) ||
                           (it_b->first < it_c->first)) {
                  ++it_b;
                } else if ((it_c->first < it_a->first) ||
                           (it_c->first < it_b->first)) {
                  ++it_c;
                }
              }
            }, wait_a);

        a.waiting_flags.set(it_a - a.data.begin(),
                            it_a_break - a.data.begin(),
                            flag);
        b.waiting_flags.set(it_b - b.data.begin(),
                            it_b_break - b.data.begin(),
                            flag);
        c.waiting_flags.set(it_c - c.data.begin(),
                            it_c_break - c.data.begin(),
                            flag);

        it_a = it_a_break;
        it_b = it_b_break;
        it_c = it_c_break;
      }

      auto wait_a = a.waiting_flags.get(it_a - a.data.begin(),
                                        a.data.size());
      auto wait_b = b.waiting_flags.get(it_b - b.data.begin(),
                                        b.data.size());
      auto wait_c = c.waiting_flags.get(it_c - c.data.begin(),
                                        c.data.size());

      wait_a.insert(wait_a.end(), wait_b.begin(), wait_b.end());
      wait_a.insert(wait_a.end(), wait_c.begin(), wait_c.end());

      auto flag = pool.push_task(
                                 [f, payload,
           afirst = it_a, alast = a.data.end(),
           bfirst = it_b, blast = b.data.end(),
           cfirst = it_c, clast = c.data.end()]() {
            auto it_a = afirst;
            auto it_b = bfirst;
            auto it_c = cfirst;
            while ((it_a != alast) && (it_b != blast) && (it_c != clast)) {
              if ((it_a->first == it_b->first) && (it_a->first == it_c->first)) {
                f(it_a->second, it_b->second, it_c->second, payload);
                ++it_a;
                ++it_b;
                ++it_c;
              } else if ((it_a->first < it_b->first) ||
                          (it_a->first < it_c->first)) {
                ++it_a;
              } else if ((it_b->first < it_a->first) ||
                          (it_b->first < it_c->first)) {
                ++it_b;
              } else if ((it_c->first < it_a->first) ||
                          (it_c->first < it_b->first)) {
                ++it_c;
              }
            }
          }, wait_a);

      a.waiting_flags.set(it_a - a.data.begin(),
                          a.data.size(),
                          flag);
      b.waiting_flags.set(it_b - b.data.begin(),
                          b.data.size(),
                          flag);
      c.waiting_flags.set(it_c - c.data.begin(),
                          c.data.size(),
                          flag);
    }


  };

} // end namespace ecs


template <typename T>
inline std::ostream &operator<<(std::ostream &out, ecs::IntervalMap<T> &im) {
  out << '[';
  for (auto &[first, last, value] : im.data) {
    out << '(' << first << ' ' << value << ' ' << last << ')';
  }
  out << ']';
  return out;
}

inline std::ostream &operator<<(std::ostream &out, ecs::Flowpool &pool) {
  std::scoped_lock lock(pool.tasks_mutex);
  std::cout << pool.n_tasks << " unfinished out of " << pool.total_tasks
            << " total" << std::endl;
  for (int i = 0; i < pool.total_tasks; ++i) {
    std::string flag;
    switch (pool.flags[i]) {
    case ecs::Flowpool::WAITING:
      flag = "waiting";
      break;
    case ecs::Flowpool::IN_PROGRESS:
      flag = "in_progress";
      break;
    case ecs::Flowpool::DONE:
      flag = "done";
      break;
    }
    std::cout << '(' << i << ' ' << flag;
    for (auto &w : pool.conditions[i]) {
      std::cout << ' ' << w;
    }
    std::cout << ')' << std::endl;
  }
  return out;
}

template <typename T>
inline std::ostream &operator<<(std::ostream &out, ecs::Component<T> &c) {
  out << '[';
  for (auto &[id, value]: c.data) {
    out << '(' << id << ' ' << value << ')';
  }
  out << ']';
  return out;
}


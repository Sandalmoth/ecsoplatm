#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

/*
 * This thread pool is essentially a rewrite and modified version of
 * SandSnip3r's fork ( https://github.com/SandSnip3r/thread-pool ) of
 * Barak Shashany's thread_pool.hpp library ( https://github.com/bshoshany/thread-pool ).
 *
 * This version uses a priority queue of tasks, though note that the locking
 * of the priority queue will stronly disfavour the use of many small tasks.
 * It also implements a waiting system where a pushed task can be required
 * to wait for some prior task to finish before entering the work queue.
 * This enables us to queue tasks that will sequentiall modify the same data.
 */


struct compare_task_priority {
  bool operator()(const std::tuple<int, std::shared_ptr<std::atomic_flag>, std::function<void()>> &a,
                  const std::tuple<int, std::shared_ptr<std::atomic_flag>, std::function<void()>> &b) {
    return std::get<0>(a) < std::get<0>(b);
  }
};


bool all_set(const std::vector<std::shared_ptr<std::atomic_flag>> &conditions) {
  // test whether all conditions are true
  // note, const ref to shared ptr is fine, since we know it is kept alive by the vector
  return std::all_of(conditions.begin(), conditions.end(), [](const std::shared_ptr<std::atomic_flag> &x) {
    return x->test();
  });
}


class Flowpool {
public:
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
  }


  template <typename F>
  std::shared_ptr<std::atomic_flag> push_task(const F &task)
  {
    return push_task(0, task);
  }


  template <typename F>
  std::shared_ptr<std::atomic_flag> push_task(int priority, const F &task)
  {
    auto flag = std::make_shared<std::atomic_flag>();
    {
      const std::scoped_lock lock(tasks_mutex);
      tasks.push(std::make_tuple(priority, flag, std::function<void()>(task)));
      ++n_tasks;
    }
    task_available_condition.notify_one();
    return flag;
  }

  template <typename F, typename... C>
  std::shared_ptr<std::atomic_flag>
  push_task(const F &task,
            const std::vector<std::shared_ptr<std::atomic_flag>> &conditions) {
    return push_task(0, task, conditions);
  }

  template <typename F>
  std::shared_ptr<std::atomic_flag>
  push_task(int priority, const F &task,
            const std::vector<std::shared_ptr<std::atomic_flag>> &conditions) {
    auto flag = std::make_shared<std::atomic_flag>();
    // std::vector<std::shared_ptr<std::atomic_flag>> conditions{conds...};
    if (all_set(conditions)) {
      // all conditions are already finished
      // so just ignore them and start a task as normal
      {
        const std::scoped_lock lock(tasks_mutex);
        tasks.push(
            std::make_tuple(priority, flag, std::function<void()>(task)));
        ++n_tasks;
      }
      task_available_condition.notify_one();
    } else {
      // this task cannot start yet, because it is waiting on another
      // put it in the waiting list
      {
        const std::scoped_lock lock(tasks_mutex);
        waiting_tasks.push_back(std::make_tuple(
            priority, flag, std::function<void()>(task), conditions));
        ++n_tasks;
      }
    }
    return flag;
  }


  template <typename F, typename... C>
  std::shared_ptr<std::atomic_flag> push_task(const F &task, C... conds)
  {
    return push_task(0, task, conds...);
  }


  template <typename F, typename... C>
  std::shared_ptr<std::atomic_flag> push_task(int priority, const F &task, C... conds)
  {
    std::vector<std::shared_ptr<std::atomic_flag>> conditions {conds...};
    return push_task(priority, task, conditions);
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
      task_available_condition.wait(lock, [&]{ return !tasks.empty() || !running; });
      if (running) {
        auto [priority, flag, task] = tasks.top();
        tasks.pop();
        lock.unlock();

        task();
        flag->test_and_set();

        lock.lock();
        --n_tasks;
        // check if any waiting tasks are now possible
        std::vector<int> activated;
        activated.reserve(waiting_tasks.size()); // maximum capacity
        for (size_t i = 0; i < waiting_tasks.size(); ++i) {
          if (all_set(std::get<3>(waiting_tasks[i]))) {
            activated.push_back(i);
            auto [priority, flag, task, flags] = waiting_tasks[i];
            tasks.push(std::make_tuple(priority, flag, task));
          }
          task_available_condition.notify_one();
        }
        // then remove the ones that were started from the waiting list
        auto last = --waiting_tasks.end();
        for (auto i: activated) {
          std::swap(waiting_tasks[i], *last);
          waiting_tasks.erase(last--);
        }
        lock.unlock();

        tasks_done_condition.notify_one();
      }
    }
  }

  int n_threads;
  std::unique_ptr<std::thread[]> threads;

  std::mutex tasks_mutex; // locks both tasks, waiting_tasks, and the n_tasks counter

  std::condition_variable task_available_condition;
  std::condition_variable tasks_done_condition;

  std::atomic<bool> running {true};
  int n_tasks {0}; // total number of waiting, queued, and running tasks

  std::priority_queue<std::tuple<int, std::shared_ptr<std::atomic_flag>, std::function<void()>>,
                      std::vector<std::tuple<int, std::shared_ptr<std::atomic_flag>, std::function<void()>>>,
                      compare_task_priority> tasks;

  std::vector<std::tuple<int,
                         std::shared_ptr<std::atomic_flag>,
                         std::function<void()>,
                         std::vector<std::shared_ptr<std::atomic_flag>>>> waiting_tasks;
};


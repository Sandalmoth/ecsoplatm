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
 * It also implements a waiting system where a pushed task can be required
 * to wait for some prior task to finish before entering the work queue.
 * This enables us to queue tasks that will sequentially modify the same data.
 */


class Flowpool {
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
  }

  template <typename F>
  int push_task(const F &task,
                std::vector<int> conds) {
    std::scoped_lock lock(tasks_mutex);
    tasks.push_back(task);
    conditions.push_back(conds);
    flags.emplace_back(WAITING);
    ++n_tasks;
    return total_tasks++;
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


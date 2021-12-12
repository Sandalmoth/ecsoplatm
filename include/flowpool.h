#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>


struct compare_task_priority {
  bool operator()(const std::pair<int, std::function<void()>> &a,
                  const std::pair<int, std::function<void()>> &b) {
    return a.first < b.first;
  }
};


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
    /* wait_for_tasks(); */
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
  void push_task(const F &task)
  {
    {
      const std::scoped_lock lock(tasks_mutex);
      tasks.push(std::make_pair(0, std::function<void()>(task)));
      ++n_tasks;
    }
    task_available_condition.notify_one();
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
        auto task = std::move(tasks.top().second);
        tasks.pop();
        lock.unlock();

        task();

        lock.lock();
        --n_tasks;
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

  std::priority_queue<std::pair<int, std::function<void()>>,
                      std::vector<std::pair<int, std::function<void()>>>,
                      compare_task_priority> tasks;

  std::vector<std::pair<int, std::function<void()>>> waiting_tasks;
};


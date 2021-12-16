#pragma once

#include "flowpool.h"
#include "interval_map.h"


namespace ecs {

  const uint32_t BLOCK_SIZE = 2;

  struct ComponentInterface {
    virtual void update() = 0;
    void destroy(uint32_t);

    std::vector<uint32_t> destroy_queue;
    IntervalMap<std::shared_ptr<std::atomic_flag>> waiting_flags;
  };


  template <typename T>
  struct Component : ComponentInterface {

    void create(uint32_t entity, T value) {
      create_queue.push_back(std::make_pair(entity, value));
    }

    void update() {
      // execute deferred destruction
      // first, the destroy queue needs to be sorted
      std::sort(destroy_queue.begin(), destroy_queue.end());
      // then they can be destroyed in reverse order
      auto last = --data.end();
      for (auto i: destroy_queue) {
        // swap to last, then erase (for speed!)
        std::swap(data[i], (*last));
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

  };


  struct Manager {
    uint32_t max_unused_id;
    int priority;
    Flowpool pool;
    std::vector<ComponentInterface *> components;
    std::vector<uint32_t> unused_ids;

    Manager();
    Manager(int n_threads);

    uint32_t get_id();
    void return_id(uint32_t);

    template <typename T> void enlist(Component<T> *);
    void update();
    void destroy(uint32_t);
    void wait();

    template <typename A>
    void apply(void (*f)(A &), Component<A> &a) {
      if (a.data.size() == 0)
        return; // no work to do

      int n = a.data.size() / BLOCK_SIZE + 1;
      int i = 0;
      while (i < a.data.size()) {
        int j = std::min(a.data.size(), static_cast<size_t>(i + BLOCK_SIZE));
        auto wait = a.waiting_flags.get(i, j);
        auto flag = pool.push_task(priority, [f,
                                  first = a.data.begin() + i,
                                  last = a.data.begin() + j]() {
          auto it = first;
          while (it != last) {
            f(it->second);
            ++it;
          }
        });
        a.waiting_flags.set(i, j, flag);
        i += BLOCK_SIZE;
      }

      ++priority;
    }

    template <typename A, typename B>
    void apply(void (*f)(A &, B &), Component<A> &a, Component<B> &b) {
      if ((a.data.size() == 0) || (b.data.size() == 0))
        return;

      int n = (a.data.size() + b.data.size())/BLOCK_SIZE/2 + 1;
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
            priority, [f,
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

      auto flag = pool.push_task(priority, [f, afirst = it_a, alast = a.data.end(),
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
      });

      a.waiting_flags.set(it_a - a.data.begin(),
                          a.data.size(),
                          flag);
      b.waiting_flags.set(it_b - b.data.begin(),
                          b.data.size(),
                          flag);
    }

  };


} // end namespace ecs


#ifdef ECSOPLATM_IMPLEMENTATION

void ecs::ComponentInterface::destroy(uint32_t id) {
  destroy_queue.push_back(id);
}

ecs::Manager::Manager()
  : max_unused_id(0)
  , priority(0) {}
ecs::Manager::Manager(int n_threads)
  : max_unused_id(0)
  , priority(0)
  , pool(n_threads) {}

uint32_t ecs::Manager::get_id() {
  uint32_t id = max_unused_id;
  if (!unused_ids.empty()) {
    id = unused_ids.back();
    unused_ids.pop_back();
  } else {
    ++max_unused_id;
  }
  return id;
}

void ecs::Manager::return_id(uint32_t id) {
  unused_ids.push_back(id);
}

template <typename T>
void ecs::Manager::enlist(Component<T> *component) {
  components.push_back(component);
}

void ecs::Manager::update() {
  for (auto c: components) {
    c->update();
  }
}

void ecs::Manager::destroy(uint32_t id) {
  for (auto c: components) {
    c->destroy(id);
  }
}

void ecs::Manager::wait() {
  pool.wait_for_tasks();
}

template <typename T>
std::ostream &operator<<(std::ostream &out, ecs::Component<T> &c) {
  out << '[';
  for (auto &[id, value]: c.data) {
    out << '(' << id << ' ' << value << ')';
  }
  out << ']';
  return out;
}

#endif // ECSOPLATM_IMPLEMENTATION


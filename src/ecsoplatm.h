#pragma once

#include "flowpool.h"
#include "interval_map.h"


namespace ecs {

  const uint32_t BLOCK_SIZE = 128;

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
        pool.push_task(priority, [f,
                                  first = a.data.begin() + i,
                                  last = a.data.begin() + std::min(a.data.size(),
                                                                   static_cast<size_t>(i + BLOCK_SIZE))]() {
          auto it = first;
          while (it != last) {
            f(it->second);
            ++it;
          }
        });
        i += BLOCK_SIZE;
      }

      ++priority;
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


#pragma once

#include "flowpool.h"


namespace ecs {


  struct ComponentInterface {
    virtual void update() = 0;
    void destroy(uint32_t);

    std::vector<uint32_t> destroy_queue;
  };


  template <typename T>
  struct Component : ComponentInterface {
    void update() {}
  };


  struct Manager {
    uint32_t max_unused_id;
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
  };


} // end namespace ecs


#ifdef ECSOPLATM_IMPLEMENTATION

void ecs::ComponentInterface::destroy(uint32_t id) {
  destroy_queue.push_back(id);
}

ecs::Manager::Manager()
  : max_unused_id(0) {}
ecs::Manager::Manager(int n_threads)
  : max_unused_id(0)
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

void ecs::Manager::return_id(uint32_t id) { unused_ids.push_back(id); }

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

#endif // ECSOPLATM_IMPLEMENTATION


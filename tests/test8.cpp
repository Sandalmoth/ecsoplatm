#include <iostream>
#include <chrono>

using namespace std::chrono_literals;

#include "ecsoplatm.h"


void foo(float &a, float &b) {
  // std::this_thread::sleep_for(100ms);
  a *= b;
  b -= a;
}

void bar(float &a) {
  // std::this_thread::sleep_for(100ms);
  a += 1.0f;
}


int main() {
  ecs::Manager ecs;

  ecs::Component<float> a;
  ecs::Component<float> b;
  ecs.enlist(&a, "a");
  ecs.enlist(&b, "b");

  for (int i = 0; i < 10; ++i) {
    a.create(i, static_cast<float>(i));
    b.create(i, static_cast<float>(i));
  }
  ecs.update();

  std::cout << a << std::endl;
  std::cout << b << std::endl;

  a.destroy(3);
  b.destroy(7);
  b.destroy(9);
  ecs.destroy(6);
  ecs.update();

  std::cout << a << std::endl;
  std::cout << b << std::endl;

  for (int i = 0; i < 10; ++i) {
    ecs.debug_print_entity_components(i);
  }

  std::cout << "applying bar to b" << std::endl;
  ecs.apply(&bar, b);
  std::cout << "applying foo to a, b" << std::endl;
  ecs.apply(&foo, a, b);
  std::cout << "applying bar to a" << std::endl;
  ecs.apply(&bar, a);
  std::cout << ecs.pool << std::endl;
  ecs.wait();

  std::cout << a << std::endl;
  std::cout << b << std::endl;
}

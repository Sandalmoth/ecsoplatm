#include <iostream>
#include <chrono>

using namespace std::chrono_literals;

#include "ecsoplatm.h"

// functions to be "applied"
// they are allowed to modify any of their arguments

void foo(float &a, float &b) {
  // std::this_thread::sleep_for(100ms);
  a *= b;
  b -= a;
}

void bar(float &a) {
  // std::this_thread::sleep_for(100ms);
  a += 1.0f;
}

// functions can take a void* to arbitrary data
// however since the apply step si parallel,
// be careful about how such data is handled (probably just read...)
void bar2(float &a, void *payload) {
  float *p = static_cast<float *>(payload);
  // std::this_thread::sleep_for(100ms);
  a += (*p);
}

void foobar(float &a, float &b, float &c) {
  // std::this_thread::sleep_for(100ms);
  c -= a + b;
}

int main() {
  ecs::Manager ecs;

  ecs::Component<float> a;
  ecs::Component<float> b;
  ecs::Component<float> c;
  ecs.enlist(&a, "a");
  ecs.enlist(&b, "b");
  ecs.enlist(&c, "c");

  for (int i = 0; i < 10; ++i) {
    a.create(i, static_cast<float>(i));
    b.create(i, static_cast<float>(i));
    c.create(i*i, static_cast<float>(i));
  }
  ecs.update();

  std::cout << a << std::endl;
  std::cout << b << std::endl;
  std::cout << c << std::endl;

  a.destroy(3);
  b.destroy(7);
  b.destroy(9);
  ecs.destroy(6);
  c.create(6, 66.6f);
  ecs.update(); // this actually executes all the creates and destroys

  std::cout << a << std::endl;
  std::cout << b << std::endl;
  std::cout << c << std::endl;

  for (int i = 0; i < 10; ++i) {
    ecs.debug_print_entity_components(i);
  }

  float bar2_payload = 3.0f;

  ecs.apply(&bar, b);
  ecs.apply(&foo, a, b);
  ecs.apply(&bar2, a, &bar2_payload);
  ecs.apply(&foobar, a, b, c);
  std::cout << ecs.pool << std::endl;
  ecs.wait();

  std::cout << a << std::endl;
  std::cout << b << std::endl;
  std::cout << c << std::endl;
}

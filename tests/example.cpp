#include "ecsoplatm.h"
#include <iostream>

void foo(float &a, float &b) {
  a += 1.0f;
  b -= a;
}

void bar(float &a) { a /= 2.0f; }

int main() {

  ecs::Manager ecs; // creates std::hardware_concurrency workers

  ecs::Component<float> a;
  ecs::Component<float> b;
  ecs.enlist(&a, "a");
  ecs.enlist(&b, "b");

  // create some entities
  // note that create/destroy just queues the creation for later
  // ecs.update() actually does the thing
  for (int i = 0; i < 4; ++i) {
    auto id = ecs.get_id();
    a.create(id, static_cast<float>(i));
    b.create(id, static_cast<float>(i));
  }
  ecs.update();

  a.destroy(2);
  ecs.update();

  std::cout << a << std::endl;
  std::cout << b << std::endl;
  // we now have
  // a - [(1 0)(3 2)(4 3)]
  // b - [(1 0)(2 1)(3 2)(4 3)]

  ecs.apply(a, b, &foo);
  ecs.apply(a, &bar); // will wait for foo
  ecs.wait();         // wait for all 'apply's to finish

  std::cout << a << std::endl;
  std::cout << b << std::endl;
  // we now have
  // a - [(1 0.5)(3 1.5)(4 2)]
  // b - [(1 -1)(2 1)(3 -1)(4 -1)]
}

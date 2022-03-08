# ecsoplatm
A simple arrays-for-everything ECS. The idea is that an entity is just a number, and components are each stored as vectors of `(entity_id, value)` pairs. A system can then take two (or more) such vectors, and apply some function to all pairs (or more) of `value`s that have the same `entity_id`.

This is then coupled with a thread pool and a per-value waiting setup so that many operations can be started, and they will all be executed in parallel and in the right order, with as little waiting as possible.

## Installation
It's just a single header, copy it wherever it's needed. It uses C++20 though, so there's that.

## Example
```C++
#include <iostream>
#include "ecsoplatm.h"

void foo(float &a, float &b) {
  a += 1.0f;
  b -= a;
}

void bar(float &a) {
  a /= 2.0f;
}

int main() {

  ecs::Manager ecs; // creates std::hardware_concurrency workers

  ecs::Component<float> a;
  ecs::Component<float> b;
  ecs.enlist(&a, "a"); // the string argument is just a name for debug printing
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

  ecs.apply(&foo, a, b);
  ecs.apply(&bar, a); // will wait for foo
  ecs.wait();         // wait for all 'apply's to finish

  std::cout << a << std::endl;
  std::cout << b << std::endl;
  // we now have
  // a - [(1 0.5)(3 1.5)(4 2)]
  // b - [(1 -1)(2 1)(3 -1)(4 -1)]
  
  // apply to all entities that have b but no a
  ecs.apply(b, &baz, a);
  ecs.wait();

  std::cout << a << std::endl;
  std::cout << b << std::endl;
  // we now have
  // a - [(1 0.5)(3 1.5)(4 2)]
  // b - [(1 -1)(2 2)(3 -1)(4 -1)]
}
```

Checkout the tests folder for a bit more code examples.

## Preemptive Q&A
__Is this better than ...?__  
Probably not.

__Should I use this for ...?__  
You could, and I might, but it's really more intended as an example and a demo of an idea.

__Does it even work?__  
I think so, but no promises.


#define ECSOPLATM_IMPLEMENTATION
#include "ecsoplatm.h"


void foo(float &a, float &b) {
  a *= b;
  b -= a;
}

void bar(float &a) {
  a += 1.0f;
}


int main() {
  ecs::Manager ecs;
  ecs::Component<float> a;
  ecs::Component<float> b;
  ecs.enlist(&a);
  ecs.enlist(&b);

  for (int i = 0; i < 10; ++i) {
    a.create(i, static_cast<float>(i));
    b.create(i, static_cast<float>(i));
  }
  ecs.update();

  std::cout << a << std::endl;
  std::cout << b << std::endl;

  a.destroy(3);
  b.destroy(8); // FIXME not all numbers work
  ecs.destroy(6);
  // ecs.destroy(99);
  ecs.update();

  std::cout << a << std::endl;
  std::cout << b << std::endl;

  ecs.apply(&bar, b);
  ecs.wait();

  std::cout << a << std::endl;
  std::cout << b << std::endl;
}

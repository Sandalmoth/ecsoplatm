#define ECSOPLATM_IMPLEMENTATION
#include "ecsoplatm.h"


void foo(float &a, float &b) {
  std::cout << "foo on " << a << ' ' << b << std::endl;
  a *= b;
  b -= a;
}

void bar(float &a) {
  std::cout << "bar on " << a << std::endl;
  a += 1.0f;
}


int main() {
  ecs::Manager ecs(1);

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
  ecs.update();

  std::cout << a << std::endl;
  std::cout << b << std::endl;

  ecs.apply(&bar, b);
  // ecs.wait(); // this fixes the segfault, but is not desirable
  ecs.apply(&foo, a, b);
  ecs.wait();

  std::cout << a << std::endl;
  std::cout << b << std::endl;
}

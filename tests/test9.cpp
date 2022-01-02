#include <iostream>

#include "ecsoplatm.h"

void t1(int &a) {
  ++a;
}

void t1_p(int &a, void *p) {
  a += *static_cast<int *>(p);
}

void t2(int &a, int &b) {
  ++a;
  ++b;
}

void t2_p(int &a, int &b, void *p) {
  a += *static_cast<int *>(p);
  b += *static_cast<int *>(p);
}

void t3(int &a, int &b, int &c) {
  ++a;
  ++b;
  ++c;
}

void t3_p(int &a, int &b, int &c, void *p) {
  a += *static_cast<int *>(p);
  b += *static_cast<int *>(p);
  c += *static_cast<int *>(p);
}

int main() {
  ecs::Manager ecs;

  ecs::Component<int> a;
  ecs::Component<int> b;
  ecs::Component<int> c;
  ecs.enlist(&a, "a");
  ecs.enlist(&b, "b");
  ecs.enlist(&c, "c");

  for (int i = 0; i < 8; ++i) {
    auto id = ecs.get_id();
    a.create(id, i);
    b.create(id, i);
    c.create(id, i);
  }
  ecs.update();

  std::cout << a << std::endl;
  std::cout << b << std::endl;
  std::cout << c << std::endl;
  std::cout << std::endl;

  a.destroy(2);
  b.destroy(3);
  c.destroy(4);
  ecs.destroy(5);
  ecs.return_id(5);
  a.create(12, 144);
  b.create(13, 169);
  c.create(14, 196);
  ecs.update();

  std::cout << a << std::endl;
  std::cout << b << std::endl;
  std::cout << c << std::endl;
  std::cout << std::endl;

  int payload = -1;

  ecs.apply(&t1, a);
  ecs.wait();
  std::cout << a << std::endl;
  ecs.apply(&t1_p, a, &payload);
  ecs.wait();
  std::cout << a << std::endl;
  std::cout << std::endl;

  ecs.apply(&t2, a, b);
  ecs.wait();
  std::cout << a << std::endl;
  std::cout << b << std::endl;
  ecs.apply(&t2_p, a, b, &payload);
  ecs.wait();
  std::cout << a << std::endl;
  std::cout << b << std::endl;
  std::cout << std::endl;

  ecs.apply(&t3, a, b, c);
  ecs.wait();
  std::cout << a << std::endl;
  std::cout << b << std::endl;
  std::cout << c << std::endl;
  ecs.apply(&t3_p, a, b, c, &payload);
  ecs.wait();
  std::cout << a << std::endl;
  std::cout << b << std::endl;
  std::cout << c << std::endl;
  std::cout << std::endl;

  for (int i = 0; i < 16; ++i) {
    ecs.debug_print_entity_components(i);
  }
  std::cout << std::endl;

  for (int i = 0; i < 3; ++i) {
    auto id = ecs.get_id();
    a.create(id, i);
    b.create(id, i);
    c.create(id, i);
  }
  ecs.update();

  for (int i = 0; i < 16; ++i) {
    ecs.debug_print_entity_components(i);
  }
}

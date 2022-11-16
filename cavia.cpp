#include <iostream>

struct X {
  static void fun_S() { std::cout << __FUNCTION__ << '\n'; }
  void fun_A() { std::cout << __FUNCTION__ << '\n'; }
};

void func1() { std::cout << __FUNCTION__ << '\n'; }
void func2() { std::cout << __FUNCTION__ << '\n'; }

int main()
{
  X::fun_S();
  X x;
  x.fun_A();
  func1();
  func2();
}

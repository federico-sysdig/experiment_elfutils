#include "symbol_resolver.h"
#include <iostream>

int main(int argc, char* argv[])
{
  std::cout << "filename: " << argv[1] << '\n';

  symbol_resolver R(argv[1]);

  for(int i = 2; i < argc; ++i) {
    uintptr_t addr = std::stoul(argv[i], nullptr, 16);

    std::string symbol;
    int res = R.resolve(addr, symbol);

    std::cout << "  " << (void*)addr << " - " << symbol << '\n';

    if(res != 0)
      return res;
  }

  return 0;
}

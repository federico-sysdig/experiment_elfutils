
#include <backtrace.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

static void error_callback_create(void* data, const char* message, int error_number)
{
  if(error_number == -1) {
    std::cerr << "If you want backtraces, you have to compile with -g\n\n";
  }
  else {
    fprintf(stderr, "Backtrace error %d: %s\n", error_number, message);
  }
}

static void error_callback(void* data, const char* message, int error_number)
{
  if(error_number == -1) {
    std::cerr << "If you want backtraces, you have to compile with -g\n\n";
  }
  else {
    fprintf(stderr, "Backtrace error %d: %s\n", error_number, message);
  }
}

static void full_callback(void* data, uintptr_t pc, const char* symname, uintptr_t symval, uintptr_t symsize)
{
  auto ptr = reinterpret_cast<std::map<uintptr_t, std::string>*>(data);

  if(symname) {
    ptr->emplace(pc, symname);
  }
  else
    ptr->emplace(pc, "<unknown>");
}

char binary_path[1024];

int main(int argc, char* argv[])
{
  std::cout << "filename: " << argv[1] << '\n';
  std::cout << "address: " << argv[2] << '\n';

  strcpy(binary_path, argv[1]);
  backtrace_state* state = backtrace_create_state(binary_path, 0, error_callback_create, nullptr);

  //uintptr_t addr = atoi(argv[2]);
  uintptr_t addr = std::stoul(argv[2], nullptr, 16);

  std::cout << "address: " << (void*)addr << '\n';

  std::map<uintptr_t, std::string> symbols;
  backtrace_syminfo(state, addr, full_callback, error_callback, &symbols);

  for(auto& el : symbols)
  {
    std::cout << "pc: " << std::hex << el.first << " - symbol: " << el.second << '\n';
  }
}

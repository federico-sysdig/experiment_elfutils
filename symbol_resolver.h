//

#pragma once

#include <dwarf.h>
#include <libdwfl.h>

#include <cinttypes>
#include <string>
#include <vector>

//int resolve_symbols(const std::string& fname, const std::vector<uintptr_t>& addrs);

struct Dwfl;
struct Dwfl_Module;

class symbol_resolver
{
public:
  symbol_resolver(const std::string& fname);
  ~symbol_resolver();

  int resolve(uintptr_t addrs, std::string& symbol);

private:
  int handle_address(const char* string, std::string& symbol);
  const char* symname(const char* name);
  bool print_dwarf_function(Dwfl_Module* mod, Dwarf_Addr addr);
  void print_addrsym(Dwfl_Module* mod, GElf_Addr addr, std::string& symbol);
  bool adjust_to_section(const char* name, uintmax_t* addr);

  Dwfl* m_dwfl = nullptr;
  size_t demangle_buffer_len = 0;
  char* demangle_buffer = nullptr;
};

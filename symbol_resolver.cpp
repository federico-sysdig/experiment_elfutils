// Locate source files and line information for given addresses

#include "symbol_resolver.h"

#include <argp.h>
#include <dwarf.h>
#include <libdwfl.h>
#include <libintl.h>

#include <cassert>
#include <cinttypes>
#include <cinttypes>
#include <cstdbool>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cxxabi.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <locale.h>
#include <sstream>
#include <stdexcept>
#include <stdio_ext.h>
#include <unistd.h>

// Definitions of arguments for argp functions.
static const argp_option options[] =
{
  { nullptr, 0, nullptr, 0, "Input format options:", 2 },
  { "section", 'j', "NAME", 0, "Treat addresses as offsets relative to NAME section.", 0 },
  { nullptr, 0, nullptr, 0, "Output format options:", 3 },
  { "addresses", 'a', nullptr, 0, "Print address before each entry", 0 },
  { "basenames", 's', nullptr, 0, "Show only base names of source files", 0 },
  { "absolute", 'A', nullptr, 0, "Show absolute file names using compilation directory", 0 },
  { "flags", 'F', nullptr, 0, "Also show line table flags", 0 },
  { "inlines", 'i', nullptr, 0, "Show all source locations that caused inline expansion of subroutines at the address.", 0 },
  { nullptr, 0, nullptr, 0, "Miscellaneous:", 0 },
  { nullptr, 0, nullptr, 0, nullptr, 0 }
};

// Short description of program.
//static const char doc[] = "Locate source files and line information for ADDRs (in a.out by default).";

// Strings for arguments in help texts.
//static const char args_doc[] = "[ADDR...]";

// Prototype for option handler.
static error_t parse_opt(int key, char* arg, argp_state* state);

static argp_child argp_children[2]; // [0] is set in main.

// Data structure to communicate with argp functions.
static const argp argp =
{
  //options, parse_opt, args_doc, doc, argp_children, nullptr, nullptr
  options, parse_opt, nullptr, nullptr, argp_children, nullptr, nullptr
};

static bool print_addresses;             // True when we should print the address for each entry.
static bool only_basenames;              // True if only base names of files should be shown.
static bool use_comp_dir;                // True if absolute file names based on DW_AT_comp_dir should be shown.
static bool show_flags;                  // True if line flags should be shown.
static bool show_functions = true;       // True if function names should be shown.
static bool show_symbols = true;         // True if ELF symbol or section info should be shown.
static bool show_symbol_sections = true; // True if section associated with a symbol address should be shown.
static const char* just_section;         // If non-null, take address parameters as relative to named section.
static bool show_inlines;                // True if all inlined subroutines of the current address should be shown.
static bool demangle = true;             // True if all names need to be demangled.
static bool pretty = true;               // True if all information should be printed on one line.

symbol_resolver::symbol_resolver(const std::string& fname)
{
  char buff0[256];
  char buff1[256];
  char buff2[256];
  strcpy(buff0, "exe");
  strcpy(buff1, "-e");
  strcpy(buff2, fname.c_str());

  const int argc = 3;
  char* argv[] = { buff0, buff1, buff2 };

  // We use no threads here which can interfere with handling a stream.
  __fsetlocking(stdout, FSETLOCKING_BYCALLER);

  // Parse and process arguments.  This includes opening the modules.
  argp_children[0].argp = dwfl_standard_argp();
  argp_children[0].group = 1;

  int remaining;
  argp_parse(&argp, argc, argv, 0, &remaining, &m_dwfl);
  assert(m_dwfl);
}

symbol_resolver::~symbol_resolver()
{
  dwfl_end(m_dwfl);
  free(demangle_buffer);
}

int symbol_resolver::resolve(uintptr_t addr, std::string& symbol)
{
  std::ostringstream os;
  os << std::hex << addr;
  return handle_address(os.str().c_str(), symbol);
}

const char* symbol_resolver::symname(const char* name)
{
  // Require GNU v3 ABI by the "_Z" prefix.
  if(demangle && name[0] == '_' && name[1] == 'Z')
  {
    int status = -1;
    char* dsymname = __cxxabiv1::__cxa_demangle(name, demangle_buffer,
                                                &demangle_buffer_len, &status);
    if(status == 0)
      name = demangle_buffer = dsymname;
  }

  return name;
}

// Handle program arguments
static error_t parse_opt(int key, char* arg, argp_state* state)
{
  switch(key)
  {
  case ARGP_KEY_INIT:
    state->child_inputs[0] = state->input;
    break;

  case 'a':
    print_addresses = true;
    break;

  case 's':
    only_basenames = true;
    break;

  case 'A':
    use_comp_dir = true;
    break;

  case 'F':
    show_flags = true;
    break;

  case 'j':
    just_section = arg;
    break;

  case 'i':
    show_inlines = true;
    break;

  default:
    return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static const char* get_diename(Dwarf_Die* die)
{
  Dwarf_Attribute attr;
  const char* name =
    dwarf_formstring(dwarf_attr_integrate(die, DW_AT_MIPS_linkage_name, &attr)
               ?: dwarf_attr_integrate(die, DW_AT_linkage_name, &attr));

  if(!name)
    name = dwarf_diename(die) ?: "??";

  return name;
}

bool symbol_resolver::print_dwarf_function(Dwfl_Module* mod, Dwarf_Addr addr)
{
  Dwarf_Addr bias = 0;
  Dwarf_Die* cudie = dwfl_module_addrdie(mod, addr, &bias);

  Dwarf_Die* scopes;
  int nscopes = dwarf_getscopes(cudie, addr - bias, &scopes);
  if(nscopes <= 0)
    return false;

  bool res = false;
  for(int i = 0; i < nscopes; ++i)
    switch(dwarf_tag(&scopes[i]))
    {
      case DW_TAG_subprogram:
      {
        const char* name = get_diename(&scopes[i]);
        if(!name)
          goto done;
        //printf("%s%c", symname(name), pretty ? ' ' : '\n');
        res = true;
        goto done;
      }

      case DW_TAG_inlined_subroutine:
      {
        const char* name = get_diename(&scopes[i]);
        if(!name)
          goto done;

        // When using --pretty-print we only show inlines on their own line.
        // Just print the first subroutine name.
        if(pretty)
        {
          //printf("%s ", symname(name));
          res = true;
          goto done;
        }
        else {
          //printf("%s inlined", symname(name));
        }

        Dwarf_Files* files;
        if(dwarf_getsrcfiles(cudie, &files, nullptr) == 0)
        {
          Dwarf_Attribute attr_mem;
          Dwarf_Word val;
          if(dwarf_formudata(dwarf_attr(&scopes[i],
                          DW_AT_call_file,
                          &attr_mem), &val) == 0)
          {
            const char* file = dwarf_filesrc(files, val, nullptr, nullptr);
            unsigned int lineno = 0;
            unsigned int colno = 0;
            if(dwarf_formudata(dwarf_attr(&scopes[i], DW_AT_call_line, &attr_mem), &val) == 0)
              lineno = val;
            if(dwarf_formudata(dwarf_attr(&scopes[i], DW_AT_call_column, &attr_mem), &val) == 0)
              colno = val;

            const char* comp_dir = "";
            const char* comp_dir_sep = "";

            if(!file)
              file = "???";
            else if(only_basenames)
              file = basename(file);
            else if(use_comp_dir && file[0] != '/')
            {
              const char* const* dirs;
              size_t ndirs;
              if(dwarf_getsrcdirs(files, &dirs, &ndirs) == 0 && dirs[0] != nullptr)
              {
                comp_dir = dirs[0];
                comp_dir_sep = "/";
              }
            }

            if(lineno == 0) {
              //printf(" from %s%s%s", comp_dir, comp_dir_sep, file);
            }
            else if(colno == 0) {
              //printf(" at %s%s%s:%u", comp_dir, comp_dir_sep, file, lineno);
            }
            else {
              //printf(" at %s%s%s:%u:%u", comp_dir, comp_dir_sep, file, lineno, colno);
            }
          }
        }
        //printf(" in ");
        continue;
      }
    }

done:
  free(scopes);
  return res;
}

void symbol_resolver::print_addrsym(Dwfl_Module* mod, GElf_Addr addr, std::string& symbol)
{
  GElf_Sym s;
  GElf_Off off;
  const char* name = dwfl_module_addrinfo(mod, addr, &off, &s, nullptr, nullptr, nullptr);
  if(!name)
  {
    // No symbol name.  Get a section name instead.
    int i = dwfl_module_relocate_address(mod, &addr);
    if(i >= 0)
      name = dwfl_module_relocation_info(mod, i, nullptr);

    if(!name) {
      //printf("??%c", pretty ? ' ': '\n');
    }
    else {
      //printf("(%s)+%#" PRIx64 "%c", name, addr, pretty ? ' ' : '\n');
    }
  }
  else
  {
    name = symname(name);
    if(off == 0) {
      symbol.assign(name);
      //printf("%s", name);
    }
    else {
      //printf("%s+%#" PRIx64 "", name, off);
    }

    // Also show section name for address.
    if(show_symbol_sections)
    {
      Dwarf_Addr ebias;
      Elf_Scn* scn = dwfl_module_address_section(mod, &addr, &ebias);
      if(scn)
      {
        GElf_Shdr shdr_mem;
        GElf_Shdr* shdr = gelf_getshdr(scn, &shdr_mem);
        if(shdr)
        {
          Elf* elf = dwfl_module_getelf(mod, &ebias);
          size_t shstrndx;
          if(elf_getshdrstrndx(elf, &shstrndx) >= 0) {
            //printf(" (%s)", elf_strptr(elf, shstrndx, shdr->sh_name));
          }
        }
      }
    }
    //printf("%c", pretty ? ' ' : '\n');
  }
}

static int see_one_module(Dwfl_Module* mod,
                          void** userdata,
                          const char* name,
                          Dwarf_Addr start,
                          void* arg)
{
  auto result = reinterpret_cast<Dwfl_Module**>(arg);
  if(*result)
    return DWARF_CB_ABORT;
  *result = mod;
  return DWARF_CB_OK;
}

static int find_symbol(Dwfl_Module* mod, void** userdata, const char* name, Dwarf_Addr start, void* arg)
{
  auto looking_for = reinterpret_cast<const char*>(((void**) arg)[0]);
  auto symbol = reinterpret_cast<GElf_Sym*>(((void**) arg)[1]);
  auto value = reinterpret_cast<GElf_Addr*>(((void**) arg)[2]);

  int n = dwfl_module_getsymtab(mod);
  for(int i = 1; i < n; ++i)
  {
    const char* symbol_name =
      dwfl_module_getsym_info(mod, i, symbol, value, nullptr, nullptr, nullptr);

    if(!symbol_name || symbol_name[0] == '\0')
      continue;

    switch(GELF_ST_TYPE(symbol->st_info))
    {
    case STT_SECTION:
    case STT_FILE:
    case STT_TLS:
      break;
    default:
      if(!strcmp(symbol_name, looking_for))
        {
          ((void**)arg)[0] = nullptr;
          return DWARF_CB_ABORT;
        }
    }
  }

  return DWARF_CB_OK;
}

bool symbol_resolver::adjust_to_section(const char* name, uintmax_t* addr)
{
  // It was (section)+offset.  This makes sense if there is only one module to look in for a section.
  Dwfl_Module* mod = nullptr;
  if(dwfl_getmodules(m_dwfl, &see_one_module, &mod, 0) != 0 || !mod)
    throw std::runtime_error("Section syntax requires exactly one module");

  int nscn = dwfl_module_relocations(mod);
  for(int i = 0; i < nscn; ++i)
  {
    GElf_Word shndx;
    const char* scn = dwfl_module_relocation_info(mod, i, &shndx);
    if(!scn) // [[unlikely]]
      break;
    if(!strcmp(scn, name))
    {
      // Found the section.
      GElf_Shdr shdr_mem;
      GElf_Addr shdr_bias;
      GElf_Shdr* shdr = gelf_getshdr(elf_getscn(dwfl_module_getelf(mod, &shdr_bias), shndx), &shdr_mem);
      if(!shdr) // [[unlikely]]
        break;

      if(*addr >= shdr->sh_size) {
        char str[100];
        sprintf(str, "offset %#" PRIxMAX " lies outside section '%s'", *addr, scn);
        throw std::runtime_error(str);
      }

      *addr += shdr->sh_addr + shdr_bias;
      return true;
    }
  }

  return false;
}

static void print_src(const char* src, int lineno, int linecol, Dwarf_Die* cu)
{
  const char* comp_dir = "";
  const char* comp_dir_sep = "";

  if(only_basenames)
    src = basename(src);
  else if(use_comp_dir && src[0] != '/')
  {
    Dwarf_Attribute attr;
    comp_dir = dwarf_formstring(dwarf_attr(cu, DW_AT_comp_dir, &attr));
    if(comp_dir)
      comp_dir_sep = "/";
  }

  if(linecol != 0) {
    //printf("%s%s%s:%d:%d", comp_dir, comp_dir_sep, src, lineno, linecol);
  }
  else {
    //printf("%s%s%s:%d", comp_dir, comp_dir_sep, src, lineno);
  }
}

static int get_addr_width(Dwfl_Module* mod)
{
  // Try to find the address width if possible.
  static int width = 0;
  if(width == 0 && mod != nullptr)
  {
    Dwarf_Addr bias;
    Elf* elf = dwfl_module_getelf(mod, &bias);
    if(elf != nullptr)
    {
      GElf_Ehdr ehdr_mem;
      GElf_Ehdr* ehdr = gelf_getehdr(elf, &ehdr_mem);
      if(ehdr != nullptr)
        width = ehdr->e_ident[EI_CLASS] == ELFCLASS32 ? 8 : 16;
    }
  }
  if(width == 0)
    width = 16;

  return width;
}

static inline void show_note(int (*get)(Dwarf_Line*, bool*), Dwarf_Line* info, const char* note)
{
  bool flag;
  if((*get)(info, &flag) == 0 && flag) {
    //fputs(note, stdout);
  }
}

static inline void show_int(int(*get)(Dwarf_Line*, unsigned int*), Dwarf_Line* info, const char* name)
{
  unsigned int val;
  if((*get)(info, &val) == 0 && val != 0) {
    //printf(" (%s %u)", name, val);
  }
}

int symbol_resolver::handle_address(const char* addr_str, std::string& symbol)
{
  char* endp;
  uintmax_t addr = strtoumax(addr_str, &endp, 16);
  if(endp == addr_str || *endp != '\0')
  {
    bool parsed = false;
    int i, j;
    char* name = nullptr;

    if(sscanf(addr_str, "(%m[^)])%" PRIiMAX "%n", &name, &addr, &i) == 2 && addr_str[i] == '\0')
      parsed = adjust_to_section(name, &addr);

    switch(sscanf(addr_str, "%m[^-+]%n%" PRIiMAX "%n", &name, &i, &addr, &j))
    {
    default:
      break;
    case 1:
      addr = 0;
      j = i;
      //[[fallthrough]]
    case 2:
      if(addr_str[j] != '\0')
        break;

      // It was symbol[+offset].
      GElf_Sym sym;
      GElf_Addr value = 0;
      void* arg[3] = { name, &sym, &value };
      dwfl_getmodules(m_dwfl, &find_symbol, arg, 0);
      if(arg[0]) {
        char str[100];
        sprintf(str, "cannot find symbol '%s'", name);
        throw std::runtime_error(str);
      }
      else
      {
        if(sym.st_size != 0 && addr >= sym.st_size) {
          char str[100];
          sprintf(str, "offset %#" PRIxMAX " lies outside contents of '%s'", addr, name);
          throw std::runtime_error(str);
        }
        addr += value;
        parsed = true;
      }
      break;
    }

    free(name);
    if(!parsed)
      return 1;
  }
  else if(just_section && !adjust_to_section(just_section, &addr))
    return 1;

  Dwfl_Module* mod = dwfl_addrmodule(m_dwfl, addr);

  if(print_addresses)
  {
    int width = get_addr_width(mod);
    //printf("0x%.*" PRIx64 "%s", width, addr, pretty ? ": " : "\n");
  }

  if(show_functions)
  {
    // First determine the function name.  Use the DWARF information if possible.
    if(!print_dwarf_function(mod, addr) && !show_symbols)
    {
      const char* name = dwfl_module_addrname(mod, addr);
      name = name ? symname(name) : "??";
      symbol.assign(name);
      //printf("%s%c", name, pretty ? ' ' : '\n');
    }
  }

  if(show_symbols)
    print_addrsym(mod, addr, symbol);

  if((show_functions || show_symbols) && pretty) {
    //printf("at ");
  }

  Dwfl_Line* line = dwfl_module_getsrc(mod, addr);

  const char* src;
  int lineno, linecol;

  if(line && (src = dwfl_lineinfo(line, &addr, &lineno, &linecol, nullptr, nullptr)) != nullptr)
  {
    print_src(src, lineno, linecol, dwfl_linecu(line));
    if(show_flags)
    {
      Dwarf_Addr bias;
      Dwarf_Line* info = dwfl_dwarf_line(line, &bias);
      assert(info);

      show_note(&dwarf_linebeginstatement, info, " (is_stmt)");
      show_note(&dwarf_lineblock, info, " (basic_block)");
      show_note(&dwarf_lineprologueend, info, " (prologue_end)");
      show_note(&dwarf_lineepiloguebegin, info, " (epilogue_begin)");
      show_int(&dwarf_lineisa, info, "isa");
      show_int(&dwarf_linediscriminator, info, "discriminator");
    }
    //putchar('\n');
  }
  else {
    //puts("??:0");
  }

  if(show_inlines)
  {
    Dwarf_Addr bias = 0;
    Dwarf_Die* cudie = dwfl_module_addrdie(mod, addr, &bias);

    Dwarf_Die* scopes = nullptr;
    int nscopes = dwarf_getscopes(cudie, addr - bias, &scopes);
    if(nscopes < 0)
      return 1;

    if(nscopes > 0)
    {
      Dwarf_Die subroutine;
      Dwarf_Off dieoff = dwarf_dieoffset(&scopes[0]);
      dwarf_offdie(dwfl_module_getdwarf(mod, &bias), dieoff, &subroutine);
      free(scopes);
      scopes = nullptr;

      nscopes = dwarf_getscopes_die(&subroutine, &scopes);
      if(nscopes > 1)
      {
        Dwarf_Die cu;
        Dwarf_Files* files;
        if(dwarf_diecu(&scopes[0], &cu, nullptr, nullptr)
           && dwarf_getsrcfiles(cudie, &files, nullptr) == 0)
        {
          for(int i = 0; i < nscopes - 1; i++)
          {
            Dwarf_Word val;
            Dwarf_Attribute attr;
            Dwarf_Die* die = &scopes[i];
            if(dwarf_tag(die) != DW_TAG_inlined_subroutine)
              continue;

            if(pretty) {
              //printf(" (inlined by) ");
            }

            if(show_functions)
            {
              // Search for the parent inline or function. It might not be directly above this inline
              // -- e.g. there could be a lexical_block in between.
              for(int j = i + 1; j < nscopes; j++)
              {
                Dwarf_Die* parent = &scopes[j];
                int tag = dwarf_tag(parent);
                if(tag == DW_TAG_inlined_subroutine || tag == DW_TAG_entry_point || tag == DW_TAG_subprogram)
                {
                  //printf("%s%s", symname(get_diename(parent)), pretty ? " at " : "\n");
                  break;
                }
              }
            }

            src = nullptr;
            lineno = 0;
            linecol = 0;
            if(dwarf_formudata(dwarf_attr(die, DW_AT_call_file, &attr), &val) == 0)
              src = dwarf_filesrc(files, val, nullptr, nullptr);

            if(dwarf_formudata(dwarf_attr(die, DW_AT_call_line, &attr), &val) == 0)
              lineno = val;

            if(dwarf_formudata(dwarf_attr(die, DW_AT_call_column, &attr), &val) == 0)
              linecol = val;

            if(src)
            {
              print_src(src, lineno, linecol, &cu);
              //putchar('\n');
            }
            else {
              //puts("??:0");
            }
          }
        }
      }
    }
    free(scopes);
  }

  return 0;
}

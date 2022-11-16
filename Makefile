# Makefile

ELFUTILS_ROOT=/code/agent/dependencies/elfutils

INCLUDES=-I$(ELFUTILS_ROOT) \
    -I$(ELFUTILS_ROOT)/lib \
    -I$(ELFUTILS_ROOT)/libasm \
    -I$(ELFUTILS_ROOT)/libcpu \
    -I$(ELFUTILS_ROOT)/libdw \
    -I$(ELFUTILS_ROOT)/libdwelf \
    -I$(ELFUTILS_ROOT)/libdwfl \
    -I$(ELFUTILS_ROOT)/libebl \
    -I$(ELFUTILS_ROOT)/libelf

LDFLAGS=-std=gnu99 -Wall -Wshadow -Wformat=2 -Wold-style-definition -Wstrict-prototypes -Wtrampolines -Wlogical-op -Wduplicated-cond -Wnull-dereference -Wimplicit-fallthrough=5 -Werror -Wunused -Wextra -Wstack-usage=262144 -Wno-error=stack-usage=

LIBS=-Wl,-rpath-link,$(ELFUTILS_ROOT)/libelf:$(ELFUTILS_ROOT)/libdw $(ELFUTILS_ROOT)/libdw/libdw.so $(ELFUTILS_ROOT)/libelf/libelf.so $(ELFUTILS_ROOT)/lib/libeu.a

prova_elfutils: prova_elfutils.o
	g++ -o $@ $< -O2 $(LIBS)

prova_elfutils.o: prova_elfutils.cpp
	g++ $(CFLAGS) $(INCLUDES) -c $<

clean:
	$(RM) prova_elfutils prova_elfutils.o

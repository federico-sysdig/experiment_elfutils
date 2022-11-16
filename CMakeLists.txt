cmake_minimum_required(VERSION 3.18)

project(prova_libbacktrace)

#[[
add_executable(prova_libbacktrace
  prova_libbacktrace.cpp
)

#message("VCPKG_INSTALLED_DIR: ${VCPKG_INSTALLED_DIR}")

target_include_directories(prova_libbacktrace
PRIVATE
  ${VCPKG_INSTALLED_DIR}/x64-linux/include
)

target_link_libraries(prova_libbacktrace
PRIVATE
  ${VCPKG_INSTALLED_DIR}/x64-linux/lib/libbacktrace.a
)
#]]
# ----

add_library(symbol_resolver STATIC
  symbol_resolver.cpp
)

#set(ELFUTILS_ROOT "/home/vagrant")
set(ELFUTILS_ROOT "/code/agent/dependencies/elfutils")

target_include_directories(symbol_resolver
PUBLIC
  ${ELFUTILS_ROOT}/libdwfl
  ${ELFUTILS_ROOT}/libdw
PRIVATE
  ${ELFUTILS_ROOT}
  ${ELFUTILS_ROOT}/src
  ${ELFUTILS_ROOT}/lib
  ${ELFUTILS_ROOT}/libasm
  #${ELFUTILS_ROOT}/libcpu
  ${ELFUTILS_ROOT}/libdwelf
  ${ELFUTILS_ROOT}/libebl
  ${ELFUTILS_ROOT}/libelf
)

target_compile_definitions(symbol_resolver
PRIVATE
  #-D_GNU_SOURCE
  #-DHAVE_CONFIG_H
  #-DLOCALEDIR='"/usr/local/share/locale"'
  #-DDEBUGPRED=0
  #-DSRCDIR=\"/code/agent/dependencies/elfutils/src\"
  #-DOBJDIR=\"/code/agent/dependencies/elfutils/src\"
  #-D_FORTIFY_SOURCE=2
)

#[[
-I. -I.. -I../lib -I./../libelf -I./../libebl -I./../libdw -I./../libdwelf -I./../libdwfl -lrt -I./../libasm
-std=gnu99 -Wall -Wshadow -Wformat=2 -Wold-style-definition -Wstrict-prototypes -Wtrampolines -Wlogical-op -Wduplicated-cond -Wnull-dereference -Wimplicit-fallthrough=5 -Werror -Wunused -Wextra -Wstack-usage=262144
 -g -O2 -MT addr2line.o -MD -MP -MF
.deps/addr2line.Tpo -c -o addr2line.o addr2line.c
mv -f .deps/addr2line.Tpo .deps/addr2line.Po
#]]

# -Wl,-rpath-link,../libelf:../libdw
#target_link_options(prova_elfutils LINKER:-rpath-link,../libelf:../libdw)
target_link_directories(symbol_resolver PRIVATE ../libelf ../libdw)

target_link_libraries(symbol_resolver
PRIVATE
  ${ELFUTILS_ROOT}/libdw/libdw.so
  ${ELFUTILS_ROOT}/libelf/libelf.so
  ${ELFUTILS_ROOT}/lib/libeu.a
)

add_executable(prova_symbolresolver prova_symbolresolver.cpp)

target_link_libraries(prova_symbolresolver symbol_resolver rt)

#[[
autoreconf -i -f
./configure --disable-debuginfod --enable-libdebuginfod=dummy --enable-maintainer-mode
make && make check

nm -an ./build/cavia
#]]

# ----

add_executable(cavia cavia.cpp)

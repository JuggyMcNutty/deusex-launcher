# Cross-compile C11 for generic aarch64 Linux: the launcher.
#
# On an aarch64 machine this file does nothing and the build is native, so
# the same preset works both ways.
#
# Bootlin stable-2020.08-1 (GCC 9.3, glibc 2.31) -- shared with the TrimUI
# port. Its glibc is the floor the binary needs, so an old one reaches the
# most distros. SDL2 comes from deps/sysroots/linux-aarch64 (port.sh).
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    return()
endif()

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

get_filename_component(DXL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(A64_TOOLCHAIN "${DXL_ROOT}/deps/toolchains/aarch64--glibc--stable-2020.08-1")
set(A64_SYSROOT   "${DXL_ROOT}/deps/sysroots/linux-aarch64")

if(NOT EXISTS "${A64_TOOLCHAIN}/bin/aarch64-linux-gcc")
    message(FATAL_ERROR "toolchain missing -- run scripts/dx.sh deps linux-aarch64")
endif()

set(CMAKE_C_COMPILER   "${A64_TOOLCHAIN}/bin/aarch64-linux-gcc")
set(CMAKE_CXX_COMPILER "${A64_TOOLCHAIN}/bin/aarch64-linux-g++")
set(CMAKE_AR           "${A64_TOOLCHAIN}/bin/aarch64-linux-ar"      CACHE FILEPATH "")
set(CMAKE_RANLIB       "${A64_TOOLCHAIN}/bin/aarch64-linux-ranlib"  CACHE FILEPATH "")
set(CMAKE_STRIP        "${A64_TOOLCHAIN}/bin/aarch64-linux-strip"   CACHE FILEPATH "")

set(CMAKE_SYSROOT "${A64_TOOLCHAIN}/aarch64-buildroot-linux-gnu/sysroot")

list(APPEND CMAKE_FIND_ROOT_PATH "${A64_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

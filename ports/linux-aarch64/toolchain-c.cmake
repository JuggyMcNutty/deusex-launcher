# Cross-compile C11 for generic aarch64 Linux: the launcher.
#
# On an aarch64 machine this file does nothing and the build is native, so
# the same preset works both ways.
#
# Godot's Buildroot SDK godot-2023.08.x-4 (GCC 13.2, glibc 2.28), as
# linux-x86_64's. Its glibc is the floor the binary needs: RHEL 8's, the
# oldest of the actively maintained mainline distributions. The headers and
# libraries SDL3's backends are built against are Debian's, in
# deps/sysroots/linux-aarch64 (port.sh), laid out as Debian lays them out:
# found under it in their multiarch folders.
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    return()
endif()

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

get_filename_component(DXL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(A64_TOOLCHAIN "${DXL_ROOT}/deps/toolchains/aarch64-godot-linux-gnu_sdk-buildroot-2023.08.x-4")
set(A64_SYSROOT   "${DXL_ROOT}/deps/sysroots/linux-aarch64")

if(NOT EXISTS "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-gcc")
    message(FATAL_ERROR "toolchain missing -- run scripts/dx.sh deps linux-aarch64")
endif()

set(CMAKE_C_COMPILER   "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-g++")
set(CMAKE_AR           "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-ar"      CACHE FILEPATH "")
set(CMAKE_RANLIB       "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-ranlib"  CACHE FILEPATH "")
set(CMAKE_STRIP        "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-strip"   CACHE FILEPATH "")

set(CMAKE_SYSROOT "${A64_TOOLCHAIN}/aarch64-godot-linux-gnu/sysroot")

list(APPEND CMAKE_FIND_ROOT_PATH "${A64_SYSROOT}")
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

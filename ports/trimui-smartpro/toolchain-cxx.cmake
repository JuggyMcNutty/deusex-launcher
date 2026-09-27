# Cross-compile C++20 for the TrimUI Smart Pro: the engine (engine.cmake).
#
# Separate from toolchain-c.cmake because the launcher is C11 on a GCC 9.3
# toolchain, while Surreal Engine needs C++20. Both must stay at or below the
# device's glibc 2.33:
#
#   Bootlin bleeding-edge 2021.05-1 -- GCC 10.3, glibc 2.33 (exact match)
#
# GCC 10.3 was verified to compile the C++20 the engine uses (concepts,
# <filesystem>, designated initialisers, operator<=>) and to emit only
# GLIBC_2.17 symbol references.
#
# libstdc++ and libgcc are linked statically. The device ships
# libstdc++.so.6.0.28 (GLIBCXX_3.4.28), which GCC 10.3 would actually be
# compatible with -- static linking just removes the question, at ~1 MB.

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

get_filename_component(DXL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(TRIMUI_TOOLCHAIN "${DXL_ROOT}/deps/toolchains/aarch64--glibc--bleeding-edge-2021.05-1")
set(TRIMUI_SYSROOT   "${DXL_ROOT}/deps/sysroots/trimui-smartpro")

if(NOT EXISTS "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-g++")
    message(FATAL_ERROR "C++ toolchain missing -- run scripts/dx.sh deps trimui-smartpro")
endif()

set(CMAKE_C_COMPILER   "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-gcc")
set(CMAKE_CXX_COMPILER "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-g++")
set(CMAKE_AR      "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-ar"      CACHE FILEPATH "")
set(CMAKE_RANLIB  "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-ranlib"  CACHE FILEPATH "")
set(CMAKE_STRIP   "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-strip"   CACHE FILEPATH "")
set(CMAKE_OBJDUMP "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-objdump" CACHE FILEPATH "")

set(CMAKE_SYSROOT "${TRIMUI_TOOLCHAIN}/aarch64-buildroot-linux-gnu/sysroot")

set(CMAKE_C_FLAGS_INIT   "-mcpu=cortex-a53")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a53")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-static-libstdc++ -static-libgcc -L${TRIMUI_SYSROOT}/lib -Wl,-rpath-link,${TRIMUI_SYSROOT}/lib")
set(CMAKE_SHARED_LINKER_FLAGS_INIT
    "-static-libstdc++ -static-libgcc -L${TRIMUI_SYSROOT}/lib -Wl,-rpath-link,${TRIMUI_SYSROOT}/lib")

# Device headers/libs (SDL2, EGL, GLES, OpenAL, ALSA, Vulkan loader) come from
# the device sysroot; never from the host.
include_directories(SYSTEM "${TRIMUI_SYSROOT}/include")
list(APPEND CMAKE_FIND_ROOT_PATH "${TRIMUI_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config must resolve against the device sysroot, not the host's /usr.
set(ENV{PKG_CONFIG_LIBDIR}      "${TRIMUI_SYSROOT}/lib/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${TRIMUI_SYSROOT}")
set(ENV{PKG_CONFIG_PATH}        "")

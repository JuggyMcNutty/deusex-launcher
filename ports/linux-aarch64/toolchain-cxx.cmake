# Cross-compile C++20 for generic aarch64 Linux: the engine (engine.cmake).
#
# On an aarch64 machine this file does nothing and the build is native.
#
# The launcher's SDK (toolchain-c.cmake), Godot's Buildroot SDK
# godot-2023.08.x-4: GCC 13.2 on glibc 2.28, the floor the engine needs as
# the launcher does. What the engine links, none of it above glibc 2.28: the
# SDK's own X11, Xi, Xext, D-Bus, GLib, fontconfig and FreeType, found first;
# buster's glvnd, EGL, GL and OpenAL Soft, which port.sh lays over Debian's
# packages in deps/sysroots/linux-aarch64 (bookworm's would need a newer
# glibc than the SDK's, which GNU ld rejects); and SDL2, port.sh's, built in
# from deps/prefix/linux-aarch64. At run time the device's own libraries are
# used. libstdc++ and libgcc are linked statically: the device's may be
# older than GCC 13's.
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    return()
endif()

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

get_filename_component(DXL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(A64_TOOLCHAIN "${DXL_ROOT}/deps/toolchains/aarch64-godot-linux-gnu_sdk-buildroot-2023.08.x-4")
set(A64_SDK_ROOT  "${A64_TOOLCHAIN}/aarch64-godot-linux-gnu/sysroot")
set(A64_SYSROOT   "${DXL_ROOT}/deps/sysroots/linux-aarch64")
set(A64_MULTI     "${A64_SYSROOT}/usr/lib/aarch64-linux-gnu")

if(NOT EXISTS "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-g++" OR NOT EXISTS "${A64_SYSROOT}/.dx-recipe")
    message(FATAL_ERROR "toolchain or sysroot missing -- run scripts/dx.sh deps linux-aarch64")
endif()

set(CMAKE_C_COMPILER   "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-g++")
set(CMAKE_AR           "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-ar"      CACHE FILEPATH "")
set(CMAKE_RANLIB       "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-ranlib"  CACHE FILEPATH "")
set(CMAKE_STRIP        "${A64_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-strip"   CACHE FILEPATH "")

set(CMAKE_SYSROOT "${A64_SDK_ROOT}")

# Debian's headers after the SDK's: EGL's and GL's, which the SDK lacks.
set(A64_INCLUDE "-idirafter ${A64_SYSROOT}/usr/include -idirafter ${A64_SYSROOT}/usr/include/aarch64-linux-gnu")
set(CMAKE_C_FLAGS_INIT   "${A64_INCLUDE}")
set(CMAKE_CXX_FLAGS_INIT "${A64_INCLUDE}")
# The SDK's libraries before Debian's, which hold what the SDK lacks.
set(A64_LINK "-static-libstdc++ -static-libgcc -L${A64_SDK_ROOT}/usr/lib64 -L${A64_MULTI} -Wl,-rpath-link,${A64_SDK_ROOT}/usr/lib64:${A64_MULTI}")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${A64_LINK}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${A64_LINK}")

# SDL2 found in its prefix, the rest in the sysroots.
list(APPEND CMAKE_FIND_ROOT_PATH "${DXL_ROOT}/deps/prefix/linux-aarch64" "${A64_SYSROOT}")
set(CMAKE_LIBRARY_ARCHITECTURE aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config resolves in the SDK alone, by its files port.sh points at lib64:
# never the machine's /usr, nor bookworm's, which need a newer glibc.
set(ENV{PKG_CONFIG_LIBDIR}      "${A64_SYSROOT}/sdk-pkgconfig:${A64_SDK_ROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${A64_SDK_ROOT}")
set(ENV{PKG_CONFIG_PATH}        "")

# Cross-compile C++20 for the TrimUI Smart Pro: the engine (engine.cmake).
#
# The launcher's toolchain (toolchain-c.cmake), Godot's Buildroot SDK
# godot-2023.08.x-4: GCC 13.2 on glibc 2.28. A file of its own for the
# engine's link: libstdc++ and libgcc static, the device's libraries found
# for it, and pkg-config on the device's sysroot alone.
#
# libstdc++ and libgcc are linked statically: the SDK's libstdc++ is 6.0.32
# (GLIBCXX_3.4.32), the device's libstdc++.so.6.0.28 only GLIBCXX_3.4.28.
#
# The device's libraries were built against its glibc 2.33: SDL2, ALSA and
# SDL2_mixer reference exp, log and pow at GLIBC_2.29, its libstdc++ (which
# libopenal needs) stat, lstat and fstat64 at GLIBC_2.33. The device has
# them; the SDK's 2.28 does not, so the link ignores what shared libraries
# leave undefined. The engine's own references stay at or below 2.28
# (scripts/check-abi.sh).

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

get_filename_component(DXL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(TRIMUI_TOOLCHAIN "${DXL_ROOT}/deps/toolchains/aarch64-godot-linux-gnu_sdk-buildroot-2023.08.x-4")
set(TRIMUI_SYSROOT   "${DXL_ROOT}/deps/sysroots/trimui-smartpro")

if(NOT EXISTS "${TRIMUI_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-g++")
    message(FATAL_ERROR "toolchain missing -- run scripts/dx.sh deps trimui-smartpro")
endif()

set(CMAKE_C_COMPILER   "${TRIMUI_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${TRIMUI_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-g++")
set(CMAKE_AR      "${TRIMUI_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-ar"      CACHE FILEPATH "")
set(CMAKE_RANLIB  "${TRIMUI_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-ranlib"  CACHE FILEPATH "")
set(CMAKE_STRIP   "${TRIMUI_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-strip"   CACHE FILEPATH "")
set(CMAKE_OBJDUMP "${TRIMUI_TOOLCHAIN}/bin/aarch64-godot-linux-gnu-objdump" CACHE FILEPATH "")

set(CMAKE_SYSROOT "${TRIMUI_TOOLCHAIN}/aarch64-godot-linux-gnu/sysroot")

set(CMAKE_C_FLAGS_INIT   "-mcpu=cortex-a53")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a53")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-static-libstdc++ -static-libgcc -L${TRIMUI_SYSROOT}/lib -Wl,-rpath-link,${TRIMUI_SYSROOT}/lib -Wl,--unresolved-symbols=ignore-in-shared-libs")
set(CMAKE_SHARED_LINKER_FLAGS_INIT
    "-static-libstdc++ -static-libgcc -L${TRIMUI_SYSROOT}/lib -Wl,-rpath-link,${TRIMUI_SYSROOT}/lib -Wl,--unresolved-symbols=ignore-in-shared-libs")

# Device headers/libs (SDL2, EGL, GLES, OpenAL, Vulkan loader) come from
# the device sysroot; never from the host.
include_directories(SYSTEM "${TRIMUI_SYSROOT}/include")
list(APPEND CMAKE_FIND_ROOT_PATH "${TRIMUI_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config must resolve against the device sysroot alone: not the host's
# /usr, nor the SDK's sysroot, whose glib and fontconfig DXWidgets would take.
set(ENV{PKG_CONFIG_LIBDIR}      "${TRIMUI_SYSROOT}/lib/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${TRIMUI_SYSROOT}")
set(ENV{PKG_CONFIG_PATH}        "")

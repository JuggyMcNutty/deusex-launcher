# Cross-compile C11 for the TrimUI Smart Pro (Allwinner A133, spruceOS /
# TinaLinux): the launcher.
#
# Two sysroots are in play and they are not interchangeable:
#   - the toolchain's own (glibc 2.31 headers + crt objects), set as CMAKE_SYSROOT
#   - deps/sysroots/trimui-smartpro, holding the DEVICE's SDL2/freetype shared
#     objects, added as an extra search path. We link against the vendor SDL2
#     because it carries a custom "mali" EGL video driver that upstream SDL2
#     does not have.
#
# The toolchain's glibc is load-bearing: see this port's README.md. Fetched by
# scripts/dx.sh deps trimui-smartpro.

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

get_filename_component(DXL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(TRIMUI_TOOLCHAIN "${DXL_ROOT}/deps/toolchains/aarch64--glibc--stable-2020.08-1")
set(TRIMUI_SYSROOT   "${DXL_ROOT}/deps/sysroots/trimui-smartpro")

if(NOT EXISTS "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-gcc")
    message(FATAL_ERROR "toolchain missing -- run scripts/dx.sh deps trimui-smartpro")
endif()
if(NOT EXISTS "${TRIMUI_SYSROOT}/lib/libSDL2-2.0.so.0")
    message(FATAL_ERROR "device sysroot missing -- run scripts/dx.sh deps trimui-smartpro")
endif()

set(CMAKE_C_COMPILER   "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-gcc")
set(CMAKE_CXX_COMPILER "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-g++")
set(CMAKE_AR           "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-ar"      CACHE FILEPATH "")
set(CMAKE_RANLIB       "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-ranlib"  CACHE FILEPATH "")
set(CMAKE_STRIP        "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-strip"   CACHE FILEPATH "")
set(CMAKE_OBJDUMP      "${TRIMUI_TOOLCHAIN}/bin/aarch64-linux-objdump" CACHE FILEPATH "")

set(CMAKE_SYSROOT "${TRIMUI_TOOLCHAIN}/aarch64-buildroot-linux-gnu/sysroot")

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a53")

# Find headers/libs in the device sysroot, but never look at the host's.
list(APPEND CMAKE_FIND_ROOT_PATH "${TRIMUI_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

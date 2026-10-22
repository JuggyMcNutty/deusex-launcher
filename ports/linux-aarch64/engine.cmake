# Initial cache for the engine on aarch64 Linux (scripts/engine.sh build
# linux-aarch64): SDL2's window and X11's; not SDL3. DXWidgets builds one
# SDL backend -- SDL3's when both are on -- and SDL2's is the one the
# launcher asks for at run time.
#
# Cross-built from a PC by the SDK of toolchain-cxx.cmake, at glibc 2.28,
# with port.sh's SDL2 built in, its own X11, Wayland and audio backends
# opened at run time; the engine's own Wayland window left out, as
# linux-x86_64's: it needs waylandpp, which the SDK lacks. Built natively on
# the device, against the distro's SDL2, X11 and Wayland.

# FORCE: this file is the truth, re-applied on every configure (an initial
# cache alone never overrides a value already in an existing build).
cmake_host_system_information(RESULT DXL_HOST_ARCH QUERY OS_PLATFORM)
if(DXL_HOST_ARCH MATCHES "^(aarch64|arm64)$")
    set(ENABLE_WAYLAND ON CACHE BOOL "" FORCE)
else()
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/toolchain-cxx.cmake" CACHE FILEPATH "" FORCE)
    set(ENABLE_WAYLAND OFF CACHE BOOL "" FORCE)
endif()
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
set(ENABLE_SDL2    ON CACHE BOOL "" FORCE)
set(ENABLE_SDL3    OFF CACHE BOOL "" FORCE)
set(ENABLE_X11     ON CACHE BOOL "" FORCE)

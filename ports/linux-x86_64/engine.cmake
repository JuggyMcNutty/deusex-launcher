# Initial cache for the engine on desktop Linux (scripts/engine.sh build), at
# glibc 2.28 by the SDK of toolchain.cmake, libstdc++ and libgcc static.
#
# SDL3's window and X11's. DXWidgets builds one SDL backend, SDL3's
# when both are on; the launcher asks for it at run time
# (DXE_DISPLAY_BACKEND). SDL3 is port.sh's, the launcher's own
# version, built in, its own X11, Wayland and audio backends opened at run
# time. The engine's own Wayland window is left out: it needs waylandpp,
# which the SDK lacks, and SDL3's Wayland is the one the launcher's runs use.

# FORCE: this file is the truth, re-applied on every configure (an initial
# cache alone never overrides a value already in an existing build).
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/toolchain.cmake" CACHE FILEPATH "" FORCE)
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
set(ENABLE_SDL2    OFF CACHE BOOL "" FORCE)
set(ENABLE_SDL3    ON  CACHE BOOL "" FORCE)
set(ENABLE_X11     ON  CACHE BOOL "" FORCE)
set(ENABLE_WAYLAND OFF CACHE BOOL "" FORCE)
# For editors: DXEngine/compile_commands.json can point here.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

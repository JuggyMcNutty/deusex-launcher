# Initial cache for the engine on Windows (scripts/engine.sh build), by
# llvm-mingw (toolchain.cmake), statically but for OpenAL Soft, LGPL, its
# OpenAL32.dll beside the engine.
#
# SDL3's window, the launcher's own version (port.sh builds it into
# deps/prefix/windows-x86_64, static), which DXWidgets tries first on
# Windows when it is built: the pad and the harness's timelines are its
# alone. Its Win32 window is built besides (DXE_DISPLAY_BACKEND=Win32);
# X11, Wayland and SDL2 are not Windows'.

# FORCE: this file is the truth, re-applied on every configure (an initial
# cache alone never overrides a value already in an existing build).
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/toolchain.cmake" CACHE FILEPATH "" FORCE)
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
set(ENABLE_SDL2    OFF CACHE BOOL "" FORCE)
set(ENABLE_SDL3    ON  CACHE BOOL "" FORCE)
set(ENABLE_X11     OFF CACHE BOOL "" FORCE)
set(ENABLE_WAYLAND OFF CACHE BOOL "" FORCE)
# For editors: DXEngine/compile_commands.json can point here.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

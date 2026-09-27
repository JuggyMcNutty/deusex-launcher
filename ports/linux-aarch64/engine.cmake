# Initial cache for Surreal Engine on aarch64 Linux, built natively on the
# device (scripts/engine.sh build linux-aarch64 there). Same as desktop Linux:
# run-game.sh picks the SDL2 backend, the one with gamepad support.

# FORCE: this file is the truth, re-applied on every configure (an initial
# cache alone never overrides a value already in an existing build).
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
set(ENABLE_SDL2    ON CACHE BOOL "" FORCE)
set(ENABLE_SDL3    ON CACHE BOOL "" FORCE)
set(ENABLE_X11     ON CACHE BOOL "" FORCE)
set(ENABLE_WAYLAND ON CACHE BOOL "" FORCE)

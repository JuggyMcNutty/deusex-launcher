# Initial cache for Surreal Engine on desktop Linux (scripts/engine.sh build).
#
# Every display backend stays on; run-game.sh picks SDL2 at run time, because
# that is the backend with the fork's gamepad support.

# FORCE: this file is the truth, re-applied on every configure (an initial
# cache alone never overrides a value already in an existing build).
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
set(ENABLE_SDL2    ON CACHE BOOL "" FORCE)
set(ENABLE_SDL3    ON CACHE BOOL "" FORCE)
set(ENABLE_X11     ON CACHE BOOL "" FORCE)
set(ENABLE_WAYLAND ON CACHE BOOL "" FORCE)
# For editors: engine/SurrealEngine/compile_commands.json can point here.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

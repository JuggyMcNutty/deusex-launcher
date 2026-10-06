# Initial cache for Surreal Engine on desktop Linux (scripts/engine.sh build).
#
# SDL2, X11 and Wayland; not SDL3. SurrealWidgets builds one SDL backend --
# SDL3's when both are on -- and only SDL2's has the fork's gamepad support,
# which run-game.sh picks at run time.

# FORCE: this file is the truth, re-applied on every configure (an initial
# cache alone never overrides a value already in an existing build).
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
set(ENABLE_SDL2    ON CACHE BOOL "" FORCE)
set(ENABLE_SDL3    OFF CACHE BOOL "" FORCE)
set(ENABLE_X11     ON CACHE BOOL "" FORCE)
set(ENABLE_WAYLAND ON CACHE BOOL "" FORCE)
# For editors: VibeEngine/compile_commands.json can point here.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "" FORCE)

cmake_minimum_required(VERSION 3.16)
project(deusex-launcher C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)          # _GNU_SOURCE-style extensions (flock, strcasestr)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wshadow -Wvla -Wno-unused-parameter)

set(DXL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# core -- the original launcher's behaviour, recreated: its ini handling, its
# three command-line parsers, the FirstRun gates and the entry tree, the crash
# sentinel, the single-instance handoff, the CD check. No SDL, no platform
# assumptions. dx-reverse-info has the original it follows.
# ---------------------------------------------------------------------------
add_library(dxl_core STATIC
    src/core/common.c
    src/core/ini.c
    src/core/cmdline.c
    src/core/paths.c
    src/core/config.c
    src/core/policy.c
    src/core/sentinel.c
    src/core/install.c
    src/core/instance.c
    src/core/argv.c
    src/core/log.c
)
target_include_directories(dxl_core PUBLIC src)
target_link_libraries(dxl_core PUBLIC m)

# The hand-over to the game: the one step whose mechanism is the platform's
# (src/platform/launch.h). The original runs the engine in its own process;
# here the launcher execs the game's command.
add_library(dxl_platform STATIC src/platform/posix/launch.c)
target_link_libraries(dxl_platform PUBLIC dxl_core)

# ---------------------------------------------------------------------------
# tests -- native builds only; the core has no graphics dependency, so they
# need no display.
# ---------------------------------------------------------------------------
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(tests)
endif()

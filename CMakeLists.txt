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
# sentinel, the single-instance handoff, the CD check, the .int strings, the
# save migration, safe mode's flags, the Renderer list and the Detail
# settings. No SDL, no platform assumptions. dx-reverse-info has the original
# it follows.
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
    src/core/localize.c
    src/core/migrate.c
    src/core/safemode.c
    src/core/renderdev.c
    src/core/detail.c
)
target_include_directories(dxl_core PUBLIC src)
target_link_libraries(dxl_core PUBLIC m)

# What is the platform's: replacing this process with another
# (src/platform/launch.h -- safe mode's relaunch, and the port branches'
# hand-over), the game the launcher starts and stays with, as the original's
# process stays with its engine (process.h), and the machine the Detail page
# asks about (machine.h).
add_library(dxl_platform STATIC
    src/platform/posix/launch.c
    src/platform/posix/process.c
    src/platform/posix/machine.c
)
target_link_libraries(dxl_platform PUBLIC dxl_core)

# The original's run, WinMain and InitEngine in their order, with the screens
# behind a table of functions (src/launch/launch.h).
add_library(dxl_launch STATIC src/launch/launch.c)
target_link_libraries(dxl_launch PUBLIC dxl_platform)

# ---------------------------------------------------------------------------
# tests -- native builds only; the core has no graphics dependency, so they
# need no display.
# ---------------------------------------------------------------------------
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(tests)
endif()

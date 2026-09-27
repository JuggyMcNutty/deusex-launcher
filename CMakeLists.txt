cmake_minimum_required(VERSION 3.16)
project(port-ex-machina C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)          # _GNU_SOURCE-style extensions (flock, strcasestr)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wshadow -Wvla -Wno-unused-parameter)

# ---------------------------------------------------------------------------
# port -- which ports/<id>/ this build is for. port.cmake says how to link
# (system SDL2 or a device sysroot) and whether the binaries must stay under
# a glibc ceiling; the compiler comes from the preset's toolchain file. See
# Port Ex Machina's docs/PORTING.md.
# ---------------------------------------------------------------------------
set(DXL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(DXL_PORT "linux-x86_64" CACHE STRING "Port to build for: a directory under ports/")
set(DXL_PORT_DIR "${DXL_ROOT}/ports/${DXL_PORT}")
if(NOT EXISTS "${DXL_PORT_DIR}/port.cmake")
    message(FATAL_ERROR "no port '${DXL_PORT}' (expected ${DXL_PORT_DIR}/port.cmake)")
endif()

set(DXL_PORT_SDL system)        # system: pkg-config; sysroot: DXL_PORT_SYSROOT
set(DXL_PORT_SYSROOT "")        # headers and libraries of the device, when cross
set(DXL_PORT_GLIBC_MAX "")      # e.g. 2.33: fail the build on any newer symbol
set(DXL_PORT_LINK_OPTIONS "")   # extra link options for the SDL frontend
include("${DXL_PORT_DIR}/port.cmake")

# The device profile compiled in (src/platform/target.h): the port's own
# target.c, or the generic desktop one. Set DXL_PROFILE to another port on a
# host build to see that device's screens (dxl-shots).
set(DXL_PROFILE "${DXL_PORT}" CACHE STRING "Whose device profile to compile in: a directory under ports/")
if(EXISTS "${DXL_ROOT}/ports/${DXL_PROFILE}/target.c")
    set(DXL_TARGET_SOURCE "${DXL_ROOT}/ports/${DXL_PROFILE}/target.c")
elseif(EXISTS "${DXL_ROOT}/ports/${DXL_PROFILE}")
    set(DXL_TARGET_SOURCE "${DXL_ROOT}/src/platform/target_default.c")
else()
    message(FATAL_ERROR "no port '${DXL_PROFILE}' to take a profile from")
endif()
message(STATUS "dxl: port=${DXL_PORT} profile=${DXL_PROFILE} sdl=${DXL_PORT_SDL} glibc-max=${DXL_PORT_GLIBC_MAX}")

# Post-build ABI guard: a binary that references a newer glibc than the
# device has will not load there, whatever our own code calls.
function(dxl_check_abi target)
    if(DXL_PORT_GLIBC_MAX)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${DXL_ROOT}/scripts/check-abi.sh" --max "${DXL_PORT_GLIBC_MAX}" "$<TARGET_FILE:${target}>"
            COMMENT "checking glibc ABI of ${target} against ${DXL_PORT_GLIBC_MAX}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# core -- the launcher's core. No SDL, no platform assumptions, no I/O policy.
# docs/LAUNCHER.md; dx-reverse-info/ has the original behaviour it began from.
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
    src/core/renderers.c
    src/core/log.c
    src/core/json.c
    src/core/engine_settings.c
    src/core/bindings.c
)
# Shown on the System tab: which build is on the device.
execute_process(COMMAND git describe --always --dirty --tags
                WORKING_DIRECTORY "${DXL_ROOT}"
                OUTPUT_VARIABLE DXL_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
if(NOT DXL_VERSION)
    set(DXL_VERSION "unversioned")
endif()

target_include_directories(dxl_core PUBLIC src)
target_link_libraries(dxl_core PUBLIC m)

# ---------------------------------------------------------------------------
# session layer -- the launcher's sequence, shared by both front ends -- and
# the platform: the device profile, and what differs by OS (every port so
# far is POSIX; Port Ex Machina's docs/PORTING.md says what another OS would replace).
# ---------------------------------------------------------------------------
add_library(dxl_app STATIC
    src/app.c
    src/platform/target.c
    ${DXL_TARGET_SOURCE}
    src/platform/posix/launch.c
    src/platform/posix/gpu_probe.c
)
target_include_directories(dxl_app PUBLIC src)
# The GPU probe dlopens the Vulkan loader and EGL rather than linking them, so
# a device without either still starts the launcher. Their headers come from
# the device sysroot on a cross build and from the host otherwise.
if(DXL_PORT_SYSROOT)
    target_include_directories(dxl_app PRIVATE "${DXL_PORT_SYSROOT}/include")
endif()
target_link_libraries(dxl_app PUBLIC dxl_core ${CMAKE_DL_LIBS} m)

# Headless driver: the whole contract with no display. Always built -- it is
# how the on-device behaviour gets checked over SSH.
add_executable(dxl-cli src/cli_main.c)
target_link_libraries(dxl-cli PRIVATE dxl_app)
dxl_check_abi(dxl-cli)

# ---------------------------------------------------------------------------
# frontend -- SDL2: the distro's through pkg-config, or the device's own
# libraries from the port's sysroot.
# ---------------------------------------------------------------------------
if(DXL_PORT_SDL STREQUAL "system")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2 SDL2_ttf)
    set(DXL_SDL_INCLUDE ${SDL2_INCLUDE_DIRS})
    set(DXL_SDL_LIBS    ${SDL2_LIBRARIES})
    set(DXL_SDL_DIRS    ${SDL2_LIBRARY_DIRS})
elseif(DXL_PORT_SDL STREQUAL "sysroot")
    if(NOT EXISTS "${DXL_PORT_SYSROOT}/include/SDL2/SDL.h")
        message(FATAL_ERROR "no SDL2 in ${DXL_PORT_SYSROOT} -- run scripts/dx.sh deps ${DXL_PORT}")
    endif()
    set(DXL_SDL_INCLUDE "${DXL_PORT_SYSROOT}/include")
    set(DXL_SDL_LIBS    SDL2 SDL2_ttf)
    set(DXL_SDL_DIRS    "${DXL_PORT_SYSROOT}/lib")
else()
    message(FATAL_ERROR "DXL_PORT_SDL must be system or sysroot, not '${DXL_PORT_SDL}'")
endif()

set(DXL_UI_SOURCES
    src/ui/ui.c
    src/ui/theme.c
    src/ui/screens.c
    src/ui/tab_play.c
    src/ui/tab_video.c
    src/ui/tab_controls.c
    src/ui/tab_system.c
    src/ui/remap.c
)
add_executable(deusex-launcher src/main.c ${DXL_UI_SOURCES})
# Captured at configure time; re-run cmake to refresh it after a commit.
target_compile_definitions(deusex-launcher PRIVATE DXL_VERSION="${DXL_VERSION}")
target_include_directories(deusex-launcher PRIVATE src ${DXL_SDL_INCLUDE} ${DXL_SDL_INCLUDE}/SDL2)
target_link_directories(deusex-launcher PRIVATE ${DXL_SDL_DIRS})
target_link_libraries(deusex-launcher PRIVATE dxl_app ${DXL_SDL_LIBS} m)
if(DXL_PORT_LINK_OPTIONS)
    target_link_options(deusex-launcher PRIVATE ${DXL_PORT_LINK_OPTIONS})
endif()
dxl_check_abi(deusex-launcher)

# Development tool: renders each screen headlessly for review.
if(NOT CMAKE_CROSSCOMPILING)
    add_executable(dxl-shots tools/shots.c ${DXL_UI_SOURCES})
    target_compile_definitions(dxl-shots PRIVATE DXL_VERSION="${DXL_VERSION}")
    target_include_directories(dxl-shots PRIVATE src ${DXL_SDL_INCLUDE} ${DXL_SDL_INCLUDE}/SDL2)
    target_link_directories(dxl-shots PRIVATE ${DXL_SDL_DIRS})
    target_link_libraries(dxl-shots PRIVATE dxl_app ${DXL_SDL_LIBS} m)
endif()

# What ships: scripts/dx.sh stage installs these into build/<port>/app and
# lays the port's packaging over them.
install(TARGETS deusex-launcher dxl-cli RUNTIME DESTINATION .)

# ---------------------------------------------------------------------------
# tests -- native builds only; core has no graphics dependency so they need
# no display.
# ---------------------------------------------------------------------------
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(tests)
endif()

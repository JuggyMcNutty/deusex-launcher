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

# The port, one of Port Ex Machina's (its docs/PORTING.md): DXLauncher, the
# desktop's launcher, with the port around it. ports/<DXL_PORT>/port.cmake
# says where a cross build's headers come from, and its glibc ceiling,
# checked after each build: a binary that needs a newer glibc than the
# device has will not load there.
set(DXL_PORT "linux-x86_64" CACHE STRING "Port to build for: a directory under ports/")
set(DXL_PORT_SYSROOT "")        # headers and libraries of the device, when cross
set(DXL_PORT_INCLUDE_DIRS "")   # the sysroot's header folders, for the GPU probe's Vulkan and EGL
set(DXL_PORT_GLIBC_MAX "")      # e.g. 2.31: fail the build on any newer symbol
set(DXL_PORT_LINK_OPTIONS "")   # extra link options for the programs
set(DXL_PORT_DEPS_DEFAULT "auto")   # where SDL3 and SDL3_ttf come from: auto, system, vendored
set(DXL_PORT_DEPS_ENV "")       # VAR=value words for the vendored libraries' configure steps
set(DXL_PORT_DEPS_C_FLAGS "")   # and their C flags
include("${DXL_ROOT}/ports/${DXL_PORT}/port.cmake")
set(DXL_PORT_DEPS "${DXL_PORT_DEPS_DEFAULT}" CACHE STRING
    "Where SDL3 and SDL3_ttf come from: auto, system or vendored (cmake/deps.cmake)")
include("${DXL_ROOT}/cmake/deps.cmake")

function(dxl_check_abi target)
    if(DXL_PORT_GLIBC_MAX)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${DXL_ROOT}/scripts/check-abi.sh" --max "${DXL_PORT_GLIBC_MAX}" "$<TARGET_FILE:${target}>"
            COMMENT "checking glibc ABI of ${target} against ${DXL_PORT_GLIBC_MAX}")
    endif()
endfunction()

# The launcher's version, for --version: the branch's commit at configure
# time (run cmake again after a commit to refresh it).
execute_process(COMMAND git describe --always --dirty --tags
                WORKING_DIRECTORY "${DXL_ROOT}"
                OUTPUT_VARIABLE DXL_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
if(NOT DXL_VERSION)
    set(DXL_VERSION "unversioned")
endif()

# ---------------------------------------------------------------------------
# core -- no SDL, no platform: the original's command-line rules, inis,
# decision, crash sentinel and single instance, kept from main's launcher;
# Settings.json, the pad's bindings and the renderer list, the Smart Pro's;
# and DXLauncher's own: its folders, profiles, the game's builds and
# installs, the engine's command line, the last run. docs/LAUNCHER.md.
# ---------------------------------------------------------------------------
add_library(dxl_core STATIC
    src/core/common.c
    src/core/ini.c
    src/core/cmdline.c
    src/core/paths.c
    src/core/config.c
    src/core/policy.c
    src/core/sentinel.c
    src/core/instance.c
    src/core/argv.c
    src/core/log.c
    src/core/localize.c
    src/core/peicon.c
    src/core/json.c
    src/core/engine_settings.c
    src/core/bindings.c
    src/core/renderers.c
    src/core/xdg.c
    src/core/sha1.c
    src/core/gamedb.c
    src/core/install.c
    src/core/profile.c
    src/core/engine_args.c
    src/core/lastrun.c
    src/core/vdf.c
    src/core/bigscreen.c
    src/core/finder.c
    src/core/settings.c
    src/core/settings_table.c
    src/core/advice.c
    src/core/keys.c
    src/core/engine_sections.c
    src/core/padcurve.c
    src/core/padmap.c
    src/core/upkg.c
    src/core/upkg_assets.c
    src/core/theme_colors.c
    src/core/saves.c
    src/core/shotlist.c
)
target_include_directories(dxl_core PUBLIC src)
target_link_libraries(dxl_core PUBLIC m)

# What is the platform's: the processes the launcher starts, the game among
# them (platform/process.h), the crash-isolated GPU probe, the app menu's
# entry, and the desktop's trash.
add_library(dxl_platform STATIC
    src/platform/posix/process.c
    src/platform/posix/gpu_probe.c
    src/platform/posix/desktop.c
    src/platform/posix/trash.c
)
# The GPU probe dlopens the Vulkan loader and EGL rather than linking them;
# their headers come from the sysroot on a cross build, else the host's.
if(DXL_PORT_INCLUDE_DIRS)
    target_include_directories(dxl_platform PRIVATE ${DXL_PORT_INCLUDE_DIRS})
endif()
target_link_libraries(dxl_platform PUBLIC dxl_core ${CMAKE_DL_LIBS})

# One start of the launcher and the game it plays (src/app/run.h), shared
# by both programs.
add_library(dxl_app STATIC
    src/app/session.c
    src/app/play.c
    src/app/run.c
)
target_link_libraries(dxl_app PUBLIC dxl_platform)

# The window's UI kit, on SDL3's renderer and SDL3_ttf (src/ui/kit.h):
# focus and navigation, the theme, text, drawing, the widgets, the hints'
# glyphs, the icon.
add_library(dxl_ui STATIC
    src/ui/focus.c
    src/ui/theme.c
    src/ui/kit.c
    src/ui/text.c
    src/ui/draw.c
    src/ui/widgets.c
    src/ui/glyphs.c
    src/ui/icon.c
    src/ui/controls.c
    src/ui/osk.c
    src/ui/padmesh.c
    src/ui/pad3d.c
    src/ui/art.c
    src/ui/audio.c
    src/ui/thumbs.c
)
target_link_libraries(dxl_ui PUBLIC dxl_core dxl_sdl)

# The window and its pages (src/screens/shell.h).
add_library(dxl_screens STATIC
    src/screens/shell.c
    src/screens/home.c
    src/screens/firstrun.c
    src/screens/recovery.c
    src/screens/about.c
    src/screens/dialogs.c
    src/screens/window.c
    src/screens/uiscript.c
    src/screens/settings.c
    src/screens/log.c
    src/screens/controller.c
    src/screens/padwizard.c
    src/screens/profiles.c
    src/screens/saves.c
    src/screens/shots.c
)
target_link_libraries(dxl_screens PUBLIC dxl_ui dxl_app)

# deusex-launcher, the launcher; dxl-cli, the same with no window, for
# scripts and for looking.
add_executable(deusex-launcher src/main.c)
target_link_libraries(deusex-launcher PRIVATE dxl_screens)
add_executable(dxl-cli src/cli_main.c)
target_link_libraries(dxl-cli PRIVATE dxl_app)
foreach(program deusex-launcher dxl-cli)
    target_compile_definitions(${program} PRIVATE DXL_VERSION="${DXL_VERSION}")
    if(DXL_PORT_LINK_OPTIONS)
        target_link_options(${program} PRIVATE ${DXL_PORT_LINK_OPTIONS})
    endif()
    dxl_check_abi(${program})
endforeach()

# dxl-shots: every page drawn offscreen, for review and for the tests
# (tools/shots.c). Not part of the app.
add_executable(dxl-shots tools/shots.c)
target_link_libraries(dxl-shots PRIVATE dxl_screens)
target_compile_definitions(dxl-shots PRIVATE DXL_SHOTS_FONTS="${DXL_ROOT}/data/fonts")
# The scenes' saves, written as the tests write theirs (tests/upkg_writer.h).
target_include_directories(dxl-shots PRIVATE tests)

# The app as it ships, one flat folder (cmake --install <build> --prefix
# <app>; Port Ex Machina's scripts/dx.sh stage adds the engine): the
# programs, data/ and the notices of what is in them.
install(TARGETS deusex-launcher dxl-cli RUNTIME DESTINATION .)
install(FILES data/renderers.ini data/engine-settings.json.default
              data/io.github.juggymcnutty.DXLauncher.desktop.in DESTINATION data)
install(DIRECTORY data/fonts DESTINATION data)
install(FILES THIRD-PARTY.md DESTINATION .)

# ---------------------------------------------------------------------------
# tests -- native builds only; none needs a display.
# ---------------------------------------------------------------------------
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Included by SDL3's project() (CMAKE_PROJECT_SDL3_INCLUDE, port.cmake): adds
# this port's video driver, SDL_fbvideo.c, to the static library once SDL3's
# own CMakeLists.txt has made it. SDL_VIDEO_DRIVER_PRIVATE puts the driver
# first in SDL's list (PRIVATE_bootstrap). A deferred call's arguments are
# read as it runs, so the path is kept in a variable of its own.
set(DXL_SDL3_FBVIDEO "${CMAKE_CURRENT_LIST_DIR}/SDL_fbvideo.c")
cmake_language(DEFER CALL target_sources SDL3-static PRIVATE "${DXL_SDL3_FBVIDEO}")

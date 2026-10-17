# The libraries the launcher's window is built with -- SDL3 and SDL3_ttf --
# as the INTERFACE target dxl_sdl. DXL_PORT_DEPS (its default the port's
# port.cmake's DXL_PORT_DEPS_DEFAULT; -DDXL_PORT_DEPS= names another):
#
#   system    the system's, through pkg-config (sdl3, sdl3-ttf)
#   vendored  built here, static, from the sources cmake/deps.lock pins,
#             fetched into DXL_DEPS_SRC (deps/cache/src) beforehand: SDL3
#             with its window and audio backends opened at run time, so the
#             programs need only the C library; FreeType with no HarfBuzz,
#             PNG, bzip2 or Brotli, for SDL3_ttf
#   auto      the system's when pkg-config has them, else vendored
#
# A cross build's vendored libraries see the port's sysroot through
# DXL_PORT_DEPS_ENV (VAR=value words for their configure steps, pkg-config's
# among them) and DXL_PORT_DEPS_C_FLAGS.

set(DXL_DEPS_SRC "${DXL_ROOT}/deps/cache/src" CACHE PATH "Where cmake/deps.lock's sources are fetched to")

set(dxl_deps_mode "${DXL_PORT_DEPS}")
if(dxl_deps_mode STREQUAL "auto" OR dxl_deps_mode STREQUAL "system")
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(DXL_SDL3 IMPORTED_TARGET sdl3>=3.2 sdl3-ttf>=3.2)
    endif()
    if(DXL_SDL3_FOUND)
        set(dxl_deps_mode system)
    elseif(dxl_deps_mode STREQUAL "system")
        message(FATAL_ERROR "DXL_PORT_DEPS system: no sdl3 and sdl3-ttf through pkg-config")
    else()
        set(dxl_deps_mode vendored)
    endif()
endif()

add_library(dxl_sdl INTERFACE)

if(dxl_deps_mode STREQUAL "system")
    message(STATUS "dxl: SDL3 ${DXL_SDL3_sdl3_VERSION} and SDL3_ttf ${DXL_SDL3_sdl3-ttf_VERSION}, the system's")
    target_link_libraries(dxl_sdl INTERFACE PkgConfig::DXL_SDL3)
    return()
endif()
if(NOT dxl_deps_mode STREQUAL "vendored")
    message(FATAL_ERROR "DXL_PORT_DEPS must be auto, system or vendored, not '${DXL_PORT_DEPS}'")
endif()

# ---- vendored --------------------------------------------------------------
include(ExternalProject)

# dxl_dep(<name>): <name>'s line of cmake/deps.lock, as dep_file, dep_sha256
# and dep_version; a missing source is a configure error naming the fetch.
function(dxl_dep name)
    file(STRINGS "${DXL_ROOT}/cmake/deps.lock" lines REGEX "^${name}[ \t]")
    if(NOT lines)
        message(FATAL_ERROR "no ${name} in cmake/deps.lock")
    endif()
    string(REGEX REPLACE "[ \t]+" ";" words "${lines}")
    list(GET words 1 version)
    list(GET words 2 file)
    list(GET words 3 sha256)
    if(NOT EXISTS "${DXL_DEPS_SRC}/${file}")
        message(FATAL_ERROR "no ${DXL_DEPS_SRC}/${file} -- Port Ex Machina's scripts/dx.sh deps ${DXL_PORT} fetches it")
    endif()
    set(dep_file "${DXL_DEPS_SRC}/${file}" PARENT_SCOPE)
    set(dep_sha256 "${sha256}" PARENT_SCOPE)
    set(dep_version "${version}" PARENT_SCOPE)
endfunction()

set(dxl_prefix "${CMAKE_BINARY_DIR}/deps")
file(MAKE_DIRECTORY "${dxl_prefix}/include")
set(dxl_dep_args
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=${dxl_prefix}
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON
    -DCMAKE_PREFIX_PATH=${dxl_prefix}
    "-DCMAKE_C_FLAGS=${DXL_PORT_DEPS_C_FLAGS}")
if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND dxl_dep_args -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()
set(dxl_dep_cmake ${CMAKE_COMMAND})
if(DXL_PORT_DEPS_ENV)
    set(dxl_dep_cmake ${CMAKE_COMMAND} -E env ${DXL_PORT_DEPS_ENV} ${CMAKE_COMMAND})
endif()

dxl_dep(sdl3)
set(dxl_sdl3_version ${dep_version})
ExternalProject_Add(dep_sdl3
    URL "${dep_file}" URL_HASH SHA256=${dep_sha256}
    CMAKE_COMMAND ${dxl_dep_cmake}
    CMAKE_ARGS ${dxl_dep_args}
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_TEST_LIBRARY=OFF
        -DSDL_X11_SHARED=ON -DSDL_WAYLAND_SHARED=ON -DSDL_WAYLAND_LIBDECOR_SHARED=ON
        -DSDL_ALSA_SHARED=ON -DSDL_PULSEAUDIO_SHARED=ON -DSDL_PIPEWIRE_SHARED=ON
        -DSDL_HIDAPI_LIBUSB_SHARED=ON -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_IBUS=OFF
        -DSDL_LIBURING=OFF -DSDL_CAMERA=OFF
    BUILD_BYPRODUCTS "${dxl_prefix}/lib/libSDL3.a")

dxl_dep(freetype)
ExternalProject_Add(dep_freetype
    URL "${dep_file}" URL_HASH SHA256=${dep_sha256}
    CMAKE_COMMAND ${dxl_dep_cmake}
    CMAKE_ARGS ${dxl_dep_args} -DBUILD_SHARED_LIBS=OFF
        -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_PNG=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_BROTLI=ON
        -DFT_DISABLE_ZLIB=ON
    BUILD_BYPRODUCTS "${dxl_prefix}/lib/libfreetype.a")

dxl_dep(sdl3_ttf)
set(dxl_ttf_version ${dep_version})
ExternalProject_Add(dep_sdl3_ttf
    DEPENDS dep_sdl3 dep_freetype
    URL "${dep_file}" URL_HASH SHA256=${dep_sha256}
    CMAKE_COMMAND ${dxl_dep_cmake}
    CMAKE_ARGS ${dxl_dep_args} -DBUILD_SHARED_LIBS=OFF -DSDLTTF_VENDORED=OFF -DSDLTTF_SAMPLES=OFF
        -DSDLTTF_HARFBUZZ=OFF -DSDLTTF_PLUTOSVG=OFF
        -DSDL3_DIR=${dxl_prefix}/lib/cmake/SDL3
        -DFREETYPE_INCLUDE_DIRS=${dxl_prefix}/include/freetype2
        -DFREETYPE_LIBRARY=${dxl_prefix}/lib/libfreetype.a
    BUILD_BYPRODUCTS "${dxl_prefix}/lib/libSDL3_ttf.a")

message(STATUS "dxl: SDL3 ${dxl_sdl3_version} and SDL3_ttf ${dxl_ttf_version}, built here from cmake/deps.lock's sources")
add_dependencies(dxl_sdl dep_sdl3_ttf)
target_include_directories(dxl_sdl INTERFACE "${dxl_prefix}/include")
target_link_libraries(dxl_sdl INTERFACE
    "${dxl_prefix}/lib/libSDL3_ttf.a" "${dxl_prefix}/lib/libfreetype.a" "${dxl_prefix}/lib/libSDL3.a"
    m ${CMAKE_DL_LIBS} pthread rt)

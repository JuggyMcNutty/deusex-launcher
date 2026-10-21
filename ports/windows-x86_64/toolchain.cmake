# Windows 10 1903 or newer on x86_64: the launcher (C11) and the engine
# (C++20, engine.cmake) cross-built by llvm-mingw -- Clang, LLD and
# mingw-w64 on the UCRT, Windows' own C runtime since Windows 10 -- fetched
# into deps/toolchains by port.sh. Linked statically (-static): the
# programs need no DLL of the toolchain's (libc++, libunwind, winpthread),
# only Windows' own, which scripts/check-abi.sh --windows holds them to
# after each build, and the PE headers say Windows 10 (6.0 otherwise).
#
# The tests run here under Wine, Port Ex Machina's scripts/wine.sh beside
# this checkout, CMake's emulator for them.

get_filename_component(DXL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(WIN64_TOOLCHAIN "${DXL_ROOT}/deps/toolchains/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64")

if(NOT EXISTS "${WIN64_TOOLCHAIN}/.dx-ready")
    message(FATAL_ERROR "toolchain missing -- run scripts/dx.sh deps windows-x86_64")
endif()

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER   "${WIN64_TOOLCHAIN}/bin/x86_64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${WIN64_TOOLCHAIN}/bin/x86_64-w64-mingw32-clang++")
set(CMAKE_RC_COMPILER  "${WIN64_TOOLCHAIN}/bin/x86_64-w64-mingw32-windres")
set(CMAKE_AR           "${WIN64_TOOLCHAIN}/bin/llvm-ar"     CACHE FILEPATH "")
set(CMAKE_RANLIB       "${WIN64_TOOLCHAIN}/bin/llvm-ranlib" CACHE FILEPATH "")
set(CMAKE_STRIP        "${WIN64_TOOLCHAIN}/bin/llvm-strip"  CACHE FILEPATH "")

set(WIN64_LINK "-static -Wl,--major-os-version,10,--minor-os-version,0,--major-subsystem-version,10,--minor-subsystem-version,0")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${WIN64_LINK}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${WIN64_LINK}")

# The toolchain's own Windows headers and libraries, and the engine's SDL3,
# built by port.sh: nothing of this machine's.
set(CMAKE_FIND_ROOT_PATH "${WIN64_TOOLCHAIN}/x86_64-w64-mingw32" "${DXL_ROOT}/deps/prefix/windows-x86_64")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(ENV{PKG_CONFIG_LIBDIR} "${WIN64_TOOLCHAIN}/x86_64-w64-mingw32/lib/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${WIN64_TOOLCHAIN}/x86_64-w64-mingw32")
set(ENV{PKG_CONFIG_PATH} "")

set(DXL_WINE "${DXL_ROOT}/../../port-ex-machina/scripts/wine.sh")
if(EXISTS "${DXL_WINE}")
    set(CMAKE_CROSSCOMPILING_EMULATOR "${DXL_WINE}")
endif()

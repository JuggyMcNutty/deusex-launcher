# Desktop Linux on x86_64 at glibc 2.28: the launcher (C11) and the engine
# (C++20, engine.cmake) built with a Buildroot SDK, Godot's
# godot-2023.08.x-4 (GCC 13.2, glibc 2.28), whose glibc is the floor the
# programs need: RHEL 8's, the oldest of the actively maintained mainline
# distributions, so every one runs them.
#
# A native build against a sysroot, not a cross build: the programs run
# here too, and the tests with them. The sysroot is port.sh's,
# deps/sysroots/linux-x86_64: the SDK's own -- its glibc and the libraries
# it has, X11's, ALSA's, PulseAudio's, udev's, D-Bus's, GLib's,
# fontconfig's -- and on it Debian's packages for what it lacks, in their
# multiarch folders: bookworm's headers for what SDL3's and SDL2's backends
# and the GPU probe open at run time (Wayland, xkbcommon, libdecor,
# PipeWire, libusb, Vulkan), and buster's glvnd, EGL and GL, which the
# engine links and which, buster's glibc being 2.28, link against the SDK's.
# At run time the machine's own libraries are used. libstdc++ and libgcc
# are linked statically: the machine's may be older than GCC 13's.

get_filename_component(DXL_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(X64_SDK     "${DXL_ROOT}/deps/toolchains/x86_64-godot-linux-gnu_sdk-buildroot-2023.08.x-4")
set(X64_SYSROOT "${DXL_ROOT}/deps/sysroots/linux-x86_64")
set(X64_MULTI   "${X64_SYSROOT}/usr/lib/x86_64-linux-gnu")

if(NOT EXISTS "${X64_SDK}/bin/x86_64-godot-linux-gnu-gcc" OR NOT EXISTS "${X64_SYSROOT}/.dx-recipe")
    message(FATAL_ERROR "toolchain or sysroot missing -- run scripts/dx.sh deps linux-x86_64")
endif()

# The ceiling port.cmake holds the programs to (scripts/check-abi.sh).
set(DXL_SDK_GLIBC 2.28)

set(CMAKE_C_COMPILER   "${X64_SDK}/bin/x86_64-godot-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${X64_SDK}/bin/x86_64-godot-linux-gnu-g++")
set(CMAKE_AR           "${X64_SDK}/bin/x86_64-godot-linux-gnu-ar"     CACHE FILEPATH "")
set(CMAKE_RANLIB       "${X64_SDK}/bin/x86_64-godot-linux-gnu-ranlib" CACHE FILEPATH "")
set(CMAKE_STRIP        "${X64_SDK}/bin/x86_64-godot-linux-gnu-strip"  CACHE FILEPATH "")

set(CMAKE_SYSROOT "${X64_SYSROOT}")
set(CMAKE_LIBRARY_ARCHITECTURE x86_64-linux-gnu)

# Debian's headers of its own architecture after the rest; its libraries'
# folder for -l and for the libraries they need in turn.
set(CMAKE_C_FLAGS_INIT   "-idirafter ${X64_SYSROOT}/usr/include/x86_64-linux-gnu")
set(CMAKE_CXX_FLAGS_INIT "-idirafter ${X64_SYSROOT}/usr/include/x86_64-linux-gnu")
set(X64_LINK "-static-libstdc++ -static-libgcc -L${X64_MULTI} -Wl,-rpath-link,${X64_MULTI}")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${X64_LINK}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${X64_LINK}")

# The engine's SDL2, built by port.sh, is found as the sysroot's libraries are.
list(APPEND CMAKE_FIND_ROOT_PATH "${DXL_ROOT}/deps/prefix/linux-x86_64")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config resolves in the sysroot alone, never the machine's /usr.
set(ENV{PKG_CONFIG_LIBDIR}
    "${X64_SYSROOT}/usr/lib/pkgconfig:${X64_MULTI}/pkgconfig:${X64_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${X64_SYSROOT}")
set(ENV{PKG_CONFIG_PATH} "")

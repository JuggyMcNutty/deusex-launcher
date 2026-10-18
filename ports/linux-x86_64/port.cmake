# Desktop Linux on x86_64. Built with toolchain.cmake -- the presets' way,
# and the engine's (engine.cmake) -- the programs need glibc 2.28 or newer,
# which scripts/check-abi.sh holds them to after each build; SDL3, FreeType,
# SDL3_ttf and libxmp-lite are built in from cmake/deps.lock's sources, and
# SDL3's window and audio backends opened at run time: the programs need
# only the C library and the machine's own X11 or Wayland, ALSA, PulseAudio
# or PipeWire.
#
# Built with the machine's own compiler instead (a build with no toolchain
# file, as one with sanitizers), the binaries run where they are built: no
# ceiling, and SDL3 and SDL3_ttf the distro's through pkg-config when it
# has them (DXL_PORT_DEPS auto, cmake/deps.cmake).
if(DXL_SDK_GLIBC)
    set(DXL_PORT_GLIBC_MAX    2.28)
    set(DXL_PORT_SYSROOT      "${CMAKE_SYSROOT}")
    set(DXL_PORT_INCLUDE_DIRS "${CMAKE_SYSROOT}/usr/include" "${CMAKE_SYSROOT}/usr/include/x86_64-linux-gnu")
    set(DXL_PORT_DEPS_DEFAULT vendored)
endif()

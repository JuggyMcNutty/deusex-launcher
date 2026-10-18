# Linux on aarch64 with an ordinary distro (Raspberry Pi OS, Debian, Ubuntu,
# ROCKNIX-style handheld images...).
#
# Built natively on an aarch64 machine it sets none of this: the device's
# own libraries or their sources, no ceiling. Cross built from a PC, with
# the glibc 2.28 toolchain, SDL3, FreeType, SDL3_ttf and libxmp-lite are
# built in from cmake/deps.lock's sources, SDL3's window and audio backends
# opened at run time: the programs need only the C library, glibc 2.28 or
# newer, and the device's own X11 or Wayland, ALSA, PulseAudio or PipeWire.
# The backends are built against the headers of Debian bookworm arm64's
# packages in deps/sysroots/linux-aarch64 (port.sh), whose libraries give
# them the names to open.
if(CMAKE_CROSSCOMPILING)
    set(DXL_PORT_SYSROOT      "${DXL_ROOT}/deps/sysroots/linux-aarch64")
    set(DXL_PORT_GLIBC_MAX    2.28)
    set(DXL_PORT_INCLUDE_DIRS "${DXL_PORT_SYSROOT}/usr/include" "${DXL_PORT_SYSROOT}/usr/include/aarch64-linux-gnu")
    set(DXL_PORT_DEPS_DEFAULT vendored)
    # Debian's .pc files name /usr: pkg-config puts the sysroot before each
    # -I and -L.
    set(DXL_PORT_DEPS_ENV
        "PKG_CONFIG_LIBDIR=${DXL_PORT_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${DXL_PORT_SYSROOT}/usr/share/pkgconfig"
        "PKG_CONFIG_SYSROOT_DIR=${DXL_PORT_SYSROOT}")
    set(DXL_PORT_DEPS_C_FLAGS "-I${DXL_PORT_SYSROOT}/usr/include/aarch64-linux-gnu")
endif()

# Linux on aarch64 with an ordinary distro (Raspberry Pi OS, Debian, Ubuntu,
# ROCKNIX-style handheld images...).
#
# Built natively on an aarch64 machine it is the same as linux-x86_64. Cross
# built from a PC, it links SDL2 from a sysroot made of Debian bookworm arm64
# packages (port.sh) with the glibc 2.31 toolchain, so the binary loads on any
# distro from 2020 on.
if(CMAKE_CROSSCOMPILING)
    set(DXL_PORT_SDL       sysroot)
    set(DXL_PORT_SYSROOT   "${DXL_ROOT}/deps/sysroots/linux-aarch64")
    set(DXL_PORT_GLIBC_MAX 2.31)
    set(DXL_PORT_LINK_OPTIONS
        -Wl,--unresolved-symbols=ignore-in-shared-libs
        -Wl,-rpath-link,${DXL_PORT_SYSROOT}/lib)
else()
    set(DXL_PORT_SDL system)
endif()

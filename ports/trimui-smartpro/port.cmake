# TrimUI Smart Pro (spruceOS): the launcher links the device's own vendor SDL2,
# pulled into the sysroot by port.sh, and stays at glibc 2.28, as every Linux
# build does.
set(DXL_PORT_SDL       sysroot)
set(DXL_PORT_SYSROOT   "${DXL_ROOT}/deps/sysroots/trimui-smartpro")
set(DXL_PORT_GLIBC_MAX 2.28)
# The device resolves SDL2's own dependencies from /usr/lib and
# /usr/trimui/lib at run time; the sysroot holds only what we link directly.
# Those libraries need more than 2.28 (SDL2: exp, log and pow at 2.29), which
# the device's glibc 2.33 has: the link ignores what they leave undefined.
set(DXL_PORT_LINK_OPTIONS
    -Wl,--unresolved-symbols=ignore-in-shared-libs
    -Wl,-rpath-link,${DXL_PORT_SYSROOT}/lib)

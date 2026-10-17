# Desktop Linux on x86_64, built natively: no glibc ceiling (the binaries
# run where they are built), no sysroot.
#
# SDL3 and SDL3_ttf, the window's: the distro's through pkg-config, else
# built here from cmake/deps.lock's sources (DXL_PORT_DEPS auto,
# cmake/deps.cmake).

# TrimUI Smart Pro (spruceOS): DXLauncher cross built with linux-aarch64's
# SDK (toolchain-c.cmake), at glibc 2.28 as every Linux build. SDL3, FreeType,
# SDL3_ttf and libxmp-lite are built in from cmake/deps.lock's sources: the
# programs need only the C library, and the launcher links none of the
# device's libraries. The device's sysroot (fetch-sysroot.sh) gives the GPU
# probe its Vulkan and EGL headers, and SDL3 the name of the ALSA library it
# opens.
set(DXL_PORT_SYSROOT      "${DXL_ROOT}/deps/sysroots/trimui-smartpro")
set(DXL_PORT_GLIBC_MAX    2.28)
set(DXL_PORT_INCLUDE_DIRS "${DXL_PORT_SYSROOT}/include")
set(DXL_PORT_DEPS_DEFAULT vendored)
set(DXL_PORT_DEPS_ENV
    "PKG_CONFIG_LIBDIR=${DXL_PORT_SYSROOT}/lib/pkgconfig"
    "PKG_CONFIG_SYSROOT_DIR=${DXL_PORT_SYSROOT}")
set(DXL_PORT_DEPS_C_FLAGS "-mcpu=cortex-a53")
# SDL3 with nothing the device cannot use: no window system (a console
# build), no desktop GL, no Vulkan, no udev, D-Bus or HIDAPI. A video driver
# that fails then shows as no window, not as an invisible one. ALSA is
# opened at run time; the pads are found by an inotify scan.
set(DXL_PORT_SDL3_ARGS
    -DSDL_UNIX_CONSOLE_BUILD=ON
    -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF -DSDL_RPI=OFF -DSDL_ROCKCHIP=OFF
    -DSDL_OFFSCREEN=OFF -DSDL_DUMMYVIDEO=OFF
    -DSDL_OPENGL=OFF -DSDL_OPENGLES=ON -DSDL_VULKAN=OFF -DSDL_RENDER_VULKAN=OFF -DSDL_RENDER_GPU=OFF
    -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_ALSA=ON -DSDL_ALSA_SHARED=ON
    -DSDL_LIBUDEV=OFF -DSDL_DBUS=OFF -DSDL_IBUS=OFF -DSDL_HIDAPI=OFF)

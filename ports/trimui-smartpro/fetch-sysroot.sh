#!/usr/bin/env bash
# Build the link sysroot for the TrimUI Smart Pro: deps/sysroots/trimui-smartpro.
#
#   ports/trimui-smartpro/fetch-sysroot.sh [--force] [dest]
#
# Libraries come off the device itself, so what we link against is exactly
# what will be there at run time. We link the DEVICE's own SDL2
# (/usr/trimui/lib), not a self-built one: that vendor build carries a custom
# "mali" EGL video driver (SDL_malivideo.c) that upstream SDL2 does not have,
# and the PowerVR stack ships only libpvrNULL_WSEGL.so -- so an upstream
# KMSDRM build has nothing to draw on. The engine also links the device's EGL,
# GLESv2, OpenAL, ALSA, zlib and Vulkan loader.
#
# Headers are pinned: SDL2 and SDL_ttf from the release matching the device's
# libraries, the rest from exact package versions in the Arch Linux archive
# (the headers the engine was first built against).
set -euo pipefail
# Port Ex Machina's helpers, from the workspace this branch is checked out
# beside (scripts/dx.sh deps trimui-smartpro sets DX_WORKSPACE).
[ -f "${DX_WORKSPACE:-}/scripts/lib/common.sh" ] ||
    { echo "error: run through Port Ex Machina: scripts/dx.sh deps trimui-smartpro" >&2; exit 1; }
. "$DX_WORKSPACE/scripts/lib/common.sh"
. "$(dirname "${BASH_SOURCE[0]}")/port.sh"     # the device's address and login

# Bump when anything below changes, so an existing sysroot is rebuilt.
RECIPE="trimui-smartpro sysroot 2"

force=0
[ "${1:-}" = "--force" ] && { force=1; shift; }
SYS="${1:-$DX_DEPS/sysroots/trimui-smartpro}"

if [ "$force" = 0 ] && [ "$(cat "$SYS/.dx-recipe" 2>/dev/null)" = "$RECIPE" ]; then
    say "sysroot present: $SYS"
    exit 0
fi

SDL_VER="2.30.8"     # /usr/trimui/lib/libSDL2-2.0.so.0.3000.8
TTF_VER="2.0.15"     # libSDL2_ttf-2.0.so.0.14.1 is SDL_ttf 2.0.15
MESA_TAG="mesa-26.2.2"

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
rm -rf "$SYS"; mkdir -p "$SYS/lib/pkgconfig" "$SYS/include"

# ---- device libraries ------------------------------------------------------
# tar -h follows the device's symlinks, so each name arrives as a real file;
# the development symlinks the linker wants are made locally below.
say "pulling libraries from $DEVICE_USER@$DEVICE ..."
dx_ssh 'tar -C / -chf - \
    usr/trimui/lib/libSDL2-2.0.so.0 usr/trimui/lib/libSDL2_ttf-2.0.so.0 \
    usr/trimui/lib/libSDL2_image-2.0.so.0 usr/trimui/lib/libSDL2_mixer-2.0.so.0 \
    usr/lib/libfreetype.so.6 usr/lib/libEGL.so usr/lib/libGLESv2.so \
    usr/lib/libopenal.so usr/lib/libasound.so.2 usr/lib/libz.so \
    usr/lib/libstdc++.so.6 usr/lib/libvulkan.so.1' > "$tmp/dev.tar" \
    || die "could not reach the device (asleep? it drops off the network when it sleeps)"
mkdir "$tmp/dev"
tar -C "$tmp/dev" -xf "$tmp/dev.tar"
cp "$tmp"/dev/usr/trimui/lib/* "$tmp"/dev/usr/lib/* "$SYS/lib/"

ln -s libSDL2-2.0.so.0       "$SYS/lib/libSDL2.so"
ln -s libSDL2_ttf-2.0.so.0   "$SYS/lib/libSDL2_ttf.so"
ln -s libSDL2_image-2.0.so.0 "$SYS/lib/libSDL2_image.so"
ln -s libSDL2_mixer-2.0.so.0 "$SYS/lib/libSDL2_mixer.so"
ln -s libfreetype.so.6       "$SYS/lib/libfreetype.so"
ln -s libEGL.so              "$SYS/lib/libEGL.so.1"
ln -s libGLESv2.so           "$SYS/lib/libGLESv2.so.2"
ln -s libopenal.so           "$SYS/lib/libopenal.so.1"
ln -s libasound.so.2         "$SYS/lib/libasound.so"
ln -s libz.so                "$SYS/lib/libz.so.1"
ln -s libvulkan.so.1         "$SYS/lib/libvulkan.so"

# The engine finds SDL2 through pkg-config (toolchain-cxx.cmake points it here).
cat > "$SYS/lib/pkgconfig/sdl2.pc" <<PC
prefix=/
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: sdl2
Description: Simple DirectMedia Layer (TrimUI vendor build)
Version: $SDL_VER
Libs: -L\${libdir} -lSDL2
Cflags: -I\${includedir}/SDL2 -D_REENTRANT
PC

# ---- SDL2 and SDL_ttf headers ---------------------------------------------
say "fetching SDL $SDL_VER and SDL_ttf $TTF_VER headers ..."
curl -fsSL --retry 3 -o "$tmp/sdl.tar.gz" \
    "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-$SDL_VER.tar.gz"
tar -C "$tmp" -xzf "$tmp/sdl.tar.gz"
mkdir -p "$SYS/include/SDL2"
cp "$tmp/SDL2-$SDL_VER/include/"*.h "$SYS/include/SDL2/"
curl -fsSL --retry 3 -o "$SYS/include/SDL2/SDL_ttf.h" \
    "https://raw.githubusercontent.com/libsdl-org/SDL_ttf/release-$TTF_VER/SDL_ttf.h"

# ---- Vulkan, EGL/GLES/KHR, OpenAL, ALSA headers -----------------------------
say "fetching pinned headers ..."
dx_fetch_arch_pkg "$tmp/arch" vulkan-headers 1:1.4.357.0-1 any
dx_fetch_arch_pkg "$tmp/arch" libglvnd       1.7.0-3       x86_64
dx_fetch_arch_pkg "$tmp/arch" openal         1.25.2-2      x86_64
dx_fetch_arch_pkg "$tmp/arch" alsa-lib       1.2.16.1-1    x86_64
for d in vulkan vk_video EGL GLES2 GLES3 KHR AL alsa; do
    cp -a "$tmp/arch/usr/include/$d" "$SYS/include/"
done
# Mesa's two EGL extension headers, beside libglvnd's.
for h in eglmesaext.h eglext_angle.h; do
    curl -fsSL --retry 3 -o "$SYS/include/EGL/$h" \
        "https://gitlab.freedesktop.org/mesa/mesa/-/raw/$MESA_TAG/include/EGL/$h"
done

printf '%s\n' "$RECIPE" > "$SYS/.dx-recipe"
say "sysroot ready: $SYS"

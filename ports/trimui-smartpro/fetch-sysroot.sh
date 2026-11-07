#!/usr/bin/env bash
# Build the link sysroot for the TrimUI Smart Pro: deps/sysroots/trimui-smartpro.
#
#   ports/trimui-smartpro/fetch-sysroot.sh [--force] [dest]
#
# Libraries come off the device itself, so what we link against is exactly
# what will be there at run time. Where the device cannot be reached --
# GitHub's CI -- DX_SMARTPRO_SYSROOT=sdk takes them from TrimUI's own SDK for
# it instead: the same sonames, and every symbol the programs link, SDL2 at
# its 2.26.1 where the device has 2.30.8; and OpenAL Soft, which the SDK
# lacks, from Debian buster. Linked by soname, the programs take the
# device's own at run time either way. The engine links the DEVICE's own SDL2
# (/usr/trimui/lib), not a self-built one: that vendor build carries a custom
# "mali" EGL video driver that upstream SDL2 does not have, and the PowerVR
# stack ships only libpvrNULL_WSEGL.so -- so an upstream KMSDRM build has
# nothing to draw on. The engine links the device's SDL2 and OpenAL, and
# opens EGL, GLESv2 and the Vulkan loader at run time (through SDL and volk);
# the sysroot carries those too, and zlib and libstdc++, for the probes and
# for what the libraries link in turn. The launcher links none of them: its
# SDL3 opens ALSA (libasound.so.2) by the name the library here gives it.
#
# Headers are pinned: SDL2 from the release matching the device's library;
# Vulkan, EGL/GLES/KHR and OpenAL from exact Arch Linux packages; Mesa's two
# EGL extension headers from its $MESA_TAG tag.
set -euo pipefail
# Port Ex Machina's helpers, from the workspace this branch is checked out
# beside (scripts/dx.sh deps trimui-smartpro sets DX_WORKSPACE).
[ -f "${DX_WORKSPACE:-}/scripts/lib/common.sh" ] ||
    { echo "error: run through Port Ex Machina: scripts/dx.sh deps trimui-smartpro" >&2; exit 1; }
. "$DX_WORKSPACE/scripts/lib/common.sh"
. "$(dirname "${BASH_SOURCE[0]}")/port.sh"     # the device's address and login

# TrimUI's SDK for the Smart Pro (TG5040), pinned by our own SHA256.
SDK_TAG=20231018
SDK_FILE=SDK_usr_tg5040_a133p.tgz
SDK_URL="https://github.com/trimui/toolchain_sdk_smartpro/releases/download/$SDK_TAG/$SDK_FILE"
SDK_SHA256=b6b615d03204e9d9bb1a91c31de0c3402434f6b8fc780743fa88a5f1da6f3c79

# Bump when anything below changes, so an existing sysroot is rebuilt.
SOURCE="${DX_SMARTPRO_SYSROOT:-device}"
case "$SOURCE" in
    device) RECIPE="trimui-smartpro sysroot 4" ;;
    sdk)    RECIPE="trimui-smartpro sysroot 4 (TrimUI's SDK $SDK_TAG, buster's OpenAL Soft)" ;;
    *)      die "DX_SMARTPRO_SYSROOT is device or sdk, not $SOURCE" ;;
esac

force=0
[ "${1:-}" = "--force" ] && { force=1; shift; }
SYS="${1:-$DX_DEPS/sysroots/trimui-smartpro}"

if [ "$force" = 0 ] && [ "$(cat "$SYS/.dx-recipe" 2>/dev/null)" = "$RECIPE" ]; then
    say "sysroot present: $SYS"
    exit 0
fi

SDL_VER="2.30.8"     # /usr/trimui/lib/libSDL2-2.0.so.0.3000.8
SDL_SHA256=380c295ea76b9bd72d90075793971c8bcb232ba0a69a9b14da4ae8f603350058
MESA_TAG="mesa-26.2.2"

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
rm -rf "$SYS"; mkdir -p "$SYS/lib/pkgconfig" "$SYS/include"

# ---- device libraries ------------------------------------------------------
if [ "$SOURCE" = device ]; then
    # tar -h follows the device's symlinks, so each name arrives as a real
    # file; the development symlinks the linker wants are made locally below.
    say "pulling libraries from $DEVICE_USER@$DEVICE ..."
    dx_ssh 'tar -C / -chf - \
        usr/trimui/lib/libSDL2-2.0.so.0 usr/lib/libEGL.so usr/lib/libGLESv2.so \
        usr/lib/libopenal.so usr/lib/libasound.so.2 usr/lib/libz.so \
        usr/lib/libstdc++.so.6 usr/lib/libvulkan.so.1' > "$tmp/dev.tar" \
        || die "could not reach the device (asleep? it drops off the network when it sleeps)"
    mkdir "$tmp/dev"
    tar -C "$tmp/dev" -xf "$tmp/dev.tar"
    cp "$tmp"/dev/usr/trimui/lib/* "$tmp"/dev/usr/lib/* "$SYS/lib/"
else
    # Under the device's names. buster's OpenAL Soft needs sndio, and sndio
    # libbsd, for the link to find; the device's libstdc++ and Vulkan loader,
    # which only the probes link, are left out.
    say "libraries from TrimUI's SDK $SDK_TAG ..."
    sdk="$(dx_fetch_cached "$SDK_FILE" "$SDK_URL" "$SDK_SHA256")"
    tar -C "$tmp" -xzf "$sdk" usr/lib/libSDL2-2.0.so.0.2600.1 usr/lib/libEGL.so usr/lib/libGLESv2.so \
        usr/lib/libasound.so.2.0.0 usr/lib/libz.so.1.2.8
    cp "$tmp/usr/lib/libSDL2-2.0.so.0.2600.1" "$SYS/lib/libSDL2-2.0.so.0"
    cp "$tmp/usr/lib/libEGL.so" "$tmp/usr/lib/libGLESv2.so" "$SYS/lib/"
    cp "$tmp/usr/lib/libasound.so.2.0.0" "$SYS/lib/libasound.so.2"
    cp "$tmp/usr/lib/libz.so.1.2.8" "$SYS/lib/libz.so"
    say "OpenAL Soft from Debian buster arm64 ..."
    dx_fetch_debs "$tmp/buster" buster arm64 libopenal1 libsndio7.0 libbsd0
    cp "$(readlink -f "$tmp/buster/usr/lib/aarch64-linux-gnu/libopenal.so.1")" "$SYS/lib/libopenal.so"
    cp "$(readlink -f "$tmp/buster/usr/lib/aarch64-linux-gnu/libsndio.so.7.0")" "$SYS/lib/libsndio.so.7.0"
    cp "$(readlink -f "$tmp/buster/usr/lib/aarch64-linux-gnu/libbsd.so.0")" "$SYS/lib/libbsd.so.0"
fi

ln -s libSDL2-2.0.so.0       "$SYS/lib/libSDL2.so"
ln -s libasound.so.2         "$SYS/lib/libasound.so"
ln -s libEGL.so              "$SYS/lib/libEGL.so.1"
ln -s libGLESv2.so           "$SYS/lib/libGLESv2.so.2"
ln -s libopenal.so           "$SYS/lib/libopenal.so.1"
ln -s libz.so                "$SYS/lib/libz.so.1"
[ ! -e "$SYS/lib/libvulkan.so.1" ] || ln -s libvulkan.so.1 "$SYS/lib/libvulkan.so"

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

# ---- SDL2 headers -----------------------------------------------------------
say "fetching SDL $SDL_VER headers ..."
sdl="$(dx_fetch_cached "SDL2-$SDL_VER.tar.gz" \
    "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-$SDL_VER.tar.gz" "$SDL_SHA256")"
tar -C "$tmp" -xzf "$sdl"
mkdir -p "$SYS/include/SDL2"
cp "$tmp/SDL2-$SDL_VER/include/"*.h "$SYS/include/SDL2/"

# ---- Vulkan, EGL/GLES/KHR, OpenAL headers -----------------------------------
say "fetching pinned headers ..."
dx_fetch_arch_pkg "$tmp/arch" vulkan-headers 1:1.4.357.0-1 any    2f6c34cc829c4b63c0cf08cf147841c8ded023b746324540fb02322d9c415c07
dx_fetch_arch_pkg "$tmp/arch" libglvnd       1.7.0-3       x86_64 633c2e95d7798d0ce7ee1b7fe970a22325f7f6d35b9f923bb61b223a80934437
dx_fetch_arch_pkg "$tmp/arch" openal         1.25.2-2      x86_64 f836d5ee3aa0fc4b46800ac41840e8aa35d076de93114fd07d0de43546c1bd07
for d in vulkan vk_video EGL GLES2 GLES3 KHR AL; do
    cp -a "$tmp/arch/usr/include/$d" "$SYS/include/"
done
# Mesa's two EGL extension headers, beside libglvnd's, each pinned.
for pin in "eglmesaext.h e9119482f23bfa6a4fd006278d16434328fa91abcb6c55033959c262644d85f4" \
           "eglext_angle.h df92523469632129b6b0c37bcade9fa8cc449e23e487beda2cc130b4e4bd1df5"; do
    h="${pin% *}"
    cp "$(dx_fetch_cached "$MESA_TAG-$h" "https://gitlab.freedesktop.org/mesa/mesa/-/raw/$MESA_TAG/include/EGL/$h" "${pin#* }")" \
        "$SYS/include/EGL/$h"
done

printf '%s\n' "$RECIPE" > "$SYS/.dx-recipe"
say "sysroot ready: $SYS"

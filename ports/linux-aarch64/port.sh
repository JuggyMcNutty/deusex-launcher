# Linux on aarch64 with an ordinary distro -- sourced by scripts/dx.sh.
# See README.md.
PORT_DESC="Linux aarch64, ordinary distros: DXLauncher (cross-built, or native)"

native() { [ "$(uname -m)" = aarch64 ] || [ "$(uname -m)" = arm64 ]; }

# On an aarch64 machine this port is a native build, engine included. Cross
# built from a PC, it is the launcher only: the engine would need a C++20
# toolchain and a much larger sysroot (see README.md).
if native; then PORT_ENGINE=1; else PORT_ENGINE=0; fi

# The cross toolchain: Godot's Buildroot SDK, GCC 13.2 on glibc 2.28,
# pinned by our own SHA256 (its release publishes none), as linux-x86_64's.
A64_SDK=aarch64-godot-linux-gnu_sdk-buildroot-2023.08.x-4
A64_SDK_URL=https://github.com/godotengine/buildroot/releases/download/godot-2023.08.x-4/aarch64-godot-linux-gnu_sdk-buildroot.tar.bz2
A64_SDK_SHA256=d7b673823e8078ac70ffa202b4471356eca360b80960c6d1a722af0c03aea29d

port_deps() {
    if native; then
        if pkg-config --exists 'sdl3 >= 3.2' 'sdl3-ttf >= 3.2' 'libxmp >= 4.4' 2>/dev/null; then
            say "native build: SDL3 $(pkg-config --modversion sdl3), SDL3_ttf $(pkg-config --modversion sdl3-ttf) and libxmp $(pkg-config --modversion libxmp), the distro's"
        else
            say "native build: no SDL3 and SDL3_ttf 3.2 and libxmp 4.4 or newer from the distro: their sources, built with the launcher"
            dx_fetch_src "$SRC/cmake/deps.lock" sdl3 freetype sdl3_ttf xmp_lite
        fi
        return 0
    fi
    dx_fetch_buildroot "$A64_SDK" "$A64_SDK_URL" "$A64_SDK_SHA256" 2.28
    dx_fetch_src "$SRC/cmake/deps.lock" sdl3 freetype sdl3_ttf xmp_lite

    # What SDL3's backends are built against, from Debian bookworm arm64:
    # each library's headers, and the library itself, whose soname SDL3
    # opens at run time; Vulkan's and EGL's headers for the GPU probe. Only
    # for building: at run time the device's own libraries are used.
    local sys="$DX_DEPS/sysroots/linux-aarch64" recipe="linux-aarch64 sysroot 3"
    if [ "$(cat "$sys/.dx-recipe" 2>/dev/null)" = "$recipe" ]; then
        say "sysroot present: $sys"
        return 0
    fi
    local tmp; tmp="$(mktemp -d)"
    say "fetching Debian bookworm arm64 packages ..."
    dx_fetch_debs "$tmp" bookworm arm64 \
        libx11-dev libx11-6 x11proto-dev libxext-dev libxext6 libxrandr-dev libxrandr2 \
        libxrender-dev libxrender1 libxcursor-dev libxcursor1 libxi-dev libxi6 libxfixes-dev libxfixes3 \
        libxss-dev libxss1 libxtst-dev libxtst6 libxcb1-dev libxcb1 libxau-dev libxdmcp-dev \
        libwayland-dev libwayland-client0 libwayland-cursor0 libwayland-egl1 libxkbcommon-dev libxkbcommon0 \
        libdecor-0-dev libdecor-0-0 \
        libasound2-dev libasound2 libpulse-dev libpulse0 libpipewire-0.3-dev libpipewire-0.3-0 libspa-0.2-dev \
        libudev-dev libudev1 libdbus-1-dev libdbus-1-3 libusb-1.0-0-dev libusb-1.0-0 \
        libegl-dev libegl1 libgles-dev libgles2 libgl-dev libgl1 libglx-dev libdrm-dev libdrm2 libgbm-dev libgbm1 \
        libffi-dev libffi8 libvulkan-dev
    rm -rf "$sys"; mkdir -p "$(dirname "$sys")"
    cp -a "$tmp" "$sys"
    rm -rf "$tmp"
    printf '%s\n' "$recipe" > "$sys/.dx-recipe"
    say "sysroot ready: $sys"
}

# Natively, as linux-x86_64's: the staged app runs where it is staged, on the
# gamefiles/ beside the repositories, its files in the home's XDG folders;
# --fresh gives it a scratch home, build/linux-aarch64/fresh-home.
port_run() {
    native || die "a cross build runs on the device: copy $APP there"
    local game="$DX_ROOT/gamefiles"
    [ -f "$game/System/DeusEx.exe" ] || die "no Deus Ex in $game (no System/DeusEx.exe)"
    if [ "${1:-}" = --fresh ]; then
        shift
        export DXL_HOME="$BUILD/fresh-home"
        rm -rf "$DXL_HOME"
        mkdir -p "$DXL_HOME"
        say "a fresh home: $DXL_HOME"
    fi
    [ "${1:-}" != -- ] || shift
    exec "$APP/$PORT_LAUNCHER" --game="$game" "$@"
}

# Linux on aarch64 with an ordinary distro -- sourced by scripts/dx.sh.
# See README.md.
PORT_DESC="Linux aarch64, ordinary distros: DXLauncher and the engine (cross-built, or native)"
PORT_ENGINE=1

native() { [ "$(uname -m)" = aarch64 ] || [ "$(uname -m)" = arm64 ]; }

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
        dx_fetch_src "$SRC/cmake/deps.lock" sqlite
        return 0
    fi
    dx_fetch_buildroot "$A64_SDK" "$A64_SDK_URL" "$A64_SDK_SHA256" 2.28
    dx_fetch_src "$SRC/cmake/deps.lock" sdl3 freetype sdl3_ttf xmp_lite sdl2 sqlite
    a64_sysroot
    a64_sdl2
}

# What the cross build is built against, in deps/sysroots/linux-aarch64. From
# Debian bookworm arm64: what SDL3's and SDL2's backends are built against,
# each library's headers and the library itself, whose soname they open at
# run time; Vulkan's, EGL's and GL's headers for the GPU probe and the
# engine. Over them buster's glvnd, EGL, GL and OpenAL Soft, which the engine
# links: buster's glibc is 2.28, so they link against the SDK's. And in
# sdk-pkgconfig/ the SDK's own pkg-config files, saying lib64 where its
# libraries are, for what the engine takes from the SDK (D-Bus, GLib,
# fontconfig). Only for building: at run time the device's own libraries are
# used.
a64_sysroot() {
    local sdk="$DX_DEPS/toolchains/$A64_SDK/aarch64-godot-linux-gnu/sysroot"
    local sys="$DX_DEPS/sysroots/linux-aarch64" recipe="linux-aarch64 sysroot 4 ($A64_SDK)"
    if [ "$(cat "$sys/.dx-recipe" 2>/dev/null)" = "$recipe" ]; then
        say "sysroot present: $sys"
        return 0
    fi
    local tmp; tmp="$(mktemp -d)"
    say "fetching Debian bookworm arm64 packages ..."
    dx_fetch_debs "$tmp/bookworm" bookworm arm64 \
        libx11-dev libx11-6 x11proto-dev libxext-dev libxext6 libxrandr-dev libxrandr2 \
        libxrender-dev libxrender1 libxcursor-dev libxcursor1 libxi-dev libxi6 libxfixes-dev libxfixes3 \
        libxss-dev libxss1 libxtst-dev libxtst6 libxcb1-dev libxcb1 libxau-dev libxdmcp-dev \
        libwayland-dev libwayland-client0 libwayland-cursor0 libwayland-egl1 libxkbcommon-dev libxkbcommon0 \
        libdecor-0-dev libdecor-0-0 \
        libasound2-dev libasound2 libpulse-dev libpulse0 libpipewire-0.3-dev libpipewire-0.3-0 libspa-0.2-dev \
        libudev-dev libudev1 libdbus-1-dev libdbus-1-3 libusb-1.0-0-dev libusb-1.0-0 \
        libegl-dev libegl1 libgles-dev libgles2 libgl-dev libgl1 libglx-dev libopengl-dev \
        libdrm-dev libdrm2 libgbm-dev libgbm1 libffi-dev libffi8 libvulkan-dev
    say "fetching Debian buster arm64 packages ..."
    dx_fetch_debs "$tmp/buster" buster arm64 libglvnd0 libegl1 libgl1 libglx0 libgles2 libopengl0 \
        libopenal1 libopenal-dev libsndio7.0 libbsd0
    rm -rf "$sys"; mkdir -p "$(dirname "$sys")"
    cp -a "$tmp/bookworm" "$sys"
    cp -a --remove-destination "$tmp/buster/." "$sys/"
    mkdir -p "$sys/sdk-pkgconfig"
    cp "$sdk"/usr/lib/pkgconfig/*.pc "$sys/sdk-pkgconfig/"
    sed -i -E 's#^libdir=(\$\{exec_prefix\}|\$\{prefix\}|/usr)/lib$#libdir=\1/lib64#' "$sys"/sdk-pkgconfig/*.pc
    rm -rf "$tmp"
    printf '%s\n' "$recipe" > "$sys/.dx-recipe"
    say "sysroot ready: $sys"
}

# The engine's SDL2 (engine.cmake): cmake/deps.lock's, built with the SDK
# into deps/prefix/linux-aarch64, static, its backends built against the
# sysroot's headers and opened at run time, as cmake/deps.cmake builds the
# launcher's SDL3. Its configure links test programs against bookworm's X11,
# whose own references need a newer glibc than the SDK's: the checks ignore
# what those libraries leave undefined, so they find what the libraries have.
a64_sdl2() {
    local lock="$SRC/cmake/deps.lock" ver file
    ver=$(awk '$1 == "sdl2" { print $2 }' "$lock")
    file=$(awk '$1 == "sdl2" { print $3 }' "$lock")
    local sys="$DX_DEPS/sysroots/linux-aarch64" prefix="$DX_DEPS/prefix/linux-aarch64"
    local recipe; recipe="SDL2 $ver, $(cat "$sys/.dx-recipe")"
    if [ "$(cat "$prefix/.dx-sdl2" 2>/dev/null)" = "$recipe" ]; then
        say "SDL2 $ver present: $prefix"
        return 0
    fi
    local tmp; tmp="$(mktemp -d)"
    say "building SDL2 $ver for the engine ..."
    tar -C "$tmp" -xzf "$DX_DEPS/cache/src/$file"
    env PKG_CONFIG_LIBDIR="$sys/usr/lib/aarch64-linux-gnu/pkgconfig:$sys/usr/share/pkgconfig" \
        PKG_CONFIG_SYSROOT_DIR="$sys" \
        cmake -S "$tmp/SDL2-$ver" -B "$tmp/build" -DCMAKE_TOOLCHAIN_FILE="$PORT_DIR/toolchain-c.cmake" \
        -DCMAKE_C_FLAGS="-I$sys/usr/include/aarch64-linux-gnu" \
        -DCMAKE_EXE_LINKER_FLAGS=-Wl,--unresolved-symbols=ignore-in-shared-libs \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_STATIC_PIC=ON -DSDL_TEST=OFF \
        -DSDL_X11_SHARED=ON -DSDL_WAYLAND_SHARED=ON -DSDL_WAYLAND_LIBDECOR_SHARED=ON \
        -DSDL_ALSA_SHARED=ON -DSDL_PULSEAUDIO_SHARED=ON -DSDL_PIPEWIRE_SHARED=ON \
        -DSDL_HIDAPI_LIBUSB_SHARED=ON -DSDL_KMSDRM_SHARED=ON -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_IBUS=OFF \
        >"$tmp/configure.log" 2>&1 || { tail -30 "$tmp/configure.log" >&2; die "SDL2 would not configure"; }
    cmake --build "$tmp/build" -j"$(nproc)" >"$tmp/build.log" 2>&1 || { tail -30 "$tmp/build.log" >&2; die "SDL2 would not build"; }
    rm -rf "$prefix"
    cmake --install "$tmp/build" >/dev/null
    printf '%s\n' "$recipe" > "$prefix/.dx-sdl2"
    rm -rf "$tmp"
    say "SDL2 $ver built: $prefix"
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

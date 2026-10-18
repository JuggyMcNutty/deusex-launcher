# Desktop Linux on x86_64 -- sourced by scripts/dx.sh. See README.md.
PORT_DESC="Desktop Linux, x86_64, glibc 2.28 or newer: DXLauncher, the desktop's own launcher"
PORT_ENGINE=1

# The toolchain: Godot's Buildroot SDK, GCC 13.2 on glibc 2.28, pinned by
# our own SHA256 (its release publishes none). toolchain.cmake names it.
X64_SDK=x86_64-godot-linux-gnu_sdk-buildroot-2023.08.x-4
X64_SDK_URL=https://github.com/godotengine/buildroot/releases/download/godot-2023.08.x-4/x86_64-godot-linux-gnu_sdk-buildroot.tar.bz2
X64_SDK_SHA256=b89f173b1f2f2f35f3090bc4efad732f1c0c1c1206e5480cfcb141977010bc94

# The sysroot the programs are built against (toolchain.cmake): the SDK's,
# hard-linked, and on it Debian's packages for what it lacks -- bookworm's,
# whose headers SDL3's and SDL2's backends and the GPU probe are built
# against, the libraries opened at run time by name, and glvnd's headers;
# buster's glvnd libraries, EGL and GL, under the names bookworm's headers'
# links give, and its OpenAL Soft, which the engine links: buster's glibc is
# 2.28, so they link against the SDK's. A file of Debian's put over one of the SDK's replaces
# it in the copy alone.
x64_sysroot() {
    local sdk="$DX_DEPS/toolchains/$X64_SDK/x86_64-godot-linux-gnu/sysroot"
    local sys="$DX_DEPS/sysroots/linux-x86_64" recipe="linux-x86_64 sysroot 5 ($X64_SDK)"
    if [ "$(cat "$sys/.dx-recipe" 2>/dev/null)" = "$recipe" ]; then
        say "sysroot present: $sys"
        return 0
    fi
    local tmp; tmp="$(mktemp -d)"
    say "fetching Debian bookworm amd64 packages ..."
    dx_fetch_debs "$tmp/bookworm" bookworm amd64 \
        libwayland-dev libwayland-client0 libwayland-cursor0 libwayland-egl1 libxkbcommon-dev libxkbcommon0 \
        libdecor-0-dev libdecor-0-0 libpipewire-0.3-dev libpipewire-0.3-0 libspa-0.2-dev \
        libxss-dev libxss1 libxtst-dev libxtst6 \
        libusb-1.0-0-dev libusb-1.0-0 libgbm-dev libgbm1 libffi-dev libffi8 libvulkan-dev \
        libglvnd-dev libegl-dev libgl-dev libgles-dev libglx-dev libopengl-dev
    say "fetching Debian buster amd64 packages ..."
    dx_fetch_debs "$tmp/buster" buster amd64 libglvnd0 libegl1 libgl1 libglx0 libgles2 libopengl0 \
        libopenal1 libopenal-dev libsndio7.0 libbsd0
    rm -rf "$sys"; mkdir -p "$(dirname "$sys")"
    cp -al "$sdk" "$sys"
    # The SDK's libraries are in usr/lib64, where its .pc files say usr/lib:
    # in the copy they say where they are (sed writes each anew, the SDK's
    # own left as it was).
    sed -i -E 's#^libdir=(\$\{exec_prefix\}|\$\{prefix\}|/usr)/lib$#libdir=\1/lib64#' "$sys"/usr/lib/pkgconfig/*.pc
    cp -a --remove-destination "$tmp/bookworm/." "$sys/"
    cp -a --remove-destination "$tmp/buster/." "$sys/"
    rm -rf "$tmp"
    printf '%s\n' "$recipe" > "$sys/.dx-recipe"
    say "sysroot ready: $sys"
}

# The engine's SDL2 (engine.cmake): cmake/deps.lock's, built with the SDK
# into deps/prefix/linux-x86_64, static, its backends opened at run time.
x64_sdl2() {
    local lock="$SRC/cmake/deps.lock" ver file
    ver=$(awk '$1 == "sdl2" { print $2 }' "$lock")
    file=$(awk '$1 == "sdl2" { print $3 }' "$lock")
    local prefix="$DX_DEPS/prefix/linux-x86_64"
    local recipe; recipe="SDL2 $ver, $(cat "$DX_DEPS/sysroots/linux-x86_64/.dx-recipe")"
    if [ "$(cat "$prefix/.dx-sdl2" 2>/dev/null)" = "$recipe" ]; then
        say "SDL2 $ver present: $prefix"
        return 0
    fi
    local tmp; tmp="$(mktemp -d)"
    say "building SDL2 $ver for the engine ..."
    tar -C "$tmp" -xzf "$DX_DEPS/cache/src/$file"
    cmake -S "$tmp/SDL2-$ver" -B "$tmp/build" -DCMAKE_TOOLCHAIN_FILE="$SRC/ports/linux-x86_64/toolchain.cmake" \
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

port_deps() {
    dx_fetch_buildroot "$X64_SDK" "$X64_SDK_URL" "$X64_SDK_SHA256" 2.28
    dx_fetch_src "$SRC/cmake/deps.lock" sdl3 freetype sdl3_ttf xmp_lite sdl2
    x64_sysroot
    x64_sdl2
}

# The staged app runs where it is staged, on the gamefiles/ beside the
# repositories: nothing is copied into the game's folder, and the launcher
# keeps its files in the home's XDG folders (docs/LAUNCHER.md). --fresh
# gives it a scratch home instead, build/linux-x86_64/fresh-home, emptied
# first: a first run from nothing. The rest of the line is the launcher's,
# after a "--" if one is there.
port_run() {
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

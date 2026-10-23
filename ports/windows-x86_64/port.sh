# Windows 10 1903 or newer on x86_64: DXLauncher and the engine
# cross-built here by llvm-mingw, run and tested here under Proton's wine
# (Port Ex Machina's scripts/lib/common.sh, Wine). No real Windows machine
# is in the loop: what is verified on Windows itself is the port README's.
PORT_DESC="Windows 10 (1903) or newer on x86_64: DXLauncher, cross-built by llvm-mingw, run under Wine here"
PORT_ENGINE=1
PORT_LAUNCHER=deusex-launcher.exe
PORT_ENGINE_FILES="DXEngine.exe DXEngine.pk3 OpenAL32.dll"

# llvm-mingw's release, pinned: the toolchain.cmake beside this file names
# its folder.
WIN64_LLVM_MINGW=20261006
WIN64_LLVM_MINGW_SHA256=5f9c6ed95b2d4bdb2869a488c5fd5857fbdabcf288a0aa3eb1da43f6a08d8ab4

# The engine's SDL3 (engine.cmake): cmake/deps.lock's, the launcher's own
# version, built by llvm-mingw into deps/prefix/windows-x86_64, static, as
# cmake/deps.cmake builds the launcher's.
win64_sdl3() {
    local lock="$SRC/cmake/deps.lock" ver file
    ver=$(awk '$1 == "sdl3" { print $2 }' "$lock")
    file=$(awk '$1 == "sdl3" { print $3 }' "$lock")
    local prefix="$DX_DEPS/prefix/windows-x86_64"
    local recipe="SDL3 $ver, llvm-mingw $WIN64_LLVM_MINGW"
    if [ "$(cat "$prefix/.dx-sdl3" 2>/dev/null)" = "$recipe" ]; then
        say "SDL3 $ver present: $prefix"
        return 0
    fi
    local tmp; tmp="$(mktemp -d)"
    say "building SDL3 $ver for the engine ..."
    tar -C "$tmp" -xzf "$DX_DEPS/cache/src/$file"
    cmake -S "$tmp/SDL3-$ver" -B "$tmp/build" -DCMAKE_TOOLCHAIN_FILE="$SRC/ports/windows-x86_64/toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_TEST_LIBRARY=OFF \
        -DSDL_CAMERA=OFF \
        >"$tmp/configure.log" 2>&1 || { tail -30 "$tmp/configure.log" >&2; die "SDL3 would not configure"; }
    cmake --build "$tmp/build" -j"$(nproc)" >"$tmp/build.log" 2>&1 || { tail -30 "$tmp/build.log" >&2; die "SDL3 would not build"; }
    rm -rf "$prefix"
    cmake --install "$tmp/build" >/dev/null
    printf '%s\n' "$recipe" > "$prefix/.dx-sdl3"
    rm -rf "$tmp"
    say "SDL3 $ver built: $prefix"
}

port_deps() {
    dx_fetch_llvm_mingw "$WIN64_LLVM_MINGW" "$WIN64_LLVM_MINGW_SHA256"
    dx_fetch_src "$SRC/cmake/deps.lock" sdl3 freetype sdl3_ttf xmp_lite
    win64_sdl3
    dx_wine_ready
}

# The staged app, as it ships: a folder, and the same as a zip beside it,
# build/windows-x86_64/DX-windows-x86_64.zip (as Port Ex Machina's releases
# name it), its one top folder DXLauncher. In its licenses/, the notices of
# what THIRD-PARTY.md says the programs carry: the MinGW-w64 runtime's and
# winpthreads', and OpenAL Soft's licence beside its library.
port_stage() {
    local zip="$BUILD/DX-windows-x86_64.zip" tmp
    local runtime="$DX_DEPS/toolchains/llvm-mingw-$WIN64_LLVM_MINGW-ucrt-ubuntu-22.04-x86_64/x86_64-w64-mingw32/share/mingw32"
    mkdir -p "$APP/licenses"
    cp "$runtime/COPYING.MinGW-w64-runtime.txt" "$APP/licenses/MinGW-w64-runtime.txt"
    cp "$runtime/COPYING.winpthreads.txt" "$APP/licenses/winpthreads.txt"
    [ ! -f "$APP/OpenAL32.dll" ] || cp "$DX_ENGINE/Thirdparty/openal-soft/COPYING" "$APP/licenses/OpenAL-Soft-COPYING.txt"
    tmp="$(mktemp -d "$BUILD/.zip.XXXXXX")"
    cp -a "$APP" "$tmp/DXLauncher"
    rm -f "$zip"
    (cd "$tmp" && cmake -E tar cf "$zip" --format=zip DXLauncher)
    rm -rf "$tmp"
    say "zipped $zip"
}

# The staged app under Wine, on the gamefiles/ beside the repositories, by
# Windows' name for them (Z:\...): nothing is copied into the game's
# folder. --fresh gives it a scratch home instead, build/windows-x86_64/
# fresh-home, emptied first: a first run from nothing. The rest of the line
# is the launcher's, after a "--" if one is there.
port_run() {
    local game="$DX_ROOT/gamefiles"
    [ -f "$game/System/DeusEx.exe" ] || die "no Deus Ex in $game (no System/DeusEx.exe)"
    if [ "${1:-}" = --fresh ]; then
        shift
        rm -rf "$BUILD/fresh-home"
        mkdir -p "$BUILD/fresh-home"
        export DXL_HOME; DXL_HOME="$(dx_winpath "$BUILD/fresh-home")"
        say "a fresh home: $BUILD/fresh-home"
    fi
    [ "${1:-}" != -- ] || shift
    dx_wine_ready
    dx_wine_run "$APP/$PORT_LAUNCHER" --game="$(dx_winpath "$game")" "$@"
}

# The branch's own tests, cross-built, under Wine (CMake's emulator, the
# toolchain's): Wine's Recycle Bin is the desktop's own trash, so the tests'
# goes to a scratch one.
port_test() {
    (cd "$SRC" && cmake --preset windows-x86_64 >/dev/null && cmake --build --preset windows-x86_64 -j"$(nproc)")
    dx_wine_ready
    # Wine's server and the prefix's services up, and kept up, before the
    # tests start several at a time: started by them at once, they race,
    # and a test can wait for ever. A server already running (exit 2) is
    # up as it is.
    local server rc=0
    server="$(dirname "$(dx_wine)")/wineserver"
    env WINEPREFIX="$(dx_wine_prefix)" "$server" -p 60 || [ $? = 2 ]
    dx_wine_run cmd /c exit
    mkdir -p "$BUILD/wine-xdg"
    (cd "$SRC" && XDG_DATA_HOME="$BUILD/wine-xdg" ctest --preset windows-x86_64) || rc=$?
    # Then taken down: the server and its services hold the output they
    # were started with, and a pipe from the tests (CI's log) would wait for
    # them for ever.
    env WINEPREFIX="$(dx_wine_prefix)" "$server" -k || true
    return "$rc"
}

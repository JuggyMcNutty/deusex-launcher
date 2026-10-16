# Linux on aarch64 with an ordinary distro -- sourced by scripts/dx.sh.
# See README.md.
PORT_DESC="Linux aarch64, ordinary distros: main's launcher (cross-built, or native)"
PORT_LAUNCHER=DeusEx

native() { [ "$(uname -m)" = aarch64 ] || [ "$(uname -m)" = arm64 ]; }

# On an aarch64 machine this port is a native build, engine included. Cross
# built from a PC, it is the launcher only: the engine would need a C++20
# toolchain and a much larger sysroot (see README.md).
if native; then PORT_ENGINE=1; else PORT_ENGINE=0; fi

port_deps() {
    if native; then
        pkg-config --exists sdl2 SDL2_ttf || die "install the SDL2 and SDL2_ttf development packages"
        say "native build: SDL2 $(pkg-config --modversion sdl2)"
        return 0
    fi
    dx_fetch_bootlin aarch64--glibc--stable-2020.08-1 2.31

    # SDL2 and SDL2_ttf from Debian bookworm arm64, only for linking: at run
    # time the device's own libraries are used.
    local sys="$DX_DEPS/sysroots/linux-aarch64" recipe="linux-aarch64 sysroot 2"
    if [ "$(cat "$sys/.dx-recipe" 2>/dev/null)" = "$recipe" ]; then
        say "sysroot present: $sys"
        return 0
    fi
    local tmp; tmp="$(mktemp -d)"
    say "fetching Debian bookworm arm64 packages ..."
    dx_fetch_debs "$tmp" bookworm arm64 \
        libsdl2-2.0-0 libsdl2-dev libsdl2-ttf-2.0-0 libsdl2-ttf-dev
    rm -rf "$sys"; mkdir -p "$sys/lib"
    cp -a "$tmp/usr/include" "$sys/include"
    # Debian keeps SDL's generated config header in the multiarch directory,
    # beside symlinks back to the ordinary headers: copy what it adds only.
    cp -rnL "$tmp/usr/include/aarch64-linux-gnu/." "$sys/include/"
    cp -a "$tmp/usr/lib/aarch64-linux-gnu/." "$sys/lib/"
    rm -rf "$tmp"
    printf '%s\n' "$recipe" > "$sys/.dx-recipe"
    say "sysroot ready: $sys"
}

# Natively, as linux-x86_64's: DeusEx finds the game by its own folder, so
# the staged app -- DeusEx, run-game.sh and the engine's two files -- goes
# into the System folder of the game beside the repositories, and starts
# there. scripts/recreation.sh uninstall takes the four files out.
port_run() {
    native || die "a cross build runs on the device: copy $APP there"
    local sys="$DX_ROOT/gamefiles/System" f
    [ -f "$sys/DeusEx.exe" ] || die "no Deus Ex in $DX_ROOT/gamefiles (no System/DeusEx.exe)"
    for f in DeusEx run-game.sh SurrealEngine SurrealEngine.pk3; do
        [ -f "$APP/$f" ] || die "no $f in $APP -- scripts/dx.sh stage $PORT"
        cp -p "$APP/$f" "$sys/$f"
    done
    say "installed in $sys: DeusEx run-game.sh SurrealEngine SurrealEngine.pk3"
    exec "$sys/DeusEx" "$@"
}

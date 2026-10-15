# Desktop Linux on x86_64 -- sourced by scripts/dx.sh. See README.md.
PORT_DESC="Desktop Linux, x86_64 (native): main's launcher"
PORT_ENGINE=1
PORT_LAUNCHER=DeusEx

# Nothing to download: the launcher and the engine build against the
# distro's own libraries. This only says what is missing.
port_deps() {
    command -v pkg-config >/dev/null || die "pkg-config is needed"
    pkg-config --exists sdl2 SDL2_ttf || die "install the SDL2 and SDL2_ttf development packages"
    say "SDL2 $(pkg-config --modversion sdl2), SDL2_ttf $(pkg-config --modversion SDL2_ttf)"
}

# DeusEx finds the game by its own folder, as the original DeusEx.exe does,
# so the staged app -- DeusEx, run-game.sh and the engine's two files -- goes
# into the System folder of the game beside the repositories, and starts
# there. While DeusEx is there the original does not start under Wine or
# Proton; scripts/recreation.sh uninstall takes the four files out.
port_run() {
    local sys="$DX_ROOT/gamefiles/System" f
    [ -f "$sys/DeusEx.exe" ] || die "no Deus Ex in $DX_ROOT/gamefiles (no System/DeusEx.exe)"
    for f in DeusEx run-game.sh SurrealEngine SurrealEngine.pk3; do
        [ -f "$APP/$f" ] || die "no $f in $APP -- scripts/dx.sh stage $PORT"
        cp -p "$APP/$f" "$sys/$f"
    done
    say "installed in $sys: DeusEx run-game.sh SurrealEngine SurrealEngine.pk3"
    exec "$sys/DeusEx" "$@"
}

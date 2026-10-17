# Desktop Linux on x86_64 -- sourced by scripts/dx.sh. See README.md.
PORT_DESC="Desktop Linux, x86_64 (native): DXLauncher, the desktop's own launcher"
PORT_ENGINE=1

# Nothing to download: the launcher and the engine build against the
# distro's own libraries.
port_deps() {
    say "$PORT: nothing to fetch; the engine's packages are the workspace's docs/DEVELOPMENT.md's"
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

# Desktop Linux on x86_64 -- sourced by scripts/dx.sh. See README.md.
PORT_DESC="Desktop Linux, x86_64 (native)"
PORT_ENGINE=1

# Nothing to download: the launcher and the engine build against the
# distro's own libraries. This only says what is missing.
port_deps() {
    command -v pkg-config >/dev/null || die "pkg-config is needed"
    pkg-config --exists sdl2 SDL2_ttf || die "install the SDL2 and SDL2_ttf development packages"
    say "SDL2 $(pkg-config --modversion sdl2), SDL2_ttf $(pkg-config --modversion SDL2_ttf)"
}

# A fresh app directory points at the workspace's own game copy, when there
# is one, so a developer can stage and run with no editing.
port_stage() {
    local fresh="$1"
    if [ "$fresh" = 1 ] && [ -d "$DX_ROOT/gamefiles/System" ]; then
        sed -i "s|^GameDir=.*|GameDir=$DX_ROOT/gamefiles|" "$APP/launcher.ini"
        say "launcher.ini: GameDir=$DX_ROOT/gamefiles"
    fi
}

port_run() {
    exec "$APP/deusex-launcher" "$@"
}

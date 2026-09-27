# TrimUI Smart Pro (spruceOS) -- sourced by scripts/dx.sh. See README.md.
PORT_DESC="TrimUI Smart Pro, spruceOS (aarch64, cross-built)"
PORT_ENGINE=1

# The device. The password is the stock firmware one; SSH is root.
DEVICE="${DEVICE:-192.168.1.211}"
DEVICE_USER="${DEVICE_USER:-spruce}"
DEVICE_PASS="${DEVICE_PASS:-happygaming}"
DEVICE_APPDIR="${DEVICE_APPDIR:-/mnt/SDCARD/App/DeusEx}"

# Two toolchains, both at or below the device's glibc 2.33: the launcher is
# C11 on GCC 9.3, the engine needs C++20 and GCC 10.3.
port_deps() {
    dx_fetch_bootlin aarch64--glibc--stable-2020.08-1        2.33
    dx_fetch_bootlin aarch64--glibc--bleeding-edge-2021.05-1 2.33
    "$PORT_DIR/fetch-sysroot.sh" "$@"
}

#   scripts/dx.sh deploy trimui-smartpro [--no-engine] [--run [-- launcher args]]
#
# Sends build/trimui-smartpro/app (built and staged by dx.sh just before) with
# dx_ssh_sync: changed files only, the device's previous copies kept in
# .prev-<date-time>, every file verified. launcher.ini belongs to the device's
# owner (it holds the path to their game files): it is installed only when
# missing, from launcher.ini.default. exFAT has no permission bits worth
# trusting, so the executables are chmod'ed on arrival.
port_deploy() {
    local run=0 args=() skip=(./launcher.ini)
    while [ $# -gt 0 ]; do
        case "$1" in
            --run)       run=1; shift ;;
            --no-engine) skip+=(./SurrealEngine ./libSurrealVideo.so ./SurrealEngine.pk3); shift ;;
            --)          shift; args=("$@"); break ;;
            *)           die "unknown option: $1" ;;
        esac
    done
    local d="$DEVICE_APPDIR"
    say "deploying $APP to $DEVICE_USER@$DEVICE:$d ..."
    dx_ssh_sync "$APP" "$d" "${skip[@]}"
    dx_ssh "chmod +x '$d'/deusex-launcher '$d'/dxl-cli '$d'/*.sh && { [ ! -f '$d/SurrealEngine' ] || chmod +x '$d/SurrealEngine'; }"
    dx_ssh "[ -s '$d/launcher.ini' ] || cp '$d/launcher.ini.default' '$d/launcher.ini'"
    # MSAA must be off on the PowerVR GE8300 (speckle). The launcher writes
    # Settings.json before every launch and run-game.sh seeds it; this is the
    # last fallback, for an engine run by hand.
    dx_ssh "[ -s '$d/home/.config/SurrealEngine/Settings.json' ] || { mkdir -p '$d/home/.config/SurrealEngine'; cp '$d/engine-settings.json.default' '$d/home/.config/SurrealEngine/Settings.json'; }"
    say "done"
    if [ "$run" = 1 ]; then
        say "--- running ---"
        dx_ssh "cd '$d' && LD_LIBRARY_PATH=/usr/trimui/lib:/usr/lib:/lib ./deusex-launcher ${args[*]:-} 2>&1; echo \"exit=\$?\""
    fi
}

#   scripts/dx.sh profile trimui-smartpro [seconds] [label] [cpu] [turn] [map]
#
# Runs tools/profile-map.sh on the device (its arguments; the engine there must
# be a `vibe/tools/perf/perf.sh on` build) and prints its summary. The script is
# copied over every time: the device's /tmp does not survive a reboot. The whole
# log comes back as build/trimui-smartpro/profile/perf-<label>.log. SHOT,
# SAMPLE and SURREAL_PERF_DETAIL pass through; with SHOT the screen comes back
# as fb-<label>.png beside it (needs ImageMagick), with SAMPLE=1 the CPU
# samples as samples-<label> and samples-<label>.maps, for
# vibe/tools/perf/sample-report.py, with the engine that made them as
# samples-<label>.engine: a sample is an address in one particular build.
port_profile() {
    local label="${2:-run}" out="$BUILD/profile"
    mkdir -p "$out"
    dx_ssh "mkdir -p /tmp/dxl-test && cat > /tmp/dxl-test/profile-map.sh && chmod +x /tmp/dxl-test/profile-map.sh" \
        < "$PORT_DIR/tools/profile-map.sh"
    dx_ssh "SHOT='${SHOT:-}' SAMPLE='${SAMPLE:-}' SURREAL_PERF_DETAIL='${SURREAL_PERF_DETAIL:-}' /tmp/dxl-test/profile-map.sh $*"
    dx_ssh "cat /tmp/dxl-test/perf-$label.log" > "$out/perf-$label.log" && say "log: $out/perf-$label.log"
    if [ "${SAMPLE:-}" = 1 ]; then
        dx_ssh "cat /tmp/dxl-test/samples-$label" > "$out/samples-$label" &&
            dx_ssh "cat /tmp/dxl-test/samples-$label.maps" > "$out/samples-$label.maps" &&
            say "samples: $out/samples-$label (vibe/tools/perf/sample-report.py)"
        local ran staged
        ran=$(dx_ssh "md5sum '$DEVICE_APPDIR/SurrealEngine'" | awk '{ print $1 }')
        staged=$(md5sum "$APP/SurrealEngine" | awk '{ print $1 }')
        if [ -n "$ran" ] && [ "$ran" = "$staged" ]; then
            cp "$APP/SurrealEngine" "$out/samples-$label.engine" && say "engine that ran: $out/samples-$label.engine"
        else
            say "warning: the device's engine is not the staged build -- symbolise with the build that ran"
        fi
    fi
    if [ -n "${SHOT:-}" ]; then
        dx_ssh "cat /tmp/dxl-test/fb-$label.gz" | gunzip -c 2>/dev/null | head -c $((1280 * 720 * 4)) > "$out/fb-$label.bgra" || true
        if command -v magick >/dev/null; then
            magick -size 1280x720 -depth 8 "bgra:$out/fb-$label.bgra" -alpha off "$out/fb-$label.png" &&
                rm -f "$out/fb-$label.bgra" && say "screen: $out/fb-$label.png"
        else
            say "screen (raw BGRA, 1280x720): $out/fb-$label.bgra"
        fi
    fi
}

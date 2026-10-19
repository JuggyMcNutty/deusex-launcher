# TrimUI Smart Pro (spruceOS) -- sourced by scripts/dx.sh. See README.md.
PORT_DESC="TrimUI Smart Pro, spruceOS (aarch64, cross-built)"
PORT_ENGINE=1

# The device. The password is the stock firmware one; SSH is root.
DEVICE="${DEVICE:-192.168.1.211}"
DEVICE_USER="${DEVICE_USER:-spruce}"
DEVICE_PASS="${DEVICE_PASS:-happygaming}"
DEVICE_APPDIR="${DEVICE_APPDIR:-/mnt/SDCARD/App/DeusEx}"

# One toolchain for the launcher and the engine: linux-aarch64's, Godot's
# Buildroot SDK, GCC 13.2 on glibc 2.28, pinned by our own SHA256 (its
# release publishes none).
A64_SDK=aarch64-godot-linux-gnu_sdk-buildroot-2023.08.x-4
A64_SDK_URL=https://github.com/godotengine/buildroot/releases/download/godot-2023.08.x-4/aarch64-godot-linux-gnu_sdk-buildroot.tar.bz2
A64_SDK_SHA256=d7b673823e8078ac70ffa202b4471356eca360b80960c6d1a722af0c03aea29d

port_deps() {
    dx_fetch_buildroot "$A64_SDK" "$A64_SDK_URL" "$A64_SDK_SHA256" 2.28
    dx_fetch_src "$SRC/cmake/deps.lock" sdl3 freetype sdl3_ttf xmp_lite
    "$PORT_DIR/fetch-sysroot.sh" "$@"
}

#   scripts/dx.sh deploy trimui-smartpro [--no-engine] [--run [-- launcher args]]
#
# Sends build/trimui-smartpro/app (built and staged by dx.sh just before) with
# dx_ssh_sync: changed files only, the device's previous copies kept in
# .prev-<date-time>, every file verified. The launcher's own files --
# launcher.ini, the profiles, their saves -- are in the app's home/
# (DXL_HOME, launch.sh), which nothing staged touches. exFAT has no
# permission bits worth trusting, so the executables are chmod'ed on
# arrival. --run starts it as the menu does, through launch.sh, with the
# menu paused while it runs.
port_deploy() {
    local run=0 args=() skip=()
    while [ $# -gt 0 ]; do
        case "$1" in
            --run)       run=1; shift ;;
            --no-engine) skip+=(./DXEngine ./DXEngine.pk3); shift ;;
            --)          shift; args=("$@"); break ;;
            *)           die "unknown option: $1" ;;
        esac
    done
    local d="$DEVICE_APPDIR"
    say "deploying $APP to $DEVICE_USER@$DEVICE:$d ..."
    dx_ssh_sync "$APP" "$d" ${skip[@]+"${skip[@]}"}
    dx_ssh "chmod +x '$d'/deusex-launcher '$d'/dxl-cli '$d'/*.sh && { [ ! -f '$d/DXEngine' ] || chmod +x '$d/DXEngine'; }"
    say "done"
    if [ "$run" = 1 ]; then
        say "--- running ---"
        dx_ssh "M=\$(pidof MainUI); [ -z \"\$M\" ] || kill -STOP \$M; cd '$d' && sh ./launch.sh ${args[*]:-} 2>&1; echo \"exit=\$?\"; [ -z \"\$M\" ] || kill -CONT \$M"
    fi
}

#   scripts/dx.sh profile trimui-smartpro [seconds] [label] [cpu] [turn] [map]
#
# Runs tools/profile-map.sh on the device (its arguments; the engine there must
# be a `tools/perf/perf.sh on` build) and prints its summary. The script is
# copied over every time: the device's /tmp does not survive a reboot. The whole
# log comes back as build/trimui-smartpro/profile/perf-<label>.log. SHOT,
# SAMPLE and DXE_PERF_DETAIL pass through; with SHOT the screen comes back
# as fb-<label>.png beside it (needs ImageMagick), with SAMPLE=1 the CPU
# samples as samples-<label> and samples-<label>.maps, for
# tools/perf/sample-report.py, with the engine that made them as
# samples-<label>.engine: a sample is an address in one particular build.
port_profile() {
    local label="${2:-run}" out="$BUILD/profile"
    mkdir -p "$out"
    dx_ssh "mkdir -p /tmp/dxl-test && cat > /tmp/dxl-test/profile-map.sh && chmod +x /tmp/dxl-test/profile-map.sh" \
        < "$PORT_DIR/tools/profile-map.sh"
    dx_ssh "SHOT='${SHOT:-}' SAMPLE='${SAMPLE:-}' DXE_PERF_DETAIL='${DXE_PERF_DETAIL:-}' /tmp/dxl-test/profile-map.sh $*"
    dx_ssh "cat /tmp/dxl-test/perf-$label.log" > "$out/perf-$label.log" && say "log: $out/perf-$label.log"
    if [ "${SAMPLE:-}" = 1 ]; then
        dx_ssh "cat /tmp/dxl-test/samples-$label" > "$out/samples-$label" &&
            dx_ssh "cat /tmp/dxl-test/samples-$label.maps" > "$out/samples-$label.maps" &&
            say "samples: $out/samples-$label (tools/perf/sample-report.py)"
        local ran staged
        ran=$(dx_ssh "md5sum '$DEVICE_APPDIR/DXEngine'" | awk '{ print $1 }')
        staged=$(md5sum "$APP/DXEngine" | awk '{ print $1 }')
        if [ -n "$ran" ] && [ "$ran" = "$staged" ]; then
            cp "$APP/DXEngine" "$out/samples-$label.engine" && say "engine that ran: $out/samples-$label.engine"
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

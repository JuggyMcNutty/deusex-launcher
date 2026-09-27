#!/bin/sh
# Frame-time profile of one map, on the handheld.
#
# Needs an engine built with VibeEngine's vibe/tools/perf/perf-instrumentation.patch
# applied (the hooks are temporary and never committed -- see the patch header
# comments). From the Port Ex Machina workspace's root:
#
#   engine/SurrealEngine/vibe/tools/perf/perf.sh on
#   scripts/dx.sh deploy trimui-smartpro      (builds and stages it too)
#   scripts/dx.sh profile trimui-smartpro [seconds] [label] [cpu] [turn] [map]
#   engine/SurrealEngine/vibe/tools/perf/perf.sh off
#   scripts/dx.sh deploy trimui-smartpro
#
# dx.sh profile copies this script to the device's /tmp/dxl-test (a reboot
# clears it) and runs it there:
#
#   profile-map.sh [seconds] [label] [cpu] [turn] [map]
#
#   seconds  how long to run (default 60)
#   label    names the log: /tmp/dxl-test/perf-<label>.log
#   cpu      "launcher" (default): the CPU mode launcher.ini's CpuMode names,
#            applied and restored by the app's port-hooks.sh exactly as a
#            launch does, so a profile measures what playing gets. "none"
#            leaves the CPU as the menu has it (power-save).
#   turn     aBaseX to turn the player on the spot (3000 = keyboard turn speed),
#            so an unattended run still brings new surfaces into view
#   map      default 01_NYC_UNATCOIsland.dx (Liberty Island). The engine only
#            takes --url=<map>; "-u <map>" silently loads the default map.
#
# The engine runs with the device's own Settings.json, and the log's settings:
# line records its Distant AI and render scale.
#
# SURREAL_PERF_DETAIL=1 in the environment adds tick by actor class and script
# functions by self time; those hooks slow what they measure. SAMPLE=1 samples
# the main thread's CPU every millisecond into /tmp/dxl-test/samples-<label>
# (and .maps), for vibe/tools/perf/sample-report.py on the PC. SHOT=<seconds>
# (a multiple of 5) saves the screen at that point to
# /tmp/dxl-test/fb-<label>.gz -- the raw framebuffer, gzipped; its first
# 1280x720 is BGRA (magick -size 1280x720 -depth 8 bgra:fb -alpha off out.png).
#
# The spruceOS menu is paused while the engine runs (two programs drawing to
# one framebuffer fight) and resumed on exit. The engine ignores SIGTERM, so
# it is stopped with SIGKILL; that leaves Running.ini like any crash.
SECS=${1:-60}; LABEL=${2:-run}; MODE=${3:-launcher}; TURN=${4:-0}; MAP=${5:-01_NYC_UNATCOIsland.dx}
APPDIR=/mnt/SDCARD/App/DeusEx
OUT=/tmp/dxl-test/perf-$LABEL.log
LOG=$OUT    # where port-hooks.sh reports the CPU mode
mkdir -p /tmp/dxl-test
: > $OUT
M=$(pidof MainUI)
[ -n "$M" ] && kill -STOP $M
cpu_after() { :; }
restore() { cpu_after; [ -n "$M" ] && kill -CONT $M; }
trap restore EXIT INT TERM HUP
if [ "$MODE" = "launcher" ]; then
    . $APPDIR/port-hooks.sh
    port_before_game
    cpu_after() { port_after_game; cpu_after() { :; }; }
fi
C=/sys/devices/system/cpu
echo "cpu: online=$(cat $C/online) gov=$(cat $C/cpu0/cpufreq/scaling_governor) max=$(cat $C/cpu0/cpufreq/scaling_max_freq) map=$MAP" >> $OUT
# The engine runs with the device's own Settings.json: record what changes the frame.
echo "settings: $(grep -E '"(AiLevelOfDetail|RenderScale)"' $APPDIR/home/.config/SurrealEngine/Settings.json | tr -d ' ",' | tr '\n' ' ')" >> $OUT
cd /mnt/SDCARD/Roms/PORTS/DeusEx || exit 1
export HOME=$APPDIR/home
export LD_LIBRARY_PATH=$APPDIR:/usr/trimui/lib:/usr/lib:/lib
export SURREALWIDGETS_DISPLAY_BACKEND=SDL2 SURREAL_PERF_LOG=1 SURREAL_PERF_TURN=$TURN
rm -f /tmp/dxl-test/samples-$LABEL /tmp/dxl-test/samples-$LABEL.maps
[ "${SAMPLE:-}" = 1 ] && export SURREAL_PERF_SAMPLE=/tmp/dxl-test/samples-$LABEL
setsid $APPDIR/SurrealEngine --no-launcher /mnt/SDCARD/Roms/PORTS/DeusEx "--url=$MAP" </dev/null >>$OUT 2>&1 &
PID=$!
i=0
while [ $i -lt "$SECS" ]; do
    sleep 5; i=$((i + 5))
    echo "t=$i cpu: $(cat $C/cpu0/cpufreq/scaling_cur_freq) load: $(cat /proc/loadavg)" >> $OUT
    [ "$i" = "${SHOT:-}" ] && cat /dev/fb0 | gzip -1 -f > /tmp/dxl-test/fb-$LABEL.gz
done
kill -9 $PID 2>/dev/null
sleep 1
echo "engines left: $(pidof SurrealEngine | wc -w)" >> $OUT
cpu_after
grep -E "^cpu|^settings|^perf:|engines left" $OUT

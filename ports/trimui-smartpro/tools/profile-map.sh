#!/bin/sh
# Frame-time profile of one map, on the handheld.
#
# Needs an engine built with DXEngine's tools/perf/perf-instrumentation.patch
# applied (the hooks are temporary and never committed -- see the patch header
# comments). From the Port Ex Machina workspace's root:
#
#   ../DXEngine/tools/perf/perf.sh on
#   scripts/dx.sh deploy trimui-smartpro      (builds and stages it too)
#   scripts/dx.sh profile trimui-smartpro [seconds] [label] [cpu] [turn] [map]
#   ../DXEngine/tools/perf/perf.sh off
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
#   map      default 01_NYC_UNATCOIsland.dx (Liberty Island). The engine takes
#            --url=<map> (or -u=<map>); "-u <map>" silently loads the default map.
#
# The engine runs as the launcher starts it for the profile in use, its
# command line dxl-cli --print-command's (the profile's inis, its engine
# folder's Settings.json), with --url=<map> added; the log's settings: line
# records that Settings.json's render scale.
#
# DXE_PERF_DETAIL=1 in the environment adds tick by actor class and script
# functions by self time; those hooks slow what they measure. SAMPLE=1 samples
# the main thread's CPU (asked for every millisecond; the device's
# tick gives one every 4 ms) into /tmp/dxl-test/samples-<label>
# (and .maps), for tools/perf/sample-report.py on the PC. SHOT=<seconds>
# (a multiple of 5) saves the screen at that point to
# /tmp/dxl-test/fb-<label>.gz -- the raw framebuffer, gzipped; its first
# 1280x720 is BGRA (magick -size 1280x720 -depth 8 bgra:fb -alpha off out.png).
#
# The spruceOS menu is paused while the engine runs (two programs drawing to
# one framebuffer fight) and resumed on exit. The engine is stopped with
# SIGKILL, at once, not at its next frame; started without the launcher, it
# leaves no Running.ini.
SECS=${1:-60}; LABEL=${2:-run}; MODE=${3:-launcher}; TURN=${4:-0}; MAP=${5:-01_NYC_UNATCOIsland.dx}
APPDIR=/mnt/SDCARD/App/DeusEx
export DXL_HOME=$APPDIR/home
INI=$DXL_HOME/config/dxlauncher/launcher.ini
OUT=/tmp/dxl-test/perf-$LABEL.log
mkdir -p /tmp/dxl-test
: > $OUT
export LD_LIBRARY_PATH=$APPDIR:/usr/trimui/lib:/usr/lib:/lib
CMD=$(cd $APPDIR && DXL_ENGINE_ARGS="--url=$MAP" ./dxl-cli --print-command 2>>$OUT | head -n 1)
case "$CMD" in
    cd\ *) ;;
    *) echo "no profile to play: start the launcher once (dxl-cli --print-command said: $CMD)" | tee -a $OUT; exit 1 ;;
esac
M=$(pidof MainUI)
[ -n "$M" ] && kill -STOP $M
cpu_after() { :; }
restore() { cpu_after; [ -n "$M" ] && kill -CONT $M; }
trap restore EXIT INT TERM HUP
if [ "$MODE" = "launcher" ]; then
    cpu_mode=$(sed -n 's/^[[:space:]]*CpuMode[[:space:]]*=[[:space:]]*//p' "$INI" 2>/dev/null | tr -d '\r' | head -n 1)
    sh $APPDIR/port-hooks.sh before "${cpu_mode:-Overclock}" >> $OUT 2>&1
    cpu_after() { sh $APPDIR/port-hooks.sh after >> $OUT 2>&1; cpu_after() { :; }; }
fi
C=/sys/devices/system/cpu
echo "cpu: online=$(cat $C/online) gov=$(cat $C/cpu0/cpufreq/scaling_governor) max=$(cat $C/cpu0/cpufreq/scaling_max_freq) map=$MAP" >> $OUT
# The profile in use's Settings.json, as the engine reads it: what changes the frame.
PROFILE=$(sed -n 's/^[[:space:]]*Profile[[:space:]]*=[[:space:]]*//p' "$INI" 2>/dev/null | tr -d '\r' | head -n 1)
JSON=$DXL_HOME/config/dxlauncher/profiles/${PROFILE:-default}/engine/Settings.json
echo "settings: $(grep -E '"(RenderScale|Type|Antialias)"' "$JSON" | tr -d ' ",' | tr '\n' ' ')" >> $OUT
export DXE_PERF_LOG=1 DXE_PERF_TURN=$TURN
rm -f /tmp/dxl-test/samples-$LABEL /tmp/dxl-test/samples-$LABEL.maps
[ "${SAMPLE:-}" = 1 ] && export DXE_PERF_SAMPLE=/tmp/dxl-test/samples-$LABEL
setsid sh -c "$CMD" </dev/null >>$OUT 2>&1 &
i=0
while [ $i -lt "$SECS" ]; do
    sleep 5; i=$((i + 5))
    echo "t=$i cpu: $(cat $C/cpu0/cpufreq/scaling_cur_freq) load: $(cat /proc/loadavg)" >> $OUT
    [ "$i" = "${SHOT:-}" ] && cat /dev/fb0 | gzip -1 -f > /tmp/dxl-test/fb-$LABEL.gz
done
# shellcheck disable=SC2046
kill -9 $(pidof DXEngine) 2>/dev/null
sleep 1
echo "engines left: $(pidof DXEngine | wc -w)" >> $OUT
cpu_after
grep -E "^cpu|^settings|^perf:|engines left" $OUT

#!/bin/sh
# The Smart Pro's game hook: the CPU mode while the game runs.
#
#   sh port-hooks.sh before <mode>   just before the engine starts
#   sh port-hooks.sh after           once it has ended, however it ended
#
# DXLauncher runs it (src/app/gamehook.c), each call at most 10 s, its
# output in the run's last-run.log. A missing or failing hook never stops
# the game.
#
# The spruceOS menu leaves the handheld in power-save -- two cores, the
# conservative governor -- which costs the game about a third of its frame
# rate. before applies the mode (launcher.ini's [Launcher] CpuMode: Smart,
# Performance or Overclock, src/core/cpumode.c) with spruceOS's own
# helpers, the ones its Ports launcher uses; after puts back the state
# before saved, so the menu gets what it had.
#
# The state is kept in a file, written by before only when there is none: a
# launcher killed mid-game leaves the boost on, and the next game's before
# keeps the state from before it, which its after then puts back. The
# helpers are sourced in subshells only: helperFunctions.sh exports its own
# LD_LIBRARY_PATH.
#
# For tests: DXL_HOOK_CPU (the sysfs cpu folder), DXL_HOOK_HELPERS (spruce's
# helpers), DXL_HOOK_STATE (the state file).
CPU=${DXL_HOOK_CPU:-/sys/devices/system/cpu}
HELPERS=${DXL_HOOK_HELPERS:-/mnt/SDCARD/spruce/scripts/helperFunctions.sh}
STATE=${DXL_HOOK_STATE:-/tmp/dxlauncher-cpu}
# For a mode this script does not know: src/core/cpumode.c's default
# (tests/test_cpumode.c checks).
MODE_DEFAULT=Overclock

cpu_now() {
    echo "online $(cat $CPU/online), $(cat $CPU/cpu0/cpufreq/scaling_governor)" \
         "$(cat $CPU/cpu0/cpufreq/scaling_min_freq)-$(cat $CPU/cpu0/cpufreq/scaling_max_freq)"
}

# Runs inside the helpers' subshell: online governor min max.
restore_cpu_state() {
    # "0,3" or "0-3" -> the digit string spruce's cores_online takes
    cores=""
    for part in $(echo "$1" | tr ',' ' '); do
        case "$part" in
            *-*) i=${part%-*}
                 while [ "$i" -le "${part#*-}" ]; do cores="$cores$i"; i=$((i + 1)); done ;;
            *)   cores="$cores$part" ;;
        esac
    done
    cores_online "$cores" >/dev/null 2>&1
    unlock_governor >/dev/null 2>&1
    echo "$2" > $CPU/cpu0/cpufreq/scaling_governor
    # min and max each must stay within the other: write min, max, then min
    # again so the order works whichever way the range is moving.
    echo "$3" > $CPU/cpu0/cpufreq/scaling_min_freq 2>/dev/null
    echo "$4" > $CPU/cpu0/cpufreq/scaling_max_freq
    echo "$3" > $CPU/cpu0/cpufreq/scaling_min_freq
    lock_governor >/dev/null 2>&1
}

[ -f "$HELPERS" ] || { echo "cpu mode: no spruceOS helpers ($HELPERS), the CPU as it is"; exit 0; }

case "$1" in
    before)
        mode=$2
        case "$mode" in
            Smart|Performance|Overclock) ;;
            *) echo "cpu mode: '$mode' unknown, $MODE_DEFAULT"; mode=$MODE_DEFAULT ;;
        esac
        if [ -s "$STATE" ]; then
            echo "cpu mode: the state before an earlier game kept ($(cat "$STATE"))"
        else
            echo "$(cat $CPU/online) $(cat $CPU/cpu0/cpufreq/scaling_governor)" \
                 "$(cat $CPU/cpu0/cpufreq/scaling_min_freq) $(cat $CPU/cpu0/cpufreq/scaling_max_freq)" > "$STATE"
        fi
        (
            . "$HELPERS" >/dev/null 2>&1
            case "$mode" in
                Smart)       set_smart       >/dev/null 2>&1 ;;
                Performance) set_performance >/dev/null 2>&1 ;;
                Overclock)   set_overclock   >/dev/null 2>&1 ;;
            esac
        )
        echo "cpu mode: $mode ($(cpu_now))"
        ;;
    after)
        [ -s "$STATE" ] || { echo "cpu mode: no state to put back"; exit 0; }
        # shellcheck disable=SC2046
        ( . "$HELPERS" >/dev/null 2>&1; restore_cpu_state $(cat "$STATE") )
        rm -f "$STATE"
        echo "cpu restored: $(cpu_now)"
        ;;
    *)
        echo "usage: port-hooks.sh before <mode> | after" >&2
        exit 2
        ;;
esac
exit 0

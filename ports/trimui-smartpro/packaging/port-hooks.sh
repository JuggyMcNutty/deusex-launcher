# Sourced by run-game.sh (ports/common/packaging) on the TrimUI Smart Pro.

# The vendor SDL2 lives in /usr/trimui/lib and carries the "mali" video driver
# that wires Vulkan surface creation to the PowerVR implementation -- the only
# display path here, presenting at 1280x720 via VK_KHR_display.
PORT_LIB_PATH="/usr/trimui/lib:/usr/lib:/lib"

# CPU mode. The spruceOS menu leaves the handheld in power-save -- two cores,
# conservative governor -- which costs the game about a third of its frame
# rate. Apply the launcher's CpuMode (launcher.ini) with spruceOS's own
# helpers, the ones its Ports launcher uses, and put the previous state back
# on the way out so the menu gets what it had.
#
# The helpers are sourced in subshells only: helperFunctions.sh exports its
# own LD_LIBRARY_PATH, which would hide libSurrealVideo.so from the engine.
CPU=/sys/devices/system/cpu
HELPERS=/mnt/SDCARD/spruce/scripts/helperFunctions.sh
cpu_saved=""
# When launcher.ini names no mode, or one this script does not know. The same
# as target.c's cpu_mode_default (tests/test_target_port.c checks).
CPU_MODE_DEFAULT=Overclock

port_before_game() {
    cpu_mode=$(sed -n 's/^[[:space:]]*CpuMode[[:space:]]*=[[:space:]]*//p' "$APPDIR/launcher.ini" 2>/dev/null | tr -d '\r' | head -n 1)
    case "$cpu_mode" in
        Smart|Performance|Overclock) ;;
        *) cpu_mode=$CPU_MODE_DEFAULT ;;
    esac
    [ -f "$HELPERS" ] || return 0
    cpu_saved="$(cat $CPU/online) $(cat $CPU/cpu0/cpufreq/scaling_governor) $(cat $CPU/cpu0/cpufreq/scaling_min_freq) $(cat $CPU/cpu0/cpufreq/scaling_max_freq)"
    (
        . "$HELPERS" >/dev/null 2>&1
        case "$cpu_mode" in
            Smart)       set_smart       >/dev/null 2>&1 ;;
            Performance) set_performance >/dev/null 2>&1 ;;
            Overclock)   set_overclock   >/dev/null 2>&1 ;;
        esac
    )
    echo "cpu mode: $cpu_mode (online $(cat $CPU/online), $(cat $CPU/cpu0/cpufreq/scaling_governor) up to $(cat $CPU/cpu0/cpufreq/scaling_max_freq))" >> "$LOG"
}

port_after_game() {
    [ -n "$cpu_saved" ] || return 0
    ( . "$HELPERS" >/dev/null 2>&1; restore_cpu_state $cpu_saved )
    echo "cpu restored: online $(cat $CPU/online), $(cat $CPU/cpu0/cpufreq/scaling_governor)" >> "$LOG"
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

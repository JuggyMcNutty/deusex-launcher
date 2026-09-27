#!/bin/sh
# Hands off from the launcher to the engine. Shared by every port.
#
# The launcher has already settled configuration (Settings.json, the game's
# ini files, FirstRun) and created the crash sentinel, and it chdir's to
# GameDir before exec'ing this script. Everything here is about starting
# Surreal Engine and honouring the sentinel contract on the way out.
#
# A port adjusts this through port-hooks.sh beside it (see Port Ex Machina's docs/PORTING.md):
#   PORT_LIB_PATH       extra library directories, searched after the app dir
#   port_env            runs once the environment below is set up
#   port_before_game    runs just before the engine starts (e.g. CPU mode)
#   port_after_game     runs after it exits, with its status in $status

APPDIR="$(cd "$(dirname "$0")" && pwd)"
GAMEDIR="$(pwd)"
LOG="$APPDIR/run-game.log"

PORT_LIB_PATH=""
port_env()         { :; }
port_before_game() { :; }
port_after_game()  { :; }
[ -f "$APPDIR/port-hooks.sh" ] && . "$APPDIR/port-hooks.sh"

# APPDIR first: the engine links libSurrealVideo.so, which ships beside it.
export LD_LIBRARY_PATH="$APPDIR${PORT_LIB_PATH:+:$PORT_LIB_PATH}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# The engine reads Settings.json and writes SE-Log-LastRun.txt under
# $HOME/.config/SurrealEngine. Pin HOME to the app directory so both live
# where the launcher looks for them (and are readable over SSH on a device),
# instead of wherever HOME points.
export HOME="$APPDIR/home"
mkdir -p "$HOME/.config/SurrealEngine"

# The launcher writes Settings.json before every launch; this seed only
# matters when something runs the script without it. Without the file the
# engine falls back to its own defaults, 4x MSAA included.
if [ ! -s "$HOME/.config/SurrealEngine/Settings.json" ]; then
    cp "$APPDIR/engine-settings.json.default" "$HOME/.config/SurrealEngine/Settings.json"
fi

# SDL2 is the display backend with the fork's gamepad support
# (engine patch 0003); the others report no pad.
export SURREALWIDGETS_DISPLAY_BACKEND="${SURREALWIDGETS_DISPLAY_BACKEND:-SDL2}"

port_env

{
    echo "--- $(date '+%Y-%m-%d %H:%M:%S') ---"
    echo "gamedir: $GAMEDIR"
    echo "args:    $*"
} >> "$LOG"

if [ ! -x "$APPDIR/SurrealEngine" ]; then
    echo "SurrealEngine binary missing -- nothing to launch" >> "$LOG"
    exit 127
fi

port_before_game

"$APPDIR/SurrealEngine" --no-launcher "$GAMEDIR" "$@" >> "$LOG" 2>&1
status=$?
echo "engine exit: $status" >> "$LOG"

port_after_game

# Set DXL_SIMULATE_CRASH=1 to leave the sentinel behind whatever the engine did,
# so the recovery path can be exercised without actually crashing anything.
if [ "$DXL_SIMULATE_CRASH" = "1" ]; then
    echo "DXL_SIMULATE_CRASH=1, leaving Running.ini in place" >> "$LOG"
    exit 1
fi

# The sentinel is the launcher's crash contract: clear it only on a clean exit,
# so an engine crash still produces the crash notice on the next launch.
if [ "$status" -eq 0 ]; then
    rm -f "$GAMEDIR/System/Running.ini"
    echo "clean exit, removed Running.ini" >> "$LOG"
else
    echo "unclean exit, leaving Running.ini for recovery" >> "$LOG"
fi

exit $status

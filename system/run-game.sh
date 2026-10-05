#!/bin/sh
# The game, as DeusEx starts it: the engine beside this script -- VibeEngine's
# SurrealEngine, installed in the game's System folder with it -- given the
# original's command line as the player gave it.
#
# DeusEx runs this from System/, with its command line's words as the
# arguments, and waits for it: the engine's output goes into DeusEx's log,
# and DXL_LAUNCHER_FD, when set, is the line between the two (main's README.md,
# "The game and the launcher"). The engine reads and writes the game's own
# DeusEx.ini and User.ini, unless the command line's INI= and USERINI= name
# others. Running.ini is the launcher's business.
here="$(cd "$(dirname "$0")" && pwd)"
game="$(dirname "$here")"
engine="$here/SurrealEngine"
if [ ! -x "$engine" ]; then
    echo "run-game.sh: no engine at $engine" >&2
    exit 127
fi

# libSurrealVideo.so is beside the engine.
export LD_LIBRARY_PATH="$here${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# SDL2: the display backend with the fork's pad support.
export SURREALWIDGETS_DISPLAY_BACKEND="${SURREALWIDGETS_DISPLAY_BACKEND:-SDL2}"

# DXL_ENGINE_ARGS: more of the engine's own options, for a scripted run
# (--timeline=<file>, VibeEngine's ENGINE.md).
exec "$engine" --no-launcher "$game" --ini="$here/DeusEx.ini" --userini="$here/User.ini" \
    ${DXL_ENGINE_ARGS:-} --cmdline="$*"

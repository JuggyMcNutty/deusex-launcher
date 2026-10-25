#!/bin/sh
# The game, as DeusEx starts it: the engine beside this script -- DXEngine,
# installed in the game's System folder with it -- given the
# original's command line as the player gave it.
#
# DeusEx runs this from System/, with its command line as the one argument,
# quotes and all, and waits for it: the engine's output goes into DeusEx's log,
# and DXL_LAUNCHER_FD, when set, is the line between the two (main's README.md,
# "The game and the launcher"). The engine reads and writes the game's own
# DeusEx.ini and User.ini, unless the command line's INI= and USERINI= name
# others. Running.ini is the launcher's business.
here="$(cd "$(dirname "$0")" && pwd)"
game="$(dirname "$here")"
engine="$here/DXEngine"
if [ ! -x "$engine" ]; then
    echo "run-game.sh: no engine at $engine" >&2
    exit 127
fi

# SDL3: the window of the engine linux-x86_64 builds, which the recreation
# installs (Port Ex Machina's scripts/recreation.sh), the one with the pad.
export DXE_DISPLAY_BACKEND="${DXE_DISPLAY_BACKEND:-SDL3}"

# DXL_ENGINE_ARGS: more of the engine's own options, for a scripted run
# (--timeline=<file>, DXEngine's ENGINE.md).
# DXL_PACKAGE: the launcher's package, its own name (DeusEx, or a mod's it
# is renamed to): the engine plays that package's ini, as the launcher
# reads and writes it. --verbose streams the engine's log into DeusEx's;
# without --no-launcher the engine opens its own error window on a critical
# error, as the original's box shows one.
pkg="${DXL_PACKAGE:-DeusEx}"
exec "$engine" --verbose "$game" --ini="$here/$pkg.ini" --userini="$here/User.ini" \
    ${DXL_ENGINE_ARGS:-} --cmdline="$1"

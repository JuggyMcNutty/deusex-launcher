#!/bin/sh
# spruceOS app entry point: DXLauncher.
#
# Follows the convention the shipped apps use (see App/Moonlight/launch.sh):
# source the helpers, keep the device awake, run, then clear the console the
# app scribbled on. MainUI is not killed here -- the App launcher owns that,
# and killing it ourselves leaves the menu in a bad state on return.
#
# Nothing here calls the helpers; what they export (their LD_LIBRARY_PATH
# among it, kept below) reaches the launcher and the engine, so they are
# sourced where they are, as port-hooks.sh does: sh ends at a . of a
# missing file, and a spruceOS that moves them would start nothing.
HELPERS=/mnt/SDCARD/spruce/scripts/helperFunctions.sh
[ -f "$HELPERS" ] && . "$HELPERS"

APPDIR="$(cd "$(dirname "$0")" && pwd)"
cd "$APPDIR" || exit 1

# The launcher's files -- launcher.ini, the profiles, their saves, logs --
# in the app's own folder on the card, whatever the helpers export.
export DXL_HOME="$APPDIR/home"

# For the engine, which the launcher starts: it links the vendor SDL2 in
# /usr/trimui/lib, the device's only display path for it. The launcher
# itself links nothing of the device's. No SDL_ variable here: the engine's
# SDL2 would read it too.
export LD_LIBRARY_PATH="/usr/trimui/lib:/usr/lib:/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

echo 1 > /tmp/stay_awake

./deusex-launcher "$@"
status=$?

rm -f /tmp/stay_awake
printf "\033c" > /dev/tty0

exit $status

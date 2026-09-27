#!/bin/sh
# spruceOS app entry point.
#
# Follows the convention the shipped apps use (see App/Moonlight/launch.sh):
# source the helpers, keep the device awake, run, then clear the console the
# SDL app scribbled on. MainUI is not killed here -- the App launcher owns
# that, and killing it ourselves leaves the menu in a bad state on return.
. /mnt/SDCARD/spruce/scripts/helperFunctions.sh

APPDIR="$(dirname "$0")"
cd "$APPDIR" || exit 1

# The launcher links the vendor SDL2, which carries the "mali" EGL video
# driver; without this it would find nothing to draw on.
export LD_LIBRARY_PATH="/usr/trimui/lib:/usr/lib:/lib:$LD_LIBRARY_PATH"

echo 1 > /tmp/stay_awake

chmod +x ./deusex-launcher ./dxl-cli ./run-game.sh 2>/dev/null

# The launcher execs the game (or re-execs itself for safe mode), so this
# script sees a single exit status for the whole chain.
./deusex-launcher "$@"
status=$?

rm -f /tmp/stay_awake
printf "\033c" > /dev/tty0

exit $status

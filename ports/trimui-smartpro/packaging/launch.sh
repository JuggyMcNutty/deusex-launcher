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
#
# For tests: DXL_LAUNCH_CARD (the card's top folder).
CARD=${DXL_LAUNCH_CARD:-/mnt/SDCARD}
HELPERS=$CARD/spruce/scripts/helperFunctions.sh
[ -f "$HELPERS" ] && . "$HELPERS"

APPDIR="$(cd "$(dirname "$0")" && pwd)"
cd "$APPDIR" || exit 1

# The launcher's files -- launcher.ini, the profiles, their saves and shots,
# logs -- in the card's Saves, whatever the helpers export: a folder of
# their own there, named for the launcher, no emulator's, which a clean
# install of a new release (the app's folder deleted first) and spruceOS's
# own update both keep (bugs.md D.91). The app's own home, where an earlier
# release kept them, moved there once: a rename on the one card, never a
# copy, so that nothing is lost half-way. Where the move cannot be made, or
# both hold the launcher's files, a line says so in launch.log beside them.
HOME_NEW="$CARD/Saves/DXLauncher"
HOME_OLD="$APPDIR/home"
NOTE=""
say() { NOTE="$NOTE$*
"; echo "launch.sh: $*" >&2; }
holds() { [ -d "$1" ] && [ -n "$(ls -A "$1" 2>/dev/null)" ]; }
# A home with a profile: what a start writes alone (launch.log, the
# launcher's log) is no player's.
profiled() { holds "$1/config/dxlauncher/profiles"; }
mount_of() { df -P "$1" 2>/dev/null | awk 'NR == 2 { print $6 }'; }
export DXL_HOME="$HOME_NEW"
if holds "$HOME_OLD"; then
    mkdir -p "$CARD/Saves"
    MOUNT_OLD="$(mount_of "$HOME_OLD")"
    if holds "$HOME_NEW" && { profiled "$HOME_NEW" || ! profiled "$HOME_OLD"; }; then
        say "$HOME_OLD left as it is: $HOME_NEW holds the launcher's files already, and they are used"
    elif [ -z "$MOUNT_OLD" ] || [ "$MOUNT_OLD" != "$(mount_of "$CARD/Saves")" ]; then
        DXL_HOME="$HOME_OLD"
        say "$HOME_OLD not moved: $CARD/Saves is on another card or file system, or df could not say; it is used where it is"
    else
        # A new home with no profile, where the old has one, is put aside,
        # never removed: a start of this release before the old home was
        # put back would otherwise hide that home for good.
        if holds "$HOME_NEW"; then
            ASIDE="$HOME_NEW.unused"
            n=1
            while [ -e "$ASIDE" ]; do ASIDE="$HOME_NEW.unused$n"; n=$((n + 1)); done
            mv "$HOME_NEW" "$ASIDE" 2>/dev/null && say "$HOME_NEW, which held no profile, put aside as $ASIDE"
        else
            rmdir "$HOME_NEW" 2>/dev/null
        fi
        # Never into a folder left there: mv would move the old home inside it.
        if [ ! -e "$HOME_NEW" ] && mv "$HOME_OLD" "$HOME_NEW" 2>/dev/null && [ -d "$HOME_NEW" ] && [ ! -e "$HOME_OLD" ]; then
            say "$HOME_OLD moved to $HOME_NEW"
        else
            DXL_HOME="$HOME_OLD"
            say "$HOME_OLD could not be moved to $HOME_NEW; it is used where it is"
        fi
    fi
elif [ -d "$HOME_OLD" ]; then
    rmdir "$HOME_OLD" 2>/dev/null
fi
mkdir -p "$DXL_HOME" && printf '%s' "$NOTE" > "$DXL_HOME/launch.log"

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

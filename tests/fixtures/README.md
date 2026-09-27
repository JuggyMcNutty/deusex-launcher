# Test fixtures

Stand-ins for the game's own `System/` files, written for this repository --
the game's files are not ours to publish, so none are here. Each has the
features of the real file that the tests depend on: CRLF throughout, repeated
keys (`Paths=`, `Suppress=`, `ServerActors=`, `EditPackages=`), `=` inside
values, empty values, bracketed `Aliases[n]` values with a double space
(`Axis aBaseY  Speed=`), and the shipped joystick lines.

`DeusEx.ini` and `Default.ini` are the same file, as they are in a fresh
install.

The same checks against the real files are in `tests/test_gamefiles.c`: it
runs when `gamefiles/System` holds an install and is skipped otherwise.

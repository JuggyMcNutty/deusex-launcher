# Port: Linux x86_64 -- the development platform

Desktop Linux (or a handheld PC running it), built natively. The unit
tests, the engine's Vulkan validation and the side-by-side checks of engine
changes run here.

## Status

The launcher is DXLauncher, the desktop's own
([`docs/LAUNCHER.md`](../../docs/LAUNCHER.md)): a window with Home, the first
run, Settings, Profiles, Recovery, the log and About, which makes profiles
from the game's files, keeps their settings and plays them. The engine runs the game, not yet fully playable
([where the engine still differs](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/NATIVES.md)).

## Build and run

Needs a C compiler, CMake 3.21+, pkg-config, and SDL3 and SDL3_ttf 3.2 or
newer, the window's, as the distro has them; without them,
`scripts/dx.sh deps linux-x86_64` fetches their sources and the build makes
its own. The engine adds a C++20 compiler and its own dependencies
([the workspace's list](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#dependencies)).

```sh
scripts/dx.sh fetch                  # once: the engine, the launcher's branches, the RE
scripts/dx.sh build linux-x86_64     # launcher, then engine
scripts/dx.sh test                   # unit tests
scripts/dx.sh stage linux-x86_64     # build/linux-x86_64/app: the launcher and the engine
scripts/dx.sh run   linux-x86_64 [--fresh] [-- <the launcher's words>]
```

`run` starts the staged `deusex-launcher` where it is staged, on the
`gamefiles/` beside the repositories (`--game=`): nothing is copied into
the game's folder, and the launcher's files go in the home's XDG folders
([where everything lives](../../docs/LAUNCHER.md#where-everything-lives)).
`--fresh` gives it a scratch home instead, `build/linux-x86_64/fresh-home`,
emptied first: a first run from nothing. `-- --play` skips Home.

The game's settings are the profile's inis, made from the game's own on the
first run, and `Settings.json` in the profile's engine folder, the engine's
renderer and the fork's own drawing settings
([settings and environment](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#settings-and-environment)):
the launcher's Settings keeps the three
([Settings](../../docs/LAUNCHER.md#settings)).

The engine also starts on its own, straight into a map
([running it](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#running-it)).

## What it is made of

| File | What it is |
|---|---|
| `port.cmake` | no glibc ceiling and no sysroot: the binaries run where they are built; SDL3 and SDL3_ttf the distro's, through pkg-config, else built from `cmake/deps.lock`'s sources (`cmake/deps.cmake`) |
| `engine.cmake` | the engine with the SDL2, X11 and Wayland display backends, not SDL3: SurrealWidgets builds one SDL backend, SDL3's when both are on, and only SDL2's has the fork's gamepad support; the launcher asks for SDL2 at run time |
| `port.sh` | `run` starts the staged launcher on `gamefiles/`, in a scratch home with `--fresh` |

Beside them, at the branch's root: the port's preset in `CMakePresets.json`,
and its block in `CMakeLists.txt` with the `scripts/check-abi.sh` it runs,
both linux-aarch64's too: a cross build's sysroot and glibc ceiling,
checked after each build; this port has neither.

## Verified

- Arch Linux, KDE Plasma on Wayland, AMD GPU (Mesa's RADV and radeonsi),
  the window on SDL3's OpenGL renderer: `scripts/dx.sh run linux-x86_64
  --fresh`, driven by a UI script, goes through the first run on
  `gamefiles/` to Home and plays, the engine on the desktop's display, to a
  clean end, and the launcher exits, with nothing newer in `gamefiles/` or
  `~/.config/SurrealEngine`; a crashing stand-in engine brings the window
  back on Recovery, the next start opens on it, and a second start while it
  is up is handed over. Settings, by `tests/ui/settings_roundtrip.script`:
  a row of each kind changed on each page, then the engine played on them
  in a window of the size chosen to a clean end, each value kept through
  its rewrite of the profile's files. Unattended engine runs drive saves,
  loads and hub travel
  ([scripted runs](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/DEVELOPMENT.md#scripted-runs-of-both-engines)).

## Gotchas

### Where it runs

The build has no glibc ceiling: it runs on the system it was built on, or
one as new. Built in a container whose glibc is newer than its host's (an
Arch container on Bazzite: 2.44 and 2.43), the staged launcher may start on
the host while its engine does not; the launcher refuses the engine and says
so ("needs glibc 2.44, this system has 2.43"). Run it in the container, or
choose an engine built for the host (Settings > Launcher, The engine).

### Audio

OpenAL Soft reaches the desktop's audio through `libpipewire` or `libpulse`.
Without either (a minimal container) it falls back to ALSA, which usually
cannot open the device a sound server holds, and the engine stops at
start-up with `Failed to initialize OpenAL device`. Install one, or play
silently with the null driver (`ALSOFT_DRIVERS=null` does the same without
a file):

```sh
printf '[general]\ndrivers = null\n' > /tmp/alsoft-null.conf
ALSOFT_CONF=/tmp/alsoft-null.conf scripts/dx.sh run linux-x86_64
```

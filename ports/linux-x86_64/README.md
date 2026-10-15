# Port: Linux x86_64 -- the development platform

Desktop Linux (or a handheld PC running it), built natively against the
distro's SDL2. The unit tests, the engine's Vulkan validation and the
side-by-side checks of engine changes run here.

## Status

The launcher is `main`'s for now, the original recreated
([`main`'s README](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md));
the desktop's own is planned
([to do](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/AGENTS.md#to-do)).
The engine runs the game, not yet fully playable
([where the engine still differs](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/NATIVES.md)).

## Build and run

Needs a C compiler, CMake 3.21+, pkg-config, and the SDL2 and SDL2_ttf
development packages; the engine adds a C++20 compiler and its own
dependencies
([the workspace's list](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#dependencies)).

```sh
scripts/dx.sh fetch                  # once: the engine, the launcher's branches, the RE
scripts/dx.sh deps  linux-x86_64     # only checks for SDL2 and SDL2_ttf
scripts/dx.sh build linux-x86_64     # launcher, then engine
scripts/dx.sh test                   # unit tests
scripts/dx.sh stage linux-x86_64     # build/linux-x86_64/app: DeusEx and the engine
scripts/dx.sh run   linux-x86_64
```

`DeusEx` finds the game by its own folder, as the original's does, so `run`
puts the staged app into the `System/` of the `gamefiles/` beside the
repositories and starts `System/DeusEx` there, with the command line given
to `run`. While `DeusEx` is there the original does not start under Wine or
Proton; Port Ex Machina's `scripts/recreation.sh uninstall` takes the four
files out.

The game's settings are the original's: `run-game.sh` gives the engine the
game's own `DeusEx.ini` and `User.ini`. The engine takes its renderer and
the fork's own settings from `~/.config/SurrealEngine/Settings.json`, which
`main`'s launcher does not write; without one, from its own defaults
([settings and environment](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#settings-and-environment)).

The engine also starts on its own, straight into a map
([running it](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#running-it)).

## What it is made of

| File | What it is |
|---|---|
| `port.cmake` | system SDL2 through pkg-config, which `DeusEx` takes |
| `engine.cmake` | the engine with the SDL2, X11 and Wayland display backends, not SDL3: SurrealWidgets builds one SDL backend, SDL3's when both are on, and only SDL2's has the fork's gamepad support; `run-game.sh` asks for SDL2 at run time |
| `port.sh` | `deps` checks for SDL2 and SDL2_ttf; `run` puts the staged app in `gamefiles/System` and starts `DeusEx` there |

Beside them, at the branch's root: the port's preset in `CMakePresets.json`
and its include in `CMakeLists.txt`. The rest is `main`'s.

## Verified

- Arch Linux, AMD GPU (Mesa's RADV and radeonsi): `run` starts `DeusEx`,
  which starts the engine on `gamefiles/`; unattended engine runs drive
  saves, loads and hub travel
  ([scripted runs](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/DEVELOPMENT.md#scripted-runs-of-both-engines)).

## Gotchas

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

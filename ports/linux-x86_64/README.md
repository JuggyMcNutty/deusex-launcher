# Port: Linux x86_64 -- the base

Desktop Linux (or a handheld PC running it), built natively against the
distro's SDL2. The unit tests, `dxl-shots`, the engine's Vulkan validation and
the side-by-side checks of engine changes run here; every other port is this
one plus what differs for its device
([`docs/PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md)).

## Status

The staged app's launcher and engine run the game ([verified](#verified)).
With SDL3 installed the game gets no pad ([no pad in game](#no-pad-in-game)).

## Build and run

Needs a C compiler, CMake 3.21+, pkg-config, and the SDL2 and SDL2_ttf
development packages; the engine adds a C++20 compiler and its own
dependencies ([its README](https://github.com/JuggyMcNutty/VibeEngine)).

```sh
scripts/dx.sh fetch                  # once: the engine, the launcher's branches, the RE
scripts/dx.sh deps  linux-x86_64     # only checks for SDL2 and SDL2_ttf
scripts/dx.sh build linux-x86_64     # launcher, then engine
scripts/dx.sh test                   # unit tests
scripts/dx.sh stage linux-x86_64     # build/linux-x86_64/app
scripts/dx.sh run   linux-x86_64
```

A freshly staged app's `launcher.ini` points `GameDir` at the `gamefiles/`
beside the repositories, when there is one; edit it for another install. The
launcher and the engine write their configuration into that game's
`System/`, as the game would.

`run-game.sh` pins `HOME` to the app's `home/`, so the engine's
`Settings.json` and logs are in `build/linux-x86_64/app/home/.config/SurrealEngine`,
where the launcher reads and writes them, and your own
`~/.config/SurrealEngine` is left alone.

The engine also starts on its own, straight into a map
([running it](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#running-it)).

## What it is made of

| File | What it is |
|---|---|
| `port.cmake` | system SDL2 through pkg-config |
| `engine.cmake` | the engine with every display backend the system has -- with SDL3 installed that is SDL3 and not SDL2 ([no pad in game](#no-pad-in-game)); `run-game.sh` asks for SDL2, the one with gamepad support, at run time |
| `port.sh` | `deps` checks for SDL2 and SDL2_ttf; `stage` points a fresh app at `gamefiles/`; `run` starts the staged launcher |

The generic device profile (`src/platform/target_default.c`) and
`ports/common/packaging` are this port's, so it has no `target.c` and no
`packaging/`; every other port lays its own files over them.

## Verified

- Arch Linux, AMD GPU (Mesa's RADV and radeonsi): `dxl-cli --probe` finds Vulkan 1.4 and OpenGL, both selectable; the staged app's `run-game.sh` runs the engine on `gamefiles/`, Vulkan with bindless textures.
- `DXL_NO_HOME=1` takes the staged launcher straight into Liberty Island; unattended engine runs drive saves, loads and hub travel ([scripted runs](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/DEVELOPMENT.md#scripted-runs-of-both-engines)).

## Gotchas

### No pad in game

A defect. With SDL3 installed, as on most current desktop distros,
SurrealWidgets builds its SDL3 window backend instead of its SDL2 one
(VibeEngine's `SurrealWidgets/CMakeLists.txt`). The fork's pad support,
`GetGamepadState`, is the SDL2 backend's alone, so `run-game.sh`'s
`SURREALWIDGETS_DISPLAY_BACKEND=SDL2` finds nothing and the engine falls
back to Wayland or X11, its log not saying which: the launcher answers the
pad, the game does not. The fix: `ENABLE_SDL3` off in `engine.cmake`, which
leaves SurrealWidgets the SDL2 backend (the Smart Pro's SDL2-only build has
the pad in game), or the pad in the SDL3 backend too.

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

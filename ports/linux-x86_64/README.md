# Port: Linux x86_64 -- the base

Desktop Linux (or a handheld PC running it), built natively with the distro's
own SDL2. This is the base port: the project is developed and tested here --
the unit tests, `dxl-shots`, the engine's Vulkan validation and the side-by-side
checks of engine changes -- and every other port is this one plus what differs
for its device ([`docs/PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md)).

## Status

Working: the staged app's launcher and engine run the game on the
development PC ([verified](#verified)). Not yet exercised: the home screen
driven by hand into a game, and a pad in game on a desktop -- which this
build does not give yet ([no pad in game](#no-pad-in-game)).

## Build and run

Needs a C compiler, CMake 3.21+, pkg-config, and SDL2 + SDL2_ttf development
packages; the engine adds a C++20 compiler and its own dependencies (see
[the engine's README](https://github.com/JuggyMcNutty/VibeEngine)).

```sh
scripts/dx.sh fetch                  # once: the engine, the launcher's branches, the RE
scripts/dx.sh deps  linux-x86_64     # only checks for SDL2
scripts/dx.sh build linux-x86_64     # launcher, then engine
scripts/dx.sh test                   # unit tests
scripts/dx.sh stage linux-x86_64     # build/linux-x86_64/app
scripts/dx.sh run   linux-x86_64
```

A freshly staged app's `launcher.ini` points `GameDir` at the `gamefiles/`
beside Port Ex Machina's repositories, when there is one; edit it to use
another install. The launcher and the engine then write their configuration
into that game's `System/`, as the game would.

`run-game.sh` pins `HOME` to the app directory for the engine, so its
`Settings.json` and logs live in `build/linux-x86_64/app/home/.config/SurrealEngine`,
where the launcher reads and writes them -- your own `~/.config/SurrealEngine`
is left alone.

The engine can also be started on its own, straight into a map, for profiling
and validation: [`vibe/docs/ENGINE.md`](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#running-it).

## What it is made of

| File | What it is |
|---|---|
| `port.cmake` | system SDL2 through pkg-config |
| `engine.cmake` | the engine with every display backend; `run-game.sh` asks for SDL2 at run time, the backend with gamepad support -- which a build with SDL3 installed does not have ([no pad in game](#no-pad-in-game)) |
| `port.sh` | `deps` checks for SDL2; `stage` points a fresh app at `gamefiles/`; `run` starts the staged launcher |

No `target.c` and no `packaging/`: the generic device profile
(`src/platform/target_default.c`) and `ports/common/packaging` are this port,
and the base every other port lays its own files over.

## Verified

On the development PC (Arch, AMD RX 6700 XT, Mesa RADV) the staged app's
`run-game.sh` starts the engine on the `gamefiles/` beside the repositories:
Vulkan with bindless textures. `dxl-cli --probe` found Vulkan 1.4 and OpenGL,
both selectable. The staged launcher took the engine straight into Liberty
Island with `DXL_NO_HOME=1` (2026-09-27), and unattended runs of the engine
drove saves, loads and hub travel in play (2026-09-25;
[scripted runs](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/DEVELOPMENT.md#scripted-runs-of-both-engines)).

## Gotchas

### No pad in game

A defect, found 2026-10-04 and not fixed yet. With SDL3 installed -- as on a
current desktop distro -- SurrealWidgets builds its SDL3 window backend and
not its SDL2 one: it compiles SDL2's only when SDL3 is not found
(VibeEngine's `SurrealWidgets/CMakeLists.txt`). The fork's pad support,
`GetGamepadState`, is the SDL2 backend's alone, so `run-game.sh`'s
`SURREALWIDGETS_DISPLAY_BACKEND=SDL2` finds no such backend and the engine
falls back to Wayland or X11 without a word: the launcher's screens answer
the pad, the game does not. The fix is one of: `ENABLE_SDL3` off in
`engine.cmake`, which leaves SurrealWidgets the SDL2 backend (the Smart Pro's
build is SDL2 only, and its pad works in game); or the pad in the SDL3
backend too. Nothing in the engine's log says which backend a run got.

### Audio

OpenAL Soft needs a sound server library to reach the desktop's audio:
`libpipewire` or `libpulse`. Without one (a minimal container) it falls back
to ALSA, which usually cannot open the device a sound server holds, and the
engine stops at start-up with `Failed to initialize OpenAL device`. Install one
of them, or play silently with a null driver:

```sh
printf '[general]\ndrivers = null\n' > /tmp/alsoft-null.conf
ALSOFT_CONF=/tmp/alsoft-null.conf scripts/dx.sh run linux-x86_64
```

`ALSOFT_DRIVERS=null` does the same without a file: OpenAL Soft reads it as
its `drivers` option.

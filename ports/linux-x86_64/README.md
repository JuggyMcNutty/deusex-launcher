# Port: Linux x86_64 -- the base

Desktop Linux (or a handheld PC running it), built natively with the distro's
own SDL2. This is the base port: the project is developed and tested here --
the unit tests, `dxl-shots`, the engine's Vulkan validation and the side-by-side
checks of engine changes -- and every other port is this one plus what differs
for its device ([`docs/PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md)).

## Status

Working. On the development PC (Arch, AMD RX 6700 XT, Mesa RADV) the staged
app's `run-game.sh` started the engine on the workspace's `gamefiles/`: Vulkan
with bindless textures, the intro level loaded and ran until stopped.
`dxl-cli --probe` found Vulkan 1.4 and OpenGL, both selectable. Not yet
exercised: the home screen driven by hand into a game, and a pad in game on a
desktop.

## Build and run

Needs a C compiler, CMake 3.21+, pkg-config, and SDL2 + SDL2_ttf development
packages; the engine adds a C++20 compiler and its own dependencies (see the
engine's README).

```sh
scripts/dx.sh deps  linux-x86_64     # only checks for SDL2
scripts/engine.sh fetch              # once
scripts/dx.sh build linux-x86_64     # launcher, then engine
scripts/dx.sh test                   # unit tests
scripts/dx.sh stage linux-x86_64     # build/linux-x86_64/app
scripts/dx.sh run   linux-x86_64
```

A freshly staged app's `launcher.ini` points `GameDir` at the workspace's
`gamefiles/`, when there is one; edit it to use another install. The launcher
and the engine then write their configuration into that game's `System/`, as
the game would.

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
| `engine.cmake` | the engine with every display backend; `run-game.sh` picks SDL2 at run time, the backend with gamepad support |
| `port.sh` | `deps` checks for SDL2; `stage` points a fresh app at `gamefiles/`; `run` starts the staged launcher |

No `target.c` and no `packaging/`: the generic device profile
(`src/platform/target_default.c`) and `ports/common/packaging` are this port,
and the base every other port lays its own files over.

## Gotchas

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

The `ALSOFT_CONF` file is the mechanism; `ALSOFT_DRIVERS` does not exist in
OpenAL Soft.

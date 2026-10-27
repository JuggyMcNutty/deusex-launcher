# Port: Linux x86_64 -- the development platform

Desktop Linux (or a handheld PC running it), with glibc 2.28 or newer
([why 2.28](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/AGENTS.md#decided)).
The unit tests, the engine's Vulkan validation and the side-by-side checks
of engine changes run here.

## Status

The launcher is DXLauncher, the desktop's own
([`docs/LAUNCHER.md`](../../docs/LAUNCHER.md)): a window with Home, the first
run, Settings, Profiles, Mods, Saves, Screenshots, Multiplayer, Recovery,
the log and About, which makes profiles from the game's files, keeps their
settings and plays them. The engine runs the game, not fully playable
([where the engine differs](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/NATIVES.md)).

## Build and run

Built with a Buildroot SDK, Godot's `godot-2023.08.x-4` (GCC 13.2 on glibc
2.28), whatever the build machine's glibc: the programs need 2.28 or newer,
which `scripts/check-abi.sh` holds them to after each build. Needs CMake
3.21+, pkg-config, curl, xz, bzip2 and binutils' `ar`; `scripts/dx.sh deps
linux-x86_64` fetches the rest: the SDK, pinned by its SHA256, Debian's
packages for its sysroot, the window's sources and the engine's SDL3, which
it builds ([what it is made of](#what-it-is-made-of)).

```sh
scripts/dx.sh fetch                  # once: the engine, the launcher's branches, the RE
scripts/dx.sh deps  linux-x86_64     # once: the SDK, the sysroot, the sources, SDL3
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
renderer, DXEngine's own drawing settings and the controller's
([settings and environment](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/ENGINE.md#settings-and-environment)):
the launcher's Settings keeps the three
([Settings](../../docs/LAUNCHER.md#settings)).

The engine also starts on its own, straight into a map
([running it](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/ENGINE.md#running-it)).

## What it is made of

| File | What it is |
|---|---|
| `toolchain.cmake` | the SDK's GCC against `deps/sysroots/linux-x86_64`, libstdc++ and libgcc linked statically: a native build, not a cross one, so the programs and the tests run here too |
| `port.cmake` | with the toolchain, the 2.28 ceiling, and SDL3, FreeType, SDL3_ttf and libxmp-lite built in from `cmake/deps.lock`'s sources, SDL3's window and audio backends opened at run time; a build with the machine's own compiler (one with sanitizers) runs where it is built, with no ceiling |
| `engine.cmake` | the engine with the SDL3 and X11 display backends, not SDL2: DXWidgets builds one SDL backend, SDL3's when both are on, the one the launcher asks for at run time. Not the engine's own Wayland window: it needs waylandpp, which the SDK lacks, and SDL3's Wayland is the one the launcher's runs use |
| `port.sh` | `deps`: the SDK; the sysroot -- the SDK's, hard-linked, its `.pc` files pointed at the lib64 folder its libraries are in, and on it Debian bookworm's headers of what SDL's backends and the GPU probe open at run time (Wayland, xkbcommon, libdecor, PipeWire, libusb, GBM, XScreenSaver, XTest, glvnd, Vulkan) and buster's glvnd and OpenAL Soft, which the engine links, buster's glibc being 2.28; the engine's SDL3, the launcher's version, static, its backends opened at run time, into `deps/prefix/linux-x86_64`. `run` starts the staged launcher on `gamefiles/`, in a scratch home with `--fresh` |

Beside them, at the branch's root: the port's preset in `CMakePresets.json`,
with the toolchain file, and its block in `CMakeLists.txt` with the
`scripts/check-abi.sh` it runs, the block linux-aarch64's too.

## Verified

- AlmaLinux 8.10 (glibc 2.28), in a container: `dxl-cli` runs and
  `deusex-launcher --dry-run` decides, with nothing but the C library; the
  engine answers `--version` with the distro's own X11, D-Bus, GLib,
  fontconfig, glvnd and OpenAL Soft packages.
- Arch Linux, KDE Plasma on Wayland, AMD GPU (Mesa's RADV and radeonsi),
  the window on SDL3's OpenGL renderer: `scripts/dx.sh run linux-x86_64
  --fresh`, driven by a UI script, goes through the first run on
  `gamefiles/` to Home and plays, the engine in its SDL3 window on the
  desktop's display, to a clean end, and the launcher exits, with nothing
  newer in `gamefiles/` or `~/.config/DXEngine`; a crashing stand-in
  engine brings the window back on Recovery, the next start opens on it, and
  a second start while it is up is handed over. Settings, by
  `tests/ui/settings_roundtrip.script`: a row of each kind changed on each
  page, then the engine played on them in a window of the size chosen to a
  clean end, each value kept through its rewrite of the profile's files.
  Continue: the engine started on the quick save it had made in the profile,
  to a clean end, its shot in the profile's Screenshots and shown there.
  Multiplayer: 333networks' master lists the servers, each answers, and one
  joined from the page is joined by the engine, its map downloaded into the
  profile's cache and loaded as a client's. Mods: a test mod outside the
  game's folder -- a package of the harness's, an ini of its own naming it
  as in the game's folder, `.int` files of its own text -- made a profile by
  `dxl-cli --import --mod` and played: the engine ran its console class from
  its package and read its text and the object its `.int` registers from its
  folder. Unattended engine runs drive saves, loads and hub travel
  ([scripted runs](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/DEVELOPMENT.md#scripted-runs-of-both-engines)).
- Bazzite 44 (glibc 2.43), KDE Plasma on Wayland, AMD Radeon 780M, played
  by hand with a real keyboard and mouse:
  `scripts/dx.sh run linux-x86_64 --fresh` through the first run, its
  controller layout offered for the pad connected, to Home, Settings and
  Multiplayer; a new game to Liberty Island, saves made in the game and one
  started from the launcher (`?loadgame=2`); movement keys held, text typed
  in the console and a save's name, a menu dragged by its title bar and Alt
  accelerators; each game to a clean end.

## Gotchas

### Where it runs

On any distribution with glibc 2.28 or newer, whatever the glibc of the
machine that built it. The launcher needs only the C library, its window
and sound the machine's own X11 or Wayland, ALSA, PulseAudio or PipeWire,
opened when found; the engine links the distribution's X11 (with Xi and Xext),
D-Bus, GLib and GIO, fontconfig, FreeType, glvnd's EGL and OpenGL, and OpenAL
Soft (`libopenal.so.1`). An engine built with the machine's own
compiler instead runs only on a glibc as new as its builder's: the launcher
refuses it where it cannot start, and says so ("needs glibc 2.44, this
system has 2.43").

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

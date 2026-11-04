# DXLauncher for Linux on aarch64

Deus Ex's launcher for aarch64 devices running an ordinary distro: an app of
its own that finds the game, keeps each way of playing in a profile, with
its own settings, saves and screenshots, and starts
[DXEngine](https://github.com/JuggyMcNutty/DXEngine) and stays with it. It
wears the game's look and sound, and takes a keyboard, a mouse or a
controller. It is linux-x86_64's launcher, built for aarch64.

<p align="center">
  <img src="docs/screenshots/home.png" width="100%"
       alt="Home: the game's logo and buttons, the profile, its latest save">
</p>

**You need your own copy of Deus Ex, version 1112fm** (1002f, the first
release, runs with a warning). None of the game's files are included.
DXLauncher is part of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina). Made with
AI assistance.

**Status**: the launcher and the engine build, cross-built from a PC; they
are untried on an aarch64 device ([the port](ports/linux-aarch64/README.md)).
Reports from a device are wanted: [where to report a bug](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#reporting-bugs).

## Features

- **It finds the game**: in Steam's libraries, Heroic's and GOG's folders,
  Lutris, Bottles and Wine prefixes, on mounted drives; or it is shown the
  folder. The first run makes a profile from it, the game's settings and
  keys imported. Nothing in the game's folder is written but an ini a
  command line names (`INI=`, `USERINI=`).
- **Profiles**: one for each way of playing, with its own settings, keys,
  controller, saves and screenshots; made, copied, renamed, or deleted to
  the trash.
- **Settings**: those the engine really reads, on eight pages: display,
  graphics, audio, keyboard and mouse, controller, gameplay and HUD,
  multiplayer, and the launcher's own.
- **Controllers**: Xbox, PlayStation, Switch and other pads; a button bound
  by pressing it, a test, and a wizard that maps a pad SDL does not know.
- **Saves and screenshots**: Continue on the latest save, load or delete
  any, look through the game's screenshots.
- **Multiplayer**: the game's servers, from the local network and
  333networks' master server; favourites, passwords, an address joined.
- **Mods**: each played through a profile of its own, its folders first.
- **Recovery**: after a crash, how the run ended, the end of its log, and
  what to try next: another renderer, a window, the defaults.
- **The big screen**: full screen, for a television or a handheld, on its
  own under Steam's Big Picture or in gamescope.
- **The game's look and sound**: its art, its menu theme's colours, its
  title music and menu sounds, read from the install as it runs.

## Screenshots

<table>
  <tr>
    <td width="50%"><img src="docs/screenshots/first-run.png" width="100%"
        alt="The first run: Deus Ex found in GOG Games"></td>
    <td width="50%"><img src="docs/screenshots/saves.png" width="100%"
        alt="Saves, each with its place, date and snapshot"></td>
  </tr>
  <tr>
    <td align="center">The first run finds the game</td>
    <td align="center">Saves, each with its snapshot</td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/graphics.png" width="100%"
        alt="Settings, Graphics: renderer, anti-aliasing"></td>
    <td width="50%"><img src="docs/screenshots/controller.png" width="100%"
        alt="Settings, Controller: an Xbox pad's bindings"></td>
  </tr>
  <tr>
    <td align="center">Settings: the engine's graphics</td>
    <td align="center">A controller, as the game binds it</td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/multiplayer.png" width="100%"
        alt="Multiplayer: the servers 333networks lists"></td>
    <td width="50%"><img src="docs/screenshots/big-screen.png" width="100%"
        alt="The big screen: Home full screen, a pad's hints"></td>
  </tr>
  <tr>
    <td align="center">The game's servers</td>
    <td align="center">The big screen, with a pad</td>
  </tr>
</table>

The shots are of linux-x86_64's build, the same launcher.

## Download

Take `DX-linux-aarch64.tar.gz` from
[Port Ex Machina's releases](https://github.com/JuggyMcNutty/port-ex-machina/releases):
DXLauncher with DXEngine. Unpack it anywhere and run `deusex-launcher` in
its `DXLauncher` folder. It needs glibc 2.28 or newer, Mesa,
OpenAL Soft (`libopenal.so.1`: Debian's and Ubuntu's `libopenal1`, Fedora's
`openal-soft`, Arch's `openal`), PipeWire or PulseAudio, and a desktop.
This repository's releases tagged `linux-aarch64-<date>-<commit>` have the
launcher alone.

## Building

Port Ex Machina fetches this branch beside the engine and cross-builds both
from a PC, as GitHub's CI does
([its quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start)):

```sh
scripts/dx.sh deps  linux-aarch64   # the SDK, the sysroot, the sources, SDL2
scripts/dx.sh build linux-aarch64   # the launcher, then the engine
scripts/dx.sh test  linux-aarch64   # the unit tests, on the PC's build
scripts/dx.sh stage linux-aarch64   # build/linux-aarch64/app: what ships
```

Building on the device itself, and what each way needs:
[the port](ports/linux-aarch64/README.md).

## The branch

`linux-aarch64` is the branch for Linux on aarch64 with an ordinary distro:
DXLauncher's source and the port (`ports/linux-aarch64`, its preset in
`CMakePresets.json`). Cross-built it makes `deusex-launcher`, `dxl-cli` and
`dxl-shots`; its host build, `ports/linux-x86_64/port.cmake` and its preset,
makes them for the PC and runs the tests. Its launcher is linux-x86_64's:
each launcher commit comes over from one to the other by a deliberate
cherry-pick, and nothing merges into it, `main` included
([the branches](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches)).

## Documentation

- [`docs/LAUNCHER.md`](docs/LAUNCHER.md): the launcher, from the files it
  writes to its known defects.
- [`ports/linux-aarch64/README.md`](ports/linux-aarch64/README.md): the
  port, how it is built and what differs from linux-x86_64.
- The other platforms, each a branch of its own:
  [Port Ex Machina's ports](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#engine-ports).
- How ports work:
  [`PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md).

## License

[zlib](LICENSE); what the app carries of others', its fonts, libraries and
engine, under their own terms ([`THIRD-PARTY.md`](THIRD-PARTY.md)). Deus Ex
belongs to its owners; this project is not affiliated with or endorsed by
them.

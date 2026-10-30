# DXLauncher for Linux

Deus Ex's launcher for desktop Linux on x86_64: an app of its own that finds
the game, keeps each way of playing in a profile, with its own settings,
saves and screenshots, and starts
[DXEngine](https://github.com/JuggyMcNutty/DXEngine) and stays with it. It
wears the game's look and sound, and takes a keyboard, a mouse or a
controller.

<p align="center">
  <img src="docs/screenshots/home.png" width="100%"
       alt="Home: the game's logo and buttons, the profile, its latest save">
</p>

**You need your own copy of Deus Ex, version 1112fm** (1002f, the first
release, runs with a warning). None of the game's files are included.
DXLauncher is part of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina). Made with
AI assistance.

**Status**: the launcher is complete; the engine runs the game, not fully
playable ([where it differs](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/NATIVES.md)), on the
development platform ([the port](ports/linux-x86_64/README.md)).

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
- **The big screen**: full screen, for a television or a handheld PC, on
  its own under Steam's Big Picture, on a Steam Deck or in gamescope.
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

## Download

Take `DX-linux-x86_64.tar.gz` from
[Port Ex Machina's releases](https://github.com/JuggyMcNutty/port-ex-machina/releases):
DXLauncher with DXEngine. Unpack it anywhere and run `deusex-launcher` in
its `DXLauncher` folder; its About page adds it to the app menu. It runs on
x86_64 Linux with glibc 2.28 or newer. This repository's releases tagged
`linux-x86_64-<date>-<commit>` have the launcher alone.

## Building

Port Ex Machina fetches this branch beside the engine, then builds, tests and
runs both
([its quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start)):

```sh
scripts/dx.sh deps  linux-x86_64   # the SDK, the sysroot, the sources
scripts/dx.sh build linux-x86_64   # the launcher, then the engine
scripts/dx.sh test  linux-x86_64   # the unit tests
scripts/dx.sh stage linux-x86_64   # build/linux-x86_64/app: what ships
scripts/dx.sh run   linux-x86_64   # on the gamefiles/ beside the repositories
```

What the build needs and makes: [the port](ports/linux-x86_64/README.md).

## The branch

`linux-x86_64` is the branch for desktop Linux on x86_64, where the project
is developed: DXLauncher's source (`src/`), and the port
(`ports/linux-x86_64`, its preset in `CMakePresets.json`, its block in
`CMakeLists.txt`). It builds `deusex-launcher`, `dxl-cli`, `dxl-shots` and
the tests. Each platform is a branch of its own, and `main` the original
launcher, `DeusEx.exe`, recreated. None merges another: a change crosses by
a deliberate cherry-pick
([the branches](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches)).

## Documentation

- [`docs/LAUNCHER.md`](docs/LAUNCHER.md): the launcher, from the files it
  writes to its known defects.
- [`ports/linux-x86_64/README.md`](ports/linux-x86_64/README.md): the port,
  what it is made of and what is verified.
- The other platforms, each a branch of its own:
  [Port Ex Machina's ports](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#engine-ports).
- How ports work:
  [`PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md).

## License

[zlib](LICENSE); what the app carries of others', its fonts, libraries and
engine, under their own terms ([`THIRD-PARTY.md`](THIRD-PARTY.md)). Deus Ex
belongs to its owners; this project is not affiliated with or endorsed by
them.

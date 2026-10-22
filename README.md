# deusex-launcher: windows-x86_64

This branch is the launcher of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina)'s port
for Windows 10 (1903) or newer on x86_64: DXLauncher, the desktop's own,
as linux-x86_64 has it, on Windows' own calls, an app outside the game's
folder that keeps the player's settings and saves in a profile, starts
[DXEngine](https://github.com/JuggyMcNutty/DXEngine) and stays with it
([`docs/LAUNCHER.md`](docs/LAUNCHER.md)). Its window has Home, the first
run, Settings, Profiles, Mods, Saves, Screenshots, Multiplayer, Recovery,
the log and About. Made with AI assistance.

You need your own copy of Deus Ex (version 1112fm); none of the game's files
are included.

## The branch

`windows-x86_64` is the default of its platform, Windows on x86_64,
cross-built on a Linux PC by llvm-mingw and tested there under Wine:
DXLauncher's source, with Windows' calls in `src/platform/win32/` and the
finder's Windows places in `src/core/finder_places_win32.c`, and the port
(`ports/windows-x86_64`, its preset in `CMakePresets.json`). Cross-built it
makes `deusex-launcher.exe`, `dxl-cli.exe` and `dxl-shots.exe`, and the
tests, which run under Wine; its host build, `ports/linux-x86_64/port.cmake`
and its preset, makes them for the PC and runs the tests there too. Its
launcher is linux-x86_64's, and each launcher commit comes over from there
by a deliberate cherry-pick; nothing merges into it, `main` included
([the branches](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches)).

## Where to read

- [`docs/LAUNCHER.md`](docs/LAUNCHER.md): the launcher, from the files it
  writes to its known defects.
- [`ports/windows-x86_64/README.md`](ports/windows-x86_64/README.md): the
  port.
- The other ports, each a branch of its own:
  [Port Ex Machina's ports](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#engine-ports).
- Building and running:
  [Port Ex Machina's quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start).
- How ports work:
  [`PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md).

## License

[zlib](LICENSE); what the app carries of others' -- its fonts and
libraries, the programs' runtimes, the engine and its OpenAL Soft -- under
their own terms ([`THIRD-PARTY.md`](THIRD-PARTY.md)). Deus Ex belongs to its
owners; this project is not affiliated with or endorsed by them.

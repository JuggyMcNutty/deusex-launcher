# deusex-launcher: linux-aarch64

This branch is the launcher of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina)'s port
for aarch64 devices with an ordinary distro: DXLauncher, the desktop's own,
as linux-x86_64 has it, an app outside the game's folder that keeps the
player's settings and saves in a profile, starts
[VibeEngine](https://github.com/JuggyMcNutty/VibeEngine) and stays with it
([`docs/LAUNCHER.md`](docs/LAUNCHER.md)). Its window has Home, the first
run, Settings, Profiles, Mods, Saves, Screenshots, Multiplayer, Recovery,
the log and About. Made with AI assistance.

You need your own copy of Deus Ex (version 1112fm); none of the game's files
are included.

## The branch

`linux-aarch64` is the default of its platform, Linux on aarch64 with an
ordinary distro, cross-built from a PC or built natively: DXLauncher's
source, and the port (`ports/linux-aarch64`, its preset in
`CMakePresets.json`). Cross-built it makes `deusex-launcher`, `dxl-cli` and
`dxl-shots`; its host build, `ports/linux-x86_64/port.cmake` and its
preset, makes them for the PC and runs the tests. Its launcher came whole
from linux-x86_64's, and each launcher commit since comes over from one to
the other by a deliberate cherry-pick; nothing merges into it, `main`
included
([the branches](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches)).

## Where to read

- [`docs/LAUNCHER.md`](docs/LAUNCHER.md): the launcher, from the files it
  writes to its known defects.
- [`ports/linux-aarch64/README.md`](ports/linux-aarch64/README.md): the port.
- The other ports, each a branch of its own:
  [Port Ex Machina's ports](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#engine-ports).
- Building and running:
  [Port Ex Machina's quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start).
- How ports work:
  [`PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md).

## License

[zlib](LICENSE); what the app carries of others', its fonts and libraries,
under their own terms ([`THIRD-PARTY.md`](THIRD-PARTY.md)). Deus Ex
belongs to its owners; this project is not affiliated with or endorsed by
them.

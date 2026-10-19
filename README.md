# deusex-launcher: trimui-smartpro

This branch is the launcher of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina)'s port
for the TrimUI Smart Pro under spruceOS: DXLauncher, the desktop's own, as
linux-aarch64 has it, tailored to the handheld -- its screen and pad, no
desktop -- an app outside the game's folder that keeps the player's
settings and saves in a profile, starts
[DXEngine](https://github.com/JuggyMcNutty/DXEngine) and stays with it
([`docs/LAUNCHER.md`](docs/LAUNCHER.md)). Its window has Home, the first
run, Settings, Profiles, Mods, Saves, Screenshots, Multiplayer, Recovery,
the log and About. Made with AI assistance.

You need your own copy of Deus Ex (version 1112fm); none of the game's files
are included.

## The branch

`trimui-smartpro` is linux-aarch64's launcher with the Smart Pro's port
(`ports/trimui-smartpro`, its preset in `CMakePresets.json`) and its
tailoring, each a commit of its own on top of linux-aarch64's
([what differs](ports/trimui-smartpro/README.md#what-differs-from-linux-aarch64)):
an SDL3 video driver for the device's framebuffer and EGL, the spruceOS
app, the CPU modes, the big screen alone, no desktop, deleting for good, a
pad without stick clicks, spruceOS's Ports folder, the device's data.
Cross-built it makes `deusex-launcher` and `dxl-cli`; its host build,
`ports/linux-x86_64/port.cmake` and its preset, makes them and `dxl-shots`
for the PC and runs the tests. Nothing merges into it, `main` included: a
change from linux-aarch64 comes over by a deliberate cherry-pick
([the branches](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches)).
The tabbed launcher this branch had before is tag `trimui-smartpro-tabbed`.

## Where to read

- [`docs/LAUNCHER.md`](docs/LAUNCHER.md): the launcher, from the files it
  writes to its known defects.
- [`ports/trimui-smartpro/README.md`](ports/trimui-smartpro/README.md): the
  port, the device as measured, its performance.
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

# deusex-launcher: the ports' launcher

This branch is the launcher that
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina)'s ports
run in place of Deus Ex's `System/DeusEx.exe`: a tabbed home screen, for a
pad or the keyboard, that writes the configuration the engine reads and hands
over to [VibeEngine](https://github.com/JuggyMcNutty/VibeEngine). Made with
AI assistance.

You need your own copy of Deus Ex (version 1112fm); none of the game's files
are included.

## The branch

`linux-x86_64` is `main` (the original `DeusEx.exe`, recreated) plus the
ports' launcher ([`docs/LAUNCHER.md`](docs/LAUNCHER.md)), the base app every
port ships (`ports/common/packaging`) and the desktop port
(`ports/linux-x86_64`). A port branch builds `deusex-launcher`, `dxl-cli`,
`dxl-shots` and the core's unit tests. Every device branch is this branch plus
its device, with this README,
which never takes `main`'s on a merge
([the branches](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches)).

## Where to read

- [`docs/LAUNCHER.md`](docs/LAUNCHER.md): the launcher, from the settings
  the engine reads to its known defects.
- Each port's `ports/<id>/README.md`, on its branch:
  [linux-x86_64](https://github.com/JuggyMcNutty/deusex-launcher/tree/linux-x86_64/ports/linux-x86_64),
  [linux-aarch64](https://github.com/JuggyMcNutty/deusex-launcher/tree/linux-aarch64/ports/linux-aarch64),
  [trimui-smartpro](https://github.com/JuggyMcNutty/deusex-launcher/tree/trimui-smartpro/ports/trimui-smartpro),
  [android](https://github.com/JuggyMcNutty/deusex-launcher/tree/android/ports/android),
  [x360](https://github.com/JuggyMcNutty/deusex-launcher/tree/x360/ports/x360).
- Building and running:
  [Port Ex Machina's quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start).
- How ports work:
  [`PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md).
- The recreation, `main`'s program:
  [`main`'s README](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md).

## License

[zlib](LICENSE). Deus Ex belongs to its owners; this project is not
affiliated with or endorsed by them.

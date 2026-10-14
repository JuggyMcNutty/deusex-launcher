# deusex-launcher: trimui-smartpro

This branch is the launcher of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina)'s port
for the TrimUI Smart Pro under spruceOS, run in place of Deus Ex's
`System/DeusEx.exe`: a tabbed home screen, for the handheld's pad or a
keyboard, that writes the configuration the engine reads and hands over to
[VibeEngine](https://github.com/JuggyMcNutty/VibeEngine). Made with AI
assistance.

You need your own copy of Deus Ex (version 1112fm); none of the game's files
are included.

## The branch

`trimui-smartpro` is the Smart Pro's own launcher, cross-built against the
device's libraries: the launcher with the handheld's choices
([`docs/LAUNCHER.md`](docs/LAUNCHER.md); how it differs from linux-x86_64's:
[the port's README](ports/trimui-smartpro/README.md#what-differs-from-linux-x86_64)),
the app it ships in (`ports/common/packaging`, with the port's `packaging/`
over it) and the port (`ports/trimui-smartpro`). Cross-built it makes
`deusex-launcher` and `dxl-cli`; its host build,
`ports/linux-x86_64/port.cmake` and its preset, makes `dxl-shots` and runs
the unit tests. Nothing merges into it, `main` included: a change from
another branch comes over by a deliberate cherry-pick
([the branches](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches)).

## Where to read

- [`docs/LAUNCHER.md`](docs/LAUNCHER.md): the launcher, from the settings
  the engine reads to its known defects.
- [`ports/trimui-smartpro/README.md`](ports/trimui-smartpro/README.md): the
  port, the device as measured, its performance.
- The other ports, each a branch of its own:
  [Port Ex Machina's ports](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#engine-ports).
- Building and running:
  [Port Ex Machina's quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start).
- How ports work:
  [`PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md).
- The recreation, `main`'s program:
  [`main`'s README](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md).

## License

[zlib](LICENSE). Deus Ex belongs to its owners; this project is not
affiliated with or endorsed by them.

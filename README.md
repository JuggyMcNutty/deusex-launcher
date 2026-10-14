# deusex-launcher: linux-x86_64

This branch is the launcher of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina)'s
desktop Linux port, run in place of Deus Ex's `System/DeusEx.exe`: a tabbed
home screen, for a pad or the keyboard, that writes the configuration the
engine reads and hands over to
[VibeEngine](https://github.com/JuggyMcNutty/VibeEngine). Made with AI
assistance.

You need your own copy of Deus Ex (version 1112fm); none of the game's files
are included.

## The branch

`linux-x86_64` is the default of its platform, desktop Linux on x86_64,
built natively: the launcher ([`docs/LAUNCHER.md`](docs/LAUNCHER.md)), the
app it ships in (`ports/common/packaging`) and the port
(`ports/linux-x86_64`), with no device's own choices. It builds
`deusex-launcher`, `dxl-cli`, `dxl-shots` and the core's unit tests. Nothing
merges into it, `main` included: a change from another branch comes over by
a deliberate cherry-pick
([the branches](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches)).

## Where to read

- [`docs/LAUNCHER.md`](docs/LAUNCHER.md): the launcher, from the settings
  the engine reads to its known defects.
- [`ports/linux-x86_64/README.md`](ports/linux-x86_64/README.md): the port.
- The other ports, each a branch of its own:
  [Port Ex Machina's ports](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#ports).
- Building and running:
  [Port Ex Machina's quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start).
- How ports work:
  [`PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md).
- The recreation, `main`'s program:
  [`main`'s README](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md).

## License

[zlib](LICENSE). Deus Ex belongs to its owners; this project is not
affiliated with or endorsed by them.

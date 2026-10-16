# deusex-launcher: linux-x86_64

This branch is the launcher of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina)'s
desktop Linux port, run in place of Deus Ex's `System/DeusEx.exe`. For now
it is `main`'s launcher, the original recreated almost 1:1
([`main`'s README](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md)),
with the desktop's port around it; the desktop's own launcher is planned
([Port Ex Machina's to-do list](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/AGENTS.md#to-do)).
It starts
[VibeEngine](https://github.com/JuggyMcNutty/VibeEngine) and waits for it.
Made with AI assistance.

You need your own copy of Deus Ex (version 1112fm); none of the game's files
are included.

## The branch

`linux-x86_64` is the default of its platform, desktop Linux on x86_64,
built natively: `main`'s source, and the port (`ports/linux-x86_64`, its
preset in `CMakePresets.json`, its block in `CMakeLists.txt`). It builds
`DeusEx`, `pageshots` and the tests. Nothing merges into it, `main`
included: a change from another branch comes over by a deliberate
cherry-pick
([the branches](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches)).

## Where to read

- [`main`'s README](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md):
  the launcher, from its screens to its known defects.
- [`ports/linux-x86_64/README.md`](ports/linux-x86_64/README.md): the port.
- The other ports, each a branch of its own:
  [Port Ex Machina's ports](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#engine-ports).
- Building and running:
  [Port Ex Machina's quick start](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/README.md#quick-start).
- How ports work:
  [`PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md).

## License

[zlib](LICENSE). Deus Ex belongs to its owners; this project is not
affiliated with or endorsed by them.

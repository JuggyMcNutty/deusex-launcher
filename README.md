# deusex-launcher

Deus Ex's original launcher, `System/DeusEx.exe`, recreated almost 1:1 for
desktop Linux, from the reverse-engineering record in
[dx-reverse-info](https://github.com/JuggyMcNutty/dx-reverse-info): its launch
sequence, its first-run wizard and safe mode, its command line, its crash
detection and its single-instance handoff. It is part of
[Port Ex Machina](https://github.com/JuggyMcNutty/port-ex-machina), which puts
it together with [VibeEngine](https://github.com/JuggyMcNutty/VibeEngine) into
ports of the game. Made with AI assistance.

You need your own copy of Deus Ex (version 1112fm); none of the game's files
are included.

## Branches

| Branch | What it is |
|---|---|
| `main` | the original, recreated almost 1:1 -- no additions, no ports |
| `linux-x86_64` | desktop Linux: `main` plus the launcher as Port Ex Machina grew it -- a tabbed home screen driven by a pad or the keyboard, the engine's real settings, pad layouts, a crash-isolated GPU probe ([`LAUNCHER.md`](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/docs/LAUNCHER.md)) -- and the desktop app around it |
| `trimui-smartpro` | the TrimUI Smart Pro (spruceOS): `linux-x86_64` plus the device's profile, CPU modes, cross toolchains and packaging |
| `linux-aarch64` | aarch64 Linux with an ordinary distro: `linux-x86_64`, cross-built |
| `android` | Android: the plan |
| `x360` | the Xbox 360: the plan |

A port's branch holds that port's configs and additions, and its
`ports/<id>/README.md` says what runs there and what was verified. Changes
flow one way: from `main` into `linux-x86_64`, and from `linux-x86_64` into
each device's branch. A fix to the original's behaviour is made on `main` and
merged up; a port branch adds files rather than editing `main`'s where it can,
and resolves a merge's conflicts itself. The device branches build through
Port Ex Machina (`scripts/dx.sh`), which fetches their toolchains and
sysroots.

## Where main stands

The recreation is in progress. In, and tested: the original's ini handling
(byte-identical round trips), its three command-line parsers, the `FirstRun`
gates and the entry decision tree, the crash sentinel, the single-instance
handoff, the CD check, and the hand-over to the game; and what the pages
decide -- the game's own strings from its `.int` files, the save migration,
safe mode's flags with all eight boxes wired, the Renderer page's list and
choice, and the Detail page's settings. Next: the launch sequence, the
splash, and the wizard's six pages on screen.

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

C11; the tests need no display. `test_gamefiles` runs the same checks against
a real install when `gamefiles/System` holds one, and is skipped otherwise.

## License

[zlib](LICENSE). Deus Ex belongs to its owners; this project is not
affiliated with or endorsed by them.

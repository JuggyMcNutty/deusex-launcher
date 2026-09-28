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
handoff, and the CD check; what the pages decide -- the game's own strings
from its `.int` files, the save migration, safe mode's flags with all eight
boxes wired, the Renderer page's list and choice, and the Detail page's
settings; and the launch sequence itself (`src/launch/`), from forwarding
to the game's end, down every road it can end by, with the screens as
stand-ins. Next: the screens -- the look, the six pages, the splash, the
message boxes -- and the `DeusEx` program they make with the sequence.

## The game and the launcher

The original's process becomes the game, so its launcher code stays for the
whole run: it holds the single-instance lock a second launch finds, closes
the splash once the engine is up, hands on what a second launch forwards,
and deletes `Running.ini` when the game ends cleanly. Here the engine is a
program of its own, started as `run-game.sh` beside the launcher with the
command line as given, its output into the launcher's log; the launcher
stays as its parent and does the same. Between the two runs a line, a
socket whose descriptor the engine finds in `DXL_LAUNCHER_FD`, one text line
per message:

| From | Line | Meaning |
|---|---|---|
| the engine | `hello` | it speaks this line; an engine silent for 2 s is taken not to, and the splash closes |
| the engine | `ready` | it is up, its first map in: the splash closes, as the original's does once the engine exists |
| the launcher | `TakeFocus` | a second launch forwarded its command line: the game's window to the front |
| the launcher | `Open <url>` | then the command line's first word, unless it starts with `-`: travel there |

A forwarded command line that arrives before `ready` is dropped, as the
original's log window drops one before its main loop runs.

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

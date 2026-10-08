# deusex-launcher

Deus Ex's original launcher, `System/DeusEx.exe`, recreated almost 1:1 for
desktop Linux from the reverse-engineering record in
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
| `linux-x86_64` | desktop Linux, where the project is developed: `main` plus the launcher the ports run ([`LAUNCHER.md`](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/docs/LAUNCHER.md)) and the desktop app around it |
| `trimui-smartpro` | the TrimUI Smart Pro (spruceOS): `linux-x86_64` plus the device's profile, CPU modes, cross toolchains and packaging |
| `linux-aarch64` | aarch64 Linux with an ordinary distro: `linux-x86_64`, cross-built |
| `android` | Android: the plan |
| `x360` | the Xbox 360: the plan |

`main` is the working base: each port branch is its own variant of it and
takes `main`'s changes by merge, but for `README.md`, which each port branch
keeps as its own. The rules: [`PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches).

## Where main stands

The recreation is whole:

- **Core**: ini handling (byte-identical round trips), the three command-line parsers, the `FirstRun` gates and entry decision tree, the crash sentinel, the single-instance handoff, the CD check.
- **The pages' decisions**: the game's `.int` strings, the save migration, safe mode's flags (all eight boxes wired), the Renderer page's list and choice, the Detail page's settings.
- **`DeusEx`**: the launch sequence (`src/launch/`) down every road it can end by; the wizard's six pages, the splash, the two message boxes; [VibeEngine](https://github.com/JuggyMcNutty/VibeEngine) started through `run-game.sh`.
- **Tests**: `ctest` for the core, the launch sequence and the pages; `tools/livecheck.py` runs every road a player can take live, with the real engine ([checking it live](#checking-it-live)).

## Installing

`DeusEx` goes in the game's `System/` folder beside the original's
`DeusEx.exe`, with `run-game.sh` and VibeEngine's `SurrealEngine`,
`libSurrealVideo.so` and `SurrealEngine.pk3`. Port Ex Machina's
[`scripts/recreation.sh`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/scripts/recreation.sh)
puts them there (`build`, then `install <GameDir>`); by hand,
`cmake --install build --prefix <GameDir>/System` and copy in the engine's
three files. Start `System/DeusEx` with the original's command line, from
anywhere: like the original, it finds the game and its package by its own
folder and name.

While `DeusEx` is there, the original game does not start under Wine or
Proton: it takes that file for its `DeusEx` package and stops
([a package's file](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)).
`scripts/recreation.sh uninstall` removes the five files.

## The look

The screens are the original's, drawn with SDL2: each control where
`Window.dll`'s and `DeusEx.exe`'s dialog templates put it
([page layouts](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/wizard.md#page-layouts)),
in the colours, fonts and details the original shows under wine.
`tools/pageshots.c` draws each screen with no display. Beside captures of
the original, 14 of the 15 match to the pixel; the splash differs only in
its picture, which wine shows colour-reduced.

Fonts come from a wine or Proton install: MS Sans Serif (`sserife.fon`) for
the pages, Arial at 12 pixels for the Driver page's link, Tahoma 8 pt for
the message boxes. Without wine, Liberation Sans or DejaVu Sans stand in;
`DXL_FONT`, `DXL_FONT_URL` and `DXL_FONT_MSG` name others. Text is
unsmoothed and unkerned, as Windows draws it. The error box's icon is
wine's `user32.dll` `IDI_HAND`, read from the same install
([`peicon.h`](src/core/peicon.h)) and blended as wine blends it
([`dialog.c`](src/gui/dialog.c)); without wine, one is drawn.

## Where it differs from the original

- **The game is a program of its own**, started and waited for
  ([below](#the-game-and-the-launcher)); the original's process becomes the
  game.
- **Detection finds no 3D device**: no Windows driver starts here, so the
  Renderer page leaves every device incompatible and picks the software
  renderer, unless the configuration says otherwise
  ([`renderdev.h`](src/core/renderdev.h)).
- **Safe mode's eight boxes are all wired**; in the original three do
  nothing and one does four boxes' work
  ([the shipped bug](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/wizard.md#-shipped-bug-three-safe-mode-checkboxes-are-dead)).
- **Safe mode relaunches after the run has ended**, this process becoming
  the launcher again; the original starts a second process while it is
  still ending.
- **A missing splash bitmap is not fatal**: no splash shows. The original
  asserts and dies.
- **Web pages open with the desktop's handler** (`xdg-open`).
- **No log window.** The original opens one every run, shown with `-log`;
  here `-log` only keeps the splash away, and the log is the file,
  `<Package>.log`, as the original's is too.
- **The window frames are the desktop's.** The wizard's carries the game's
  icon from the install's `DeusEx.exe` ([`peicon.h`](src/core/peicon.h));
  the message boxes and the splash have none, as in the original.
- **A flag's name ends at a space or the line's end** (`ParseParam`,
  `src/core/cmdline.c`); the original's (`Core.dll` `0x10146d00`) checks
  nothing after it, so there `-safemode` counts as `-safe` and `-log` is
  found in `-LOG=<file>`
  ([the parsers](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md)).
  Both take `/` as a switch, and a Linux path is full of them: the
  original's rule would find `-server` in `INI=/srv/server/x.ini`. A path
  that ends in a flag's name still counts. `tests/test_cmdline.c` asserts
  the launcher's way.

## Known defects

- **The command line's quotes are lost on the way to the engine.**
  `src/launch/launch.c` starts `run-game.sh` through `dxl_game_start`, which
  splits the line as a shell would (`src/core/argv.c`: quotes group words
  and are dropped), and `run-game.sh` joins the words with `$*` for
  `--cmdline=`. So `INI="My Mod.ini"` arrives as `INI=My Mod.ini`, which the
  engine, reading a quoted value whole as the original does, takes as
  `INI=My`; `EXEC=` and `USERINI=` alike. Handing `run-game.sh` the line as
  one argument (`--cmdline="$1"`) would keep it as given.

## The game and the launcher

The original's process becomes the game, so its launcher code stays for the
whole run: it holds the single-instance lock a second launch finds, closes
the splash when the engine is up, hands on what a second launch forwards,
and deletes `Running.ini` when the game ends cleanly. Here the engine is a
program of its own, `run-game.sh` started beside the launcher with the
command line as given, its output in the launcher's log. The launcher stays
as its parent and does the same, over a socket whose descriptor the engine
finds in `DXL_LAUNCHER_FD`, one text line per message:

| From | Line | Meaning |
|---|---|---|
| the engine | `hello` | it speaks this line; an engine silent for 2 s is taken not to, and the splash closes |
| the engine | `ready` | it is up, its first map in: the splash closes, as the original's does when the engine exists |
| the launcher | `TakeFocus` | a second launch forwarded its command line: the game's window to the front |
| the launcher | `Open <url>` | then the command line's first word, unless it starts with `-`: travel there |

A forwarded command line that arrives before `ready` is dropped, as the
original's log window drops one before its main loop runs.

`run-game.sh` starts the engine from `System/` with the game's own
`DeusEx.ini` and `User.ini` and the command line as one string,
`--cmdline=`
([what the engine reads of it](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/NATIVES.md#the-command-line));
`DXL_ENGINE_ARGS` adds engine options of its own, for a scripted run. Asked
to stop (SIGINT, SIGTERM, SIGHUP), the launcher sends the game SIGTERM,
and SIGKILL if it still runs 5 s later
([how the engine takes them](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#running-it)).
A game stopped so has not ended cleanly: `Running.ini` stays, and the next
launch opens RecoveryMode, as after a crash -- the usual end of a `-server`
run, which has no window to quit.

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build      # CMake 3.20 on; before it, cd build && ctest
```

C11, with SDL2 and SDL2_ttf (through `pkg-config`) for `DeusEx` and its
screens; `-DDXL_SCREENS=OFF` builds the core and its tests alone. The tests
need no display. `test_gamefiles` checks a real install when
`gamefiles/System` holds one, and `test_wizard` clicks through the pages
drawn into memory when a font is found; each is skipped otherwise.
`build/pageshots gamefiles/System shots/` draws the screens as pictures from
an install's strings and configuration, writing nothing there.

### Checking it live

With `DeusEx` installed, `tools/livecheck.py <GameDir> <out dir>` (or Port
Ex Machina's `scripts/recreation.sh check`) runs every scenario its header
lists, end to end, on a copy of the install (the files a run writes copied,
the rest linked) and a private X display where `xdotool` clicks, so neither
the install nor the desktop is touched. It needs Xvfb, xdotool,
ImageMagick's `import`, libX11 and `pgrep`, and leaves its screens and logs
in the out dir.

## License

[zlib](LICENSE). Deus Ex belongs to its owners; this project is not
affiliated with or endorsed by them.

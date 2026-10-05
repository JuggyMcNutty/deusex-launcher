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
sysroots. On a port's branch, `main`'s program -- `DeusEx`, its tools and
its tests -- rides along unbuilt: the branch builds the launcher the ports
run, `deusex-launcher`, `dxl-cli` and `dxl-shots` (its `LAUNCHER.md`), so
what this README says of `DeusEx` is `main`'s.

## Where main stands

The recreation is whole: the original's ini handling (byte-identical round
trips), its three command-line parsers, the `FirstRun` gates and the entry
decision tree, the crash sentinel, the single-instance handoff, and the CD
check; what the pages decide -- the game's own strings from its `.int`
files, the save migration, safe mode's flags with all eight boxes wired, the
Renderer page's list and choice, and the Detail page's settings; the launch
sequence itself (`src/launch/`), from forwarding to the game's end, down
every road it can end by; its screens -- the wizard's six pages, the splash
and the two message boxes -- in the `DeusEx` program; and the game it starts,
[VibeEngine](https://github.com/JuggyMcNutty/VibeEngine), through
`run-game.sh`. Unit tests cover the core, the launch sequence and the
wizard's pages (`ctest`); `tools/livecheck.py` runs the whole of it live,
the splash and the message boxes too -- every road above that the player
can take, with the real engine -- where nobody sees it ([checking it
live](#checking-it-live)).

## Installing

`DeusEx` lives in the game's `System/` folder, beside the original's
`DeusEx.exe`, with `run-game.sh` and the engine -- VibeEngine's
`SurrealEngine`, `libSurrealVideo.so` and `SurrealEngine.pk3` -- beside it.
Port Ex Machina's
[`scripts/recreation.sh`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/scripts/recreation.sh)
builds this branch and the engine and installs them there (`build`, then
`install <GameDir>`); by hand, `cmake --install build --prefix
<GameDir>/System` and the engine's three files copied in. Then start
`System/DeusEx` with the original's command line, from anywhere: its folder
and name are where it finds the game and its package, as the original's
are.

While `DeusEx` is there, the original game does not start under Wine or
Proton: it takes a file of that name in its `System` folder for its
`DeusEx` package, and stops at its start
([a package's file](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#packages-and-linkers)).
`scripts/recreation.sh uninstall` takes the five files out again.

## The look

The screens are the original's, drawn with SDL2: every control at its place
in `Window.dll`'s and `DeusEx.exe`'s dialog templates, in the colours, fonts
and details the original shows under wine, where it was captured page by
page
([live-verification.md](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/live-verification.md#the-second-run-every-page-2026-09-27)).
`tools/pageshots.c` draws each screen with no display, to lay beside those
captures: 14 of the 15 match to the pixel. The other: the splash, whose
picture wine shows colour-reduced while this shows the bitmap as it is --
its frame and place match.

The fonts are wine's own, found in a wine or Proton install: MS Sans Serif
(`sserife.fon`) for the pages, Arial at 12 pixels for the Driver page's
link, Tahoma 8 pt for the message boxes. Without wine, the nearest common
fonts stand in (Liberation Sans, DejaVu Sans); `DXL_FONT`, `DXL_FONT_URL`
and `DXL_FONT_MSG` name others. Text is drawn unsmoothed and unkerned, as
Windows draws it. The error box's icon is wine's too, its `user32.dll`'s
`IDI_HAND` read out of the same install ([`peicon.h`](src/core/peicon.h))
and blended as wine blends it ([`dialog.c`](src/gui/dialog.c)); without
wine, one is drawn.

## Where it differs from the original

- **The game is a program of its own**, which the launcher starts and
  waits for (below), where the original's process becomes the game.
- **Detection finds no 3D device.** No Windows driver can start here, so
  every device the Renderer page tests is left incompatible and the
  software renderer is chosen, unless the configuration already says
  otherwise ([`renderdev.h`](src/core/renderdev.h)).
- **Safe mode's eight boxes are all wired.** In the original three of them
  do nothing and one does four boxes' work
  ([the shipped bug](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/wizard.md#-shipped-bug-three-safe-mode-checkboxes-are-dead)).
- **Safe mode's relaunch comes after the run has ended**, as this process
  becoming the launcher again, where the original starts a second process
  while it is still ending.
- **A missing splash bitmap is not fatal**: there is no splash. The
  original asserts and dies.
- **Web pages open with the desktop's handler** (`xdg-open`).
- **No log window.** The original opens one on every run, shown with
  `-log`; here `-log` only keeps the splash away, and the log is the file,
  `<Package>.log`, as the original's is too.
- **The windows' frames are the desktop's.** The wizard's carries the
  game's icon, as the original's does, read out of the install's
  `DeusEx.exe` ([`peicon.h`](src/core/peicon.h)); the message boxes and the
  splash have none, as the original's have none.

## Known defects

Found 2026-10-04 and not fixed yet:

- **The command line's quotes are lost on the way to the engine.**
  `src/launch/launch.c` starts `run-game.sh` through `dxl_game_start`, which
  splits the line as a shell would (`src/core/argv.c`: quotes group words and
  are dropped), and `run-game.sh` joins the words again with `$*` for
  `--cmdline=`. `INI="My Mod.ini"` reaches the engine as `INI=My Mod.ini`,
  which it -- reading a quoted value whole, as the original does -- takes as
  `INI=My`; `EXEC=` and `USERINI=` alike. Handing `run-game.sh` the line as one
  argument (`--cmdline="$1"`) keeps it as given.
- **`ParseParam` is stricter than the original's.** `src/core/cmdline.c` wants
  the name to end at a space or the line's end; the original's (`Core.dll`
  `0x10146d00`) checks nothing after it, so `-safemode` counts as `-safe` there
  and `-log` is found in `-LOG=<file>`
  ([the parsers](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/cli-flags.md)).
  `tests/test_cmdline.c` asserts the launcher's way.

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

`run-game.sh` starts the engine from `System/` with the game's own
`DeusEx.ini` and `User.ini` and the command line as one string,
`--cmdline=` -- which the engine reads as the original's engine does: the
start URL, `-server`, `INI=`, `USERINI=`, `EXEC=` and safe mode's flags
([VibeEngine's NATIVES.md](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/NATIVES.md#the-command-line)).
`DXL_ENGINE_ARGS` adds engine options of its own, for a scripted run. Asked
to stop (SIGINT, SIGTERM, SIGHUP), the launcher passes SIGTERM on to the
game and, if it is still running 5 s later, SIGKILL: VibeEngine with a
window takes no notice of SIGTERM ([running it](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#running-it)),
though its dedicated server ends on it. A game stopped so has not ended
cleanly: `Running.ini` stays, and the next launch opens RecoveryMode, as
after a crash -- the usual end of a `-server` run, which has no window to
quit.

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build      # CMake 3.20 on; before it, cd build && ctest
```

C11, with SDL2 and SDL2_ttf (through `pkg-config`) for `DeusEx` and its
screens; `-DDXL_SCREENS=OFF` builds the core and its tests alone. The tests
need no display. `test_gamefiles` runs the same checks against a real
install when `gamefiles/System` holds one, and is skipped otherwise;
`test_wizard` clicks through the pages drawn into memory, and is skipped
where no font is found.

The screens as pictures, from an install's strings and configuration, which
are read and not written:

```sh
build/pageshots gamefiles/System shots/
```

### Checking it live

With `DeusEx` installed, `tools/livecheck.py <GameDir> <out dir>` (or
Port Ex Machina's `scripts/recreation.sh check`) runs it end to end on a copy
of the install -- the files a run writes copied, the rest linked -- on a
private X display where `xdotool` clicks, so neither the install nor the
desktop is touched: `-consolecommand=` and `-testrendev=`; `-make`'s box;
`-changevideo`; a first run through detection and every page to the game,
with `EXEC=`, a second launch forwarded to it and travelled, and its clean
end; safe mode's Run! and the relaunch, the engine silent in a 640×480
window with no pad; a killed game, then RecoveryMode; the CD prompt's
Cancel; and `-server`, stopped by a signal. It needs Xvfb, xdotool,
ImageMagick's `import`, libX11 and `pgrep`; its screens and logs are left in
the out dir.


## License

[zlib](LICENSE). Deus Ex belongs to its owners; this project is not
affiliated with or endorsed by them.

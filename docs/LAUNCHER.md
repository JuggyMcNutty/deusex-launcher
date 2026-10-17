# DXLauncher

DXLauncher is the desktop's launcher for Deus Ex: an app of its own,
outside the game's folder, that finds the game, keeps the player's settings
and saves in a profile, and starts
[VibeEngine](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md)
and stays with it. Two programs: `deusex-launcher`, the launcher, and
`dxl-cli`, the same with no window, for scripts and for looking. C11, its
core with no SDL; this branch's own, as each port branch's launcher is
([the branches](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches)).
It keeps part of the original, which `main` recreates
([what it changes](#what-it-changes-from-the-original)).

Its screens are not built yet: both programs start the game with no
window, and a screen that is due is a message
([known defects](#known-defects)).

## Where everything lives

Nothing is written to the game's folder. The launcher's files are in the
home's XDG folders, each with a `dxlauncher` folder in it
(`src/core/xdg.h`); `DXL_HOME=<dir>` puts all four under one folder, a
scratch home:

| Folder | What is in it |
|---|---|
| `$XDG_CONFIG_HOME/dxlauncher/` (`~/.config`) | `launcher.ini`: the active profile (`[Launcher] Profile`), each install met (`[Install.<id>]` `Path`, `Version`, `SHA1`) |
| `$XDG_CONFIG_HOME/dxlauncher/profiles/<id>/` | `profile.ini` (`[Profile]` `Name`, `Install`); `DeusEx.ini` and `User.ini`, the engine's `--ini` and `--userini`; the `engine` folder, its `--userdir`: `Settings.json`, a class's own config files, the engine's log and crash reports |
| `$XDG_DATA_HOME/dxlauncher/profiles/<id>/` (`~/.local/share`) | `Save`, the inis' `SavePath`; `Screenshots`, the engine's shots (the `engine` folder's `Screenshots` is a link to it) |
| `$XDG_STATE_HOME/dxlauncher/` (`~/.local/state`) | `launcher.log`, each start's; per profile `Running.ini`, `LastRun.ini`, and in `logs` the engine's output, `last-run.log` and `previous-run.log` |
| `$XDG_CACHE_HOME/dxlauncher/profiles/<id>/Cache/` (`~/.cache`) | the inis' `CachePath`: a net game's downloads |

A profile's inis keep `[Core.System]`'s `Paths` as the game has them: the
engine resolves them against the game's `System` folder. `SavePath` and
`CachePath` are absolute, the profile's. The inis are LF, as the engine
writes them back. The engine never touches `~/.config/SurrealEngine`,
which the harness's runs read: an engine whose `--version` lacks
`userdir` is refused.

The launcher's own folder is where `deusex-launcher` is: the engine beside
it (`SurrealEngine`, its `.pk3`), and `data/`. `DXL_ENGINE=<path>` runs
another engine.

## An install

A folder is an install when `System/DeusEx.exe` is there by that name, and
the engine plays it only when that file's SHA1 is one it knows
(`src/core/gamedb.c`, the engine's own list, which Port Ex Machina's
`scripts/dx.sh check` holds them to): 1112fm, GOG's and Steam's; 1002f, the
first release, runs with a warning. Beside it the files the engine cannot
start without, in any case: `System`'s `Core.u`, `Engine.u`, `Extension.u`,
`DeusEx.u`, `DeusExUI.u`, `Default.ini` and `DefUser.ini`, and
`Maps/DX.dx` (`src/core/install.c`).

## The first run

With no profile yet, the first run makes `default` from the install
`--game=<dir>` names (`src/core/profile.c`, `dxl_profile_import`), reading
it and writing nothing there:

- **`DeusEx.ini`**: the game's own, else `Default.ini`; one with no
  `[Core.System] Paths`, a stub the engine cannot start from, is
  `Default.ini` with its values over it. The engine's own sections
  (`Engine.SurrealClient`, `SurrealAudioDevice`, `SurrealRenderDevice`,
  `SurrealWindowSystem`) come from `SE-DeusEx.ini`, which the engine writes
  when no launcher names an ini, for each the game's lacks.
- **`User.ini`**: the newer of `User.ini` and `SE-User.ini`, else
  `DefUser.ini`.
- `SavePath` and `CachePath` the profile's; GameSpy's closed master server
  (`[DeusEx.MenuScreenJoinGame] MasterServerAddress=master0.gamespy.com`)
  given 333networks', `master.333networks.com`.
- **The saves** (every folder of the game's save folder but `Current`) and
  the shots (`System/Shot*.bmp`) copied. `main`'s launcher's files in
  `System` (its `DeusEx`, `run-game.sh`, `Running.ini`) are not read.

`dxl-cli --dry-run --game=<dir>` says what it would make, before anything
is made. `--game=<dir>` on a profile that has one points it at that install
from then on.

## One start

`src/app/run.h`, in this order:

1. **A second start** hands its command line to the launcher that runs and
   ends, unless the line holds one of the original's four bypass words
   (`Server`, `NewWindow`, `changevideo`, `TestRenDev`). One launcher for a
   home: the lock is its config folder's.
2. **The profile**, made by the first run if there is none.
3. **The decision**, the original's (`src/core/policy.c`): Recovery after
   `-safe`, or after a run whose `Running.ini` survived; Graphics after
   `-changevideo`; else Home. The original's first-time renderer page has no
   screen of its own here: the first run is the launcher's. `--play` and
   `DXL_NO_HOME=1` skip Home, not the others.
4. **Before the game**, after the screens, as the original does it: the
   inis saved, `FirstRun` raised to 1100, `Running.ini` made.
5. **The engine** asked what it can do (`--version`), and started.
6. **The end**: a clean one (exit 0, with the engine's "ready" seen) takes
   `Running.ini` away and runs a `RELAUNCH` the engine passed on, decided
   again in this process: `-server` a dedicated server, `-changevideo`
   Graphics. One that is not clean leaves `Running.ini`, and says how the
   run ended.

The launcher's own options start with `--` (`--game=`, `--profile=`,
`--play`); every other word is the original's command line, the player's,
which reaches the engine whole in `--cmdline`. Exit codes: the game's own
status, or 2 for an option it does not know, 3 for a screen that is due
with no window, 4 for no install or engine to play.

## The game and the launcher

The engine is the launcher's child, started by its words, with no shell
between them (`src/core/engine_args.c`, `src/platform/posix/process.c`):

```
<app>/SurrealEngine --no-launcher <game> --ini=<profile>/DeusEx.ini
    --userini=<profile>/User.ini --userdir=<profile>/engine
    [--url=<url>] [DXL_ENGINE_ARGS's words] --cmdline=<the original's command line>
```

in the game's `System` folder, its output into the profile's
`last-run.log`, with `SURREALWIDGETS_DISPLAY_BACKEND=SDL2` unless the
environment names a backend (only SDL2's has the fork's pad support), and
`DXL_LAUNCHER_FD`, the launcher's end of a socket pair. On that line the
engine says `hello` as it starts, `ready` once its first map is in, and
`relaunch <line>` for the original's `RELAUNCH`; the launcher hands it
`TakeFocus` and `Open <url>` for a line a second start forwards, once the
engine is ready (`src/app/play.c`). A start URL goes in `--url`: in
`--cmdline` the engine takes one only with `-hax0r` or `-server`.
`dxl-cli --print-command` shows the command for a profile.

The launcher waits on the line and its own socket, waking once a second
besides. Stopped (SIGINT, SIGTERM, SIGHUP), it passes SIGTERM on, and
SIGKILL 5 s later to an engine that takes no notice of SIGTERM, as the
fork's under SDL does.

## Recovery

`Running.ini`, the original's crash sentinel, in the profile's state
folder: made before the game, taken away at a clean end. Beside it,
`LastRun.ini` (`src/core/lastrun.c`) says how the last run ended: its exit
code or signal, the engine's error line (`SurrealEngine error: ...`),
whether it was ever ready. With no window, the start that finds
`Running.ini` says so with the end of the run's log, and exits 3.

## dxl-cli

| Mode | What it does |
|---|---|
| `--dry-run` | what the start would decide, make and run; nothing is written |
| `--print-command` | the engine's command line for the profile; nothing is run |
| `--import --game=<dir>` | a profile made from the install, nothing run |
| `--play` | the start, with no Home |
| `--probe` | the Vulkan, OpenGL ES and OpenGL this machine offers (`src/platform/posix/gpu_probe.c`) |

## How it is tested

`scripts/dx.sh test` runs the host tests, with no display. The stand-in
engine `tests/stub_engine.c` answers `--version` and talks on the line as
the engine does, or crashes, fails, relaunches, prints its words or never
ends (`test_process`, `test_play`). `test_profile` imports from a fake
install made read-only, which it leaves as it was; `test_gamefiles`, on a
real install in `gamefiles/`, plays the stub from a scratch home through a
first run, a crash and the Recovery that follows, and checks that nothing
in the game's folder changed.

## What it changes from the original

The original's behaviour is recorded in
[`launcher.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/launcher.md).
Kept as the original does it: the decision's order (`-safe`, the
`FirstRun` gates, `-changevideo`, `Running.ini`), `Running.ini` made after
the screens and taken away at a clean end, `FirstRun` raised to 1100, the
three command-line parsers (`ParseParam` stricter, as `main`'s), and the
second start's hand-over, its line now to a launcher that keeps it
whatever the install. Changed:

| Original | DXLauncher | Why |
|---|---|---|
| `System/DeusEx.exe`, in the game's folder, its settings in the game's `DeusEx.ini` and `User.ini` | An app of its own; the settings, saves and shots in a profile under the home's XDG folders | Nothing written to the game's folder; more than one way to play one install |
| The launcher and the engine one process, which stays for the game's run | The engine a child, started by its words, which the launcher waits on, holding its lock to the end | The engine is a program of its own; started by its words, a quoted word in the command line arrives whole, where `main`'s split them |
| The CD check | Dropped; an install is checked by `DeusEx.exe`'s build and the files the engine needs | No disc to wait for |
| Safe mode's eight flags on a relaunch; the save migration below FirstRun 220; the Detail page | Not carried | The engine reads none of the Detail page's keys; the saves are 1112fm's |

## Known defects

- **No screens yet.** Home is skipped, and First run (with no
  `--game=<dir>`), Recovery (after `-safe` or a run that did not end
  cleanly) and Graphics (after `-changevideo`) are a message and exit 3:
  removing the profile's `Running.ini` lets the next start play.
- **The settings are the inis' alone.** `Settings.json` is not written:
  the engine takes its own defaults, the Vulkan renderer and 4x
  anti-aliasing, and `data/` (the renderer list, the engine settings'
  defaults) is the Smart Pro's, unused.
- **No install is found by itself**: `--game=<dir>` names it.

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

`deusex-launcher`'s screens are a window of its own, drawn with SDL3 and
SDL3_ttf: Home, the first run, Recovery, About
([the window](#the-window)).

## Where everything lives

Nothing is written to the game's folder. The launcher's files are in the
home's XDG folders, each with a `dxlauncher` folder in it
(`src/core/xdg.h`); `DXL_HOME=<dir>` puts all four under one folder, a
scratch home:

| Folder | What is in it |
|---|---|
| `$XDG_CONFIG_HOME/dxlauncher/` (`~/.config`) | `launcher.ini`: the active profile (`[Launcher] Profile`), each install met (`[Install.<id>]` `Path`, `Version`, `SHA1`), the window as it last closed (`[Window]` `Width`, `Height`, `Maximized`) |
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
it (`SurrealEngine`, its `.pk3`), `THIRD-PARTY.md`, and `data/`: the
window's type (`data/fonts`), the app menu's entry
([the app menu](#the-app-menu)), and the Smart Pro's renderer list and
engine settings' defaults. `DXL_ENGINE=<path>` runs another engine.

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

With no profile yet, the first run makes `default`, in three pages:
Welcome; The game, the install it is made from; Your profile, what will be
made, before anything is. The game lists what the finder found
(`src/core/finder.c`), each folder once and checked: the folder
`--game=<dir>` names, the installs `launcher.ini` lists, the game in
Steam's own library, as Steam's distro package, its Flatpak and its Snap
keep it, and each folder in `~/Games/Heroic` and `~/GOG Games`. Another is shown by the launcher's folder browser, which a
pad works, or by the system's folder dialog. `dxl-cli` makes the profile
from `--game=<dir>`'s install. Either way the profile is made from the
install by `dxl_profile_import` (`src/core/profile.c`), reading it and
writing nothing there:

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
from then on. A profile whose install is not there to play any more (moved,
or deleted) opens on The game: the one chosen there is its install from
then on.

## One start

`src/app/run.h`, in this order:

1. **A second start** hands its command line to the launcher that runs and
   ends, unless the line holds one of the original's four bypass words
   (`Server`, `NewWindow`, `changevideo`, `TestRenDev`). One launcher for a
   home: the lock is its config folder's.
2. **The profile**, made by the first run if there is none; pointed at
   another install, on The game, if its own is not there to play.
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
with no window (`dxl-cli`'s), 4 for no install or engine to play.

## The window

`deusex-launcher`'s window (`src/screens/`) opens only when a screen is
due -- a start that plays at once opens none -- and closes before the game
starts, its renderer and SDL's gamepads with it, so the game has the pads
to itself; a screen after the game, Recovery, opens it again. Its pages:

| Page | What it has |
|---|---|
| Home | Play, About, Quit; the profile's game, its build, the engine, its saves, its last run |
| Welcome, The game, Your profile | [the first run](#the-first-run) |
| Recovery | how the last run ended and when, the engine's error, the end of its log; Play, the log opened, Quit |
| About | the launcher's version, the engine's and its features, the four folders, the libraries and fonts; the app menu's entry, the settings folder opened |

Graphics (`-changevideo`) opens Home ([known defects](#known-defects)).

It is our own drawing on SDL3's renderer (`src/ui/`), with none of the
game's art: steel panels with cut corners and lit edges, title bars in the
theme's colour, and round what has the focus the four corner brackets the
game puts round what can be used. The colours are made from the game's
default menu theme, `ColorThemeMenu_Default` (`src/ui/theme.c`); its type
is in `data/fonts` (`THIRD-PARTY.md` says whose). It is laid out in dp,
which the display's scale makes pixels, so it is as sharp at any size: for
1280 x 720 dp, and no smaller than 960 x 600; the window opens at
1280 x 720, or as it last closed.

**Controls.** The keyboard: the arrows or WASD move, Enter or Space
chooses, Esc or Backspace goes back, Tab and Shift-Tab step through, Page
Up and Page Down scroll a list, F5 plays. The mouse: the pointer focuses
what it moves onto, a click chooses, the wheel scrolls a list. A pad: the
d-pad or the left stick moves (held, again after 350 ms and then every
90), its confirm button chooses and its back button goes back -- south and
east, or on a Nintendo pad, whose A is east, east and south -- and Start
plays. The foot of each page shows what to press on the device last used,
keycaps or the pad's own buttons as SDL names them, drawn as shapes of our
own, no maker's logo; a hint clicked does what its key does. The pointer
hides while a pad is used.

Idle, it costs nothing: it waits on its events and draws only while
something moves, VSync-paced. A second start while it is up hands it its
command line and raises it; a pad connected or lost is a note in its
corner.

## The app menu

`deusex-launcher --install-desktop`, or About's button, puts the launcher on
the desktop's app menu: `$XDG_DATA_HOME/applications/io.github.juggymcnutty.DXLauncher.desktop`,
from `data/`'s template with the launcher's own path, and its icon, our own
drawing, at 32 to 256 px under `$XDG_DATA_HOME/icons/hicolor`
(`src/platform/posix/desktop.c`, `src/ui/icon.c`). The window has the same
id, so the desktop puts the two together. `--remove-desktop`, or the button
again, takes them away; nothing else writes them.

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
whether it was ever ready. The window opens on Recovery after a run that
was not clean, and at the next start that finds `Running.ini`; with no
window (`dxl-cli`), that start says so with the end of the run's log, and
exits 3.

## dxl-cli

| Mode | What it does |
|---|---|
| `--dry-run` | what the start would decide, make and run; nothing is written |
| `--print-command` | the engine's command line for the profile; nothing is run |
| `--import --game=<dir>` | a profile made from the install, nothing run |
| `--play` | the start, with no Home |
| `--probe` | the Vulkan, OpenGL ES and OpenGL this machine offers (`src/platform/posix/gpu_probe.c`) |

## How it is tested

`scripts/dx.sh test` runs the host tests, with no display, side by side.
The stand-in
engine `tests/stub_engine.c` answers `--version` and talks on the line as
the engine does, or crashes, fails, relaunches, prints its words or never
ends (`test_process`, `test_play`). `test_profile` imports from a fake
install made read-only, which it leaves as it was; `test_gamefiles`, on a
real install in `gamefiles/`, plays the stub from a scratch home through a
first run, a crash and the Recovery that follows, and checks that nothing
in the game's folder changed.

The window's kit is tested on SDL's software renderer and a surface:
focus and navigation (`test_ui_focus`), text (`test_ui_text`), and the
keyboard, the mouse and virtual pads with an Xbox's, a DualSense's and a
Switch Pro's ids (`test_ui_input`). `dxl-shots` (`tools/shots.c`) draws
every page and dialog, from states it makes up in a scratch home, at
1280x720, 1280x800, 1920x1080, 2560x1440 at 1.5, 3840x2160 at 2 and
960x600, with the keyboard, the mouse and four virtual pads, and fails on
any diagnostic of the kit's: a label cut short, focusables on one another
or off the window (`test_ui_shots_*`); with `--out=<dir>` it writes them as
PNGs for review. On the desktop's display, `DXL_UI_SCRIPT=<file>` gives the
window a script of keys, page checks and shots, through its own event
queue (`src/screens/uiscript.h`): a first run from a fresh home to the game
and its clean end, a crash into Recovery, and a second start handed over
are run that way.

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
| The setup wizard's pages, Win32 dialogs of a fixed size | A window of pages of its own, laid out in dp, for the keyboard, the mouse and a pad | The desktop's screens, from a laptop's to a television's, and pads |
| Safe mode's eight flags on a relaunch; the save migration below FirstRun 220; the Detail page | Not carried | The engine reads none of the Detail page's keys; the saves are 1112fm's |

## Known defects

- **No Graphics page**: `-changevideo`, and an engine's `RELAUNCH` with
  it, open Home.
- **Not the player's theme**: the window's colours are the default menu
  theme's, whatever the profile's `MenuThemeName`.
- **The settings are the inis' alone.** `Settings.json` is not written:
  the engine takes its own defaults, the Vulkan renderer and 4x
  anti-aliasing, and `data/` (the renderer list, the engine settings'
  defaults) is the Smart Pro's, unused.
- **The finder looks in a few places**: Steam's own library, not the
  others `libraryfolders.vdf` lists; `~/Games/Heroic` and `~/GOG Games`;
  not Lutris, Bottles or Wine prefixes. The folder browser, the system's
  dialog and `--game=<dir>` find the rest.

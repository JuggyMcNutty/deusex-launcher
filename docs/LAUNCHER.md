# DXLauncher

DXLauncher is the desktop's launcher for Deus Ex: an app of its own,
outside the game's folder, that finds the game, keeps the player's settings
and saves in a profile, and starts
[VibeEngine](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md)
and stays with it. Two programs: `deusex-launcher`, the launcher, and
`dxl-cli`, the same with no window, for scripts and for looking. C11, its
core with no SDL; the desktop Linux branches', linux-x86_64's and
linux-aarch64's, whose launcher commits each come over to the other by a
cherry-pick, so this file is the same on both
([the branches](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#the-branches)).
It keeps part of the original, which `main` recreates
([what it changes](#what-it-changes-from-the-original)).

`deusex-launcher`'s screens are a window of its own, drawn with SDL3 and
SDL3_ttf: Home, the first run, Settings, Recovery, the log, About
([the window](#the-window)).

## Where everything lives

Nothing is written to the game's folder. The launcher's files are in the
home's XDG folders, each with a `dxlauncher` folder in it
(`src/core/xdg.h`); `DXL_HOME=<dir>` puts all four under one folder, a
scratch home:

| Folder | What is in it |
|---|---|
| `$XDG_CONFIG_HOME/dxlauncher/` (`~/.config`) | `launcher.ini`: the active profile (`[Launcher] Profile`) and the launcher's own settings (`UIScale`, `ReducedMotion`, `ReturnAfterPlaying`, `RenderDriver`: [Settings](#settings)), each install met (`[Install.<id>]` `Path`, `Version`, `SHA1`), the window as it last closed (`[Window]` `Width`, `Height`, `Maximized`) |
| `$XDG_CONFIG_HOME/dxlauncher/profiles/<id>/` | `profile.ini` (`[Profile]` `Name`, `Install`, `EngineArgs`); `DeusEx.ini` and `User.ini`, the engine's `--ini` and `--userini`; the `engine` folder, its `--userdir`: `Settings.json` (its renderer and how it draws), a class's own config files, the engine's log and crash reports |
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
([the app menu](#the-app-menu)), the engine's renderers and what each needs
of the machine (`renderers.ini`), and `Settings.json`'s defaults
(`engine-settings.json.default`). `DXL_ENGINE=<path>` runs another engine.

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
  when no launcher names an ini, for each the game's lacks; then each key
  of them they lack is written, as the engine would take it
  (`src/core/engine_sections.h`).
- **`User.ini`**: the newer of `User.ini` and `SE-User.ini`, else
  `DefUser.ini`. The bindings the original game saved, in
  `[Extension.InputExt]`, put in `[Engine.Input]`, the one section of them
  the engine reads, and taken out of the other.
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
   inis saved, `FirstRun` raised to 1100, `Running.ini` made; and any key
   of the engine's sections they lack written.
5. **The engine** asked what it can do (`--version`), and started.
6. **The end**: the inis read again, as the engine left them. A clean end
   (exit 0, with the engine's "ready" seen) takes `Running.ini` away and
   runs a `RELAUNCH` the engine passed on, decided again in this process:
   `-server` a dedicated server, `-changevideo` Graphics; with none, Home
   again when `[Launcher] ReturnAfterPlaying` is on. One that is not clean
   leaves `Running.ini`, and says how the run ended.

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
| Home | Play, Settings, About, Quit; the profile's game, its build, the engine, its saves, its last run |
| Welcome, The game, Your profile | [the first run](#the-first-run) |
| Settings | [the settings](#settings), a tab a page |
| Recovery | how the last run ended and when, the engine's error, what to do about it, the end of its log, the next run's words; Play, the fix, the log, the defaults, Quit ([Recovery](#recovery)) |
| The log | the last run's output, the run's before it, or the engine's own log, scrolled by the keys, the wheel or a pad; the file opened |
| About | the launcher's version, the engine's and its features, the four folders, the libraries and fonts; the app menu's entry, the settings folder opened |

Graphics (`-changevideo`, and an engine's `RELAUNCH` with it) opens
Settings on its Graphics tab, whose Done is Play.

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
Up and Page Down scroll a list, Ctrl-Tab and Ctrl-Shift-Tab change tabs,
F5 plays. The mouse: the pointer focuses
what it moves onto, a click chooses, the wheel scrolls a list. A pad: the
d-pad or the left stick moves (held, again after 350 ms and then every
90), its confirm button chooses and its back button goes back -- south and
east, or on a Nintendo pad, whose A is east, east and south -- its
shoulders change tabs, and Start plays. The foot of each page shows what to press on the device last used,
keycaps or the pad's own buttons as SDL names them, drawn as shapes of our
own, no maker's logo; a hint clicked does what its key does. The pointer
hides while a pad is used.

Idle, it costs nothing: it waits on its events and draws only while
something moves, VSync-paced. A second start while it is up hands it its
command line and raises it; a pad connected or lost is a note in its
corner.

## Settings

Settings' tabs are its pages: Display, Graphics, Audio, Keyboard & mouse,
Gameplay & HUD, Multiplayer, Launcher; a page's advanced rows come after
its others. Each row is the registry's (`src/core/settings_table.c`): what
it is, its help, and the places it is kept, each with the codec its reader
takes (`src/core/settings.c`):

| Kept in | What |
|---|---|
| `DeusEx.ini` | the engine's sections, `[Engine.SurrealClient]`, `SurrealAudioDevice`, `SurrealRenderDevice`: full screen, the window's size, brightness, decals, moving lights, flashes, detail textures, fog, the volumes and the mixer; `[DeusEx.MenuScreenJoinGame] MasterServerAddress` |
| `User.ini` | `[Engine.PlayerPawn]`: the field of view, the mouse; `[DeusEx.DeusExPlayer]`: the gameplay and HUD rows and the colour themes; `[DefaultPlayer] Name`; `[Engine.Input]`: the keys |
| the engine folder's `Settings.json` | the renderer, anti-aliasing, lighting, the brightness curve, vertical sync, the render scale, bloom, HDR |
| `launcher.ini`, `profile.ini` | the launcher's own: its size, reduced motion, Home after playing, its renderer; the engine's extra words |

**As the engine reads them.** The engine reads its own sections once
`[Engine.SurrealAudioDevice]` has a key, else the retail ones
(`[WinDrv.WindowsClient]`, `[Galaxy.GalaxyAudioSubsystem]`, the render
device's), taking its built-in default for a key missing there; a row reads
the same way, and is written to both, so the two agree. The engine stops at
start on a value its converters refuse (`yes` for a bool, a rate without
`Hz`): to a row such text is no value either. A value is written as the
engine writes it (`True`, `0.600000`, `153`, `44100Hz`), and only as the
player changes it. Before the page opens and before each run, each key of
the engine's sections that is missing is written, from the retail value
where there is one, else the engine's default (`src/core/engine_sections.c`),
so the engine reads what the page shows. A row's default is what is taken
with no value kept: the engine's own, the class's for `User.ini`'s keys,
the defaults file's for `Settings.json`.

**Back to the defaults** gives a page what a new profile has: the install's
`Default.ini` as the import makes it, its `DefUser.ini`, `Settings.json`'s
defaults file; on Keyboard & mouse, the keys too. Recovery's The defaults...
does it for a page or for all.

**Graphics** has the presets: As in 2000 (the game as its Direct3D renderer
drew it: no multisampling, bloom or detail textures), Enhanced (4x
multisampling, bloom, detail textures) and Performance (less to draw, three
quarters of the window's size); Custom once a row is changed by itself. Its
Renderer offers the engine's renderers that run here, Vulkan, OpenGL 4.2
and OpenGL ES 3.2 (`data/renderers.ini`), which `dxl-cli --probe --ini`
finds in a child process as the page needs them.

**Keyboard & mouse** has the game's key menu's actions, and the commands
the game's defaults bind besides (the pause menu, the belt's slots, the
augmentations' F-keys), two keys each from `User.ini`'s `[Engine.Input]`. A
slot takes the next key, mouse button or wheel notch; Esc puts it back,
Delete takes the key away. A key bound to another action asks: that action
instead, or both (`a|b`), the key then each one's. Keys are named as the engine's window gives them
(`src/core/keys.c`): a key that window passes on as none is not taken, and
a binding on one is marked ([known defects](#known-defects)). The game's own
key menu refuses Escape, F1 to F18, the digits, Tilde, PrintScrn and Pause,
whose bindings it does not show; this page shows them, and takes all but
Escape.

**A text row** takes the keyboard's text, Enter keeping it and Esc putting
it back, or a pad's through the on-screen keyboard (`src/ui/osk.c`). Your
name keeps what the game takes in a name, letters, digits and `[ ] ( )`,
and is `Player` when nothing of it is left.

**The next run's words**, `-nosound`, `-nohard -noddraw` (a window),
`-defaultres` (640 x 480) and `-nojoy`, are Launcher's advanced rows and
Recovery's: added to the original's command line of the run Play starts
next, and of no other.

The changes are in the open files at once, and written as Settings is
left, by Done or otherwise. The launcher's size and reduced motion apply at
once, its renderer from the window's next opening.

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
    [--url=<url>] [profile.ini's EngineArgs] [DXL_ENGINE_ARGS's words]
    --cmdline=<the original's command line>
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

The page says what to do about the run's end (`src/core/advice.c`). For a
crash, and for the errors it knows -- the Vulkan renderer not made, OpenAL
not started, a package or the engine's `.pk3` not found, an engine that
ended before its first map -- it says the likely cause, and has its fix as
a button: another renderer, of those that run here; a run without sound;
the game's folder. Below are the end of the log where it fits, the [next
run's words](#settings), and Play, the fix, the log, the defaults of a
page or of all, Quit.

## dxl-cli

| Mode | What it does |
|---|---|
| `--dry-run` | what the start would decide, make and run; nothing is written |
| `--print-command` | the engine's command line for the profile; nothing is run |
| `--import --game=<dir>` | a profile made from the install, nothing run |
| `--play` | the start, with no Home |
| `--probe` | the Vulkan, OpenGL ES and OpenGL this machine offers (`src/platform/posix/gpu_probe.c`); with `--ini`, as an ini, as the window reads it |

## How it is tested

`scripts/dx.sh test` runs the host tests side by side, all but
`test_ui_scripts` with no display.
`test_settings` takes the registry's rows through each codec, both
generations of the system ini, the sizes, the next run's words,
`Settings.json`, the presets and a new profile's defaults; `test_keys` the
key names, bindings and their reset; `test_engine_sections` the keys
written; `test_advice` the advice. The stand-in
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
Switch Pro's ids (`test_ui_input`), and the controls and the on-screen
keyboard (`test_ui_controls`). `dxl-shots` (`tools/shots.c`) draws every
page and dialog, each settings page too, from states it makes up in a
scratch home, at
1280x720, 1280x800, 1920x1080, 2560x1440 at 1.5, 3840x2160 at 2 and
960x600, with the keyboard, the mouse and four virtual pads, and fails on
any diagnostic of the kit's: a label cut short, focusables on one another
or off the window (`test_ui_shots_*`); with `--out=<dir>` it writes them as
PNGs for review. On the desktop's display, `DXL_UI_SCRIPT=<file>` gives the
window a script of keys, text, controls found by their labels, page checks
and shots, through its own event queue (`src/screens/uiscript.h`): a first
run from a fresh home to the game and its clean end, a crash into Recovery,
and a second start handed over are run that way. `test_ui_scripts` runs
`tests/ui/`'s, on a profile made from `gamefiles/` in a scratch home and
with the stub for the engine, and reads what they changed:
`settings_roundtrip` changes a row of each kind on each page and finds each
in its file as its reader takes it, and the next run's words on that run;
`recovery` goes from a crash to Recovery's log, the next run in a window
and another renderer, and finds those on the next run alone. It is skipped
with no display or no install.

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
| The renderer page, the first run's and `-changevideo`'s: the drivers installed | Settings' Graphics page: the engine's renderers that run here, and its other drawing settings; `-changevideo` opens it, and its Done plays | The engine's renderers are its own; the probe says which run on this machine |
| Safe mode's eight flags, for a relaunch | Four of them, `-nosound`, `-nohard -noddraw`, `-defaultres` and `-nojoy`, for the run Play starts next | The others name what the engine does not use: MMX, KNI, 3DNow!, A3D and EAX |
| The save migration below FirstRun 220; the Detail page | Not carried | The engine reads none of the Detail page's keys; the saves are 1112fm's |

## Known defects

- **Not the player's theme**: the window's colours are the default menu
  theme's, whatever the profile's `MenuThemeName`.
- **Keys the engine's window does not pass on.** Under SDL2, the window the
  launcher starts the engine with, the engine has the shifts and controls
  only as `LShift`, `RShift`, `LControl` and `RControl`, and no Alt, Caps
  Lock, punctuation (`; = , - . / [ \ ] '`) or keypad operator: the
  defaults' Shift (walk), Alt (strafe), Caps Lock (walk or run), Comma and
  Period (strafe), the brackets (scope, laser), Semicolon (reload), the
  quote (ammunition) and the keypad's quick save, quick load and screenshot
  reach nothing. The keyboard page marks such a binding.
- **The finder looks in a few places**: Steam's own library, not the
  others `libraryfolders.vdf` lists; `~/Games/Heroic` and `~/GOG Games`;
  not Lutris, Bottles or Wine prefixes. The folder browser, the system's
  dialog and `--game=<dir>` find the rest.

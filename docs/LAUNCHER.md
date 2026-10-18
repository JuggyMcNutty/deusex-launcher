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
SDL3_ttf: Home, the first run, Settings, Profiles, Saves, Screenshots,
Multiplayer, Recovery, the log, About ([the window](#the-window)).

## Where everything lives

Nothing is written to the game's folder. The launcher's files are in the
home's XDG folders, each with a `dxlauncher` folder in it
(`src/core/xdg.h`); `DXL_HOME=<dir>` puts all four under one folder, a
scratch home:

| Folder | What is in it |
|---|---|
| `$XDG_CONFIG_HOME/dxlauncher/` (`~/.config`) | `launcher.ini`: the active profile (`[Launcher] Profile`), the engine chosen (`Engine`) and the launcher's own settings (`UIScale`, `BigScreen`, `ReducedMotion`, `ReturnAfterPlaying`, `RenderDriver`, `MusicVolume`, `SoundVolume`: [Settings](#settings)), whether the big screen has been offered (`BigScreenOffered`), each install met (`[Install.<id>]` `Path`, `Version`, `SHA1`), the window as it last closed out of the big screen (`[Window]` `Width`, `Height`, `Maximized`) |
| `$XDG_CONFIG_HOME/dxlauncher/profiles/<id>/` | `profile.ini` (`[Profile]` `Name`, `Install`, `EngineArgs`, `Favourites`: [Multiplayer](#multiplayer)); `DeusEx.ini` and `User.ini`, the engine's `--ini` and `--userini`; the `engine` folder, its `--userdir`: `Settings.json` (its renderer and how it draws), a class's own config files, the engine's log and crash reports |
| `$XDG_DATA_HOME/dxlauncher/` (`~/.local/share`) | `gamecontrollerdb.txt`: the controllers the launcher has mapped, in SDL's words ([the controller](#the-controller)) |
| `$XDG_DATA_HOME/dxlauncher/profiles/<id>/` | `Save`, the inis' `SavePath`; `Screenshots`, the engine's shots (the `engine` folder's `Screenshots` is a link to it) |
| `$XDG_STATE_HOME/dxlauncher/` (`~/.local/state`) | `launcher.log`, each start's; per profile `Running.ini`, `LastRun.ini`, and in `logs` the engine's output, `last-run.log` and `previous-run.log` |
| `$XDG_CACHE_HOME/dxlauncher/profiles/<id>/Cache/` (`~/.cache`) | the inis' `CachePath`: a net game's downloads |
| `$XDG_DATA_HOME/Trash/` | the desktop's trash, freedesktop.org's: a deleted profile's four folders, a save, a screenshot, to be put back from the file manager ([profiles](#profiles), [saves and screenshots](#saves-and-screenshots)) |

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
(`engine-settings.json.default`). That engine is the one played with
unless another is chosen, on Settings' Launcher page or Home's The
engine..., `launcher.ini`'s `[Launcher] Engine`; `DXL_ENGINE=<path>` names
one over both.

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
made, before anything is, and how it begins: its name, its graphics, the
engine's defaults or one of [Graphics'](#settings) presets, and with a
controller connected its [layout](#the-controller), Modern over the game's
own three buttons or none, else the controls as imported, a layout of the
player's (`src/app/session.c`). The game lists what the finder finds
(`src/core/finder.c`), in a worker of the window's, each folder once and
checked, as it is found: the folder `--game=<dir>` names, the installs
`launcher.ini` lists; the libraries Steam's `libraryfolders.vdf` lists,
the game where its `appmanifest_6910.acf` puts it, as Steam's distro
package, its Flatpak and its Snap keep them; Heroic's GOG games, the
`installed.json` of its `gog_store`, and each folder in `~/Games/Heroic`;
each folder in `~/GOG Games` and `~/Games`; what each of Lutris' games runs,
and its Wine prefix; Bottles' bottles and the other Wine prefixes,
`$WINEPREFIX`, `~/.wine`, winetricks', those in `~/Games` and Heroic's;
the drives `/proc/self/mountinfo` has mounted in `/run/media`, `/media`
and `/mnt`, however deep, each once, at its top and two folders down. In
a prefix's `drive_c`, and on a drive, it looks where
Windows' installers put the game: `DeusEx`, the CD's folder; each folder
in `GOG Games`, GOG Galaxy's `Games` and `Games`; Steam's library.
Another is shown by the launcher's folder browser, which a pad works, or
by the system's folder dialog. `dxl-cli` makes the profile from
`--game=<dir>`'s install. Either way the profile is made from the
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
from then on, as Settings' Launcher page's The game's folder does, and
Home's The game's folder... when the install is not there to play: The
game, the one chosen its install from then on, and back to the page it came
from. A profile whose install is not there to play any more (moved, or
deleted) opens on The game.

## Profiles

Each profile is one way to play: its own settings, keys, controller,
saves and screenshots, and its install. Profiles, from Home
(`src/screens/profiles.c`), lists each, its game, its saves and its last
run, the one in use marked; its buttons act on the one chosen, the row
last focused:

- **Use this one**, or the row chosen: the profile played from now on,
  `launcher.ini`'s `Profile` (`dxl_run_use_profile`, `src/app/run.c`: the
  run's inis, `Running.ini` and last run its own).
- **New profile...**: The game, then Your profile, as the first run makes
  one, its folders named after its name (`Deus Ex 2`, `deus-ex-2`); Back
  out of them leaves the one in use as it was. Made, it is the one in use.
- **Duplicate**: a new profile with the chosen one's settings, keys and
  controller -- `profile.ini`, the inis with its own `SavePath` and
  `CachePath`, the engine folder's `Settings.json` and a class's config
  files -- and none of its saves, shots or logs (`dxl_profile_copy`).
- **Name**, under the list: the chosen one's, edited in place.
- **Delete...**: the chosen one's four folders to the desktop's trash, all
  or none (`src/platform/posix/trash.c`); never the one in use.

## Saves and screenshots

A profile's saves are its `Save` folder's, the engine's `SavePath`: each
`SaveNNNN` under the engine's own name for slot NNNN, and `QuickSave`, the
quick save, slot -1 (`src/core/saves.c`). Each is read from its
`SaveInfo.dxs`, the game's `DeusExSaveInfo` (`src/core/upkg_assets.c`):
what it says, where and when it was made, how long the game had been
played, its snapshot; the newest first, one whose info does not read last.

- **Continue**, Home's first button where there is a save: the game started
  on the newest, which Home names, its snapshot under the profile's panel.
  Play is the game's own start.
- **Saves**, from Home: each save, the one chosen beside the list with its
  snapshot (the game's 160 x 120 of its picture), when it was made, how long
  it had been played, its map and folder. Load, or Enter or a click on its
  row, starts the game on it; Delete... puts its folder in the desktop's
  trash.
- **Screenshots**, from Home: the profile's `Screenshots` folder's pictures,
  the engine's `ShotNNNN.bmp` and those the import copied, BMPs and PNGs,
  newest first (`src/core/shotlist.c`), as a gallery of thumbnails read off
  the window's thread (`src/ui/thumbs.c`), the one chosen beside it with
  when it was taken and its size. Show, or Enter or a click on one, shows
  it whole, Previous and Next stepping through them; Open the folder opens
  it in the file manager; Delete... puts it in the trash.

The game starts on a save by the engine's start URL, `?loadgame=N` in
`--url`, which only an engine whose `--version` names `loadgame-url` takes
([the start URL](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/NATIVES.md#the-command-line)):
with an older one there is no Continue, and Load is off, saying why. A run
started on a save that the engine stopped with an error before its first
map opens Recovery saying the save did not load, its fix Saves...; a
crash, or an error Recovery knows, has its own advice.

## Multiplayer

Multiplayer, from Home (`src/screens/servers.c`), lists the game's
servers as the game's own Join Internet screen asks for them, in a worker
of the window's (`src/app/browser.c`): the profile's master server,
`DeusEx.ini`'s `[DeusEx.MenuScreenJoinGame] MasterServerAddress`
(333networks' since the first run), over TCP on 28900, its challenge
answered as `IpDrv.dll`'s `Validate` answers it, GameSpy's RC4 keyed by six
spaces for Deus Ex, and its list asked for
([the master server](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/ipdrv-dll.md#the-master-server));
then each server it lists, and each of the profile's favourites
(`profile.ini`'s `Favourites`, asked whether the master lists them or
not), for its `\status\`, all at once over UDP, one silent after a second
asked again, and given up three seconds after that (`src/core/gamespy.c`).
Each shows as it answers -- its name, map, game, players and ping, the
time to its first packet -- and, once all have, by the sort chosen: ping,
players or name; the filter shows them all, those with players, those not
full, those with no password, or the favourites. Beside the one chosen:
its address, map, game, players and each player's frags and ping, a version
not the game's 1100 said; Join, with the password typed in its field when
it asks for one; a favourite or not. An address the list does not have,
typed, `host` or `host:port` (7790 when none), is joined as it is. A server
joined is the game's start, its `--url` the server's `host:port`,
`?password=` after it when it asks for one; the last run keeps where it
went, not the password.

A `deusex://host:port` link, of which the app menu's entry is the handler,
opens Multiplayer asking whether to join its server; with `--play`, with no
window, or after another screen's Play, it is joined. A second start with
one hands it on: the window asks, or the game, while it plays, opens the
server.

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
5. **The engine** asked what it can do (`--version`), and started: or
   refused, saying why -- not there, one this system cannot start (a glibc
   newer than its own, a library it lacks: the loader's words, which the
   launcher keeps), no VibeEngine, or one without `userdir`.
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
| Home | Continue, Play, Multiplayer, Saves, Screenshots, Settings, Profiles, About, Quit; the profile's game, its build, the engine, its saves, its latest save and its snapshot, its last run; when the game or the engine is not there to play, The game's folder... or The engine... first, and why under it |
| Welcome, The game, Your profile | [the first run](#the-first-run) |
| Settings | [the settings](#settings), a tab a page |
| Profiles | [the profiles](#profiles): use one, a new one, a copy, a name, delete one |
| Saves | [the saves](#saves-and-screenshots): one loaded, one deleted |
| Screenshots | [the shots](#saves-and-screenshots), a gallery: one shown whole, their folder opened, one deleted |
| Multiplayer | [the servers](#multiplayer): the master's and the favourites, sorted and filtered; one joined, with its password; a favourite or not; an address joined |
| Recovery | how the last run ended and when, the engine's error, what to do about it, the end of its log, the next run's words; Play, the fix, the log, the defaults, Quit ([Recovery](#recovery)) |
| The log | the last run's output, the run's before it, or the engine's own log, scrolled by the keys, the wheel or a pad; the file opened |
| About | the launcher's version, the engine's and its features, the four folders, the libraries and fonts; the app menu's entry, the settings folder opened |

Graphics (`-changevideo`, and an engine's `RELAUNCH` with it) opens
Settings on its Graphics tab, whose Done is Play.

It is our own drawing on SDL3's renderer (`src/ui/`), wearing the game's:
steel panels with cut corners and lit edges, title bars in the theme's
colour, and round what has the focus the four corner brackets the game puts
round what can be used. Its colours are the player's menu theme's, as the
game takes it -- `User.ini`'s `[DeusEx.DeusExPlayer] MenuThemeName`, else
Grey -- the install's colours of it, from `DeusEx.u`'s `ColorThemeMenu_*`
(`src/core/theme_colors.c`), and the default theme's, the kit's own, until
there is an install (`src/ui/theme.c`). Home wears the game's art, read
from the install at run time and never kept: its logo's "Deus Ex"
(`Help/Logo.bmp`, without the makers' logos) and its main menu's buttons
(`DeusExUI.u`'s `MenuMainButton*` and `MenuMainLight_*`), tinted by the
theme and added to what is under them, as the game draws them
(`src/ui/art.c`); ours stand in for what the install lacks. The game's
packages are read by our own code from their format (`src/core/upkg.c`).
Its type is in `data/fonts` (`THIRD-PARTY.md` says whose). It is laid out in dp,
which the display's scale makes pixels, so it is as sharp at any size: for
1280 x 720 dp, and no smaller than 960 x 600; the window opens at
1280 x 720, or as it last closed.

**The big screen**, for a television or a handheld
(`src/core/bigscreen.h`): the window full screen, its layout 1280 x 720 dp
an eighth larger -- the screen 640 dp tall, and no narrower than 960 dp --
and the hints a pad's when one is there as it opens. Settings' Launcher
page has it Always, Never, or Automatic, the default: the big screen when
`--big-screen` asks for it, as the app menu's Big screen does, under
Steam's Big Picture (`SteamTenfoot` or `SteamGamepadUI` set to 1), on a
Steam Deck (`SteamDeck=1`) or in gamescope (`GAMESCOPE_WAYLAND_DISPLAY`).
F11 switches it while the window is up, and a second start's
`--big-screen` -- the app menu's Big screen with the launcher up -- puts
it on; the desktop taking the window out of full screen ends it. In a
window, Home and Welcome offer it once, as a pad is first pressed there,
the press the question's; switched to, it is Always. Its size is the
screen's, not kept as the window's.

**Controls.** The keyboard: the arrows or WASD move, Enter or Space
chooses, Esc or Backspace goes back, Tab and Shift-Tab step through, Page
Up and Page Down scroll a list, Ctrl-Tab and Ctrl-Shift-Tab change tabs,
F5 plays, F11 switches the big screen. The mouse: the pointer focuses
what it moves onto, a click chooses, the wheel scrolls a list and a list's
scroll bar drags by its thumb or pages by its track, the back button goes
back. A pad: the d-pad or the left stick moves (held, again 350 ms after
the frame that took it and then every 90), its confirm button chooses and
its back button goes back -- south and
east, or on a Nintendo pad, whose A is east, east and south -- its
shoulders change tabs, and Start plays. The foot of each page shows what to
press on the device last used, keycaps or the pad's own buttons as SDL
names them, drawn as shapes of our own, no maker's logo; a hint clicked
does what its key does. The pointer hides while a pad is used.

**Text** (a text row, a profile's name) is edited in place as a desktop's
fields are (`src/ui/controls.c`): a click puts the caret where it is, a
drag or a Shift-click selects, a double click takes a word and a triple
all; Shift with the arrows, Home and End selects, Ctrl moves and erases by
words, Ctrl+A, C, X and V work the clipboard. A field with the focus takes
typing at once, what is typed in its place -- W, A, S and D too -- and
Backspace begins its edit; Enter begins it with the caret at the end. Enter
keeps the edit, Esc puts it back, Tab, Up and Down keep it and move on, a
click elsewhere keeps it. While it is edited the pointer takes no focus and
nothing scrolls. A pad's confirm opens the on-screen keyboard
(`src/ui/osk.c`).

**Sound.** The game's title music, `Music/Title_Music.umx`'s module,
played from its first order and looped by libxmp, fading as the game
starts, and the game's six menu sounds (`DeusExSounds.u`'s `Menu_*`) for
the focus moving, a choice, play, back, a value stepped and a dialog
(`src/ui/audio.c`), at the launcher's own volumes: Settings' Audio page's
The launcher's music and The launcher's sounds (`launcher.ini`'s
`MusicVolume`, 0.5, and `SoundVolume`, 0.7). With no sound output, none.

**Text that types itself in**, as the game's menus' does: a page's title,
the help lines, a dialog's words, Recovery's advice, at 220 characters a
second. Enter, Space, Esc, the play key, Start or a click shows it whole,
as each does what it does; reduced motion turns it off.

Idle, it costs next to nothing: it waits on its events and draws only
while something moves or a picture is being read, VSync-paced; the title
music, while it plays, is mixed on SDL's audio thread. A second start while it is up hands it its
command line and raises it; a pad connected or lost is a note in its
corner.

## Settings

Settings' tabs are its pages: Display, Graphics, Audio, Keyboard & mouse,
Controller ([the controller](#the-controller)), Gameplay & HUD,
Multiplayer, Launcher; a page's advanced rows come after its others. Each row is the registry's (`src/core/settings_table.c`): what
it is, its help, and the places it is kept, each with the codec its reader
takes (`src/core/settings.c`):

| Kept in | What |
|---|---|
| `DeusEx.ini` | the engine's sections, `[Engine.SurrealClient]`, `SurrealAudioDevice`, `SurrealRenderDevice`: full screen, the window's size, brightness, decals, moving lights, flashes, detail textures, fog, the volumes and the mixer; `[DeusEx.MenuScreenJoinGame] MasterServerAddress` |
| `User.ini` | `[Engine.PlayerPawn]`: the field of view, the mouse; `[DeusEx.DeusExPlayer]`: the gameplay and HUD rows and the colour themes; `[DefaultPlayer] Name`; `[Engine.Input]`: the keys, the controller's buttons |
| the engine folder's `Settings.json` | the renderer, anti-aliasing, lighting, the brightness curve, vertical sync, the render scale, bloom, HDR; the controller's dead zone and speeds |
| `launcher.ini`, `profile.ini` | the launcher's own: the engine, its size, the big screen, reduced motion, Home after playing, its renderer; the profile's game, the engine's extra words |

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
defaults file; on Keyboard & mouse the keys too, on Controller its
buttons. Recovery's The defaults...
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
instead, or both (`a|b`), the key then each one's. Keys are named as the
engine's window gives them, as the original does (`src/core/keys.c`): a
shift, control or alt as `Shift`, `Ctrl` or `Alt`, the name of both sides,
which the engine follows with its side's own (`LShift`); a binding on a name
that window never gives is marked. The game's own
key menu refuses Escape, F1 to F18, the digits, Tilde, PrintScrn and Pause,
whose bindings it does not show; this page shows them, and takes all but
Escape.

**Launcher** begins with the engine and the game's folder. The engine:
its version, or what is wrong with it, and as its choices the launcher's
own or another, in a folder shown by the launcher's browser or the
system's dialog, VibeEngine or SurrealEngine in it, checked before it is
taken (one that finds no `SurrealEngine.pk3` where the engine looks is said
to). The game's folder: the profile's install, another chosen on The game.

**A text row** is edited as [the window's text](#the-window) is. Your name
keeps what the game takes in a name, letters, digits and `[ ] ( )`, and is
`Player` when nothing of it is left.

**The next run's words**, `-nosound`, `-nohard -noddraw` (a window),
`-defaultres` (640 x 480) and `-nojoy`, are Launcher's advanced rows and
Recovery's: added to the original's command line of the run Play starts
next, and of no other.

The changes are in the open files at once, and written as Settings is
left, by Done or otherwise. The launcher's size, the big screen and
reduced motion apply at once, its renderer from the window's next opening.

## The controller

The engine takes the first controller SDL has a mapping for, and reads its
buttons by where they are, whoever made it: `User.ini`'s `Joy1` is the
bottom face button, `Joy2` the right, `Joy3` the left, `Joy4` the top,
`Joy5` and `Joy6` the shoulders, `Joy7` and `Joy8` the middle two, `Joy9`
and `Joy10` the stick clicks, `Joy11` and `Joy12` the triggers,
`JoyPovUp` to `JoyPovRight` the d-pad; in the game's menus they have uses of
their own
([the gamepad](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#running-on-our-devices)).

Settings' Controller page (`src/screens/controller.c`) has on its left the
controller shown: a model of its family, of our own shapes, no maker's --
an Xbox pad's, a PlayStation pad's, a Nintendo pad's, any other's
(`src/ui/padmesh.c`) -- drawn in 3D in the theme's colour and live, a
button held sinking and lit, a stick tilting, a trigger pulled
(`src/ui/pad3d.c`); dragged, or turned by the right stick, it turns. A line
from each control names what it does, the chosen row's lit; where a side's
do not fit, the chosen one's alone. Under it, each stick and trigger as the
engine takes it, through the dead zone, the curve and the look speeds
(`src/core/padcurve.c`), so a change of those is seen before it is played.
With several controllers, the one shown is chosen above the model, and the
line under it says how it is joined, its battery, whose mapping SDL has for
it -- the launcher's, Steam's, SDL's own -- and whether the game takes it.

On its right, the rows: Layout, a preset -- Modern, Modern with the sticks
swapped, Original Deus Ex, the game's own -- or Custom once a button is
changed by itself (`src/core/bindings.c`); Test the controller...; Find a
button, the next button pressed or trigger pulled then its row, as a click
on the model's is; a
row for each of the sixteen buttons, named as the controller's maker names
it (the bottom face button A on an Xbox pad, Cross on a PlayStation pad, B
on a Nintendo pad), whose choice is Keyboard & mouse's actions; the dead
zone, the look speeds, inverted looking, the pointer's speed in the game's
menus and, advanced, whether the game reads the controller at all,
`Settings.json`'s `Gamepad`; Map this controller....

**Test the controller...** shows the controller's model larger, and while
it is up every pad's input drives the model alone, none of the window: each
control pressed lit and its line's label with it, the sticks no longer
turning the view, the triggers rumbling the controller as far as they are
pulled, each its own motor. Its back button held a second ends it, as do
Esc, Stop testing and the controller going.

**Map this controller...** learns a controller SDL has no mapping for, or
one it maps wrongly: each of SDL's controls asked for in turn -- the face
buttons, the shoulders, the triggers, the middle buttons and the maker's,
the stick clicks, the d-pad, each stick's two ways -- and pressed or moved,
the model showing which; Skip for one the controller lacks, Back for a step
again (`src/screens/padwizard.c`). It begins once the controller is let go
of, at rest as SDL first read it, or as it has then been still a second;
while it is up, the controller moves nothing of the window. Its mapping,
SDL's text for it (`src/core/padmap.c`), is given to SDL at once and kept
in `gamecontrollerdb.txt`, a line a controller, by its GUID, which the
window loads as it opens its pads; with nothing mapped, nothing is kept.
The value of `SDL_GAMECONTROLLERCONFIG` the launcher was started with, as
Steam sets it, is SDL's but for its lines of a controller the launcher
maps, which SDL would take over the launcher's. The engine is given that,
then the file (`src/app/run.c`): each mapping, and a copy of it for an
SDL2 before 2.26, whose GUIDs have no CRC of the pad's name: its GUID
without the CRC, a `crc:` field naming it, which a newer SDL reads, so that
the copy maps the pad of that name alone.

## The app menu

`deusex-launcher --install-desktop`, or About's button, puts the launcher on
the desktop's app menu: `$XDG_DATA_HOME/applications/io.github.juggymcnutty.DXLauncher.desktop`,
from `data/`'s template with the launcher's own path, and its icon, our own
drawing, at 32 to 256 px under `$XDG_DATA_HOME/icons/hicolor`
(`src/platform/posix/desktop.c`, `src/ui/icon.c`). The window has the same
id, so the desktop puts the two together; its Big screen action starts it
with `--big-screen`, and its MimeType makes it the handler of `deusex://`
links. `--remove-desktop`, or the button again, takes them away; nothing
else writes them.

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
environment names a backend (only SDL2's has the fork's pad support),
`SDL_GAMECONTROLLERCONFIG` with the controllers the launcher has mapped
([the controller](#the-controller)), and `DXL_LAUNCHER_FD`, the launcher's
end of a socket pair. On that line the
engine says `hello` as it starts, `ready` once its first map is in, and
`relaunch <line>` for the original's `RELAUNCH`; the launcher hands it
`TakeFocus` and `Open <url>` for a line a second start forwards, once the
engine is ready (`src/app/play.c`). A start URL goes in `--url`, a save's
`?loadgame=N` for Continue and Load ([saves](#saves-and-screenshots)): in
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
whether it was ever ready, and the save it started on. The window opens on Recovery after a run that
was not clean, and at the next start that finds `Running.ini`; with no
window (`dxl-cli`), that start says so with the end of the run's log, and
exits 3.

The page says what to do about the run's end (`src/core/advice.c`). For a
crash, and for the errors it knows -- the Vulkan renderer not made, OpenAL
not started, a package or the engine's `.pk3` not found, an engine that
ended before its first map, a save it started on that did not load -- it
says the likely cause, and has its fix as a button: another renderer, of
those that run here; a run without sound; the game's folder; Saves. Below
are the end of the log where it fits, the [next
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
| `--find` | the installs the first run's finder finds here, but `launcher.ini`'s: each folder, where it was found, its build or what is wrong with it |
| `--servers[=<master>]` | the servers the master lists (333networks' when none is named), each asked as Multiplayer asks it: its name, map, game, players, ping and address |

## How it is tested

`scripts/dx.sh test` runs the host tests side by side, all but
`test_ui_scripts` with no display.
`test_settings` takes the registry's rows through each codec, both
generations of the system ini, the sizes, the next run's words,
`Settings.json`, the presets and a new profile's defaults; `test_keys` the
key names, bindings and their reset; `test_bindings` the controller's
layouts, labels and actions; `test_padcurve` its sticks and triggers as the
engine takes them; `test_padmap` SDL's mappings, `gamecontrollerdb.txt`
and what the engine is given; `test_engine_sections` the keys written;
`test_advice` the advice; `test_upkg` the game's packages, on packages it
writes itself and the same broken a thousand ways; `test_saves` a profile's
saves and shots, the saves' infos written as the game writes them
(`tests/upkg_writer.h`); `test_vdf` Steam's KeyValues files, and
`test_finder` each of the finder's places, in homes it makes up;
`test_gamespy` the master's and the servers' words, the challenge's answer
against the original's own; `test_servers` a server's answer, the sort,
the filters and the favourites; `test_browser` a look at a master and
servers of its own on this machine, one of them silent. The stand-in
engine `tests/stub_engine.c` answers `--version` -- or fails it as a loader
that cannot start it does -- and talks on the line as
the engine does, or crashes, fails, relaunches, prints its words or never
ends (`test_process`, which has the engine's refusals and its choice too,
`test_play`). `test_profile` imports from a fake
install made read-only, which it leaves as it was, and makes, lists and
copies profiles; `test_trash` puts folders in a scratch trash;
`test_gamefiles`, on a
real install in `gamefiles/`, reads every asset the window takes from it --
the menu themes, the art, the menu sounds, the title music, and the
infos and snapshots of any saves it has -- plays the stub from a scratch home through a first
run, a crash and the Recovery that follows, and checks that nothing in the
game's folder changed.

The window's kit is tested on SDL's software renderer and a surface:
focus and navigation (`test_ui_focus`), text (`test_ui_text`), and the
keyboard, the mouse and virtual pads with an Xbox's, a DualSense's and a
Switch Pro's ids (`test_ui_input`), the controls and the on-screen
keyboard (`test_ui_controls`), the sound on SDL's dummy output
(`test_ui_audio`) and the thumbnails (`test_ui_thumbs`). `dxl-shots`
(`tools/shots.c`) draws every
page and dialog, each settings page too, from states it makes up in a
scratch home, at
1280x720, 1280x800, 1920x1080, 2560x1440 at 1.5, 3840x2160 at 2 and
960x600, and on the big screen at 1920x1080 and 1280x800, with the keyboard, the mouse and four virtual pads, and fails on
any diagnostic of the kit's: a label cut short, focusables on one another
or off the window (`test_ui_shots_*`); with `--out=<dir>` it writes them as
PNGs for review. On the desktop's display, `DXL_UI_SCRIPT=<file>` gives the
window a script of keys, text, controls found by their labels, virtual
controllers, page checks and shots, through its own event queue
(`src/screens/uiscript.h`): a first
run from a fresh home to the game and its clean end, a crash into Recovery,
and a second start handed over are run that way. `test_ui_scripts` runs
`tests/ui/`'s, on a profile made from `gamefiles/` in a scratch home and
with the stub for the engine, and reads what they changed:
`settings_roundtrip` changes a row of each kind on each page and finds each
in its file as its reader takes it, and the next run's words on that run;
`recovery` goes from a crash to Recovery's log, the next run in a window
and another renderer, and finds those on the next run alone; `remap_pad`
binds three buttons of a virtual Xbox pad, two found by pressing or pulling
them, maps that pad with the wizard its own confirm opened, and a
controller SDL has no mapping for, its trigger resting at the top and first
read before it was, and finds the bindings in `User.ini`, the mappings in
`gamecontrollerdb.txt` and in the engine's environment; `pad_test` tries
a virtual pad, whose buttons move nothing of the window until its back is
held; `engine_and_game` starts with an engine this system cannot start,
puts another in its place from Home, and the game's folder from Settings;
`profiles` makes a profile, duplicates one, renames and deletes the copy to
the trash, and uses the first again; `continue` starts the game on the
latest save from Home, `saves_load` on a save from Saves, `saves_delete`
deletes one, and `shots` shows a shot whole and deletes it, each finding
the save's URL on the engine's command line or what was deleted in the
trash; `big_screen` is offered the big screen on a pad's first press, an
A that does not play, and stays as it is, switches it by F11, then has it
Always on Settings' Launcher page, and finds the two in `launcher.ini`;
`big_screen_switch` is offered it and switches to it, and
`big_screen_handover` is put on it by a second start; `multiplayer` joins
the server a master on this machine lists, and `multiplayer_address` an
address typed, each finding it in the engine's `--url`; `first_run` and
`first_run_defaults` make a profile from nothing, a controller connected,
with Your profile's choices made and as they come. SDL takes in each
only controllers of the virtual ones' makes and models
(`SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT`): a pad of another plugged in
moves nothing of a run, though a real one of those models would. It is
skipped with no display or no install.

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
| No server browser: the game's menus have one | Multiplayer: the master's servers and the favourites, one joined as the game starts; `deusex://` links | A server chosen before the game, with a pad or from a link |

## Known defects

- **Multiplayer lists no servers on the local network**: the game's own
  LAN screen asks the servers' beacons (`REPORTQUERY`, port 8777); the
  launcher does not. An address typed joins one.
- **The finder reads no database and no Windows path**: a Lutris game
  whose program is named from its folder, which only Lutris' `pga.db`
  holds, and a library a Windows Steam in a Wine prefix lists, are found
  by the folder browser, the system's dialog or `--game=<dir>`.

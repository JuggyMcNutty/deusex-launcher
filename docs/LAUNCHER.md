# The launcher

`deusex-launcher` is what a player starts in place of the game's
`System/DeusEx.exe`: a tabbed home screen, for a pad or the keyboard, that
writes the configuration the engine reads and hands over to it. `dxl-cli`
prints, with no display and writing nothing, what it would decide and write
(`--dry-run`) and the GPU it finds (`--probe`). It is C11 and SDL2, the same
on every port's branch; what differs per device is in
[`PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md).
It keeps part of the original, which `main` recreates
([what it changes](#what-it-changes-from-the-original)).

## Where the settings actually live

Surreal Engine does not read the keys the original launcher writes. It reads:

| File | What the engine takes from it | Who writes it |
| --- | --- | --- |
| `<AppDir>/home/.config/SurrealEngine/Settings.json` | The renderer (`RenderDevice.Type`), VSync, anti-aliasing, lighting and gamma mode, bloom, HDR; the fork's `Gamepad` and `Performance` blocks | The launcher (`core/engine_settings.c`), before a launch and on Quit, when it is missing, incomplete or changed. The engine saves it only from its own launcher window (`LauncherWindow.cpp`), which `--no-launcher` skips |
| `System/DeusEx.ini` | Everything else (`[Core.System]` etc.); client settings (`[WinDrv.WindowsClient]` `Brightness`, `Decals`, viewport) **only while there is no** `SE-DeusEx.ini` | The launcher, from `Default.ini` when missing; the game's options |
| `System/SE-DeusEx.ini` | Written by the engine on its **first clean exit** (`PackageManager::SaveAllIniFiles`), then the **only** one it reads; client settings under `[Engine.SurrealClient]` | The engine; the launcher's client settings when it exists |
| `System/User.ini`, then `System/SE-User.ini` | Key and pad bindings (`[Engine.Input]`), the same way round | The launcher's pad layouts; the game's key menu |

- **The renderer goes to `Settings.json`**: the engine overrides
  `[Engine.Engine] GameRenderDevice` (`Engine.cpp`).
- **A stub `DeusEx.ini` is fatal** (`Could not find package Core`): the
  engine falls back to `Default.ini` only when the file is absent.
  `core/config.c` creates it from `Default.ini` when missing, rebuilds one
  with no `[Core.System] Paths` (keeping its values), and creates `User.ini`
  from `DefUser.ini`.
- **`Settings.json` is parsed inside a catch-all**: one malformed byte
  reverts every setting to the engine's defaults, 4x MSAA included (speckle
  on a PowerVR). A missing `RenderDevice` flag or number reads as false or 0
  (`HdrScale` 0); its choices and the fork's blocks fall back to the
  engine's defaults. So the launcher always writes every member of every
  block it knows (`RenderDevice`, `Gamepad`, `Performance`) and replaces a
  file it cannot parse. A member the file lacks takes the port's
  `engine-settings.json.default`, so a new setting reaches an existing
  install with the port's value.
- **Texture and skin detail, sound quality and `MinDesiredFrameRate` are
  read by neither the engine's renderer nor its mixer**, so the original
  Detail page's choices are left out.

## The screens

Four tabs, switched with L1/R1; START launches from any. Without the game's
files one screen takes their place (`core/install.c`): the game folder, and
whether `System`, its `DeusEx.u`, `Engine.u` and `Core.u`,
`Textures/Palettes.utx` and `Maps` are there, with Exit and Check again,
which opens the home screen when the files are in.

| Tab | Contents |
| --- | --- |
| Play | Play / Quit; what will happen (renderer and GPU, controller and layout, game folder); a crash banner quoting the engine's last error when `Running.ini` survived; notes when the launcher repaired `DeusEx.ini` or changed the pad layout |
| Video | Renderer (the `renderers.ini` list, with why each can or cannot run), **CPU mode** (where the device has modes), Resolution (`Performance.RenderScale`, down to 480 lines; Vulkan only, a [known defect](#known-defects)), Distant AI (`Performance.AiLevelOfDetail`), VSync, Brightness, Lighting, Gamma curve, Bloom and its strength, Anti-aliasing (locked off on a PowerVR GPU, saying why), Decals |
| Controls | Controller detected, in-game pad support on/off, layout preset, **Customize buttons**, look speed X/Y, invert, dead zone, menu pointer speed |
| System | Last run (from `run-game.log`), the engine log, clear crash marker, reset video / controls / game configuration (each confirmed first), game files, version |

A row's help line says what it changes in the engine, not its name again; a
row whose choices differ in kind (CPU mode, Distant AI, Lighting, Gamma
curve) describes the current one. Rows are data (`ui/screens_internal.h`
`dxl_row`), drawn by `ui/screens.c` with the help pane and button hints.

**Renderers** (`core/renderers.c`, `platform/posix/gpu_probe.c`). One can be
chosen when the engine build has a backend for it (`renderers.ini`
`EngineType`, the `Settings.json` value; empty means none) and the device
has the API it needs (the GPU probe); the picker shows both. The probe runs
in a forked child with a timeout, before the display comes up, so a driver
that crashes or hangs costs the child, not the launcher. Each port has its
own `renderers.ini` (the base's is `ports/common/packaging/renderers.ini`).
A launch swaps a renderer that cannot run here for one that can, and logs
it.

**CPU mode** (`launcher.ini` `CpuMode`). Offered only where the device
profile lists modes
([the Smart Pro's](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md#cpu-mode));
the port's `port-hooks.sh` applies it just before the engine starts
(`port_before_game`) and restores the previous state after. Elsewhere the
row is hidden and `CpuMode` is never written.

**Launch flow** (`app.c`, shared by `main.c` and `cli_main.c`). The home
screen opens every time, on the tab the entry decision picks
(`core/policy.c`, the original's `FirstRun` gates and flags): Video on a
first run and `-changevideo`, System on `-safe`, Play with the cursor on
Troubleshoot when a crash sentinel survived. `DXL_NO_HOME=1` skips it for
unattended runs over SSH, unless the decision has a question. Quit saves
settings and creates no sentinel.

**The hand-over** (`platform/launch.h`). On POSIX the launcher execs
`launcher.ini` `GameCommand` (default `./run-game.sh`) with the command line
as given. `run-game.sh` (`ports/common/packaging/run-game.sh`, adjusted by a
port's `port-hooks.sh`) starts
[the engine](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md)
and clears `Running.ini` only on a clean exit, so a crash shows on the next
launch.

**Logs**, deliberately verbose for development. The launcher logs what it
decides and does to `System/DeusExLauncher.log`, beside the game's own, and
to stderr. In the app directory, `run-game.sh` appends each run to
`run-game.log`, and the engine writes
`home/.config/SurrealEngine/SE-Log-LastRun.txt`; the System tab shows both.

**The device profile** (`platform/target.h`) is a port's `target.c`
([what it holds](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#what-a-port-is));
without one a port gets `platform/target_default.c`, a generic desktop. GPU
facts are keyed on the probe instead: `dxl_gpu_msaa_broken`
(`core/renderers.h`) locks anti-aliasing on any PowerVR the probe finds
through Vulkan, by the Vulkan device's name, so one seen only through
OpenGL ES is not locked.

## Controller support

**In the launcher**: d-pad/left stick move, A select, B back (on Play:
quit), X reset the focused setting, L1/R1 tabs, START play, SELECT/MENU
quit. Keyboard (`ui/ui.c`): arrows/WASD, Enter/Space/Z, Backspace/X, R,
PageUp/PageDown (or `,`/`.`/Tab), P/F5, Escape/Q.

**In the game**
([engine patch 0003](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#running-on-our-devices)):
the engine's SDL2 backend reads the pad (with SDL3 installed, the desktop
build has no SDL2 backend: [no pad in game](../ports/linux-x86_64/README.md#no-pad-in-game)),
and `GamepadInput` turns it into UE1's joystick keys and axes. Each control
is then an ordinary `User.ini` `[Engine.Input]` binding in the keyboard's
table, which the game's key menu edits
([lists](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/NATIVES.md#lists)).

- Buttons: A=`Joy1` B=`Joy2` X=`Joy3` Y=`Joy4` L1=`Joy5` R1=`Joy6`
  SELECT=`Joy7` START=`Joy8` L2=`Joy11` R2=`Joy12` (past half travel),
  d-pad `JoyPov*`. The stick clicks, `Joy9`/`Joy10`, are in no preset and
  not offered: the layouts are made for a pad without them
  ([the Smart Pro's](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md)).
- Sticks: `JoyX`/`JoyY` left, `JoyU`/`JoyV` right, up positive, ±100 at full
  deflection after a radial dead zone; the look stick has a response curve.
  The engine multiplies axis input by 16 and the binding's `Speed`, so
  `Axis aBaseY Speed=3.75` is keyboard run speed (6000).
- In a Deus Ex modal window (menus, inventory, conversations, keypads):
  either stick moves the pointer, A/X click, B/Y/SELECT/START are Escape,
  the d-pad is the arrow keys, L1/R1 scroll.

**Layouts** (`core/bindings.c`) are data: Modern (default), Modern with
sticks swapped, and the bindings Deus Ex shipped (three buttons). They and
the Customize screen bind only what the game's key menu offers
(`MenuScreenCustomizeKeys`, read out of `DeusEx.u`), plus `ShowMainMenu`,
the belt slots and the F3–F12 augmentation hotkeys. A launch that finds the
shipped bindings and no `Gamepad.Layout` switches to Modern; a revised
preset is recognised and upgraded (`retired[]`), with a note on Play saying
what changed. `Gamepad.Layout` in `Settings.json` records the preset last
applied, which X restores a single button to.

## How it is tested

`scripts/dx.sh test` (`scripts/dx.sh test <port>` for a device branch's host
build) runs the host tests, with no display. The ini round-trip is checked
byte for byte on stand-ins for the shipped files (`tests/fixtures`), and by
`test_gamefiles` on a real install when there is one. Each device profile is
checked against its `port-hooks.sh`: every CPU mode it offers is named in
the hooks (not proven handled: known defects), and the hooks fall back to
its default.

`dxl-shots` renders every tab and overlay headlessly at the profile's panel
size (`DXL_WINDOW`); a host build with `-DDXL_PROFILE=<port>` shows a
device's screens.

## What it changes from the original

The original's behaviour is recorded in
[`launcher.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/launcher.md).
Kept as the original does it: the `FirstRun` gates (220/400/1100) and the
up-only clamp, the `Running.ini` sentinel lifecycle and its create-after-UI
ordering, the three non-equivalent command-line parsers, and the
single-instance handoff's protocol (its gaps are
[known defects](#known-defects)). Changed or dropped:

| # | Original | The launcher | Why |
| --- | --- | --- | --- |
| 1 | Safe mode: eight checkboxes become flags (`-nosound`, `-nohard -noddraw`, `-defaultres`, ...) on a re-exec; three are dead in the shipped binary | Dropped; the System tab (engine log, clear crash marker, resets) replaces it | The engine reads four of them (`-nosound`, `-defaultres`, `-nohard -noddraw`, `-nojoy`) only through `--cmdline=` ([the command line](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/NATIVES.md#the-command-line)), which the ports' `run-game.sh` does not pass ([known defects](#known-defects)); `main` keeps the page |
| 2 | Missing splash bitmap → assert → process dies before the wizard | No splash | Nothing to be missing (`0x109090A4`, [`launch-flow.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/launch-flow.md)) |
| 3 | `MPLAYER` / `HEAT` console commands, one `HKLM\software\mpath` read | Dropped | Dead services; `GotoHEAT.exe` is not shipped ([`launch-flow.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/launch-flow.md)) |
| 4 | `.ICD`→`.EXE` rewrite in `InitPathnames` | Dropped | SafeDisc artifact; the GOG build is not wrapped |
| 5 | `-make` rejected with a fatal error | Dropped | Points at `ucc`, shipped separately |
| 6 | Renderer page: Win32 3D device detection, the program run again to test Direct3D, `GameRenderDevice` written | Crash-isolated GPU probe; the choice in `Settings.json` `RenderDevice.Type` | Surreal Engine overrides `GameRenderDevice`. `-testrendev` is parsed and names `dxl-cli --probe`, its replacement |
| 7 | CD check loops on `<CdPath>Textures\Palettes.utx` with a modal box | Install-validation screen | Missing game files are the real first-run failure here |
| 8 | Driver page (2022) names the detected Direct3D card | The renderer picker's status line (GPU, API version) | The page names a D3D card and points at a driver download |
| 9 | FirstTime page (2019) | Dropped; a first run opens on Video with a note on Play | It holds one line of text and a Run button |
| 10 | Web button `ShellExecute`s a troubleshooting URL | Dropped | No browser to hand off to; the URL is dead |
| 11 | Six-page modal wizard, only on first run, `-changevideo`, `-safe` or after a crash | Tabbed home screen on every launch; `DXL_NO_HOME=1` for the original's behaviour | Settings must be reachable with a pad, not only after a crash or a flag |
| 12 | Detail page (sound quality, skin/world texture detail, 640×480) writes `[WinDrv.WindowsClient]`/`[Galaxy...]` keys | Dropped; the Video tab has the engine's real options | The engine's renderer and mixer read none of those keys |
| 13 | RecoveryMode page ("was not shut down properly") | Crash banner on Play quoting the engine's error; Troubleshoot opens System | The engine log can say *why* |
| 14 | `<Game>.ini` missing: UE1's Core creates it from `Default.ini` before the launcher runs | The launcher does it, and rebuilds a stub with no `[Core.System] Paths` | [Where the settings actually live](#where-the-settings-actually-live) |

## Known defects

- **The game gets the launcher's words as the engine's own options.**
  `ports/common/packaging/run-game.sh` passes them (`"$@"`) after
  `--no-launcher <GameDir>`, where the engine reads a word without a dash as
  a game folder and a dashed one as its own flag. The original's syntax
  reaches the engine only through `--cmdline=`, as `main`'s `run-game.sh`
  passes it, so a map, `-server`, `INI=` or a safe-mode flag is lost.
- **The single-instance handoff works only in part.** The original's
  protocol (one string, the command line without the program's path) runs
  over a `flock` pidfile and an abstract unix socket, in place of
  `CreateMutex` and `FindWindowEx`/`WM_COPYDATA`; the four `appStrfind`
  bypass tokens skip it, matched on that line. But nothing reads the line:
  a second launch while the home screen is up forwards it and exits, and it
  is dropped. And the lock is let go at the hand-over (`src/app.c`,
  `dxl_app_launch`), so a launch during a game finds no instance and reads
  the surviving `Running.ini` as a crash. The fix: hold the lock across the
  run, its descriptor inherited by the game or a waiter as `main`'s
  launcher is.
- **The Resolution row is Vulkan's only** (`src/ui/tab_video.c`,
  `render_enabled`), though the fork's GL device (OpenGL and OpenGL ES)
  honours `Performance.RenderScale`
  ([rendering](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#rendering)):
  under either, the game draws at the stored scale, which the row cannot
  change.
- **`test_target_<port>` passes a CPU mode named anywhere in
  `port-hooks.sh`** (`tests/test_target_port.c`, a `strstr` over the file,
  comments included), so it does not prove a `case` handles it.
- **No pad in game on the desktop build**:
  [its gotcha](../ports/linux-x86_64/README.md#no-pad-in-game).

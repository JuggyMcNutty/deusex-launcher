# The launcher

`deusex-launcher` is what a player starts, in place of the game's
`System/DeusEx.exe`: a tabbed home screen, driven by a pad or the keyboard, that
writes the configuration the engine actually reads and then hands over to it.
`dxl-cli` runs the same sequence with no display. It is C11 and SDL2, and the
same on every port's branch, which this one is the base of; what differs per
device is in
[`PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md).

It started from the original. `DeusEx.exe` was reverse-engineered first
([`launcher.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/launcher.md)),
to have known behaviour to begin from, and the launcher kept what still
matters -- the `FirstRun` gates, the crash sentinel, the command-line
parsing, the single-instance handoff -- which is `main`'s, the original
recreated almost 1:1 ([the branches](../README.md#branches)). Its screens are
new: most of the original's set things Surreal Engine never reads
([what it changes from the original](#what-it-changes-from-the-original)).

## Where the settings actually live

Surreal Engine does not read the keys the original launcher wrote. Finding out
what it *does* read decided most of the launcher:

| File | What the engine takes from it | Who writes it |
| --- | --- | --- |
| `<AppDir>/home/.config/SurrealEngine/Settings.json` | The renderer (`RenderDevice.Type`), VSync, anti-aliasing, lighting and gamma mode, bloom, HDR; in our fork the `Gamepad` and `Performance` blocks | The launcher (`core/engine_settings.c`) before each launch. The engine only reads it: it saves it solely from its desktop launcher window (`LauncherWindow.cpp`), which `--no-launcher` skips |
| `System/DeusEx.ini` | Everything else in `[Core.System]` etc.; client settings from `[WinDrv.WindowsClient]` (`Brightness`, `Decals`, viewport) **only until** `SE-DeusEx.ini` exists | The launcher creates it from `Default.ini` when missing; the game's own options |
| `System/SE-DeusEx.ini` | Written by the engine on its **first clean exit** (`PackageManager::SaveAllIniFiles`); from then on the engine reads **only** this, with client settings under `[Engine.SurrealClient]` | The engine; the launcher writes client settings here once it exists |
| `System/User.ini`, then `System/SE-User.ini` | Key and pad bindings (`[Engine.Input]`), the same way round | The launcher's controller layouts; the game's key menu |

So:

- **The renderer choice goes to `Settings.json`.** `[Engine.Engine]
  GameRenderDevice` is overridden inside the engine (`Engine.cpp`, "Override
  the ini file for things that are internal in Surreal Engine") — writing it,
  as the old launcher did, changed nothing.
- **A stub `DeusEx.ini` is fatal.** UE1's Core creates it from
  `Default.ini` before the launcher ever runs; Surreal Engine falls back to
  `Default.ini` only when the file is *absent*. On the device an earlier
  launcher build wrote a 212-byte `DeusEx.ini` holding only its own keys, and
  the engine died with `Could not find package Core`. `core/config.c` now
  creates `DeusEx.ini` from `Default.ini` when missing, rebuilds one that has no
  `[Core.System] Paths` (keeping its values), and creates `User.ini` from
  `DefUser.ini`.
- **`Settings.json` is parsed inside a catch-all.** One malformed byte and
  every setting reverts to the engine's defaults — including 4x MSAA, which is
  speckle on the PowerVR. The launcher always writes every member of every
  block it knows (`RenderDevice`, and the fork's `Gamepad` and `Performance`)
  and replaces a file it cannot parse. A missing member reads as
  empty/false/0 (`HdrScale` 0), which is why none is ever left out; one the
  file lacks takes the port's packaged default
  (`engine-settings.json.default`), so a new setting reaches an existing
  install with the port's value.
- **Texture/skin detail, sound quality and `MinDesiredFrameRate` are not read
  by Surreal Engine's renderer or mixer**, so the original Detail page's four
  choices are gone rather than kept as switches that do nothing.

## The screens

A home screen with four tabs, switched with L1/R1; START launches from any tab.

An install without the game's files gets one screen in its place
(`core/install.c`): the game folder, and whether each of `System`, its
`DeusEx.u`, `Engine.u` and `Core.u`, `Textures/Palettes.utx` and `Maps` is
there, with Check again and Exit. With the files in, Check again opens the
home screen.

| Tab | Contents |
| --- | --- |
| Play | Play / Quit; what will happen (renderer and GPU, controller and layout, game folder); a crash banner quoting the engine's last error when `Running.ini` survived; notes when the launcher repaired `DeusEx.ini` or changed the pad layout |
| Video | Renderer (a picker listing the renderers in `renderers.ini` with why each can or cannot run), **CPU mode** (when the device offers modes), Resolution (`Performance.RenderScale`, down to 480 lines; Vulkan only), Distant AI (`Performance.AiLevelOfDetail`), VSync, Brightness, Lighting, Gamma curve, Bloom and its strength, Anti-aliasing (locked off on a PowerVR GPU, with the reason), Decals |
| Controls | Controller detected, in-game pad support on/off, layout preset, **Customize buttons**, look speed X/Y, invert, dead zone, menu pointer speed |
| System | Last run (from `run-game.log`), the engine log on screen, clear crash marker, reset video / controls / game configuration (each confirmed first), game files, version |

Every row has a line of help that says what it changes in the engine, not its
name again; a row whose choices differ in kind (CPU mode, Distant AI, Lighting,
Gamma curve) says what the one it is on does. The rows are a table
(`ui/screens_internal.h` `dxl_row`); drawing, scrolling, the help pane and
button hints are written once in `ui/screens.c`.

**Renderers** (`core/renderers.c`, `platform/posix/gpu_probe.c`). Two facts decide
whether one can be chosen, and the picker shows both: whether the *engine build*
has a backend for it (`renderers.ini` `EngineType`, the `Settings.json` value;
empty means no), and whether the *device* has the API it needs (the GPU probe).
The probe runs in a forked child with a timeout, before the display comes up,
so a driver that crashes or hangs costs the child, not the launcher (the
original ran `-testrendev` in a child for the same reason). On the Smart Pro:
Vulkan selectable; OpenGL ES present on the device but not in the engine;
Software not in the engine. On a desktop with a current GPU: Vulkan and OpenGL
selectable. The list is each port's `renderers.ini` (the desktop one is
`ports/common/packaging/renderers.ini`). If `Settings.json` names a renderer that cannot run
here, a launch switches to one that can, and says so in the log.

**CPU mode** (Video tab; `launcher.ini` `CpuMode`). Offered only where the
device profile lists modes, and applied by the port's `port-hooks.sh` just
before the engine starts (`port_before_game`), with the previous state put
back when it exits. On the Smart Pro these are spruceOS's Smart / Performance /
Overclock: the menu leaves the handheld in power-save, which costs about a
third of the frame rate ([its README](https://github.com/JuggyMcNutty/deusex-launcher/blob/trimui-smartpro/ports/trimui-smartpro/README.md#cpu-mode)).
A desktop offers none: the row is hidden and `CpuMode` is never written.

**Launch flow** (`app.c`, shared by `main.c` and `cli_main.c`). The home screen
opens every time; the entry decision (`core/policy.c`, the original's
`FirstRun` gates and flags) picks *where* it opens: first run and
`-changevideo` on Video, `-safe` on System, a surviving crash sentinel on Play
with the cursor on Troubleshoot. `DXL_NO_HOME=1` skips the home screen for
unattended runs over SSH (it still shows a screen when the decision itself has
a question). Leaving with Quit saves settings but creates no sentinel.

**The hand-over** (`platform/launch.h`). On POSIX the launcher execs
`launcher.ini` `GameCommand` (default `./run-game.sh`) with the command line as
given. Because the command is configured, the whole launcher could be verified
before any engine existed. `run-game.sh` (`ports/common/packaging/run-game.sh`,
adjusted by a port's `port-hooks.sh`) starts [the engine](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md) and keeps
the sentinel: `Running.ini` is cleared only on a clean exit, so an engine crash
still produces the crash notice on the next launch.

**The device profile** (`platform/target.h`). What the launcher needs to know
about the device it runs on is one struct, from the port's `target.c`: the
fonts to try first, the panel size, the CPU modes and their help text, a note
about the built-in pad, the About line, and the GPU `dxl-shots` pretends to
have. A port without one gets `platform/target_default.c`, a generic desktop.
Facts about a *GPU* are not device facts and are keyed on the probe instead:
`dxl_gpu_msaa_broken` (`core/renderers.h`) locks anti-aliasing on any
PowerVR.

## Controller support

**In the launcher**: d-pad/left stick move, A select, B back (on Play: quit),
X reset the focused setting, L1/R1 tabs, START play, SELECT/MENU quit. The
keyboard does the same: arrows/WASD, Enter/Space/Z, Backspace/X, R,
PageUp/PageDown (or `,`/`.`/Tab), P/F5, Escape/Q (`ui/ui.c`).

**In the game** ([engine patch 0003](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#running-on-our-devices)): the SDL2
backend exposes the pad as polled state; `GamepadInput` turns it into the UE1
joystick keys and axes, so what each control does is ordinary `User.ini`
`[Engine.Input]` bindings — the same table as the keyboard, which the game's
key menu edits. In the fork that menu cannot start a rebinding yet
([lists](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/NATIVES.md#lists)).

- Buttons: A=`Joy1` B=`Joy2` X=`Joy3` Y=`Joy4` L1=`Joy5` R1=`Joy6`
  SELECT=`Joy7` START=`Joy8` L2=`Joy11` R2=`Joy12` (triggers count past half
  travel), d-pad = `JoyPov*`. `Joy9`/`Joy10` are the stick clicks, which the
  Smart Pro does not have (and its MENU button belongs to spruceOS), so no
  preset uses them.
- Sticks: `JoyX`/`JoyY` left, `JoyU`/`JoyV` right, up = positive, ±100 at full
  deflection after a radial dead zone; the look stick has a response curve.
  The engine multiplies axis input by 16 and by the binding's `Speed`, so
  `Axis aBaseY Speed=3.75` equals keyboard run speed (6000).
- While a Deus Ex modal window is open (menus, inventory, conversations,
  keypads): either stick moves the pointer, A/X click, B/Y/SELECT/START are
  Escape, the d-pad is the arrow keys, L1/R1 scroll.

**Layouts** (`core/bindings.c`): presets are data -- Modern (default),
Modern with sticks swapped, and the bindings Deus Ex shipped (three buttons).
Every command a preset or the Customize screen can bind is one the game's own
key menu offers (its `MenuScreenCustomizeKeys` list, read out of `DeusEx.u`),
plus `ShowMainMenu`, the belt slots and the F3–F12 augmentation hotkeys. The
first launch that finds the shipped bindings switches to Modern once; a preset
an earlier launcher applied and a later one revised is recognised and upgraded
(`retired[]`), with a note on the Play tab saying what changed. `Gamepad.Layout`
in `Settings.json` records the preset last applied, which is what X restores a
single button to.

## How it is tested

Host (`scripts/dx.sh test`, no display): the byte-identical ini
round-trip on the shipped files; the three command-line parsers including the
`appStrfind` surprises; the entry matrix; config seeding, stub repair and the
`SE-` file targeting; the JSON model and `Settings.json` rules (corrupt file
replaced, every member written, choices validated, a member an older file
lacks taken from the packaged default, the render resolutions offered and the
exact scale each is kept as);
renderer resolution and the PowerVR MSAA rule; layouts, per-button remapping
and retired-layout detection; argv construction for the exec; the device
profiles -- the generic one, and each port's checked against its own
`port-hooks.sh` (every CPU mode it offers is one the hooks handle, and the
hooks fall back to its default).

`dxl-shots` renders every tab and overlay headlessly at the profile's panel
size (`DXL_WINDOW`) for review; configure a host build with
`-DDXL_PROFILE=<port>` to see another device's screens.

What was checked on real hardware is per port, in each port's README.

## What it changes from the original

Kept as the original does it: the `FirstRun` gates (220/400/1100) and the
up-only clamp, the `Running.ini` sentinel lifecycle and its create-after-UI
ordering, the three non-equivalent command-line parsers, the single-instance
handoff. Changed or dropped:

| # | Original | The launcher | Why |
| --- | --- | --- | --- |
| 1 | Safe mode: eight checkboxes become flags (`-nosound`, `-nohard`, `-window`, ...) on a re-exec of the launcher; three of the eight were dead in the shipped binary | Dropped. The System tab (engine log, clear crash marker, resets) replaces it | Surreal Engine honours none of those flags. The corrected eight-box wiring existed (Port Ex Machina's commit `254d13d`) and was removed with the page |
| 2 | Missing splash bitmap → assert → process dies before the wizard | No splash | Nothing to be missing (`0x109090A4`, [`live-verification.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/live-verification.md)) |
| 3 | `MPLAYER` / `HEAT` console commands, one `HKLM\software\mpath` read | Dropped | Services dead since ~2001; `GotoHEAT.exe` is not shipped. [`porting-notes.md`](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/porting-notes.md) |
| 4 | `.ICD`→`.EXE` rewrite in `InitPathnames` | Dropped | SafeDisc artifact; the GOG build is not wrapped |
| 5 | `-make` rejected with a fatal error | Dropped | Points at `ucc`, shipped separately |
| 6 | Renderer page runs Win32 3D device detection, re-execing itself per candidate, and writes `GameRenderDevice` | Crash-isolated GPU probe (forked child); choice written to `Settings.json` `RenderDevice.Type` | Surreal Engine overrides `GameRenderDevice`. `-testrendev` is still parsed; `dxl-cli --probe` is its replacement |
| 7 | `CreateMutex` + `FindWindowEx`/`WM_COPYDATA` handoff | `flock` pidfile + abstract unix socket | Same protocol (one string, the command-line tail), different transport. The four `appStrfind` bypass tokens still skip it |
| 8 | CD check loops on `<CdPath>Textures\Palettes.utx` with a modal box | Install-validation screen | Generalises to "did the user supply the game files?", which is the actual first-run failure here |
| 9 | Driver page (2022) names the detected Direct3D card | The renderer picker's status line (GPU, API version) | The page existed to name a D3D card and point at a driver download |
| 10 | FirstTime page (2019) | Dropped; a first run opens on the Video tab with a note on Play | Its whole content was "Deus Ex is starting up for the first time" and a Run button |
| 11 | Web button `ShellExecute`s a troubleshooting URL | Dropped | No browser to hand off to, and the URL is long dead |
| 12 | Six-page modal wizard, shown only on first run, `-changevideo`, `-safe` or after a crash | Tabbed home screen on every launch; `DXL_NO_HOME=1` for the old behaviour | Settings must be reachable with a pad; a screen that appears only after a crash or a command-line flag is not |
| 13 | Detail page (sound quality, skin/world texture detail, 640×480) writes a block of `[WinDrv.WindowsClient]`/`[Galaxy...]` keys | Dropped; the Video tab carries the engine's real options | The engine's renderer and mixer read none of those keys |
| 14 | RecoveryMode page ("was not shut down properly") | Crash banner on Play quoting the engine's reported error; Troubleshoot opens System | The engine log can say *why* |
| 15 | `<Game>.ini` missing: UE1's Core creates it from `Default.ini` (before the launcher runs) | The launcher does it, and also rebuilds a stub with no `[Core.System] Paths` | [Where the settings actually live](#where-the-settings-actually-live) |

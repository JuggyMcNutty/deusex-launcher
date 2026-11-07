# Port: TrimUI Smart Pro (spruceOS)

An Allwinner A133 handheld (4× Cortex-A53, PowerVR GE8300) running spruceOS. The port is
cross-built from a PC, and the performance work is measured here. The launcher is DXLauncher,
linux-aarch64's, tailored to the handheld: the app the spruceOS menu starts.

## Status

The launcher and the engine run on the device, and the game plays on Vulkan or OpenGL ES (the
Graphics page's choice; Software draws it on the CPU, at half their frame rate or less), at
853x480 by default: [Performance](#performance) has the numbers, and
[where a frame goes](#where-a-frame-goes) the work left.

## Build and run

```sh
# from Port Ex Machina
scripts/dx.sh fetch                        # once: the engine, the launcher's branches, the RE
scripts/dx.sh deps   trimui-smartpro       # toolchain + sysroot (the device must be awake)
scripts/dx.sh deploy trimui-smartpro       # build, stage, send (--no-engine; --run starts it)
```

`deploy` builds (incrementally) and stages the current tree first, warning when the engine has
uncommitted changes (the profiling hooks, say). It sends only the files whose checksum differs,
keeps the device's copies of those in `.prev-<date-time>` (the device's clock) in the app
directory, and checksums every file there afterwards. Nothing is ever deleted on the device:
copying a previous copy back is a rollback (newest last). `--no-engine` leaves the engine and
its `DXEngine.pk3` out. `--run` starts the app as the menu does, through `launch.sh`, the menu
paused while it runs.

The app lives in `/mnt/SDCARD/App/DeusEx`, the game in `/mnt/SDCARD/Roms/PORTS/DeusEx` (all 38
`.u` packages), which the first run finds there. The launcher's own files -- `launcher.ini`, the
profiles, their saves and logs -- are in the app's `home/` (`DXL_HOME`, set by `launch.sh`),
which `deploy` never touches.

**Access.** `spruce@192.168.1.211`, password `happygaming`: the stock firmware password, a
default in `port.sh` on purpose; SSH is root. Override with `DEVICE`, `DEVICE_USER`,
`DEVICE_PASS`, `DEVICE_APPDIR`. Asleep, the device drops off the network (no ping, SSH times
out): ask its owner to wake it.

### Diagnosing

```sh
export DXL_HOME=$PWD/home    # the app's home, as launch.sh names it: an absolute path, or none
./dxl-cli --dry-run          # the files, profile, install, decision and command; writes nothing
./dxl-cli --probe            # the GPU alone, nothing else run: Vulkan device, OpenGL ES version
./dxl-cli --print-command    # the engine's command line for the profile in use
```

Run them in the app directory. The launcher's log is `home/state/dxlauncher/launcher.log`, a
game's output its profile's `last-run.log`
([where everything lives](../../docs/LAUNCHER.md#where-everything-lives)); the window's log page
shows them.

## What differs from linux-aarch64

The branch is linux-aarch64's launcher, with this port in place of linux-aarch64's and the
handheld's tailoring, each a commit of its own. The shared files each one touches are where a
cherry-pick from linux-aarch64 may meet them:

| Edit | Its own files | Shared files touched |
|---|---|---|
| The port: the SDK's toolchain for both, SDL3 without what the device cannot use | `ports/trimui-smartpro/*` | `CMakePresets.json` (the preset swapped); `ports/linux-aarch64/` removed (the host build's `ports/linux-x86_64` kept); `cmake/deps.cmake` (`DXL_PORT_SDL3_ARGS`); `cmake/deps.lock` (no SDL2: the engine's is the device's own) |
| [The video driver](#the-video-driver) | `ports/trimui-smartpro/sdl3/` | `cmake/deps.cmake` (`DXL_PORT_SDL3_DRIVER`); `THIRD-PARTY.md` |
| The spruceOS app | `packaging/{config.json,launch.sh,deusex.png}` | |
| [CPU modes](#cpu-mode) and the game hook | `src/core/cpumode.{c,h}`, `src/app/gamehook.{c,h}`, `packaging/port-hooks.sh`, `tests/test_cpumode.c` | `src/app/run.c` (two calls), `src/core/settings_table.c` (one Graphics row), `CMakeLists.txt`, `tests/CMakeLists.txt` |
| The big screen alone, the pad's hints from the first frame | | `src/screens/window.c` (no launcher size, no window size kept, no switch), `src/app/session.h` (no `[Window]`), `src/ui/kit.{c,h}` (no F11), `src/screens/{shell,settings}.c` and `shell.h` (no offer of it, no `dxl_shell_big`), `src/core/settings_table.c` (Big screen and the launcher's size gone), `tools/shots.c` (no offer's scene); `src/core/bigscreen.{c,h}` and `test_bigscreen` gone, with `--big-screen` (`src/main.c`, `src/app/run.{c,h}`, `tests/test_play.c`), the UI scripts' `big` (`src/screens/uiscript.{c,h}`), `CMakeLists.txt`, `tests/CMakeLists.txt`; the `big_screen*` UI scripts gone, with their checks and the environment they cleared in `tests/test_ui_scripts.c` |
| No desktop: no app menu, `xdg-open` or folder dialog | | `src/platform/desktop.h`, `src/platform/posix/desktop.c`, `test_desktop` and the `.desktop.in` gone; `src/main.c`, `src/screens/{window,about,log,shots,mods,firstrun,shell}.c`, `shell.h`, `window.h`, `src/platform/process.h`, `src/platform/posix/process.c`, `src/ui/icon.{c,h}` (no `dxl_icon_save`), `CMakeLists.txt` |
| Deleting for good, after the page's confirm: no trash to put back from | `src/platform/delete.h`, `src/platform/posix/delete.c`, `tests/test_delete.c` | `src/platform/trash.h`, `src/platform/posix/trash.c` and `test_trash` gone; `src/app/session.{c,h}`, `src/screens/{shots,saves,profiles}.c`, `shell.h`; the `profiles`, `saves_delete` and `shots` UI scripts and their checks in `tests/test_ui_scripts.c` |
| A pad without stick clicks | | `src/core/bindings.{c,h}` (Modern and its swap leave `Joy9`/`Joy10` unbound; fourteen rows), `src/screens/controller.c`; `tests/test_bindings.c` |
| spruceOS's Ports folder in the finder | | `src/core/finder.{c,h}` (`Roms/PORTS` among a drive's folders), `src/screens/firstrun.c` (the first run's words name it); `tests/test_finder.c` |
| The game full screen alone, the device showing a window only in its corner: no Full screen, Full screen size, window size or `-defaultres` row, every run full screen whatever the profile's ini says, the next run on the software renderer with `-nohard` alone | | `src/core/settings_table.c`, `src/core/settings.h` (an id's example), `src/screens/recovery.c`, `src/app/run.c` (`commit`), `docs/LAUNCHER.md`; `tests/test_settings.c`, `tests/test_ui_scripts.c`, the `recovery` and `settings_roundtrip` UI scripts |
| `dxl-shots` at 1280x720 on the big screen | | `tools/shots.c`, `tests/CMakeLists.txt` |
| Full screen's size as imported: linux-aarch64's, the desktop's (`FullscreenViewportX` and `Y` 0), not taken, as the engine draws borderless over the whole 720p framebuffer whatever size the ini names, there being no desktop mode to switch from | | `src/core/profile.c`, `tests/test_profile.c` |
| The device's data | | `data/renderers.ini` (Vulkan, OpenGL ES, Software), `data/engine-settings.json.default` (Off, VSync off, RenderScale 0.6666667); `tests/test_settings.c`, the `settings_roundtrip` UI script |
| The launcher's renderer row: OpenGL ES or Software, this SDL3 having no OpenGL or Vulkan (`port.cmake`) | | `src/core/settings_table.c` (the `render_drivers` choices) |
| Mesa's CPU drivers left out, there being no Mesa here: linux-aarch64's Vulkan (lavapipe) and OpenGL (llvmpipe) renderers, the probe's CPU drivers and `CPUDriver` not taken | | `data/renderers.ini`, `data/engine-settings.json.default`, `src/core/{renderers,engine_settings}.{c,h}`, `src/core/{settings,settings_table}.c`, `src/platform/posix/gpu_probe.c`, `src/screens/{settings,recovery,firstrun,shell}.c`, `src/cli_main.c`, `docs/LAUNCHER.md`; `tests/test_{renderers,engine_settings,settings}.c` |
| Deploy and profile | `port.sh`, `tools/profile-map.sh` | |

Each commit's own paragraphs of [`docs/LAUNCHER.md`](../../docs/LAUNCHER.md) come with it.

### The toolchains

The device runs glibc 2.33; the launcher and the engine are built at 2.28, as every Linux build
is ([the glibc ceiling](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#gotchas-that-cost-time)),
by one toolchain, linux-aarch64's: Godot's Buildroot SDK
`aarch64-godot-linux-gnu_sdk-buildroot-2023.08.x-4`, GCC 13.2 on glibc 2.28, kernel headers
5.15 ([the glibc ABI](#verified)).

The launcher links none of the device's libraries: SDL3, FreeType, SDL3_ttf and libxmp-lite
are built into it from `cmake/deps.lock`'s sources, SDL3 opening ALSA (`libasound.so.2`) at run
time; it needs only libc, libm, libdl, libpthread and librt. The engine links the device's SDL2
and OpenAL. Those libraries need more than 2.28: the device's SDL2 and ALSA reference `exp`,
`log` and `pow` at `GLIBC_2.29`, its SDL2_mixer `pow`, and its `libstdc++.so.6.0.28` (which
`libopenal` needs) `stat`, `lstat` and `fstat64` at `GLIBC_2.33`. The device's glibc has them,
so they load; GNU ld rejects them against the SDK's 2.28, so the engine's link ignores what
shared libraries leave undefined (`-Wl,--unresolved-symbols=ignore-in-shared-libs`,
`toolchain-cxx.cmake`), and any probe linked to them needs the same.

The engine links `libstdc++` and `libgcc` statically (~1 MB): the SDK's `libstdc++` is 6.0.32
(`GLIBCXX_3.4.32`), newer than the device's 6.0.28. `toolchain-cxx.cmake` keeps pkg-config on
the device's sysroot alone: the SDK's sysroot has glib and fontconfig, which DXWidgets would
otherwise link. `scripts/check-abi.sh --max 2.28` runs on every build of both and fails on any
reference above 2.28: the launcher's build runs it because `port.cmake` sets
`DXL_PORT_GLIBC_MAX`, and `scripts/engine.sh build` runs it on the engine.

[`fetch-sysroot.sh`](fetch-sysroot.sh) (run by `deps`) copies the device's own libraries into
`deps/sysroots/trimui-smartpro/lib` -- SDL2, OpenAL, EGL, GLESv2, the Vulkan loader, ALSA, zlib,
libstdc++ -- and fetches the headers to build against, each pinned by its SHA256 (SDL 2.30.8;
Vulkan, EGL/GLES/KHR and OpenAL from exact Arch Linux packages; Mesa's two EGL extension
headers). Vulkan is not linked: DXGPU loads the loader through volk, and the launcher's GPU probe
`dlopen`s it and EGL.

Where the device cannot be reached -- GitHub's CI
([CI](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#ci)) --
`DX_SMARTPRO_SYSROOT=sdk` makes the sysroot from TrimUI's own SDK for the device instead, its
release `20231018`, pinned: its SDL2, EGL, GLESv2, ALSA and zlib under the device's names and
sonames, every symbol the programs link among them (SDL2's at its 2.26.1, where the device has
2.30.8; the headers are 2.30.8's either way), and OpenAL Soft, which the SDK lacks, Debian
buster's, checked against the SHA256 its pinned index gives. Linked by soname, the programs take
the device's own at run time. The libstdc++ and Vulkan loader only the probes link are left out.

### The video driver

The device has no DRM, X11 or Wayland: its display is `/dev/fb0` and the PowerVR stack's EGL,
which only the vendor's SDL2 (`/usr/trimui/lib`, its `mali` driver, the engine's) draws on.
SDL3 has no driver for it, so the launcher's SDL3 carries one of this port's own,
[`sdl3/SDL_fbvideo.c`](sdl3/SDL_fbvideo.c), SDL's private driver: `SDL_VIDEO_DRIVER_PRIVATE`
puts it first, and [`sdl3/sdl3.cmake`](sdl3/sdl3.cmake), SDL3's project include, adds it to the
library, with no patch to SDL's sources.

- One display, fb0's 1280x720; every window that size and full screen
  (`VIDEO_DEVICE_CAPS_FULLSCREEN_ONLY`).
- OpenGL ES through SDL's EGL code on the default display with a **NULL native window**, the
  only one the PowerVR EGL takes; its swaps flip fb0 between its first two pages at 60 Hz, at
  any swap interval. The launcher draws with SDL's `opengles2` renderer, VSync on.
- A framebuffer path for SDL's software renderer, copied into the page fb0 shows.
- No ShowWindow: SDL then gives the window the keyboard focus, without which it drops the pads'
  events. The pads are SDL's own joystick driver's, found by an inotify scan; the driver reads
  no input.
- The console's cursor hidden through `/dev/tty0` while the video is up; fb0's mode and page
  put back as it goes, the menu's.

SDL3 is built with no window system (a console build), no desktop GL, Vulkan, udev, D-Bus or
HIDAPI (`port.cmake`): a driver that fails shows as no window. The launcher's EGL is gone
before the game starts: its window closes, SDL unloads EGL with the last GL window, and the
engine's vendor SDL2 opens its own; it draws again after.

### CPU mode

The spruceOS menu leaves the handheld in power-save ([as measured](#the-device-as-measured)),
far too slow for the game. The Graphics page's CPU mode (`launcher.ini`'s `[Launcher]
CpuMode`) offers spruceOS's own: Smart, Performance (all four cores at 1.8 GHz) and Overclock
(2.0 GHz), the default: the game is CPU-bound. The game hook, `packaging/port-hooks.sh`,
applies it with the spruceOS helpers its Ports launcher uses just before the engine starts,
and puts the state from before back (online cores, governor, min/max) as the game ends,
however it ends ([the game hook](../../docs/LAUNCHER.md#the-game-and-the-launcher)). The
helpers are sourced in subshells only: `helperFunctions.sh` exports its own `LD_LIBRARY_PATH`.
`test_cpumode` runs the hook against stand-in helpers and a scratch sysfs.

### Engine settings

The profile's engine folder has the engine's `Settings.json`; the defaults file
(`data/engine-settings.json.default`) is the device's: `Antialias: Off`, as the GE8300's
resolve turns partially covered pixels into speckle (the launcher holds it off on any PowerVR,
and writes Off before a game that would multisample, a first run's "the engine's defaults"
too); VSync off, as below the panel's 60 Hz vsync would hold the game to 30 or 20; and
`Performance.RenderScale` 0.6666667, 853x480. The Display page's render scale changes it.

## The device, as measured

Probed over SSH, not assumed; Port Ex Machina's
[device probes](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#device-probes)
and `dxl-cli --probe` reproduce it.

| Fact | As measured |
|---|---|
| Device | TrimUI Smart Pro (`hwserial TG5040`), spruceOS `PLATFORM=SmartPro` |
| SoC | Allwinner A133 `sun50iw10p1`, 4× Cortex-A53; 3 GB RAM (`free`) |
| CPU modes | the spruceOS menu: **power-save, cores 0 and 3 only, `conservative`, 408 MHz–1.49 GHz**. `set_performance`: all four cores, `performance`, 1.8 GHz; `set_overclock`: 2.0 GHz (`spruce/scripts/platform/SmartPro.cfg`) |
| OS | TinaLinux "Neptune", kernel 4.9.191, **glibc 2.33**, busybox 1.36.1 |
| Framebuffer | `/dev/fb0` alone (`/sys/class/graphics`); 1280x720, 32 bpp, BGRA in memory (red at bit 16), `line_length` 5120, 64 MB, found at a virtual 1280x13107; takes a 2x and 3x `yres_virtual`, pans to any page; `FBIO_WAITFORVSYNC` returns at once. A 1280x720 copy into it ~3.5 ms at power-save (`probe-fbdev-egl.c`) |
| EGL | 1.4, Imagination's `1.19@6345021`, OpenGL ES alone; `libEGL.so.1`, `libEGL.so`, `libGLESv2.so.2` and `libGLESv2.so` all load. Window surfaces: a NULL native window only (`{ushort w,h}` and `{int w,h}` refused, `EGL_BAD_NATIVE_WINDOW`); the surface's height not given by `eglQuerySurface`; swaps flip fb0 between pages 0 and 1, 16.6 ms apart at interval 0 and 1. Five create-draw-terminate cycles in one process draw, ~170 ms each; `dxl-cli --probe` beside a live surface works; a process killed mid-draw leaves the next drawing; the vendor SDL2 draws after all of it |
| Consoles | `/dev/tty0`, `/dev/tty1` in text mode; no fbcon cursor control in sysfs |
| SDL (the engine's) | vendor 2.30.8 in `/usr/trimui/lib`; video driver **`mali`**; 1280x720 @60Hz, `opengles2`; a window not full screen drawn at its size in the screen's bottom left corner, the rest black (the engine at 640x480 with `-nohard -noddraw`, read from the framebuffer) |
| GPU APIs | Vulkan **1.3.225** on the PowerVR Rogue GE8300; **OpenGL ES 3.2** (`build 1.19@6345021`) through EGL; no desktop OpenGL. Detection takes ~0.5 s. Vulkan: no `VK_EXT_descriptor_indexing`; BC1–5, RGB8 and RGBA32F not sampled or filtered as the engine needs (`probe-vulkan-caps.c`, `probe-texture-formats.c`). OpenGL ES: no S3TC, float-linear filtering, anisotropy or `glBufferStorage`; `GL_MIRROR_CLAMP_TO_EDGE` rejected (`probe-gles-sampler.c`); no `EXT_clip_cull_distance`; RGB10_A2 drawn, resolved, sampled and read with its ten bits, multisampled up to 4x (`probe-gles-scene.c`; 1 beside an R32UI attachment, which the engine's scene lacks) |
| Input | one pad, `event3` `"TRIMUI Player1"`, USB ids `045e:028e` (an Xbox 360 pad's), 11 keys, both sticks ±32767, triggers 0–255, a hat; besides it the power key, the audio jack and two keys (`sunxi-keyboard`). `libudev.so.1` loads |
| Pad | to SDL `"Xbox 360 Controller"`, GUID `0300a3845e0400008e02000014010000`, with SDL's built-in mapping |
| Pad controls | A B X Y, L1 R1, SELECT START MENU (MENU is spruceOS's), d-pad (a hat), two sticks. **L2/R2 are digital**, though mapped to axes `a2`/`a5`. **No L3/R3**: the sticks do not click, though the device reports `BTN_THUMBL` and the mapping lists `leftstick:b9`/`rightstick:b10` |
| Storage | SD is **exFAT**: case-insensitive, no links, no meaningful permission bits |
| Profiling | no `perf` (`CONFIG_PERF_EVENTS` off); CPU-time timers fire only on the scheduler tick, every 4 ms |
| Pipelines | the engine's 37 Vulkan scene pipelines, at `b26be5f`: ~2.8 s to build, at start and on each change of resolution or anti-aliasing (a desktop's Radeon: under 2 ms) |

### What is not available

The original x86 Windows `Core.dll`/`Engine.dll`/`DeusEx.dll` cannot run here: the device has
no box64, box86, wine or qemu, and box64's own notes record Deus Ex under Wine crashing before
the menu on far stronger hardware.

### Probes

Port Ex Machina's
[device probes](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#device-probes),
each one compiled against this port's sysroot, run over SSH:

```sh
# from the parent folder of Port Ex Machina's repositories
TC=deps/toolchains/aarch64-godot-linux-gnu_sdk-buildroot-2023.08.x-4/bin/aarch64-godot-linux-gnu-gcc
SYS=deps/sysroots/trimui-smartpro
$TC -O2 -mcpu=cortex-a53 -std=c11 -I $SYS/include -I $SYS/include/SDL2 \
    port-ex-machina/tools/probes/probe-vulkan-caps.c -o /tmp/probe \
    -L $SYS/lib -lvulkan -Wl,-rpath-link,$SYS/lib -Wl,--unresolved-symbols=ignore-in-shared-libs
deusex-launcher/trimui-smartpro/scripts/check-abi.sh --max 2.28 /tmp/probe
# scp to the device, then: LD_LIBRARY_PATH=/usr/trimui/lib:/usr/lib:/lib ./probe
```

Pause the spruceOS menu before one that draws, or `probe-sdl.c --pad` ([gotchas](#gotchas)).
`dxl-shots` from the host build renders every launcher screen at the device's 1280x720 on the
big screen.

## Verified

On the device, with the launcher started through `launch.sh` on a fresh `DXL_HOME`:

| Check | Result |
|---|---|
| glibc ABI | the launcher references `GLIBC_2.17` and `GLIBC_2.27`, `dxl-cli` `GLIBC_2.17` only, the engine ≤ 2.27 (built at 2.28); the launcher needs libc, libm, libdl, libpthread, librt alone |
| The first run | finds the game in `Roms/PORTS`; makes the profile on the exFAT card (as `dxl-cli --import` does), the engine's `Screenshots` a folder |
| The first play | `Settings.json` made with Antialias Off ("Off, not MSAA4x: PowerVR Rogue GE8300 multisamples badly"), VSync off, RenderScale 0.6666667 |
| The window | the big screen through the driver, `opengles2` with VSync, the pad's hints from the first frame; its EGL gone before the game, back for Recovery; fb0 at page 0 as it quits |
| Vulkan and OpenGL ES | Liberty Island drawn on both, through the launcher |
| Full screen alone | a fresh `DXL_HOME`'s profile whose `DeusEx.ini` asks for a window, played by `dxl-cli --play`: full screen at 1280x720 on Vulkan, the launcher's log saying it set it |
| CPU mode | Overclock (cores 0–3, 2.0 GHz) while the engine runs, the hook's lines in `last-run.log`; power-save restored after, a SIGKILLed engine's too, Recovery then up |
| Descriptors | the engine holds none of the launcher's: its fonts not among them |
| A second start | handed to the running launcher, which takes its line |
| The launcher while a game runs | 17 MB resident, no CPU; up in ~1 s |
| `dxl-cli --probe` | the GPU [as measured](#the-device-as-measured), in 0.5 s |
| `scripts/dx.sh deploy` | sends only the changed files, keeps the previous copies; every file's checksum matches |
| [A profile](#measuring) | the engine as the launcher starts it, on Vulkan, OpenGL ES and Software ([performance](#performance)) |

## Performance

Liberty Island's level start (`01_NYC_UNATCOIsland.dx`) unless a row names another map;
Overclock; milliseconds a frame, averaged over 60 frames, measured with the profiling hooks at
the engine each row names: `503e90d`, `590635b`, `a8c47c3` and `cfb396b` with every pawn
thinking, `15392c0` with Distant AI on, an option of that engine's: far pawns skipping their
thinking. `cfb396b` is measured with both toolchains, the same engine and launcher: Bootlin's
GCC 10.3 (glibc 2.33) and the SDK's GCC 13.2 (2.28), [the toolchain](#the-toolchains).
`b26be5f`, `aea5d4c`, `4de67a5` and `c7ac0a3` with every pawn thinking and the SDK's GCC;
`aea5d4c` and `4de67a5` differ only in the light map pages' borders, which Battery Park's thin
maps under its fire feel. `590635b`'s second row has Surreal Engine's one pass of detail texture
in place of `D3DDrv`'s three bands, which sample it only nearer than 380 units.

| Engine | Renderer, resolution | FPS | Frame | Tick | Render CPU | GPU wait |
|---|---|---|---|---|---|---|
| `503e90d` | Vulkan, 853x480 | 16.8 | ~60 | ~14 | ~42 | ~10.5 |
| `590635b` | Vulkan, 853x480 | 20.0 | ~50 | ~14 | ~33 | ~2.2 |
| `590635b`, one pass of detail | Vulkan, 853x480 | 17.0 | ~59 | ~15 | ~40 | ~9.5 |
| `a8c47c3` | OpenGL ES, 853x480 | 17.3 | ~58 | ~12.5 | ~44 | |
| `15392c0` | Vulkan, 1280x720 (native) | 10.4 | ~96 | ~19 | ~73 | ~31 |
| `15392c0` | Vulkan, 853x480 | 15.0 | ~67 | ~19 | ~44 | ~6 |
| `15392c0` | OpenGL ES, 1280x720 | 12.1 | ~83 | ~19 | ~61 | |
| `15392c0` | OpenGL ES, 853x480 | 14.0 | ~72 | ~16 | ~53 | |
| `15392c0` | UNATCO HQ (indoors), Vulkan, 853x480 | ~32 | | | | |
| `15392c0` | Battery Park (outdoors), Vulkan, 853x480 | ~22 | | | | |
| `cfb396b`, GCC 10.3 | Vulkan, 853x480 | 20.0 | ~49.9 | ~12.4 | ~34.2 | ~3.5 |
| `cfb396b`, GCC 13.2 | Vulkan, 853x480 | 20.7 | ~48.3 | ~11.6 | ~33.6 | ~4.3 |
| `cfb396b`, GCC 10.3 | OpenGL ES, 853x480 | 17.3 | ~57.7 | ~11.8 | ~44.3 | |
| `cfb396b`, GCC 13.2 | OpenGL ES, 853x480 | 18.1 | ~55.4 | ~11.2 | ~42.5 | |
| `b26be5f` | Vulkan, 853x480 | 20.1 | ~49.8 | ~12.1 | ~34.5 | ~3.9 |
| `b26be5f` | Vulkan, 1280x720 (native) | 14.9 | ~67.0 | ~10.6 | ~53.4 | ~20.3 |
| `b26be5f` | OpenGL ES, 853x480 | 17.7 | ~56.6 | ~11.3 | ~43.6 | |
| `b26be5f` | OpenGL ES, 1280x720 | 15.9 | ~62.9 | ~10.2 | ~49.6 | |
| `b26be5f` | Battery Park (outdoors), Vulkan, 853x480 | 24-26 | ~39-42 | ~9.1 | ~27-31 | ~6-9 |
| `4de67a5` | Vulkan, 853x480 | 22.5 | ~44.5 | ~12.2 | ~29.2 | ~3.8 |
| `aea5d4c` | Vulkan, 1280x720 (native) | 16.4 | ~61.1 | ~10.5 | ~47.6 | ~20.2 |
| `aea5d4c` | OpenGL ES, 853x480 | 19.8 | ~50.5 | ~11.3 | ~37.5 | |
| `aea5d4c` | OpenGL ES, 1280x720 | 18.1 | ~55.4 | ~9.8 | ~42.8 | |
| `4de67a5` | Battery Park (outdoors), Vulkan, 853x480 | 31-33 | ~30.5-32 | ~9.7 | ~18-21 | ~2-5 |
| `c7ac0a3` | Software, 853x480 | 10.0 | ~99.6 | ~9.6 | ~87.5 | |
| `c7ac0a3` | Software, 1280x720 (native) | 6.1 | ~163.8 | ~10.3 | ~151.2 | |

The render CPU includes the GPU wait, the lightmaps and the texture uploads. The hooks measure
only the Vulkan device's GPU wait; in GL the driver's time lands inside the render CPU. The
hooks' own timers take ~3.5 of the render CPU (`steady_clock::now`: each box and surface test
is timed).

On Software the render CPU is the drawing too, on the four cores with NEON: at 853x480, of its
~87.5, ~31 are the tiles drawn at the frame's end, ~13 the wait there for those the three other
cores drew while the frame was recorded, ~7 the frame scaled up to the screen and ~11 the
window's present (SDL's texture, through the GPU); at native, of ~151.2, ~71, ~38, none (each
tile maps itself) and ~12
([the software device](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/ENGINE.md#rendering)).

### Where a frame goes

Vulkan at native 1280x720 at the level start, from a sampled run of `4de67a5` (its frame as long
as in `aea5d4c`'s row), in ms, by the frame-time hooks and the device's CPU samples (42.3 ms of
the main thread's CPU a frame), with what is left in each area. The GPU draws the previous
frame while the input, tick and view run
([the Vulkan device](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/ENGINE.md#rendering)),
and at native it is the longer: the render waits ~21 on it, at 853x480 ~4. So **at native a
frame is about the render CPU plus the GPU's time**, and at 853x480 the CPU's work is the frame:
render-CPU savings count in full there, and the tick's too.

- **Game tick ~10** over the ~770 dynamic actors (the level ticks no static actor, as the
  original's):
  - **script VM ~7** (`CallEvent` ~7.7 under the tick by the samples). The interpreter's own
    (`Frame::Run`, `ExpressionEvaluator`, `Frame::Call` self time) ~3 is mostly the Cortex-A53
    waiting on memory for each expression node. Left: only a denser, compiled form of each
    function's code would change that, a rewrite of the evaluator's core. How the original does
    it:
    [the script interpreter](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-script-interpreter).
  - physics ~1.0 (`TickPhysics`), the walking, swimming and flying pawns and anything falling
    taking the tick at once, as the original's, without `TryStepToGround`;
  - collision traces ~0.6 (`TraceTester::Trace`), the player's ladder probe and floor and wall
    materials ~0.2;
  - per-actor work around the scripts ~2.4: `ULevel::TickDeusEx` ~1.2 and `TickActorDeusEx`
    ~0.6 self, `dynamic_cast` ~0.6 (`TryCast`).
- **Render CPU ~28** besides waits, lightmaps and uploads. By section: visibility 11.5, actors
  4.1, end of frame (`unlock`) 2.2, portals 2.1, BSP surfaces 1.8, translucent 1.6,
  `PostRenderFlash` (script) 1.3, BSP set-up (`bsp-info`) 1.2, the rest 1.3:
  - visibility: ~3,200 box tests (~2.6) and ~1,860 surface tests (~2.7) a frame against
    `BspClipper`'s occlusion grid, portals ~0.9, actor set-up ~0.9. By function: the BSP walk
    (`ProcessNodeSurface` ~2.2, `ProcessNode` ~1.7, cache misses), `DrawPolygon` ~1.7,
    `DrawSpan` ~1.2, `IsAABBVisible` ~0.8; the grid's rows a cache line each, the movers'
    mirrors looked for among the movers with one. Left: the walk's memory;
  - actor meshes ~3.9 for ~44 in view: `DrawLodMeshFaceDX` ~1.9 and the vertex lighting
    `GetVertexLight` ~0.5 of own time;
  - the Vulkan driver (`libVK_IMG` ~3.3, `libsrv_um` ~2.3, each with what it calls), per draw
    call and submit. Without descriptor indexing a batch breaks at each texture: the light maps
    share pages, each with a border repeating its edges, and the opaque surfaces go by zone,
    texture and light map, so that a run of one texture and one page is one batch;
  - `UActor::UpdateBspInfo` ~1.0, the dynamic actors' BSP set-up each frame: the 1,831 static
    actors of Liberty Island's 2,604, which do not move, are filed when the level sorts its
    actors and then a sixteenth a frame;
  - `LightSystem::BeginFrame` ~1.0, every actor looked at for a light each frame (a script can
    change a static light); the fractal textures ~1.8 (`StepWater`, the wet textures).
- **Lightmaps ~0.4, texture uploads ~0.3.** The `BarrelFire`, a dynamic light with the fire
  waver effect, rebuilds its 6 lightmaps every frame, built, converted and sent only where its
  light reaches
  ([lighting](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/NATIVES.md#lighting)).
  In Battery Park a fog map takes only the fog lights a line of sight to it crosses, and one
  they leave clear is sent once: its one volumetric light's maps cost nothing a frame at the
  level start.
- **GPU**, the time above: the render's wait plus the input, tick and view it overlaps. CPU and
  GPU compete for the SoC's shared memory.
- **`view+audio` ~2** is mostly `UDXEngineAudioDevice::StartAmbience` reading every actor's
  `AmbientSound` each frame (~1.7). The original does the same scan every frame
  ([each frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#each-frame)):
  nothing to port.

**OpenGL ES** leads Vulkan at native, where Vulkan waits on the GPU, and trails it at 853x480
([the table](#performance)): the GL driver's time lands in its render CPU and is not overlapped
by the tick. The GL device's workarounds for this driver: ENGINE.md's
[rendering](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/ENGINE.md#rendering)
and, for fullscreen,
[running on our devices](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/ENGINE.md#running-on-our-devices).

### Measuring

Build the engine with the frame-time hooks (`tools/perf/perf.sh on` applies DXEngine's
`tools/perf/perf-instrumentation.patch`;
[the profiling hooks](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/ENGINE.md#the-profiling-hooks)),
deploy, and run `scripts/dx.sh profile trimui-smartpro [seconds] [label] [cpu] [turn] [map]`.
That runs [`tools/profile-map.sh`](tools/profile-map.sh) on the device (its header documents the
arguments, `SHOT`, `SAMPLE` and `DXE_PERF_DETAIL`) and brings back
`build/trimui-smartpro/profile/perf-<label>.log`: the frame split into input, tick, render CPU,
GPU wait, lightmaps and texture uploads, the render CPU by section, the visibility pass by part,
and the time under script calls.

The script starts the engine as the launcher does for the profile in use (its command line
`dxl-cli --print-command`'s, `--url=<map>` added) and applies `launcher.ini`'s CPU mode through
`port-hooks.sh`, as a launch does. It uses the profile's own `Settings.json` and switches
nothing: check the log's `settings:` line (renderer, anti-aliasing, render scale) before taking
a run as native (one at 0.6666667 is an 853x480 run, whatever its label). For another renderer
or resolution, set it there for the run and put the owner's value back after.

`SAMPLE=1` also brings back samples of the main thread's CPU (4 ms apart) and the engine that
made them, for
[`tools/perf/sample-report.py`](https://github.com/JuggyMcNutty/DXEngine/blob/main/tools/perf/sample-report.py):

```sh
# from the parent folder of Port Ex Machina's repositories
SAMPLE=1 port-ex-machina/scripts/dx.sh profile trimui-smartpro 90 mylabel
NM=deps/toolchains/aarch64-godot-linux-gnu_sdk-buildroot-2023.08.x-4/bin/aarch64-godot-linux-gnu-nm \
    DXEngine/tools/perf/sample-report.py build/trimui-smartpro/profile/samples-mylabel \
    build/trimui-smartpro/profile/samples-mylabel.engine --sysroot deps/sysroots/trimui-smartpro \
    --root ULevel::Tick
```

A profile stops the engine with SIGKILL; started without the launcher, it leaves no
`Running.ini`
([running it](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/ENGINE.md#running-it)).

## Gotchas

- **The development unit has no battery**: on USB power, spruceOS reads 0% and discharging, and
  its low-battery popup takes the screen and stalls the engine. So the warning is `Off` in
  `/mnt/SDCARD/Saves/spruce/spruce-config.json` (`Battery Settings` › `lowPowerWarningPercent`;
  the original is beside it, `.bak-lowpower-20260923`), and
  `spruce/scripts/low_power_warning.sh` has its 1% forced shutdown disabled by hand (the
  original is `low_power_warning.sh.bak-lowbat`).
- **Pause the spruceOS menu while running anything that draws over SSH**:
  `kill -STOP $(pidof MainUI)`, and `kill -CONT` after (use a `trap`). Two programs on one
  framebuffer fight, and pad presses would drive the menu too.
- **Killing over SSH**: `ps | grep deusex` and `pkill -f` match the SSH command itself
  ([why](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/DEVELOPMENT.md#gotchas-that-cost-time));
  use `pidof`. busybox `killall` rejects `-x`. Always check afterwards: SSH-launched engines
  survive sloppy kills, and two engines fight over the display.
- **Several files off the device**: one `scp` with several sources on the device brings only the
  first. Use tar over SSH: `ssh spruce@<device> 'cd <dir> && tar cf - <files>' | tar xf -`.
- **busybox has no `timeout` and no `nohup`.** Use `setsid` with all three fds redirected, or
  SSH hangs waiting on the pipes.
- **The screen is seen over SSH only by dumping the framebuffer**: the page shown is at the
  pan's line (`/sys/class/graphics/fb0/pan`, `x,y`; EGL flips between 0 and 720), so
  `dd if=/dev/fb0 bs=5120 skip=<y> count=720`, gzip it to copy, and decode it as BGRA
  (`magick -size 1280x720 -depth 8 bgra:frame -alpha off`). `SHOT=<seconds>` keeps the first
  page mid-profile: while a game runs, both pages hold whole frames a frame apart (five dumps on
  Liberty Island apart in 0.04% to 0.05% of their pixels), and the pan flips during the
  second's read, so the shot is the frame shown or its neighbour; a still screen's pages are
  the same.

# Port: TrimUI Smart Pro (spruceOS)

An Allwinner A133 handheld (4× Cortex-A53, PowerVR GE8300) running spruceOS. The port is
cross-built from a PC against the device's own libraries, and the performance work is measured
here. The launcher is the app the spruceOS menu starts.

## Status

The launcher and the engine run on the device, and the game plays on Vulkan or OpenGL ES (the
Video tab's choice), at 853×480 by default. It is short of
[the ~20 FPS target](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/AGENTS.md#decided):
[Performance](#performance) has the numbers, and [where a frame goes](#where-a-frame-goes) the
work left.

## Build and run

```sh
# from Port Ex Machina
scripts/dx.sh fetch                        # once: the engine, the launcher's branches, the RE
scripts/dx.sh deps   trimui-smartpro       # toolchains + sysroot (the device must be awake)
scripts/dx.sh deploy trimui-smartpro       # build, stage, send; --no-engine, --run to start it over SSH
```

`deploy` builds (incrementally) and stages the current tree first, warning when the engine has
uncommitted changes (the profiling hooks, say). It sends only the files whose checksum differs,
keeps the device's copies of those in `.prev-<date-time>` (the device's clock) in the app
directory, and checksums every file there afterwards. Nothing is ever deleted on the device:
copying a previous copy back is a rollback (newest last; older hand-made ones are `.prev-<date>`). `--no-engine` leaves the engine's three files out.

The app lives in `/mnt/SDCARD/App/DeusEx`, the game data in `/mnt/SDCARD/Roms/PORTS/DeusEx`
(all 38 `.u` packages). `launcher.ini` belongs to the device's owner: `deploy` installs it only
when missing (from `launcher.ini.default`), touches `home/` only to seed a missing
`Settings.json` (from `engine-settings.json.default`), and never touches `run-game.log`.

**Access.** `spruce@192.168.1.211`, password `happygaming`: the stock firmware password, a
default in `port.sh` on purpose; SSH is root. Override with `DEVICE`, `DEVICE_USER`,
`DEVICE_PASS`, `DEVICE_APPDIR`. Asleep, the device drops off the network (no ping, SSH times
out): ask its owner to wake it.

### Diagnosing

```sh
./dxl-cli --dry-run              # decision, game config, Settings.json, renderers; writes nothing
./dxl-cli --dry-run --probe      # ...with the renderer list checked against this GPU
./dxl-cli --probe                # just the GPU: Vulkan device, OpenGL ES version
```

Run them in the app directory with `LD_LIBRARY_PATH=/usr/trimui/lib:/usr/lib:/lib`. The
launcher logs to `<GameDir>/System/DeusExLauncher.log`, `run-game.sh` to `run-game.log` in the
app directory (engine output, exit status, CPU mode), and the engine to
`home/.config/SurrealEngine/SE-Log-LastRun.txt` there. The System tab shows the engine log and
`run-game.log`'s last run.

## What differs from linux-x86_64

| File | What it is |
|---|---|
| `port.cmake` | links the device's vendor SDL2 from the sysroot; glibc ceiling 2.33 |
| `toolchain-c.cmake`, `toolchain-cxx.cmake` | Bootlin GCC 9.3 (launcher, C11) and GCC 10.3 (engine, C++20) |
| `engine.cmake` | the engine for this device: SDL2 only, no X11/Wayland |
| `port.sh` | the device's address; `deps`, `deploy` over SSH, `profile` on the device |
| `fetch-sysroot.sh` | the link sysroot: libraries off the device, pinned headers |
| `target.c` | the device profile: fonts, spruceOS CPU modes, the pad note |
| `packaging/` | the spruceOS app (`config.json`, `launch.sh`, icon), `port-hooks.sh` (vendor library path, CPU mode), `launcher.ini`, `renderers.ini`, `engine-settings.json.default` |
| `tools/profile-map.sh` | frame-time profile of one map, run on the device |

### The toolchains

The device runs glibc 2.33. glibc 2.34 folded `libpthread`/`libdl` into `libc` and re-versioned
the startup symbols, so any toolchain built against 2.34 or later emits
`__libc_start_main@GLIBC_2.34` from `crt1.o`: the binary does not load, whatever our code calls.
Hence two Bootlin toolchains:

| Toolchain | gcc | glibc | Builds |
| --- | --- | --- | --- |
| `aarch64--glibc--stable-2020.08-1` | 9.3 | 2.31 | the launcher (C11); emits only `GLIBC_2.17` |
| `aarch64--glibc--bleeding-edge-2021.05-1` | 10.3 | 2.33 | the engine (C++20, beyond gcc 9.3) |

One modern compiler aimed at the old sysroot does not work: the sysroot's `libc.so` linker
script hardcodes absolute `/lib/...` paths, which resolve to the host's libraries.

The engine links `libstdc++` and `libgcc` statically (~1 MB): the device's `libstdc++.so.6.0.28`
(`GLIBCXX_3.4.28`) would serve, but static linking removes the question.
`scripts/check-abi.sh --max 2.33` runs on every build of both and fails on any reference above
2.33: the launcher's build runs it because `port.cmake` sets `DXL_PORT_GLIBC_MAX`, and
`scripts/engine.sh build` runs it on the engine.

### The device's own SDL2

The vendor SDL2 in `/usr/trimui/lib` is the only display path: no X11, no Wayland, no desktop
GL. Its `mali` video driver, absent upstream, is an EGL/fbdev driver: OpenGL ES draws through
it, and it wires Vulkan surface creation to the PowerVR implementation
(`SDL_Vulkan_CreateSurface` works, via `VK_KHR_display`). An upstream KMSDRM build would have no
window system to attach to: the PowerVR stack ships only `libpvrNULL_WSEGL.so`.

[`fetch-sysroot.sh`](fetch-sysroot.sh) (run by `deps`) copies the device's own libraries into
`deps/sysroots/trimui-smartpro/lib` and fetches their headers, pinned (SDL 2.30.8, SDL_ttf
2.0.15, exact Arch Linux packages). Vulkan is not linked: SurrealGPU loads the loader through
volk, and the launcher's GPU probe `dlopen`s it and EGL, so a device without either still
starts the launcher.

### CPU mode

The spruceOS menu leaves the handheld in power-save ([as measured](#the-device-as-measured)),
which costs the game about a third of its frame rate. The Video tab offers spruceOS's own modes
(`target.c`), kept in `launcher.ini` `CpuMode`: Smart, Performance (all four cores at 1.8 GHz)
and Overclock (2.0 GHz), the default: the game is CPU-bound. `packaging/port-hooks.sh`
applies the mode with the spruceOS helpers its Ports launcher uses, just before the engine
starts, and restores the exact previous state (online cores, governor, min/max) when it exits.

In `port-hooks.sh` the helpers are sourced in subshells only: `helperFunctions.sh` exports its
own `LD_LIBRARY_PATH`, which hides `libSurrealVideo.so` from the engine. (`launch.sh` sources
them directly, as spruceOS's apps do, then puts its own library path first; `run-game.sh` puts
the app's first.)

### Engine settings

The launcher writes the app directory's `home/.config/SurrealEngine/Settings.json` before a
launch whenever it is missing, incomplete or changed (`run-game.sh` pins `HOME` there), filling
missing fields from `engine-settings.json.default`. The non-negotiable entry is
`Antialias: Off`: the engine defaults to 4x MSAA, and the GE8300's resolve turns partially
covered pixels into speckle. The launcher locks it off on any PowerVR, and the default says
`Off` too. VSync is off: below the panel's 60 Hz, vsync would hold the game to 30 or 20.
Distant AI (`Performance.AiLevelOfDetail`) is on
([what it does](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#settings-the-launcher-exposes)).
`Performance.RenderScale` 0.6666667 draws at 853×480 unless the Video tab's Resolution asks for
960×540 or the panel's 1280×720 (under Vulkan only: a
[known defect](../../docs/LAUNCHER.md#known-defects)).

## The device, as measured

Probed over SSH, not assumed; Port Ex Machina's `tools/probes/probe-sdl.c` and `dxl-cli --probe`
reproduce it.

| | |
| --- | --- |
| Device | TrimUI Smart Pro (`hwserial TG5040`), spruceOS `PLATFORM=SmartPro` |
| SoC | Allwinner A133 `sun50iw10p1`, 4× Cortex-A53; 3 GB RAM (`free`) |
| CPU modes | the spruceOS menu: **power-save, cores 0 and 3 only, `conservative`, 408 MHz–1.49 GHz**. `set_performance`: all four cores, `performance`, 1.8 GHz; `set_overclock`: 2.0 GHz (`spruce/scripts/platform/SmartPro.cfg`) |
| OS | TinaLinux "Neptune", kernel 4.9.191, **glibc 2.33**, busybox 1.36.1 |
| SDL | vendor 2.30.8 in `/usr/trimui/lib`; video driver **`mali`** (`SDL_malivideo.c`) |
| Surface | 1280×720 @60Hz, `SDL_PIXELFORMAT_RGBX8888`, fullscreen |
| SDL renderer | **`opengles2`**, accelerated + vsync, max texture 8192² |
| GPU APIs | Vulkan **1.3.225** on the PowerVR Rogue GE8300; **OpenGL ES 3.2** (`build 1.19@6345021`) through EGL; no desktop OpenGL. Detection takes ~0.5 s. Vulkan: no `VK_EXT_descriptor_indexing`; BC1–5, RGB8 and RGBA32F not sampled or filtered as the engine needs (`probe-vulkan-caps.c`, `probe-texture-formats.c`). OpenGL ES: no S3TC, float-linear filtering, anisotropy or `glBufferStorage`; `GL_MIRROR_CLAMP_TO_EDGE` rejected (`probe-gles-sampler.c`) |
| Pad | `"Xbox 360 Controller"`, GUID `0300a3845e0400008e02000014010000`, with SDL_GameController's built-in mapping |
| Pad controls | A B X Y, L1 R1, SELECT START MENU (MENU is spruceOS's), d-pad (a hat), two sticks. **L2/R2 are digital**, though mapped to axes `a2`/`a5`. **No L3/R3**: the sticks do not click, though the mapping lists `leftstick:b9`/`rightstick:b10` |
| Fonts | `/usr/trimui/res/regular.ttf`, `full.ttf`; `/mnt/SDCARD/spruce/Font Files/Noto.ttf` |
| Storage | SD is **exFAT**: case-insensitive, no meaningful permission bits |
| Profiling | no `perf` (`CONFIG_PERF_EVENTS` off); CPU-time timers fire only on the scheduler tick, every 4 ms |

### What is not available

The original x86 Windows `Core.dll`/`Engine.dll`/`DeusEx.dll` cannot run here: the device has
no box64, box86, wine or qemu, and box64's own notes record Deus Ex under Wine crashing before
the menu on far stronger hardware.

### Probes

Port Ex Machina's `tools/probes/` are small single-purpose programs that answer questions about
a device
([what each answers](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#device-probes)).
They are not part of any build: each is one compile against this port's sysroot, run over SSH.

```sh
# from the parent folder of Port Ex Machina's repositories
TC=deps/toolchains/aarch64--glibc--stable-2020.08-1/bin/aarch64-linux-gcc
SYS=deps/sysroots/trimui-smartpro
$TC -O2 -mcpu=cortex-a53 -std=c11 -I $SYS/include -I $SYS/include/SDL2 \
    port-ex-machina/tools/probes/probe-vulkan-caps.c -o /tmp/probe \
    -L $SYS/lib -lvulkan -Wl,-rpath-link,$SYS/lib
deusex-launcher/trimui-smartpro/scripts/check-abi.sh --max 2.33 /tmp/probe
# scp to the device, then: LD_LIBRARY_PATH=/usr/trimui/lib:/usr/lib:/lib ./probe
```

Pause the spruceOS menu before `probe-sdl.c --pad` ([gotchas](#gotchas)). `dxl-shots` from a
host build configured with `-DDXL_PROFILE=trimui-smartpro` renders every launcher screen as this
device shows it, in a desktop font.

## Verified

| Check on the device | Result |
| --- | --- |
| glibc ABI | the launcher's binaries reference `GLIBC_2.17` only; the engine ≤ 2.33 |
| `dxl-cli --probe`, `--dry-run` | the GPU [as measured](#the-device-as-measured), in 0.5 s; a dry run writes nothing (checksums unchanged) |
| CPU mode | Performance (cores 0–3, 1.8 GHz) or Overclock (2.0 GHz) while the engine runs; power-save restored after |
| Home screen on the panel | renders with the device font; detected `X360 Controller`; recognised the retired layout |
| Pad in game | moving, looking and firing work |
| `scripts/dx.sh deploy` | sends only the changed files, keeps the previous copies; every file's checksum matches |

## Performance

Liberty Island's level start (`01_NYC_UNATCOIsland.dx`) unless a row names another map;
Overclock, Distant AI on; milliseconds a frame, averaged over 60 frames.

| Renderer, resolution | FPS | Frame | Tick | Render CPU | GPU wait |
| --- | --- | --- | --- | --- | --- |
| Vulkan, 1280×720 (native) | 8.2 | ~121 | ~30 | ~88 | ~30 |
| Vulkan, 853×480 | 11.5 | ~87 | ~29 | ~54 | ~2 |
| OpenGL ES, 1280×720 | 8.6 (8.5–8.8) | ~115 | ~30 | ~84 | – |
| OpenGL ES, 853×480 | 10.0 | ~100 | ~23 | ~76 | – |
| UNATCO HQ (indoors), Vulkan, 853×480 | ~32 | | | | |
| Battery Park (outdoors), Vulkan, 853×480 | ~16 | | | | |

The render CPU includes the GPU wait, the lightmaps and the texture uploads. The hooks measure
only the Vulkan device's GPU wait; in GL the driver's time lands inside the render CPU.
Measured with the profiling hooks at engine `a783f0a` (Vulkan, and the two other maps) and
`0c8e99b` (OpenGL ES).

### Where a frame goes

Vulkan at native 1280×720 at the level start, in ms, by the frame-time hooks and the device's CPU samples, with
what is left in each area. The GPU draws the previous frame while the tick runs (engine patch
0004). At native resolution the tick is the shorter, so **a frame is about the GPU's time plus
the render CPU**: render-CPU savings count in full, and the tick does not move the frame until
the GPU's time comes down. At 853×480 the frame is the CPU's, and both count. ~20 FPS (~50 ms)
at native therefore also needs the GPU's ~63 under ~50, and the script VM several times faster.

- **Game tick ~30** (~35 with the detail hooks), almost all `ULevel::TickActor` over ~2,500
  actors:
  - **script VM ~15** by the samples (`Frame::Run` with the natives it calls; ~12 by the hooks'
    count, ~9.5 of it in the tick), ~2,000 calls a frame. The natives take ~8.5: the weapons'
    and shadows' `Tick`, `CheckEnemyPresence` and `CalculateAccuracy` the largest,
    `FindPathToward` ~0.5; `ScriptedPawn.CheckEnemyPresence` is the costliest script function.
    The interpreter's own ~6 (`Frame::Run`, `ExpressionEvaluator`, `Frame::Call` self time) is
    mostly the Cortex-A53 waiting on memory for each expression node. Left: only a denser,
    compiled form of each function's code would change that, a rewrite of the evaluator's core;
    even at no cost of its own, the VM's time would fall by a little under half. Smaller: calls
    without an `ExpressionValue` per argument. How the original does both:
    [the script interpreter](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/core-dll.md#the-script-interpreter).
  - physics ~6 (`TickPhysics`), mostly walking pawns: `TryStepToGround` ~1.9, `TryMove` ~2.7;
  - the pawns' own tick ~12 (`UPawn::Tick`), sight checks (`CanSee`, `FastTrace`) ~0.7 of it;
  - collision traces ~2 of own time (`TraceAABBModel::Trace` ~1.8 self; the sight rays'
    polygon tests, `NodeRayIntersect` ~1, most of the rest). `TraceTexture` ~0.2: the laser
    tripwires' `CalcTrace` traces each beam 5,000 units per reflection point every tick, and the
    player's floor and wall materials take two traces a frame;
  - per-actor work around the scripts ~9: `ULevel::TickActor` ~4 self, `UActor::Tick` ~2.3,
    animation ~1.3, `CheckPendingTouch` ~1.1, `PathNode`'s tick ~4 over 1,000 nodes;
    `IsEventEnabled`, asking whether to send each actor `Tick`, ~0.1. Left: all of it but
    `IsEventEnabled`;
  - Distant AI: ~12 pawns a frame skip their thinking at distance.
- **Render CPU ~52** besides waits, lightmaps and uploads:
  - visibility ~20, the largest render item: ~3,900 box tests (~3.9) and ~2,400 surface tests
    (~7.7) a frame against `BspClipper`'s occlusion grid, portals ~1.7, actor set-up ~1.5. By
    function: `BspClipper::DrawSpan` ~3.3, `DrawTriangle`/`DrawClippedTriangle` ~3.8, the BSP
    walk (`ProcessNode`/`ProcessNodeSurface` ~4.8, cache misses), `IsAABBVisible` ~1.3;
  - actor meshes ~7.5 for ~40 in view: per-vertex work (`DrawLodMeshFaceDX` ~3.7, the vertex
    lighting `GetVertexLight` ~1.1) and the device's set-up per run of faces. Left: the
    per-vertex work itself;
  - BSP surfaces ~8 for ~600 nodes, mostly each surface's lightmap lookup
    (`LightSystem::GetLightmap` ~3.7 self);
  - translucent 2.6, sky portal 3.4, BSP set-up (`bsp-info`) 3.5, end of frame (`unlock`) 2.3,
    `PostRenderFlash` (script) 1.6, the rest ~1.3.
- **Lightmaps ~3.7, texture uploads ~2.** The `BarrelFire`, a dynamic light with the fire waver
  effect, rebuilds 6 lightmaps every frame (~94,000 texels, ~1,800 in its radius). Each goes to
  the GPU whole, converted from float in NEON (the GE8300 cannot filter RGBA32F). Left:
  re-uploading only the rows a light changed; byte lightmaps (the fork's are floats, converted
  on the CPU); each surface's lightmap lookup
  ([lighting](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/NATIVES.md#lighting)).
- **GPU ~63** at native: input, tick and view (~33) plus the render's wait (~30). CPU and GPU
  compete for the SoC's shared memory. At 853×480 the wait is ~2.
- **`view+audio`** is mostly `USurrealAudioDevice::StartAmbience` reading every actor's
  `AmbientSound` each frame (~2.5). The original does the same scan every frame
  ([each frame](https://github.com/JuggyMcNutty/dx-reverse-info/blob/main/galaxy-dll.md#each-frame)):
  nothing to port.

**OpenGL ES** is level with Vulkan at native, or a little ahead: Vulkan waits on the GPU, and
the GL driver's time lands in the render CPU. At 853×480 it trails by the GL driver's
per-draw-call cost over the frame's ~700 calls (state, program and texture binds included); the
engine's own sections have the same shape as Vulkan's. The GL device's workarounds for this
driver: ENGINE.md's
[rendering](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#rendering) and, for
fullscreen,
[running on our devices](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#running-on-our-devices).

### Measuring

Build the engine with the frame-time hooks (`vibe/tools/perf/perf.sh on` applies VibeEngine's
`vibe/tools/perf/perf-instrumentation.patch`;
[the profiling hooks](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#the-profiling-hooks)),
deploy, and run `scripts/dx.sh profile trimui-smartpro [seconds] [label] [cpu] [turn] [map]`.
That runs [`tools/profile-map.sh`](tools/profile-map.sh) on the device (its header documents the
arguments, `SHOT`, `SAMPLE` and `SURREAL_PERF_DETAIL`) and brings back
`build/trimui-smartpro/profile/perf-<label>.log`: the frame split into input, tick, render CPU,
GPU wait, lightmaps and texture uploads, the render CPU by section, the visibility pass by part,
and the time under script calls.

The script applies `launcher.ini`'s CPU mode through `port-hooks.sh`, as a launch does. It uses
the device's own `Settings.json` and switches nothing: check the log's `settings:` line before
taking a run as native (one at 0.6666667 is an 853×480 run, whatever its label). For another
resolution, set `RenderScale` there for the run and put the owner's value back after.

`SAMPLE=1` also brings back samples of the main thread's CPU (4 ms apart) and the engine that
made them, for
[`vibe/tools/perf/sample-report.py`](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/tools/perf/sample-report.py):

```sh
# from the parent folder of Port Ex Machina's repositories
SAMPLE=1 port-ex-machina/scripts/dx.sh profile trimui-smartpro 90 mylabel
NM=deps/toolchains/aarch64--glibc--bleeding-edge-2021.05-1/bin/aarch64-linux-nm \
    VibeEngine/vibe/tools/perf/sample-report.py build/trimui-smartpro/profile/samples-mylabel \
    build/trimui-smartpro/profile/samples-mylabel.engine --sysroot deps/sysroots/trimui-smartpro \
    --root ULevel::Tick
```

A profile stops the engine with SIGKILL, which leaves `Running.ini` like any crash
([running it](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#running-it)).

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
- **busybox has no `timeout` and no `nohup`.** Use `setsid` with all three fds redirected, or
  SSH hangs waiting on the pipes.
- **The screen is seen over SSH only by dumping the framebuffer** (`SHOT=<seconds>` does it
  mid-profile): `cat /dev/fb0 > /tmp/fb.raw` (64 MB), gzip it to copy, and decode the first
  1280×720 as BGRA (`magick -size 1280x720 -depth 8 bgra:frame -alpha off`).
- **A missing `Save/` directory is fatal to the engine** (`directory iterator cannot open
  directory`). A fresh install has it empty, so a `tar` that lists only populated directories
  loses it.

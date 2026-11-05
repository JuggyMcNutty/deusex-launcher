# Port: Linux aarch64

DXLauncher and the engine for aarch64 devices running an ordinary distro:
Raspberry Pi OS, Debian, Ubuntu, and handheld images with a normal glibc
userland, Mesa and a desktop.

## Status

The launcher is DXLauncher, linux-x86_64's
([`docs/LAUNCHER.md`](../../docs/LAUNCHER.md)). It and the engine
cross-build warning-free under GCC 13.2, needing glibc 2.27 at most: the
launcher no library but the C library's, the engine the device's X11,
OpenAL, EGL and GL, D-Bus, GLib, fontconfig and FreeType. Untried on an
aarch64 device.

## Build and run

Two ways:

- **Natively, on the device.** The preset's toolchain file steps aside on an
  aarch64 host, so the launcher *and* the engine build against the device's
  own libraries: the launcher's SDL3 and SDL3_ttf 3.2 and libxmp 4.4 or
  newer, the distro's, else built from `cmake/deps.lock`'s sources, and the
  engine's SDL2, the distro's; untried on a device.
- **Cross, from a PC**, as GitHub's CI builds it: the launcher and the
  engine, the engine's SDL2 built in and its own Wayland window left out.

```sh
# cross, from a PC
scripts/dx.sh deps  linux-aarch64       # the toolchain, the sources, the sysroot, the engine's SDL2
scripts/dx.sh build linux-aarch64       # launcher and engine
scripts/dx.sh stage linux-aarch64       # build/linux-aarch64/app: copy it to the device

# natively, on the device
scripts/dx.sh fetch                     # once: the engine, the launcher's branches, the RE
scripts/dx.sh deps  linux-aarch64       # SDL3, SDL3_ttf and libxmp, the distro's or their sources
scripts/dx.sh build linux-aarch64       # launcher and engine
scripts/dx.sh stage linux-aarch64 && scripts/dx.sh run linux-aarch64 [--fresh]
```

Natively, `run` starts the staged `deusex-launcher` where it is staged, on
the `gamefiles/` beside the repositories, as linux-x86_64's does; `--fresh`
gives it a scratch home, `build/linux-aarch64/fresh-home`.

## What differs from linux-x86_64

| File | What it is |
|---|---|
| `port.cmake` | a cross build has SDL3, FreeType, SDL3_ttf and libxmp-lite built in from `cmake/deps.lock`'s sources, SDL3's window and audio backends built against `deps/sysroots/linux-aarch64`'s headers and opened at run time, and a 2.28 ceiling, which `CMakeLists.txt` checks the programs against after each build (`scripts/check-abi.sh`); a native build none of it, the device's own libraries or their sources, with no ceiling |
| `toolchain-c.cmake` | Godot's Buildroot SDK, GCC 13.2 / glibc 2.28, as linux-x86_64's, finding the sysroot's libraries in their multiarch folders; does nothing on an aarch64 host |
| `toolchain-cxx.cmake` | the engine's: the same SDK, linking the SDK's own X11, Xi, Xext, D-Bus, GLib, fontconfig and FreeType, and the sysroot's EGL, GL and OpenAL Soft, buster's, at glibc 2.28; libstdc++ and libgcc static; does nothing on an aarch64 host |
| `engine.cmake` | the engine: SDL2's and X11's windows, SDL2's the one the launcher asks for at run time; cross-built with `toolchain-cxx.cmake` and without the engine's own Wayland window, which needs waylandpp, as linux-x86_64's; natively with it |
| `port.sh` | `deps` fetches the toolchain and the sources, makes the sysroot from Debian bookworm's arm64 packages, checked against no hash (bugs.md LPORT.6) -- the headers and libraries of X11, Wayland, libdecor, ALSA, PulseAudio, PipeWire, udev, D-Bus, libusb, EGL, GL, GLES, DRM and GBM, and Vulkan's headers -- with buster's glvnd, EGL, GL and OpenAL Soft over them and the SDK's pkg-config files in `deps/sysroots/linux-aarch64/sdk-pkgconfig`, and builds the engine's SDL2 into `deps/prefix/linux-aarch64`, static, its backends opened at run time; `run` works natively |
| the shared files touched | `src/app/run.c` (`DXE_DISPLAY_BACKEND=SDL2` for the engine's window, and its comment), `src/core/keys.{c,h}` (their comments name DXWidgets' SDL2 window, whose key names match SDL3's), `CMakePresets.json` (the preset), `cmake/deps.lock`, `ports/linux-x86_64/port.cmake` (the host build's: native, no ceiling), `THIRD-PARTY.md` (the engine's SDL2): a cherry-pick from linux-x86_64 may meet them |

The glibc 2.28 ceiling keeps the cross-built programs off newer glibc
symbols, so they load on a distro with glibc 2.28 or later
([why 2.28](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/AGENTS.md#decided));
the window opens the device's own X11 or Wayland, and its ALSA,
PulseAudio or PipeWire, at run time. The engine, cross-built, holds to the
same ceiling: what it links is the SDK's own or buster's, whose glibc is
2.28, and at run time the device's.

A GPU without descriptor indexing or some texture formats takes the Vulkan
device's other paths
([rendering](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/ENGINE.md#rendering));
a Mali or Adreno under Mesa may not need them. Port Ex Machina's
[device probes](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#device-probes)
answer that before the first engine run.

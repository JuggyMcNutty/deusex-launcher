# Port: Linux aarch64

DXLauncher and the engine for aarch64 devices running an ordinary distro:
Raspberry Pi OS, Debian, Ubuntu, and handheld images with a normal glibc
userland, Mesa and a desktop.

## Status

The launcher is DXLauncher, linux-x86_64's
([`docs/LAUNCHER.md`](../../docs/LAUNCHER.md)). It cross-builds
warning-free under GCC 9.3, needing glibc 2.29 at most and no library but
the C library's. Untried on an aarch64 device.

## Build and run

Two ways:

- **Natively, on the device.** The preset's toolchain file steps aside on an
  aarch64 host, so the launcher *and* the engine build against the device's
  own libraries, as on linux-x86_64: SDL3 and SDL3_ttf 3.2 or newer, the
  distro's, else built from `cmake/deps.lock`'s sources; untried on a
  device.
- **Cross, from a PC** -- the launcher only.

```sh
# cross, from a PC
scripts/dx.sh deps  linux-aarch64       # the toolchain, the window's sources, the sysroot
scripts/dx.sh build linux-aarch64       # the launcher
scripts/dx.sh stage linux-aarch64       # build/linux-aarch64/app: copy it to the device

# natively, on the device
scripts/dx.sh fetch                     # once: the engine, the launcher's branches, the RE
scripts/dx.sh deps  linux-aarch64       # SDL3 and SDL3_ttf, the distro's or their sources
scripts/dx.sh build linux-aarch64       # launcher and engine
scripts/dx.sh stage linux-aarch64 && scripts/dx.sh run linux-aarch64 [--fresh]
```

Natively, `run` starts the staged `deusex-launcher` where it is staged, on
the `gamefiles/` beside the repositories, as linux-x86_64's does; `--fresh`
gives it a scratch home, `build/linux-aarch64/fresh-home`. A cross-built
app has no engine: one built on the device goes beside the launcher, or
`DXL_ENGINE=<path>` names it
([where everything lives](../../docs/LAUNCHER.md#where-everything-lives)).

## What differs from linux-x86_64

| File | What it is |
|---|---|
| `port.cmake` | a cross build has SDL3, FreeType and SDL3_ttf built in from `cmake/deps.lock`'s sources, SDL3's window and audio backends built against `deps/sysroots/linux-aarch64`'s headers and opened at run time, and a 2.31 ceiling, which `CMakeLists.txt` checks the programs against after each build (`scripts/check-abi.sh`); a native build is linux-x86_64's |
| `toolchain-c.cmake` | Bootlin GCC 9.3 / glibc 2.31, finding the sysroot's libraries in their multiarch folders; does nothing on an aarch64 host |
| `engine.cmake` | the engine, native builds only |
| `port.sh` | `deps` fetches the toolchain and the window's sources, and makes the sysroot from Debian bookworm's arm64 packages: the headers and libraries of X11, Wayland, libdecor, ALSA, PulseAudio, PipeWire, udev, D-Bus, libusb, EGL, GL, GLES, DRM and GBM, and Vulkan's headers; `run` works natively |

The glibc 2.31 ceiling keeps the cross-built programs off newer glibc
symbols, so they load on a distro with glibc 2.31 or later, as Debian 11 or
Ubuntu 20.04; the window opens the device's own X11 or Wayland, and its
ALSA, PulseAudio or PipeWire, at run time. Cross-building the engine needs
the C++20 toolchain and a sysroot with the engine's libraries (SDL2,
OpenAL, zlib, ...).

A GPU without descriptor indexing or some texture formats takes the Vulkan
device's other paths
([rendering](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#rendering));
a Mali or Adreno under Mesa may not need them. Port Ex Machina's
[device probes](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#device-probes)
answer that before the first engine run.

# Port: Linux aarch64

`main`'s launcher and the engine for aarch64 devices running an ordinary
distro: Raspberry Pi OS, Debian, Ubuntu, and handheld images with a normal
glibc userland, Mesa and a desktop-style SDL2.

## Status

The launcher is `main`'s for now
([`main`'s README](https://github.com/JuggyMcNutty/deusex-launcher/blob/main/README.md));
the desktop's own is planned
([to do](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/AGENTS.md#to-do)).
It cross-builds warning-free, needing glibc 2.27 at most. Untried on an
aarch64 device.

## Build and run

Two ways:

- **Natively, on the device.** The preset's toolchain file steps aside on an
  aarch64 host, so the launcher *and* the engine build against the device's own
  libraries, as on linux-x86_64; untried on a device.
- **Cross, from a PC** -- the launcher only.

```sh
# cross, from a PC
scripts/dx.sh deps  linux-aarch64
scripts/dx.sh build linux-aarch64       # the launcher
scripts/dx.sh stage linux-aarch64       # build/linux-aarch64/app: copy it to the device

# natively, on the device
scripts/dx.sh fetch                     # once: the engine, the launcher's branches, the RE
scripts/dx.sh deps  linux-aarch64       # checks for SDL2 and SDL2_ttf
scripts/dx.sh build linux-aarch64       # launcher and engine
scripts/dx.sh stage linux-aarch64 && scripts/dx.sh run linux-aarch64
```

Natively, `run` puts the staged app into the `System/` of the `gamefiles/`
beside the repositories and starts `System/DeusEx` there, as linux-x86_64's
does
([its README](https://github.com/JuggyMcNutty/deusex-launcher/blob/linux-x86_64/ports/linux-x86_64/README.md#build-and-run)).

## What differs from linux-x86_64

| File | What it is |
|---|---|
| `port.cmake` | a cross build links SDL2 from `deps/sysroots/linux-aarch64` with a 2.31 ceiling, which `CMakeLists.txt` checks `DeusEx` against after each build (`scripts/check-abi.sh`); a native build uses the system's |
| `toolchain-c.cmake` | Bootlin GCC 9.3 / glibc 2.31; does nothing on an aarch64 host |
| `engine.cmake` | the engine, native builds only |
| `port.sh` | `deps` fetches the toolchain and builds the sysroot from Debian bookworm's arm64 packages (`libsdl2`, `libsdl2-ttf`); `run` works natively, as linux-x86_64's |

The glibc 2.31 ceiling keeps the cross-built launcher off newer glibc symbols;
it also needs SDL2 2.0.16 and SDL2_ttf 2.0.18 or later, as in Debian 12 or
Ubuntu 22.04. Cross-building the engine needs the C++20 toolchain and a sysroot
with the engine's libraries (SDL2, OpenAL, zlib, ...).

A GPU without descriptor indexing or some texture formats takes the Vulkan
device's other paths
([rendering](https://github.com/JuggyMcNutty/VibeEngine/blob/deusex/vibe/docs/ENGINE.md#rendering));
a Mali or Adreno under Mesa may not need them. Port Ex Machina's
[device probes](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#device-probes)
answer that before the first engine run.

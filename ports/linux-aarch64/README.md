# Port: Linux aarch64

linux-x86_64 for aarch64 devices running an ordinary distro: Raspberry Pi OS,
Debian, Ubuntu, and handheld images with a normal glibc userland, Mesa and a
desktop-style SDL2. The same generic device profile and `ports/common/packaging`.

## Status

The launcher cross-builds warning-free, with `GLIBC_2.17` symbols only. Not yet
run on an aarch64 device.

## Build and run

Two ways:

- **Natively, on the device.** The preset's toolchain file steps aside on an
  aarch64 host, so the launcher *and* the engine build against the device's own
  libraries, exactly as on linux-x86_64.
- **Cross, from a PC** -- the launcher only, for now.

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

## What differs from linux-x86_64

| File | What it is |
|---|---|
| `port.cmake` | a cross build links SDL2 from `deps/sysroots/linux-aarch64` with a 2.31 ceiling; a native build uses the system's |
| `toolchain-c.cmake` | Bootlin GCC 9.3 / glibc 2.31 (shared with the Smart Pro); does nothing on an aarch64 host |
| `engine.cmake` | the engine, native builds only |
| `port.sh` | `deps` fetches the toolchain and builds the sysroot from Debian bookworm's arm64 packages (`libsdl2`, `libsdl2-ttf`, and the Vulkan and EGL headers the GPU probe compiles against); `run` works natively |

The glibc 2.31 ceiling means the cross-built launcher loads on any distro from
2020 on. Cross-building the engine needs the C++20 toolchain and a sysroot with
the engine's libraries (SDL2, OpenAL, zlib, ...): the Smart Pro's `port.sh` and
`fetch-sysroot.sh`
([on its branch](https://github.com/JuggyMcNutty/deusex-launcher/tree/trimui-smartpro/ports/trimui-smartpro))
show the shape, with Debian packages in place of the device's own.

A device whose GPU lacks descriptor indexing or some texture formats is what
engine patch 0002 exists for (the Smart Pro's PowerVR); a Mali or Adreno under
Mesa may not need it. Port Ex Machina's probes, `tools/probes/`, answer that before the
first engine run -- see [`docs/PORTING.md`](https://github.com/JuggyMcNutty/port-ex-machina/blob/main/docs/PORTING.md#device-probes).

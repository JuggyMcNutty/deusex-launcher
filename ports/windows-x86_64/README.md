# Port: Windows x86_64

DXLauncher and the engine for Windows 10 (1903) or newer on x86_64, built
on the Linux PC the other ports are built on, by llvm-mingw, and tried
there under Proton's wine: no Windows machine is in the loop.

## Status

The launcher is DXLauncher, linux-x86_64's on Windows' own calls
([`docs/LAUNCHER.md`](../../docs/LAUNCHER.md)), the engine DXEngine's
Windows build beside it. Both build warning-free and play the game under
Wine; untried on Windows itself ([verified](#verified)).

## Build and run

On the build machine, with the tools linux-x86_64's build needs:

```sh
scripts/dx.sh deps  windows-x86_64   # llvm-mingw, the window's sources, the engine's SDL3, the Wine prefix
scripts/dx.sh build windows-x86_64   # launcher, then engine
scripts/dx.sh test  windows-x86_64   # the host build's tests, then Windows' under Wine
scripts/dx.sh stage windows-x86_64   # build/windows-x86_64/app, and the same as a zip beside it
scripts/dx.sh run   windows-x86_64 [--fresh] [-- <the launcher's words>]
```

`run` starts the staged `deusex-launcher.exe` under Proton's wine, in the
port's own prefix (`build/windows-x86_64/prefix`), on the `gamefiles/`
beside the repositories by Wine's name for them (`Z:\...`): nothing is
copied into the game's folder. `--fresh` gives it a scratch home instead,
`build/windows-x86_64/fresh-home`, emptied first. On Windows, the zip's one
folder, `DXLauncher`, goes wherever the user may write: nothing is
installed, and About's button puts it in the Start menu
([the app menu](../../docs/LAUNCHER.md#the-app-menu)).

## What differs from linux-x86_64

The port's files, each Windows':

| File | What it is |
|---|---|
| `toolchain.cmake` | llvm-mingw 20261006 (Clang and LLD 23.1.3, mingw-w64 on the UCRT), its pin and SHA256 in `port.sh`, linking statically, so the programs need none of its DLLs, with Windows 10 as their PE headers' OS and subsystem versions; the tests' emulator Port Ex Machina's `scripts/wine.sh` |
| `port.cmake` | Windows 10 1903 the floor (`DXL_PORT_WINDOWS_MIN`): Windows 10 by the programs' PE headers, which `scripts/check-abi.sh --windows` holds them to after each build, with the DLLs they may import, by name (not the functions imported; 1903 is the manifest's UTF-8 code page): Windows' own, the UCRT's and the engine's `OpenAL32.dll`; SDL3, FreeType, SDL3_ttf and libxmp-lite built in from `cmake/deps.lock`'s sources; each program's resources (`win32/`) |
| `engine.cmake` | the engine with its SDL3 window, static, the one the launcher asks for, and Windows' own beside it; its Vulkan, OpenGL, D3D11 and software devices; OpenAL Soft as `OpenAL32.dll` |
| `port.sh` | `deps`: llvm-mingw, checked by a C and a C++ program that must import none of its DLLs; the window's sources; the engine's SDL3, the launcher's version, static, into `deps/prefix/windows-x86_64`; the Wine prefix. `stage` adds the notices the programs' runtimes and OpenAL Soft ask for, in `licenses/`, and zips the app. `run` and `test` under Wine |
| `win32/` | the programs' manifest -- run as the user, Windows 10's, the UTF-8 code page, each monitor's own DPI, long paths -- and their resource scripts, with their versions |

The UTF-8 code page lets the C library's narrow calls take UTF-8, as every
path above the platform is kept: it is what needs 1903. The engine's
D3D11 device is built but not offered
([the renderers](../../docs/LAUNCHER.md#settings)), nor are linux-x86_64's
renderers on Mesa's CPU drivers, lavapipe and llvmpipe, which the probe
takes for a GPU's where they are all a machine has
([known defects](../../docs/LAUNCHER.md#known-defects)).

## Verified

Under Proton-CachyOS 11.0's wine (`wine-11.0 (CachyOS)`), in the
development container on Arch Linux, on the desktop's Xwayland (KDE Plasma
on Wayland), AMD Radeon 780M under Mesa's RADV and radeonsi:

- `scripts/dx.sh test windows-x86_64`: the 51 tests under Wine, Windows'
  own among them ([how it is tested](../../docs/LAUNCHER.md#how-it-is-tested)).
- The window, driven by a UI script from a fresh home: the first run on
  `gamefiles/` to Home, and Play, the engine on Vulkan to a clean end.
- `dxl-cli`: a profile imported and played to `ready`; a second start's
  map forwarded and opened by the engine; Ctrl+C a `Quit` on the line and a
  clean end, `LastRun.ini` clean; the engine ended by `taskkill /F`, its
  run not clean, and the next start at Recovery.
- The engine, by the harness's proving runs
  ([a Windows build](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/DEVELOPMENT.md#a-windows-build)):
  Liberty Island on Vulkan, its shots within 0.16% of linux-x86_64's build
  by RMSE on the same device, on OpenGL within 0.09%, in its SDL3 window
  and in Windows' own; D3D11 through Proton's DXVK, in both, its HLSL
  compiled by Wine's own `d3dcompiler_47`; a dedicated server's `RELAUNCH`
  started again by the engine itself.
- `dxl-cli --probe`: Vulkan (RADV) and OpenGL found.

Untried on Windows itself, what Wine cannot show: Windows' own Vulkan and
OpenGL drivers, D3D11 on a real `d3dcompiler_47`, the timer's resolution
the engine asks for (`timeBeginPeriod`), sharing violations on a rename,
SmartScreen and Defender on unsigned programs, a restore from the Recycle
Bin, DPI scaling, paths past `MAX_PATH`, a second start bringing the window
or the game to the front past Windows' foreground lock, and a real Steam's,
GOG's or installer's registry keys.

## Gotchas

- **Wine on Wayland**: the programs run on Xwayland, `WAYLAND_DISPLAY`
  unset (Port Ex Machina's `dx_wine_run`). The prefix is the port's own,
  made by `dx_wine_ready` with no Mono or Gecko and vkd3d's libraries copied
  in, which Proton's wined3d needs and a bare prefix lacks.
- **Wine's Recycle Bin is the desktop's trash**: the tests' goes to a
  scratch one (`XDG_DATA_HOME`, `port.sh`'s `port_test`).
- **Several Wine programs started at once** can race to start Wine's server
  and the prefix's services, and a test then waits for ever: `port_test`
  starts them first, and keeps the server up.
- **`wine` leaves a program it started running when killed**
  ([`start.exe`](https://github.com/JuggyMcNutty/DXEngine/blob/main/docs/DEVELOPMENT.md#a-windows-build)):
  kill the program itself.
- **Proton-CachyOS Latest moves** when Proton updates; `DX_PROTON` names
  another build.

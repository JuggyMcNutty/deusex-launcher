# Port: Android (planned)

## Status

Not built yet. `port.cmake` stops the configure with a pointer here, and
`port.sh` refuses to fetch anything. This is what the port needs, in the order
it would be done.

## What differs from linux-x86_64

On Linux the launcher is a separate program that `exec`s the engine
(`platform/posix/launch.c`). An Android app is one process around one
`Activity`: SDL2's Java glue owns the window and calls the app's `SDL_main`.
There is no second program to start -- the engine has to run inside the same
process, after the launcher's screens.

1. **Toolchain.** The NDK (r26 or later) provides clang and a sysroot for
   `arm64-v8a`; its `build/cmake/android.toolchain.cmake` becomes this port's
   `toolchain-c.cmake`/`toolchain-cxx.cmake`, and bionic replaces glibc (no
   glibc ceiling).
2. **The app.** SDL2's `android-project` (Gradle) is the template: the
   launcher, the engine and SDL2 as shared libraries in one APK.
3. **The hand-over.** An Android implementation of `dxl_platform_launch`
   (`platform/launch.h`, beside the POSIX one) that tears down the launcher's
   SDL window and calls the engine's entry point in-process, with the argument
   vector `dxl_argv_build` produces. The engine would need an entry point
   callable as a library function rather than only `main` -- a small patch in
   the fork.
4. **Where the game files are.** `SDL_AndroidGetExternalStoragePath()` for the
   app's own storage; `GameDir` defaults there, and the install screen says
   what to copy. Scoped storage rules out arbitrary paths.
5. **GPU probe.** `fork` exists on Android but a forked child of a running
   app is fragile; probe in-process (Vulkan is required on 64-bit devices since
   Android 10) or skip the probe and trust `renderers.ini`.
6. **Single instance.** Android already runs one instance of an activity;
   `core/instance.c` can stay (abstract sockets work) or be bypassed.
7. **Surreal Engine on Android.** Upstream has no Android support. Its SDL2
   display backend and Vulkan renderer are the right starting points (the
   Smart Pro already runs that combination); OpenAL Soft has an Android
   backend (OpenSL ES / AAudio). File access, the resource zip and the engine's
   `HOME`-based settings path need Android equivalents.
8. **Controls.** Android reports pads through SDL's game controller API as on
   Linux, so the fork's gamepad support should carry over; touch input for
   menus is new work.

A device profile (`target.c`) would carry the fonts Android guarantees, no CPU
modes (the OS manages them), and a pad note if the port targets a particular
handheld.

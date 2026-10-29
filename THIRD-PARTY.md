# Third-party parts of DXLauncher

What the app carries that is not this repository's own, and the terms it comes under.

## Fonts

In `data/fonts/`, unchanged, as the [google/fonts](https://github.com/google/fonts) repository
has them, in its `ofl` folder:

| Font | Files | Licence |
|---|---|---|
| Barlow Semi Condensed, by the Barlow Project Authors | `BarlowSemiCondensed-Regular.ttf`, `-Medium.ttf`, `-SemiBold.ttf` | SIL Open Font License 1.1, `OFL-BarlowSemiCondensed.txt` |
| Share Tech Mono, by Carrois Type Design (Reserved Font Name "Share") | `ShareTechMono-Regular.ttf` | SIL Open Font License 1.1, `OFL-ShareTechMono.txt` |

## Libraries

The window is drawn with [SDL3](https://libsdl.org) and
[SDL3_ttf](https://github.com/libsdl-org/SDL_ttf), both under the zlib licence, and its title
music played with [libxmp-lite](https://github.com/libxmp/libxmp), under the MIT licence below.
Where the system has them they are its own; where it has not they are built into the programs
from the sources `cmake/deps.lock` pins, and with them [FreeType](https://freetype.org), under
the FreeType License: portions of this software are copyright © 2026 The FreeType Project
(www.freetype.org). All rights reserved. The engine's window is drawn with
[SDL2](https://libsdl.org), under the zlib licence: built on the device, the system's;
cross-built, built into it from the source `cmake/deps.lock` pins.

The finder reads Lutris' games database with [SQLite](https://sqlite.org), which its authors
have dedicated to the public domain, built into the programs from the amalgamation
`cmake/deps.lock` pins.

libxmp-lite's notice:

> Extended Module Player Lite
> Copyright (C) 1996-2026 Claudio Matsuoka and Hipolito Carraro Jr
>
> Permission is hereby granted, free of charge, to any person obtaining a
> copy of this software and associated documentation files (the "Software"),
> to deal in the Software without restriction, including without limitation
> the rights to use, copy, modify, merge, publish, distribute, sublicense,
> and/or sell copies of the Software, and to permit persons to whom the
> Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in
> all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
> THE SOFTWARE.

## The engine

`DXEngine` and `DXEngine.pk3` are [DXEngine](https://github.com/JuggyMcNutty/DXEngine)'s, under
the zlib licence, with the third-party code it carries: the notices of each are in
`DXEngine-LICENSE.md` beside them.

## The game

Deus Ex is its makers'. DXLauncher carries none of it: it plays the copy the player has.

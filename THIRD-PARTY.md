# Third-party parts of DXLauncher

What the app carries that is not this repository's own, and the terms it comes under.

## Fonts

In `data/fonts/`, unchanged, as the [google/fonts](https://github.com/google/fonts) repository
has them, in its `ofl` folder:

| Font | Files | Licence |
|---|---|---|
| Barlow Semi Condensed, by the Barlow Project Authors | `BarlowSemiCondensed-Regular.ttf`, `-Medium.ttf`, `-SemiBold.ttf` | SIL Open Font License 1.1, `OFL-BarlowSemiCondensed.txt` |
| Share Tech Mono, by Carrois Type Design (Reserved Font Name "Share") | `ShareTechMono-Regular.ttf` | SIL Open Font License 1.1, `OFL-ShareTechMono.txt` |
| Noto Sans 2.015, by the Noto Project Authors, from [notofonts/latin-greek-cyrillic](https://github.com/notofonts/latin-greek-cyrillic)'s release (its unhinted TTF) | `NotoSans-Regular.ttf` | SIL Open Font License 1.1, `OFL-NotoSans.txt` |

## Libraries

The window is drawn with [SDL3](https://libsdl.org) and
[SDL3_ttf](https://github.com/libsdl-org/SDL_ttf), both under the zlib licence, and its title
music played with [libxmp-lite](https://github.com/libxmp/libxmp), under the MIT licence below.
Where the system has them they are its own; where it has not they are built into the programs
from the sources `cmake/deps.lock` pins, and with them [FreeType](https://freetype.org), under
the FreeType License: portions of this software are copyright © 2026 The FreeType Project
(www.freetype.org). All rights reserved. On the TrimUI Smart Pro they are always built in, and
SDL3 carries a video driver of this port's own, `ports/trimui-smartpro/sdl3/SDL_fbvideo.c`,
under the zlib licence as SDL is.

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

## DXController

[DXController](https://github.com/dsgls/DXController), a mod by dsgls that gives Deus Ex full
controller support, is free software under the GNU General Public License, version 3 or (at
your option) any later version, as its README says. `data/dxcontroller/` holds:

- `DXController.u`, the mod's scripts: the author's own build, unchanged, as the release zip
  `DXController-v1.6.1.zip` carries it, which `cmake/deps.lock` pins by its SHA256. Its source
  is the mod's repository at the tag
  [`v1.6.1`](https://github.com/dsgls/DXController/tree/v1.6.1); `COPYING` beside it is the
  licence's text, and `README` says what the folder is.
- `DeusEx-v1.6.1.dxlpatch`, a binary difference made by `tools/dxpatch.py` from the game's
  stock `DeusEx.u` to the one that release carries, the mod's hooks in it: the bytes of that
  file the stock one lacks, with which DXLauncher makes it on the player's machine from the
  player's own copy ([`docs/LAUNCHER.md`](docs/LAUNCHER.md#dxcontroller)). The changed
  `DeusEx.u` is Ion Storm's package; the mod's README makes no licence claim on it, and none
  ships. The changes the patch carries are in the mod's repository at the same tag.

DXController.u is a work of its own beside DXLauncher and DXEngine, which only name it to the
game; the GPL covers it, and DXLauncher and DXEngine stay under the zlib licence.

## The game

Deus Ex is its makers'. DXLauncher carries none of it: it plays the copy the player has. The
one file it ships made from the game's, DXController's patch, is of use only with the player's
own `DeusEx.u`.

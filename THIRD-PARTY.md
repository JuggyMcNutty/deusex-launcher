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
[SDL3_ttf](https://github.com/libsdl-org/SDL_ttf), both under the zlib licence. Where the system
has them they are its own; where it has not they are built into the programs from the sources
`cmake/deps.lock` pins, and with them [FreeType](https://freetype.org), under the FreeType
License: portions of this software are copyright © 2026 The FreeType Project
(www.freetype.org). All rights reserved.

## The game

Deus Ex is its makers'. DXLauncher carries none of it: it plays the copy the player has.

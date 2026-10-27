/* The launcher's icon, drawn: a steel tile, the game's corner brackets and
 * "DX" in the theme's accent -- our own art. For the window. No fonts:
 * strokes alone. */
#ifndef DXL_UI_ICON_H
#define DXL_UI_ICON_H

#include <SDL3/SDL.h>

/* The icon px pixels square; caller destroys. NULL on failure. */
SDL_Surface *dxl_icon(int px);

#endif

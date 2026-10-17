/* The launcher's icon, drawn: a steel tile, the game's corner brackets and
 * "DX" in the theme's accent -- our own art. For the window, and the app
 * menu's PNGs (platform/desktop.h). No fonts: strokes alone. */
#ifndef DXL_UI_ICON_H
#define DXL_UI_ICON_H

#include <SDL3/SDL.h>

/* The icon px pixels square; caller destroys. NULL on failure. */
SDL_Surface *dxl_icon(int px);
/* Written as a PNG, its folders made. 0 when it was. */
int dxl_icon_save(const char *path, int px);

#endif

/* The splash: DeusEx.exe's dialog 119 (dx-reverse-info/launch-flow.md
 * section 3) -- a popup with a dialog frame, as big as the bitmap and
 * centred, the bitmap in it from the frame inwards. Kept off the taskbar, as
 * a tool window. */
#ifndef DXL_SPLASH_H
#define DXL_SPLASH_H

#include "gui/gui.h"

typedef struct dxl_splash dxl_splash;

dxl_splash *dxl_splash_new(void);
void        dxl_splash_free(dxl_splash *s);
/* From this bitmap; NULL shows the last one again. */
void        dxl_splash_show(dxl_splash *s, const char *bmp_path);
void        dxl_splash_hide(dxl_splash *s);
/* An event, which the splash answers if it is its window's: drawn again
 * when uncovered. */
void        dxl_splash_event(dxl_splash *s, const SDL_Event *e);
/* The splash as drawn, into a .bmp, with no display: for pictures of it. */
int         dxl_splash_save(const char *bmp_path, const char *out_path);

#endif

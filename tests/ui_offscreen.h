/* The kit on a software renderer and a surface, for the UI tests: no
 * display, no window. */
#ifndef DXL_TEST_UI_OFFSCREEN_H
#define DXL_TEST_UI_OFFSCREEN_H

#include "ui/kit.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef DXL_FONTS
#define DXL_FONTS "data/fonts"
#endif

typedef struct {
    SDL_Surface  *surface;
    SDL_Renderer *renderer;
} offscreen;

static inline int offscreen_open(offscreen *o, dxl_ui *ui, int w, int h, float scale) {
    o->surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_XRGB8888);
    o->renderer = o->surface ? SDL_CreateSoftwareRenderer(o->surface) : NULL;
    if (!o->renderer || dxl_ui_init(ui, NULL, o->renderer, DXL_FONTS, scale) != 0) {
        fprintf(stderr, "no offscreen kit: %s\n", SDL_GetError());
        return -1;
    }
    return 0;
}

static inline void offscreen_close(offscreen *o, dxl_ui *ui) {
    dxl_ui_free(ui);
    SDL_DestroyRenderer(o->renderer);
    SDL_DestroySurface(o->surface);
}

#endif

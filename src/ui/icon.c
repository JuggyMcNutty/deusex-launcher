#include "ui/icon.h"
#include "core/paths.h"
#include "ui/draw.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static dxl_rgba rgb(int r, int g, int b) {
    dxl_rgba c = { (uint8_t)r, (uint8_t)g, (uint8_t)b, 255 };
    return c;
}

/* A stroke through points, w wide about them, its joins round. */
static void stroke(dxl_ui *ui, const float *xy, int n, float w, dxl_rgba c) {
    for (int i = 0; i + 1 < n; i++) {
        float ax = xy[2 * i], ay = xy[2 * i + 1], bx = xy[2 * i + 2], by = xy[2 * i + 3];
        float dx = bx - ax, dy = by - ay, len = sqrtf(dx * dx + dy * dy);
        if (len <= 0) continue;
        float nx = -dy / len * w / 2, ny = dx / len * w / 2;
        SDL_FPoint q[4] = { { ax + nx, ay + ny }, { bx + nx, by + ny }, { bx - nx, by - ny }, { ax - nx, ay - ny } };
        dxl_poly(ui, q, 4, c);
    }
    for (int i = 0; i < n; i++) dxl_disc(ui, xy[2 * i], xy[2 * i + 1], w / 2, c);
}

/* Drawn on a 64 dp tile. */
static void draw(dxl_ui *ui) {
    dxl_bevel_style tile = { rgb(48, 56, 70), rgb(14, 17, 23), rgb(130, 142, 160), rgb(0, 0, 0), 11 };
    dxl_bevel(ui, (dxl_rect){ 3, 3, 58, 58 }, &tile);
    dxl_brackets(ui, (dxl_rect){ 10, 10, 44, 44 }, 10, 2.5f, rgb(236, 240, 248));
    dxl_rgba ink = rgb(158, 172, 255);
    static const float d[] = { 18, 22, 26, 22, 31, 27, 31, 37, 26, 42, 18, 42, 18, 22 };
    stroke(ui, d, 7, 3.6f, ink);
    static const float x1[] = { 35.5f, 22, 47, 42 }, x2[] = { 47, 22, 35.5f, 42 };
    stroke(ui, x1, 2, 3.6f, ink);
    stroke(ui, x2, 2, 3.6f, ink);
}

SDL_Surface *dxl_icon(int px) {
    /* Drawn four times as large and halved twice: its edges smoothed. */
    int big = px * 4;
    SDL_Surface *s = SDL_CreateSurface(big, big, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return NULL;
    SDL_Renderer *r = SDL_CreateSoftwareRenderer(s);
    if (!r) {
        SDL_DestroySurface(s);
        return NULL;
    }
    SDL_SetRenderDrawColor(r, 0, 0, 0, 0);
    SDL_RenderClear(r);
    dxl_ui ui;
    memset(&ui, 0, sizeof ui);
    ui.renderer = r;
    ui.scale = big / 64.0f;
    ui.w = ui.h = 64;
    dxl_theme_default(&ui.theme);
    draw(&ui);
    SDL_RenderPresent(r);
    SDL_DestroyRenderer(r);
    for (int size = big / 2; size >= px; size /= 2) {
        SDL_Surface *half = SDL_ScaleSurface(s, size, size, SDL_SCALEMODE_LINEAR);
        SDL_DestroySurface(s);
        if (!half) return NULL;
        s = half;
    }
    return s;
}

int dxl_icon_save(const char *path, int px) {
    char *dir = dxl_path_dirname(path);
    int made = dxl_path_mkdirs(dir) == 0;
    free(dir);
    if (!made) return -1;
    SDL_Surface *s = dxl_icon(px);
    if (!s) return -1;
    int ok = SDL_SavePNG(s, path);
    SDL_DestroySurface(s);
    return ok ? 0 : -1;
}

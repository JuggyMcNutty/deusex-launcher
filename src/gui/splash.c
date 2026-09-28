#include "gui/splash.h"

#include <stdlib.h>
#include <string.h>

struct dxl_splash {
    char         *path;
    SDL_Window   *win;
    SDL_Renderer *ren;
    SDL_Surface  *bmp;    /* while shown, to draw it again */
};

dxl_splash *dxl_splash_new(void) {
    dxl_splash *s = dxl_xmalloc(sizeof *s);
    memset(s, 0, sizeof *s);
    return s;
}

void dxl_splash_free(dxl_splash *s) {
    if (!s) return;
    dxl_splash_hide(s);
    free(s->path);
    free(s);
}

/* The frame, over the bitmap's edges: a raised edge and a line of the face. */
static void draw(SDL_Renderer *r, SDL_Surface *bmp) {
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, bmp);
    SDL_Rect dst = { 3, 3, bmp->w, bmp->h };
    if (t) { SDL_RenderCopy(r, t, NULL, &dst); SDL_DestroyTexture(t); }
    SDL_Rect all = { 0, 0, bmp->w, bmp->h };
    SDL_Rect in = dxl_edge(r, all, DXL_EDGE_RAISED);
    SDL_SetRenderDrawColor(r, dxl_colors.face.r, dxl_colors.face.g, dxl_colors.face.b, 255);
    SDL_RenderDrawRect(r, &in);
}

void dxl_splash_show(dxl_splash *s, const char *bmp_path) {
    if (bmp_path) { free(s->path); s->path = dxl_xstrdup(bmp_path); }
    if (!s->path || s->win) return;
    s->bmp = SDL_LoadBMP(s->path);
    if (!s->bmp) return;
    s->win = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, s->bmp->w,
                              s->bmp->h,
                              SDL_WINDOW_BORDERLESS | SDL_WINDOW_SKIP_TASKBAR | SDL_WINDOW_HIDDEN);
    if (s->win) {
        s->ren = SDL_CreateRenderer(s->win, -1, SDL_RENDERER_SOFTWARE);
        if (s->ren) {
            draw(s->ren, s->bmp);
            SDL_RenderPresent(s->ren);
            SDL_ShowWindow(s->win);
            SDL_RenderPresent(s->ren);
        }
    }
}

void dxl_splash_hide(dxl_splash *s) {
    if (s->ren) SDL_DestroyRenderer(s->ren);
    if (s->win) SDL_DestroyWindow(s->win);
    if (s->bmp) SDL_FreeSurface(s->bmp);
    s->ren = NULL;
    s->win = NULL;
    s->bmp = NULL;
}

void dxl_splash_event(dxl_splash *s, const SDL_Event *e) {
    if (!s->ren || e->type != SDL_WINDOWEVENT || e->window.windowID != SDL_GetWindowID(s->win))
        return;
    if (e->window.event == SDL_WINDOWEVENT_EXPOSED) {
        draw(s->ren, s->bmp);
        SDL_RenderPresent(s->ren);
    }
}

int dxl_splash_save(const char *bmp_path, const char *out_path) {
    SDL_Surface *bmp = SDL_LoadBMP(bmp_path);
    if (!bmp) return -1;
    SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, bmp->w, bmp->h, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *r = out ? SDL_CreateSoftwareRenderer(out) : NULL;
    int rc = -1;
    if (r) {
        draw(r, bmp);
        SDL_RenderPresent(r);
        rc = SDL_SaveBMP(out, out_path);
        SDL_DestroyRenderer(r);
    }
    if (out) SDL_FreeSurface(out);
    SDL_FreeSurface(bmp);
    return rc;
}

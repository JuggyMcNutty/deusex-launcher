#include "ui/art.h"
#include "core/log.h"
#include "core/paths.h"
#include "core/upkg_assets.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The logo's band of "Deus Ex", px of Help/Logo.bmp's 512 x 410: under its
 * emblem, clear of the makers' logos in its top corners. */
static const SDL_Rect WORDMARK = { 32, 334, 448, 56 };

/* Pixel art scaled with its edges kept, where this SDL has the mode. */
static void pixel_art(SDL_Texture *t) {
#if SDL_VERSION_ATLEAST(3, 4, 0)
    if (!SDL_SetTextureScaleMode(t, SDL_SCALEMODE_PIXELART)) SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
#else
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
#endif
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);
}

static SDL_Texture *texture_of(SDL_Renderer *r, const dxl_uimage *im) {
    SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, im->w, im->h);
    if (!t) return NULL;
    SDL_UpdateTexture(t, NULL, im->rgba, im->w * 4);
    pixel_art(t);
    return t;
}

static SDL_Texture *menu_texture(SDL_Renderer *r, const dxl_upkg *p, const char *name, float *w, float *h) {
    dxl_uimage im;
    dxl_err e;
    if (dxl_upkg_texture(p, dxl_upkg_find(p, "Texture", name), &im, &e) != 0) {
        dxl_log("art: %s", dxl_err_msg(&e));
        return NULL;
    }
    SDL_Texture *t = texture_of(r, &im);
    if (w) *w = (float)im.w;
    if (h) *h = (float)im.h;
    dxl_uimage_free(&im);
    return t;
}

static void wordmark(dxl_art *a, SDL_Renderer *r, const char *root) {
    char *help = dxl_path_resolve_ci(root, "Help");
    char *bmp = help ? dxl_path_resolve_ci(help, "Logo.bmp") : NULL;
    SDL_Surface *s = bmp ? SDL_LoadBMP(bmp) : NULL;
    SDL_Surface *rgba = s ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32) : NULL;
    if (rgba && rgba->w >= WORDMARK.x + WORDMARK.w && rgba->h >= WORDMARK.y + WORDMARK.h) {
        SDL_Surface *band = SDL_CreateSurface(WORDMARK.w, WORDMARK.h, SDL_PIXELFORMAT_RGBA32);
        if (band && SDL_BlitSurface(rgba, &WORDMARK, band, NULL)) {
            a->wordmark = SDL_CreateTextureFromSurface(r, band);
            if (a->wordmark) {
                SDL_SetTextureScaleMode(a->wordmark, SDL_SCALEMODE_LINEAR);
                SDL_SetTextureBlendMode(a->wordmark, SDL_BLENDMODE_ADD);
                a->wordmark_w = (float)WORDMARK.w;
                a->wordmark_h = (float)WORDMARK.h;
            }
        }
        SDL_DestroySurface(band);
    } else if (bmp) {
        dxl_log("art: %s: %s", bmp, s ? "not the logo it should be" : SDL_GetError());
    }
    SDL_DestroySurface(rgba);
    SDL_DestroySurface(s);
    free(bmp);
    free(help);
}

void dxl_art_free(dxl_art *a) {
    if (a->wordmark) SDL_DestroyTexture(a->wordmark);
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++)
            if (a->button[i][j]) SDL_DestroyTexture(a->button[i][j]);
        if (a->light[i]) SDL_DestroyTexture(a->light[i]);
    }
    free(a->from);
    memset(a, 0, sizeof *a);
}

void dxl_art_load(dxl_art *a, dxl_ui *ui, const char *root) {
    if (!root) root = "";
    if (a->renderer == ui->renderer && a->from && !strcmp(a->from, root)) return;
    dxl_art_free(a);
    a->renderer = ui->renderer;
    a->from = dxl_xstrdup(root);   /* whole: a key cut would never match again */
    if (!*root || !ui->renderer) return;
    wordmark(a, ui->renderer, root);
    char *system = dxl_path_resolve_ci(root, "System");
    char *ui_u = system ? dxl_path_resolve_ci(system, "DeusExUI.u") : NULL;
    dxl_upkg p;
    dxl_err e;
    if (ui_u && dxl_upkg_load(&p, ui_u, &e) == 0) {
        static const char *const parts[2][3] = {
            { "MenuMainButtonNormal_Left", "MenuMainButtonNormal_Center", "MenuMainButtonNormal_Right" },
            { "MenuMainButtonPressed_Left", "MenuMainButtonPressed_Center", "MenuMainButtonPressed_Right" },
        };
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 3; j++) a->button[i][j] = menu_texture(ui->renderer, &p, parts[i][j], &a->button_w[j], &a->button_h);
        a->light[0] = menu_texture(ui->renderer, &p, "MenuMainLight_Normal", &a->light_w, NULL);
        a->light[1] = menu_texture(ui->renderer, &p, "MenuMainLight_Focus", NULL, NULL);
        dxl_upkg_free(&p);
    } else if (ui_u) {
        dxl_log("art: %s", dxl_err_msg(&e));
    }
    free(ui_u);
    free(system);
    dxl_log("art: %s%s from %s", a->wordmark ? "the logo" : "no logo",
            dxl_art_has_buttons(a) ? " and the main menu's buttons" : ", no buttons", root);
}

int dxl_art_has_buttons(const dxl_art *a) {
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++)
            if (!a->button[i][j]) return 0;
        if (!a->light[i]) return 0;
    }
    return a->button_h > 0;
}

float dxl_art_wordmark(dxl_ui *ui, const dxl_art *a, dxl_rect r) {
    if (!a->wordmark || a->wordmark_h <= 0) return 0;
    float w = r.h * a->wordmark_w / a->wordmark_h;
    dxl_rect px = dxl_px(ui, (dxl_rect){ r.x, r.y, w, r.h });
    SDL_RenderTexture(ui->renderer, a->wordmark, NULL, &px);
    return w;
}

/* A piece in r, tinted by c. */
static void piece(dxl_ui *ui, SDL_Texture *t, dxl_rect r, dxl_rgba c) {
    SDL_SetTextureColorMod(t, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(t, c.a);
    dxl_rect px = dxl_px(ui, r);
    SDL_RenderTexture(ui->renderer, t, NULL, &px);
}

int dxl_art_button(dxl_ui *ui, const dxl_art *a, uint32_t id, dxl_rect r, const char *label, int flags) {
    const dxl_theme *t = &ui->theme;
    int disabled = flags & DXL_W_DISABLED;
    int hits = dxl_ui_focusable(ui, id, r, disabled);
    if (!disabled) dxl_ui_name(ui, id, label);
    int lit = (hits & (DXL_HIT_FOCUSED | DXL_HIT_HOVERED)) != 0, held = (hits & DXL_HIT_HELD) != 0;
    /* The pieces at the art's height to r's: the light, the left end, the
     * middle stretched, the right end. */
    float k = r.h / a->button_h;
    float lw = a->light_w * k, left = a->button_w[0] * k, right = a->button_w[2] * k;
    /* Added to the backdrop, the face a translucent plate: dim, lit
     * brighter, its text over it the theme's. */
    const dxl_rgba black = { 0, 0, 0, 255 };
    dxl_rgba face = dxl_rgba_mix(black, t->menu[DXL_MENU_BUTTON_FACE], disabled ? 0.18f : lit ? 0.55f : 0.36f);
    piece(ui, a->light[lit && !disabled], (dxl_rect){ r.x, r.y, lw, r.h },
          lit ? t->menu[DXL_MENU_BUTTON_TEXT_FOCUS] : face);
    float x = r.x + lw;
    piece(ui, a->button[held][0], (dxl_rect){ x, r.y, left, r.h }, face);
    float mid = r.w - lw - left - right;
    if (mid > 0) piece(ui, a->button[held][1], (dxl_rect){ x + left, r.y, mid, r.h }, face);
    piece(ui, a->button[held][2], (dxl_rect){ x + left + (mid > 0 ? mid : 0), r.y, right, r.h }, face);
    dxl_rgba c = disabled ? t->menu[DXL_MENU_BUTTON_TEXT_DISABLED]
               : lit ? t->menu[DXL_MENU_BUTTON_TEXT_FOCUS] : t->menu[DXL_MENU_BUTTON_TEXT_NORMAL];
    dxl_rect text = { x + left + 6, r.y, r.w - lw - left - right - 6, r.h };
    dxl_label(ui, text, DXL_FONT_BUTTON, label, c, DXL_W_LEFT | (flags & DXL_W_MAY_CUT));
    return (hits & DXL_HIT_ACTIVATED) != 0;
}

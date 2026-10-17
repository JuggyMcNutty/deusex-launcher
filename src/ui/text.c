#include "ui/text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct {
    const char *file;
    float       dp;
} roles[DXL_FONT_COUNT] = {
    [DXL_FONT_TITLE]   = { "BarlowSemiCondensed-SemiBold.ttf", 44 },
    [DXL_FONT_HEADING] = { "BarlowSemiCondensed-SemiBold.ttf", 26 },
    [DXL_FONT_BUTTON]  = { "BarlowSemiCondensed-Medium.ttf", 19 },
    [DXL_FONT_LABEL]   = { "BarlowSemiCondensed-Medium.ttf", 15 },
    [DXL_FONT_BODY]    = { "BarlowSemiCondensed-Regular.ttf", 16 },
    [DXL_FONT_SMALL]   = { "BarlowSemiCondensed-Regular.ttf", 13 },
    [DXL_FONT_MONO]    = { "ShareTechMono-Regular.ttf", 14 },
};

#define CACHE_SIZE 384

typedef struct {
    TTF_Text     *text;
    char         *s;
    uint32_t      hash;
    int           role;
    int           wrap_px;
    unsigned long used;
} entry;

struct dxl_text_cache {
    entry         e[CACHE_SIZE];
    unsigned long tick;
};

static uint32_t hash_of(const char *s) {
    uint32_t h = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= 16777619u;
    }
    return h;
}

int dxl_text_open(dxl_ui *ui) {
    if (!ui->cache) {
        ui->cache = calloc(1, sizeof *ui->cache);
        if (!ui->cache) return -1;
    }
    dxl_text_flush(ui);
    for (int i = 0; i < DXL_FONT_COUNT; i++) {
        if (ui->fonts[i]) TTF_CloseFont(ui->fonts[i]);
        ui->fonts[i] = NULL;
    }
    for (int i = 0; i < DXL_FONT_COUNT; i++) {
        char path[1200];
        snprintf(path, sizeof path, "%s/%s", ui->fonts_dir, roles[i].file);
        ui->fonts[i] = TTF_OpenFont(path, roles[i].dp * ui->scale);
        if (!ui->fonts[i]) {
            SDL_SetError("cannot open the font %s", path);
            return -1;
        }
        TTF_SetFontHinting(ui->fonts[i], TTF_HINTING_LIGHT);
    }
    ui->font_scale = ui->scale;
    return 0;
}

void dxl_text_flush(dxl_ui *ui) {
    if (!ui->cache) return;
    for (int i = 0; i < CACHE_SIZE; i++) {
        entry *e = &ui->cache->e[i];
        if (e->text) TTF_DestroyText(e->text);
        free(e->s);
        memset(e, 0, sizeof *e);
    }
}

void dxl_text_close(dxl_ui *ui) {
    dxl_text_flush(ui);
    for (int i = 0; i < DXL_FONT_COUNT; i++) {
        if (ui->fonts[i]) TTF_CloseFont(ui->fonts[i]);
        ui->fonts[i] = NULL;
    }
    free(ui->cache);
    ui->cache = NULL;
}

/* The laid-out text, made or found. */
static TTF_Text *get(dxl_ui *ui, dxl_font_role role, const char *s, int wrap_px) {
    if (!ui->engine || !ui->cache || !ui->fonts[role]) return NULL;
    dxl_text_cache *c = ui->cache;
    uint32_t h = hash_of(s);
    entry *oldest = &c->e[0];
    for (int i = 0; i < CACHE_SIZE; i++) {
        entry *e = &c->e[i];
        if (e->text && e->hash == h && e->role == (int)role && e->wrap_px == wrap_px && strcmp(e->s, s) == 0) {
            e->used = ++c->tick;
            return e->text;
        }
        if (!e->text) { oldest = e; break; }
        if (e->used < oldest->used) oldest = e;
    }
    if (oldest->text) TTF_DestroyText(oldest->text);
    free(oldest->s);
    oldest->text = TTF_CreateText(ui->engine, ui->fonts[role], s, 0);
    oldest->s = strdup(s);
    oldest->hash = h;
    oldest->role = role;
    oldest->wrap_px = wrap_px;
    oldest->used = ++c->tick;
    if (oldest->text && wrap_px > 0) TTF_SetTextWrapWidth(oldest->text, wrap_px);
    return oldest->text;
}

SDL_FPoint dxl_text_size(dxl_ui *ui, dxl_font_role role, const char *s, float wrap) {
    SDL_FPoint size = { 0, 0 };
    if (!s || !*s) {
        size.y = dxl_text_line(ui, role);
        return size;
    }
    int wrap_px = wrap > 0 ? (int)dxl_pxf(ui, wrap) : 0;
    TTF_Text *t = get(ui, role, s, wrap_px);
    int w = 0, h = 0;
    if (t) TTF_GetTextSize(t, &w, &h);
    size.x = w / ui->scale;
    size.y = h / ui->scale;
    return size;
}

float dxl_text_line(dxl_ui *ui, dxl_font_role role) {
    return ui->fonts[role] ? TTF_GetFontLineSkip(ui->fonts[role]) / ui->scale : roles[role].dp * 1.2f;
}

SDL_FPoint dxl_text_draw(dxl_ui *ui, dxl_font_role role, const char *s, float x, float y, float wrap,
                         dxl_rgba c, float reveal) {
    SDL_FPoint size = { 0, 0 };
    if (!s || !*s || c.a == 0) return dxl_text_size(ui, role, s, wrap);
    int wrap_px = wrap > 0 ? (int)dxl_pxf(ui, wrap) : 0;
    TTF_Text *t = get(ui, role, s, wrap_px);
    if (!t) return size;
    int w = 0, h = 0;
    TTF_GetTextSize(t, &w, &h);
    size.x = w / ui->scale;
    size.y = h / ui->scale;
    TTF_SetTextColor(t, c.r, c.g, c.b, c.a);
    float px = dxl_pxf(ui, x), py = dxl_pxf(ui, y);
    /* Whole pixels: a glyph between two blurs on the software renderer. */
    px = (float)(int)(px + 0.5f);
    py = (float)(int)(py + 0.5f);
    if (reveal >= 1) {
        TTF_DrawRendererText(t, px, py);
        return size;
    }
    if (reveal <= 0) return size;
    /* The typewriter: whole lines, and the next one growing from the left. */
    int lines = t->num_lines > 0 ? t->num_lines : 1;
    float line_px = (float)h / lines;
    float at = reveal * lines;
    int whole = (int)at;
    float part = at - whole;
    SDL_Rect old;
    bool clipped = SDL_RenderClipEnabled(ui->renderer);
    if (clipped) SDL_GetRenderClipRect(ui->renderer, &old);
    if (whole > 0) {
        SDL_Rect r = { (int)px, (int)py, w, (int)(whole * line_px + 0.5f) };
        if (clipped) SDL_GetRectIntersection(&r, &old, &r);
        SDL_SetRenderClipRect(ui->renderer, &r);
        TTF_DrawRendererText(t, px, py);
    }
    if (whole < lines && part > 0) {
        SDL_Rect r = { (int)px, (int)(py + whole * line_px + 0.5f), (int)(w * part), (int)(line_px + 0.5f) };
        if (clipped) SDL_GetRectIntersection(&r, &old, &r);
        SDL_SetRenderClipRect(ui->renderer, &r);
        TTF_DrawRendererText(t, px, py);
    }
    SDL_SetRenderClipRect(ui->renderer, clipped ? &old : NULL);
    dxl_ui_animate(ui);
    return size;
}

int dxl_text_fit(dxl_ui *ui, dxl_font_role role, const char *s, float w, char *out, size_t size) {
    snprintf(out, size, "%s", s ? s : "");
    TTF_Font *f = ui->fonts[role];
    if (!f || !*out) return 0;
    int max_px = (int)dxl_pxf(ui, w), width = 0;
    size_t fits = 0;
    if (!TTF_MeasureString(f, out, 0, max_px, &width, &fits) || fits >= strlen(out)) return 0;
    int dots = 0;
    TTF_GetStringSize(f, "...", 0, &dots, NULL);
    if (!TTF_MeasureString(f, out, 0, max_px - dots, &width, &fits)) fits = 0;
    /* Not inside a UTF-8 sequence. */
    while (fits > 0 && ((unsigned char)out[fits] & 0xc0) == 0x80) fits--;
    if (fits + 4 > size) fits = size > 4 ? size - 4 : 0;
    memcpy(out + fits, "...", 4);
    return 1;
}

int dxl_text_fit_tail(dxl_ui *ui, dxl_font_role role, const char *s, float w, char *out, size_t size) {
    snprintf(out, size, "%s", s ? s : "");
    TTF_Font *f = ui->fonts[role];
    if (!f || !*out) return 0;
    int max_px = (int)dxl_pxf(ui, w), width = 0;
    if (TTF_GetStringSize(f, out, 0, &width, NULL) && width <= max_px) return 0;
    /* The shortest cut that fits: the narrower, the later the start. */
    size_t len = strlen(s), lo = 0, hi = len;
    char probe[1024];
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        while (mid < len && ((unsigned char)s[mid] & 0xc0) == 0x80) mid++;
        if (mid >= hi) break;
        snprintf(probe, sizeof probe, "...%s", s + mid);
        if (TTF_GetStringSize(f, probe, 0, &width, NULL) && width <= max_px) hi = mid;
        else lo = mid + 1;
    }
    while (hi < len && ((unsigned char)s[hi] & 0xc0) == 0x80) hi++;
    snprintf(out, size, "...%s", s + hi);
    return 1;
}

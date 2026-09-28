#ifndef _GNU_SOURCE
#define _GNU_SOURCE   /* SDL's flags may have it already */
#endif
#include "gui/gui.h"

#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* As the original shows them under wine here: its system colours. */
const dxl_palette dxl_colors = {
    .face      = { 0xf5, 0xf5, 0xf5 },
    .highlight = { 0xff, 0xff, 0xff },
    .light     = { 0xe3, 0xe3, 0xe3 },
    .shadow    = { 0xa6, 0xa6, 0xa6 },
    .dkshadow  = { 0x6a, 0x6a, 0x6a },
    .window    = { 0xff, 0xff, 0xff },
    .text      = { 0x00, 0x00, 0x00 },
    .sel       = { 0x30, 0x96, 0xfa },
    .sel_text  = { 0xff, 0xff, 0xff },
    .url       = { 0x00, 0x00, 0xff },
    .frame     = { 0x9e, 0x9e, 0x9e },
};

/* ---- the font ----------------------------------------------------------- */

/* Wine's MS Sans Serif, a bitmap font: the original's own letters, where
 * wine is installed. */
static const char *const wine_fonts[] = {
    "/usr/share/wine/fonts/sserife.fon",
    "/usr/lib/wine/fonts/sserife.fon",
    "/usr/lib64/wine/fonts/sserife.fon",
    "/opt/wine*/share/wine/fonts/sserife.fon",
    "~/.local/share/Steam/compatibilitytools.d/*/files/share/wine/fonts/sserife.fon",
    "~/.steam/steam/compatibilitytools.d/*/files/share/wine/fonts/sserife.fon",
    "~/.local/share/Steam/steamapps/common/Proton*/files/share/wine/fonts/sserife.fon",
    "~/.steam/steam/steamapps/common/Proton*/files/share/wine/fonts/sserife.fon",
    NULL,
};

/* Wine's user32.dll, whose icons are the ones its message boxes show. */
static const char *const wine_user32[] = {
    "/usr/lib/wine/i386-windows/user32.dll",
    "/usr/lib32/wine/i386-windows/user32.dll",
    "/usr/lib/wine/x86_64-windows/user32.dll",
    "/usr/lib64/wine/x86_64-windows/user32.dll",
    "/opt/wine*/lib/wine/i386-windows/user32.dll",
    "~/.local/share/Steam/compatibilitytools.d/*/files/lib/wine/i386-windows/user32.dll",
    "~/.steam/steam/compatibilitytools.d/*/files/lib/wine/i386-windows/user32.dll",
    "~/.local/share/Steam/steamapps/common/Proton*/files/lib/wine/i386-windows/user32.dll",
    "~/.steam/steam/steamapps/common/Proton*/files/lib/wine/i386-windows/user32.dll",
    NULL,
};

/* Else a common sans, drawn unsmoothed at 8 pt. */
static const char *const sans_fonts[] = {
    "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
    "/usr/local/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
    NULL,
};

/* Window.dll's link font (hFontUrl) is Arial, 9 pt, underlined: wine's
 * Arial where wine is installed, else Arial itself, else a sans with its
 * metrics. */
static const char *const url_fonts[] = {
    "~/.local/share/Steam/compatibilitytools.d/*/files/share/fonts/arial.ttf",
    "~/.steam/steam/compatibilitytools.d/*/files/share/fonts/arial.ttf",
    "~/.local/share/Steam/steamapps/common/Proton*/files/share/fonts/arial.ttf",
    "~/.steam/steam/steamapps/common/Proton*/files/share/fonts/arial.ttf",
    "/usr/share/fonts/msttcore/arial.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
    "/usr/share/fonts/TTF/arial.ttf",
    "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/TTF/LiberationSans-Regular.ttf",
    "/usr/local/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
    NULL,
};

/* A message box's font is the system's message font, which is wine's
 * Tahoma, 8 pt, under wine; else a sans much like it. */
static const char *const msg_fonts[] = {
    "/usr/share/wine/fonts/tahoma.ttf",
    "/usr/lib/wine/fonts/tahoma.ttf",
    "/usr/lib64/wine/fonts/tahoma.ttf",
    "/opt/wine*/share/wine/fonts/tahoma.ttf",
    "~/.local/share/Steam/compatibilitytools.d/*/files/share/wine/fonts/tahoma.ttf",
    "~/.steam/steam/compatibilitytools.d/*/files/share/wine/fonts/tahoma.ttf",
    "~/.local/share/Steam/steamapps/common/Proton*/files/share/wine/fonts/tahoma.ttf",
    "~/.steam/steam/steamapps/common/Proton*/files/share/wine/fonts/tahoma.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    NULL,
};

/* The first existing file a pattern names, ~ for $HOME. Caller frees. */
static char *first_match(const char *pattern) {
    char path[1024];
    const char *home = getenv("HOME");
    if (pattern[0] == '~') snprintf(path, sizeof path, "%s%s", home ? home : "", pattern + 1);
    else snprintf(path, sizeof path, "%s", pattern);
    glob_t gl;
    char *hit = NULL;
    if (glob(path, 0, NULL, &gl) == 0) {
        for (size_t i = 0; i < gl.gl_pathc && !hit; i++)
            if (access(gl.gl_pathv[i], R_OK) == 0) hit = dxl_xstrdup(gl.gl_pathv[i]);
        globfree(&gl);
    }
    return hit;
}

/* A .fon holds a face per size: the one 13 pixels high is 8 pt. */
static TTF_Font *open_fon(const char *path) {
    for (long i = 0; i < 12; i++) {
        TTF_Font *f = TTF_OpenFontIndex(path, 8, i);
        if (!f) break;
        if (TTF_FontHeight(f) == 13) return f;
        TTF_CloseFont(f);
    }
    return NULL;
}

/* ---- a font's height, as Windows has it ------------------------------------ */

/* The heights of the fonts opened, which lines of text are apart. */
static struct { TTF_Font *font; int height; } heights[8];

static void remember_height(TTF_Font *f, int h) {
    for (size_t i = 0; i < sizeof heights / sizeof *heights; i++)
        if (!heights[i].font || heights[i].font == f) {
            heights[i].font = f;
            heights[i].height = h;
            return;
        }
}

static void forget_height(TTF_Font *f) {
    for (size_t i = 0; i < sizeof heights / sizeof *heights; i++)
        if (heights[i].font == f) heights[i].font = NULL;
}

int dxl_font_height(TTF_Font *f) {
    for (size_t i = 0; i < sizeof heights / sizeof *heights; i++)
        if (heights[i].font == f) return heights[i].height;
    return TTF_FontAscent(f) - TTF_FontDescent(f);
}

static unsigned be16(const unsigned char *p) { return (unsigned)p[0] << 8 | p[1]; }
static unsigned long be32(const unsigned char *p) {
    return (unsigned long)p[0] << 24 | (unsigned long)p[1] << 16 | (unsigned long)p[2] << 8 | p[3];
}

/* tmHeight for a TrueType font at ppem pixels: its OS/2 table's
 * usWinAscent and usWinDescent, each scaled and rounded, as wine takes them.
 * 0 when the file has no such table. */
static int win_height(const char *path, int ppem) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char buf[12 + 64 * 16];
    size_t n = fread(buf, 1, sizeof buf, f);
    unsigned long em = 0, asc = 0, desc = 0;
    int found = 0;
    if (n >= 12) {
        unsigned tables = be16(buf + 4);
        for (unsigned t = 0; t < tables && 12 + (t + 1) * 16 <= n; t++) {
            const unsigned char *rec = buf + 12 + t * 16;
            unsigned char field[4];
            long at = (long)be32(rec + 8);
            if (memcmp(rec, "head", 4) == 0 && fseek(f, at + 18, SEEK_SET) == 0 &&
                fread(field, 1, 2, f) == 2)
                em = be16(field);
            if (memcmp(rec, "OS/2", 4) == 0 && fseek(f, at + 74, SEEK_SET) == 0 &&
                fread(field, 1, 4, f) == 4) {
                asc = be16(field);
                desc = be16(field + 2);
                found = 1;
            }
        }
    }
    fclose(f);
    if (!found || !em) return 0;
    return (int)((asc * (unsigned long)ppem + em / 2) / em) +
           (int)((desc * (unsigned long)ppem + em / 2) / em);
}

/* Drawn as Windows draws text: unsmoothed, and with no kerning. */
static TTF_Font *as_windows(TTF_Font *f) {
    if (f) {
        TTF_SetFontHinting(f, TTF_HINTING_MONO);
        TTF_SetFontKerning(f, 0);
    }
    return f;
}

static int is_fon(const char *path) {
    size_t n = strlen(path);
    return n > 4 && strcasecmp(path + n - 4, ".fon") == 0;
}

/* A font file at a size in pixels (CreateFont's negative height), its
 * height remembered. A .fon is taken at its face that high. */
static TTF_Font *open_at(const char *path, int ppem) {
    if (is_fon(path)) {
        TTF_Font *f = as_windows(open_fon(path));
        if (f) remember_height(f, TTF_FontHeight(f));
        return f;
    }
    TTF_Font *f = as_windows(TTF_OpenFontDPI(path, ppem, 72, 72));
    int h = f ? win_height(path, ppem) : 0;
    if (f && h) remember_height(f, h);
    return f;
}

/* MS Sans Serif 8 pt, CreateFont's -MulDiv(8, 96, 72): 11 pixels. */
#define DIALOG_PX 11

static int open_font(dxl_gui *g, const char *path) {
    TTF_Font *f = open_at(path, DIALOG_PX);
    if (!f) return 0;
    g->font = f;
    g->font_path = dxl_xstrdup(path);
    g->line_h = dxl_font_height(f);
    return 1;
}

/* The first of the candidates -- an environment variable's, then a list's
 * -- that opens at this size; else the dialog font again. */
static TTF_Font *open_other(const dxl_gui *g, const char *env, const char *const *list, int px) {
    const char *want = getenv(env);
    TTF_Font *f = NULL;
    if (want && *want) f = open_at(want, px);
    for (int i = 0; !f && list[i]; i++) {
        char *p = first_match(list[i]);
        if (p) f = open_at(p, px);
        free(p);
    }
    return f ? f : open_at(g->font_path, DIALOG_PX);
}

int dxl_gui_init(dxl_gui *g, int video, dxl_err *err) {
    memset(g, 0, sizeof *g);
    if (video) {
        /* The launcher's own handlers decide what a signal does. */
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            dxl_err_set(err, "SDL_Init: %s", SDL_GetError());
            return -1;
        }
        g->sdl_ready = 1;
    }
    if (TTF_Init() != 0) {
        dxl_err_set(err, "TTF_Init: %s", TTF_GetError());
        dxl_gui_quit(g);
        return -1;
    }
    g->ttf_ready = 1;

    const char *want = getenv("DXL_FONT");
    int found = want && *want && open_font(g, want);
    for (int i = 0; !found && wine_fonts[i]; i++) {
        char *p = first_match(wine_fonts[i]);
        found = p && open_font(g, p);
        free(p);
    }
    for (int i = 0; !found && sans_fonts[i]; i++) {
        char *p = first_match(sans_fonts[i]);
        found = p && open_font(g, p);
        free(p);
    }
    if (!found) {
        dxl_err_set(err, "no font found: set DXL_FONT to a .ttf or .fon");
        dxl_gui_quit(g);
        return -1;
    }
    /* Window.dll's -MulDiv(9, 96, 72); the message font's -11. */
    g->font_url = open_other(g, "DXL_FONT_URL", url_fonts, 12);
    g->font_msg = open_other(g, "DXL_FONT_MSG", msg_fonts, 11);
    if (!g->font_url || !g->font_msg) {
        dxl_err_set(err, "cannot open the fonts: %s", TTF_GetError());
        dxl_gui_quit(g);
        return -1;
    }
    TTF_SetFontStyle(g->font_url, TTF_STYLE_UNDERLINE);

    /* Wine keeps an icon's colours premultiplied by its alpha, each rounded
     * to the nearest, as it loads it (DrawIcon then adds what shows
     * through). */
    for (int i = 0; !g->error_icon.argb && wine_user32[i]; i++) {
        char *p = first_match(wine_user32[i]);
        if (p && dxl_pe_icon(p, 32513, 32, &g->error_icon, NULL) == 0) {
            for (int k = 0; k < g->error_icon.w * g->error_icon.h; k++) {
                Uint32 c = g->error_icon.argb[k], a = c >> 24;
                g->error_icon.argb[k] = a << 24 | ((c >> 16 & 0xff) * a + 127) / 255 << 16 |
                                        ((c >> 8 & 0xff) * a + 127) / 255 << 8 | ((c & 0xff) * a + 127) / 255;
            }
        }
        free(p);
    }
    return 0;
}

void dxl_gui_quit(dxl_gui *g) {
    TTF_Font *fonts[3] = { g->font, g->font_url, g->font_msg };
    for (int i = 0; i < 3; i++)
        if (fonts[i]) {
            forget_height(fonts[i]);
            TTF_CloseFont(fonts[i]);
        }
    free(g->font_path);
    dxl_icon_free(&g->error_icon);
    if (g->ttf_ready) TTF_Quit();
    if (g->sdl_ready) SDL_Quit();
    memset(g, 0, sizeof *g);
}

/* ---- drawing ------------------------------------------------------------ */

static void color(SDL_Renderer *r, dxl_rgb c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255); }

void dxl_fill(SDL_Renderer *r, SDL_Rect rc, dxl_rgb c) {
    if (rc.w <= 0 || rc.h <= 0) return;
    color(r, c);
    SDL_RenderFillRect(r, &rc);
}

void dxl_hline(SDL_Renderer *r, int x0, int x1, int y, dxl_rgb c) {
    if (x1 <= x0) return;
    color(r, c);
    SDL_RenderDrawLine(r, x0, y, x1 - 1, y);
}

void dxl_vline(SDL_Renderer *r, int x, int y0, int y1, dxl_rgb c) {
    if (y1 <= y0) return;
    color(r, c);
    SDL_RenderDrawLine(r, x, y0, x, y1 - 1);
}

/* One border: top and left in tl, bottom and right in br, as DrawEdge draws
 * it -- the top-left colour takes the corners at top right and bottom left. */
static SDL_Rect border(SDL_Renderer *r, SDL_Rect rc, dxl_rgb tl, dxl_rgb br) {
    int x0 = rc.x, y0 = rc.y, x1 = rc.x + rc.w, y1 = rc.y + rc.h;
    dxl_hline(r, x0, x1 - 1, y0, tl);
    dxl_vline(r, x0, y0, y1 - 1, tl);
    dxl_hline(r, x0, x1, y1 - 1, br);
    dxl_vline(r, x1 - 1, y0, y1 - 1, br);
    SDL_Rect in = { x0 + 1, y0 + 1, rc.w - 2, rc.h - 2 };
    return in;
}

SDL_Rect dxl_edge(SDL_Renderer *r, SDL_Rect rc, int edge) {
    const dxl_palette *p = &dxl_colors;
    int soft = (edge & DXL_BF_SOFT) != 0;
    if (edge & DXL_BDR_RAISEDOUTER) rc = border(r, rc, soft ? p->highlight : p->light, p->dkshadow);
    else if (edge & DXL_BDR_SUNKENOUTER) rc = border(r, rc, p->shadow, p->highlight);
    if (edge & DXL_BDR_RAISEDINNER) rc = border(r, rc, soft ? p->light : p->highlight, p->shadow);
    else if (edge & DXL_BDR_SUNKENINNER) rc = border(r, rc, p->dkshadow, p->light);
    return rc;
}

void dxl_focus_rect(SDL_Renderer *r, SDL_Rect rc, SDL_Point origin, dxl_rgb under) {
    SDL_SetRenderDrawColor(r, under.r ^ 0xff, under.g ^ 0xff, under.b ^ 0xff, 255);
    int x1 = rc.x + rc.w - 1, y1 = rc.y + rc.h - 1;
    for (int y = rc.y; y <= y1; y++)
        for (int x = rc.x; x <= x1; x++) {
            if (y != rc.y && y != y1 && x != rc.x && x != x1) {
                x = x1 - 1;                   /* inside: on to the right edge */
                continue;
            }
            if (((x - origin.x) + (y - origin.y)) & 1) SDL_RenderDrawPoint(r, x, y);
        }
}

/* ---- text --------------------------------------------------------------- */

/* The .int files are in the game's code page: text that is not UTF-8 is
 * read as Windows-1252's Latin-1 part. Caller frees. */
static char *utf8(const char *s, size_t n) {
    int valid = 1;
    for (size_t i = 0; i < n && valid; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) continue;
        int more = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
        if (more < 0 || i + (size_t)more >= n + 0) { valid = 0; break; }
        for (int k = 1; k <= more; k++)
            if (((unsigned char)s[i + (size_t)k] & 0xC0) != 0x80) valid = 0;
        i += (size_t)more;
    }
    if (valid) return dxl_xstrndup(s, n);
    char *out = dxl_xmalloc(n * 2 + 1), *o = out;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) *o++ = (char)c;
        else { *o++ = (char)(0xC0 | (c >> 6)); *o++ = (char)(0x80 | (c & 0x3F)); }
    }
    *o = '\0';
    return out;
}

/* The shown text of a line, its mnemonic's byte offset in it (or -1). */
static char *strip(const char *s, size_t n, int *mnemonic) {
    char *out = dxl_xmalloc(n + 1);
    size_t o = 0;
    *mnemonic = -1;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '&' && i + 1 < n) {
            if (s[i + 1] == '&') { out[o++] = '&'; i++; continue; }
            if (*mnemonic < 0) *mnemonic = (int)o;
            continue;
        }
        if (s[i] == '&') continue;
        out[o++] = s[i];
    }
    out[o] = '\0';
    return out;
}

static int width_of(TTF_Font *f, const char *s) {
    int w = 0, h = 0;
    if (*s) TTF_SizeUTF8(f, s, &w, &h);
    return w;
}

int dxl_text_width(const dxl_gui *g, TTF_Font *f, const char *text) {
    (void)g;
    int m;
    char *shown = strip(text, strlen(text), &m);
    char *u = utf8(shown, strlen(shown));
    int w = width_of(f, u);
    free(u);
    free(shown);
    return w;
}

static void draw_line(SDL_Renderer *r, TTF_Font *f, int x, int y, const char *s, size_t n,
                      dxl_rgb c) {
    int m;
    char *shown = strip(s, n, &m);
    char *u = utf8(shown, strlen(shown));
    if (*u) {
        SDL_Color col = { c.r, c.g, c.b, 255 };
        SDL_Surface *surf = TTF_RenderUTF8_Solid(f, u, col);
        if (surf) {
            SDL_Texture *t = SDL_CreateTextureFromSurface(r, surf);
            SDL_Rect dst = { x, y, surf->w, surf->h };
            if (t) { SDL_RenderCopy(r, t, NULL, &dst); SDL_DestroyTexture(t); }
            SDL_FreeSurface(surf);
        }
    }
    if (m >= 0 && shown[m]) {
        /* The mnemonic's underline, under its letter at the baseline, a
         * pixel short of the letter's width, as DrawText draws it. */
        char *pre = utf8(shown, (size_t)m);
        char one[8] = { 0 };
        size_t len = 1;
        unsigned char c0 = (unsigned char)shown[m];
        if (c0 >= 0x80) len = (c0 & 0xE0) == 0xC0 ? 2 : (c0 & 0xF0) == 0xE0 ? 3 : 4;
        memcpy(one, shown + m, len < sizeof one ? len : sizeof one - 1);
        char *ch = utf8(one, strlen(one));
        int x0 = x + width_of(f, pre), w = width_of(f, ch);
        int base = y + TTF_FontAscent(f) + 1;
        dxl_hline(r, x0, x0 + w - 1, base, c);
        free(ch);
        free(pre);
    }
    free(u);
    free(shown);
}

void dxl_text(SDL_Renderer *r, const dxl_gui *g, TTF_Font *f, int x, int y,
              const char *text, dxl_rgb c) {
    (void)g;
    draw_line(r, f, x, y, text, strlen(text), c);
}

static int measure(TTF_Font *f, const char *s, size_t n) {
    int m;
    char *shown = strip(s, n, &m);
    char *u = utf8(shown, strlen(shown));
    int w = width_of(f, u);
    free(u);
    free(shown);
    return w;
}

/* Where the character after the one at i starts: a byte on, or a UTF-8
 * sequence on when the paragraph is UTF-8. */
static size_t next_char(const char *s, size_t n, size_t i, int u8) {
    size_t k = i + 1;
    if (u8) while (k < n && ((unsigned char)s[k] & 0xC0) == 0x80) k++;
    return k;
}

static int is_utf8(const char *s, size_t n) {
    char *u = utf8(s, n);
    int same = strlen(u) == n && memcmp(u, s, n) == 0;
    free(u);
    return same;
}

/* One paragraph, wrapped at width as wine's DrawText wraps it. Of the
 * characters that fit, if the first that does not is a space, the line ends
 * before it and the next starts after it; else the line ends after the last
 * space before it (which counts in the line's width) and the next starts
 * there; a first word wider than the line stands alone. Returns the line's
 * byte length and where the one after starts. */
static size_t next_line(TTF_Font *f, const char *s, size_t n, int width, size_t *resume) {
    if (measure(f, s, n) <= width) {
        *resume = n;
        return n;
    }
    /* GetTextExtentExPoint: the characters that fit, found by halving. */
    int u8 = is_utf8(s, n);
    size_t *at = dxl_xmalloc((n + 1) * sizeof *at), count = 0;
    for (size_t i = 0; i < n; i = next_char(s, n, i, u8)) at[count++] = i;
    at[count] = n;
    size_t lo = 0, hi = count;             /* at[lo] fits; at[hi] does not */
    while (hi - lo > 1) {
        size_t mid = (lo + hi) / 2;
        if (measure(f, s, at[mid]) <= width) lo = mid;
        else hi = mid;
    }
    size_t k = at[lo];                     /* the first character that does not fit */
    free(at);
    if (k > 0 && s[k] == ' ') {
        *resume = k + 1;
        return k;
    }
    for (size_t j = k; j-- > 0;)
        if (s[j] == ' ') {
            *resume = j + 1;
            return j + 1;
        }
    size_t end = k;
    while (end < n && s[end] != ' ') end++;
    *resume = end < n ? end + 1 : end;
    return end;
}

int dxl_text_wrapped(SDL_Renderer *r, const dxl_gui *g, TTF_Font *f, SDL_Rect rc,
                     const char *text, dxl_rgb c, int *width) {
    int y = 0, widest = 0;
    const char *p = text;
    while (1) {
        const char *eol = p;
        while (*eol && *eol != '\n' && *eol != '\r') eol++;
        size_t n = (size_t)(eol - p);
        size_t at = 0;
        do {
            size_t resume;
            size_t len = next_line(f, p + at, n - at, rc.w, &resume);
            int w = measure(f, p + at, len);
            if (w > widest) widest = w;
            if (r) draw_line(r, f, rc.x, rc.y + y, p + at, len, c);
            y += dxl_font_height(f);
            at += resume;
        } while (at < n);
        if (!*eol) break;
        if (eol[0] == '\r' && eol[1] == '\n') eol++;
        p = eol + 1;
        if (!*p) break;
    }
    if (width) *width = widest;
    return y;
}

int dxl_mnemonic(const char *text) {
    for (const char *p = text; *p; p++) {
        if (*p != '&') continue;
        if (p[1] == '&') { p++; continue; }
        if (p[1]) {
            int c = (unsigned char)p[1];
            return (c >= 'A' && c <= 'Z') ? c + 32 : c;
        }
    }
    return 0;
}

/* ---- bitmaps ------------------------------------------------------------ */

SDL_Texture *dxl_bitmap(SDL_Renderer *r, const char *path, int *w, int *h) {
    SDL_Surface *s = path ? SDL_LoadBMP(path) : NULL;
    if (!s) return NULL;
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    if (w) *w = s->w;
    if (h) *h = s->h;
    SDL_FreeSurface(s);
    return t;
}

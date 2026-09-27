#include "ui.h"

#include "platform/target.h"

#include <SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_MAX 160
#define REPEAT_DELAY_MS  320
#define REPEAT_RATE_MS    90
#define STICK_THRESHOLD 16000

typedef struct {
    char        *text;
    dxl_txt      style;
    SDL_Color    color;
    SDL_Texture *tex;
    int          w, h;
    unsigned     stamp;
} cache_entry;

struct dxl_ui {
    SDL_Window   *win;
    SDL_Renderer *ren;
    int           w, h;
    dxl_metrics   m;

    TTF_Font *f_title, *f_body, *f_item, *f_footer;
    int       headless;

    cache_entry cache[CACHE_MAX];
    unsigned    clock;

    SDL_GameController *pad;

    /* Auto-repeat state for whichever direction is currently held. */
    dxl_act  held;
    Uint32   held_since, last_repeat;
    /* Edge detection for the analog stick, which reports continuously. */
    int      stick_dir;
};

static TTF_Font *font_for(dxl_ui *ui, dxl_txt s) {
    switch (s) {
    case DXL_TXT_TITLE:  return ui->f_title;
    case DXL_TXT_BODY:   return ui->f_body;
    case DXL_TXT_ITEM:   return ui->f_item;
    case DXL_TXT_FOOTER: return ui->f_footer;
    }
    return ui->f_body;
}

dxl_ui *dxl_ui_init(dxl_err *err) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        dxl_err_set(err, "SDL_Init: %s", SDL_GetError());
        return NULL;
    }
    dxl_ui *ui = dxl_xmalloc(sizeof *ui);
    memset(ui, 0, sizeof *ui);

    /* Fullscreen-desktop rather than a mode switch: the vendor "mali" driver
     * hands out one surface at the panel's native size, and asking it to
     * change modes gains nothing on a fixed screen. DXL_WINDOW=WxH asks for
     * a plain window instead -- for working on the UI at the panel's size
     * on a desktop, and for the screenshot tool. */
    const dxl_target *t = dxl_target_get();
    int ww = t->panel_w, wh = t->panel_h;
    Uint32 wflags = SDL_WINDOW_FULLSCREEN_DESKTOP;
    const char *wenv = SDL_getenv("DXL_WINDOW");
    if (wenv && sscanf(wenv, "%dx%d", &ww, &wh) == 2 && ww > 0 && wh > 0)
        wflags = 0;
    ui->win = SDL_CreateWindow("Deus Ex", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, ww, wh, wflags);
    if (!ui->win) {
        dxl_err_set(err, "SDL_CreateWindow: %s", SDL_GetError());
        free(ui);
        SDL_Quit();
        return NULL;
    }
    ui->ren = SDL_CreateRenderer(ui->win, -1, SDL_RENDERER_ACCELERATED |
                                              SDL_RENDERER_PRESENTVSYNC);
    if (!ui->ren) ui->ren = SDL_CreateRenderer(ui->win, -1, 0);   /* software */
    if (!ui->ren) {
        dxl_err_set(err, "SDL_CreateRenderer: %s", SDL_GetError());
        SDL_DestroyWindow(ui->win);
        free(ui);
        SDL_Quit();
        return NULL;
    }
    SDL_SetRenderDrawBlendMode(ui->ren, SDL_BLENDMODE_BLEND);
    SDL_GetRendererOutputSize(ui->ren, &ui->w, &ui->h);
    if (ui->w <= 0 || ui->h <= 0) { ui->w = t->panel_w; ui->h = t->panel_h; }
    dxl_metrics_for(&ui->m, ui->h);
    SDL_ShowCursor(SDL_DISABLE);

    if (TTF_Init() == 0) {
        const char *path = dxl_theme_find_font();
        if (path) {
            ui->f_title  = TTF_OpenFont(path, ui->m.title_size);
            ui->f_body   = TTF_OpenFont(path, ui->m.body_size);
            ui->f_item   = TTF_OpenFont(path, ui->m.item_size);
            ui->f_footer = TTF_OpenFont(path, ui->m.footer_size);
        }
    }
    ui->headless = !(ui->f_title && ui->f_body && ui->f_item && ui->f_footer);

    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            ui->pad = SDL_GameControllerOpen(i);
            if (ui->pad) break;
        }
    }
    return ui;
}

void dxl_ui_quit(dxl_ui *ui) {
    if (!ui) return;
    for (int i = 0; i < CACHE_MAX; i++) {
        if (ui->cache[i].tex) SDL_DestroyTexture(ui->cache[i].tex);
        free(ui->cache[i].text);
    }
    if (ui->f_title)  TTF_CloseFont(ui->f_title);
    if (ui->f_body)   TTF_CloseFont(ui->f_body);
    if (ui->f_item)   TTF_CloseFont(ui->f_item);
    if (ui->f_footer) TTF_CloseFont(ui->f_footer);
    if (TTF_WasInit()) TTF_Quit();
    if (ui->pad) SDL_GameControllerClose(ui->pad);
    if (ui->ren) SDL_DestroyRenderer(ui->ren);
    if (ui->win) SDL_DestroyWindow(ui->win);
    free(ui);
    SDL_Quit();
}

int dxl_ui_width (const dxl_ui *ui) { return ui->w; }
int dxl_ui_height(const dxl_ui *ui) { return ui->h; }
const dxl_metrics *dxl_ui_metrics(const dxl_ui *ui) { return &ui->m; }
int dxl_ui_headless(const dxl_ui *ui) { return ui->headless; }

const char *dxl_ui_pad_name(const dxl_ui *ui) {
    return ui->pad ? SDL_GameControllerName(ui->pad) : NULL;
}

/* ---- text cache ------------------------------------------------------
 * Rendering every string through SDL_ttf each frame is the easy way to make
 * a 60fps menu drop frames on a Cortex-A53. Strings here are stable between
 * frames, so cache the textures and evict the least recently used. */

static cache_entry *cache_get(dxl_ui *ui, const char *s, dxl_txt style,
                              SDL_Color c) {
    cache_entry *lru = &ui->cache[0];
    for (int i = 0; i < CACHE_MAX; i++) {
        cache_entry *e = &ui->cache[i];
        if (e->tex && e->style == style && e->text && strcmp(e->text, s) == 0 &&
            e->color.r == c.r && e->color.g == c.g && e->color.b == c.b &&
            e->color.a == c.a) {
            e->stamp = ++ui->clock;
            return e;
        }
        if (!e->tex) { lru = e; break; }
        if (e->stamp < lru->stamp) lru = e;
    }

    TTF_Font *f = font_for(ui, style);
    if (!f) return NULL;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(f, s, c);
    if (!surf) return NULL;

    if (lru->tex) SDL_DestroyTexture(lru->tex);
    free(lru->text);

    lru->tex   = SDL_CreateTextureFromSurface(ui->ren, surf);
    lru->w     = surf->w;
    lru->h     = surf->h;
    lru->text  = dxl_xstrdup(s);
    lru->style = style;
    lru->color = c;
    lru->stamp = ++ui->clock;
    SDL_FreeSurface(surf);
    return lru->tex ? lru : NULL;
}

void dxl_ui_text(dxl_ui *ui, int x, int y, const char *s, dxl_txt style,
                 SDL_Color c) {
    if (!s || !*s || ui->headless) return;
    cache_entry *e = cache_get(ui, s, style, c);
    if (!e) return;
    SDL_Rect dst = { x, y, e->w, e->h };
    SDL_RenderCopy(ui->ren, e->tex, NULL, &dst);
}

int dxl_ui_text_width(dxl_ui *ui, const char *s, dxl_txt style) {
    if (!s || !*s || ui->headless) return 0;
    int w = 0, h = 0;
    TTF_Font *f = font_for(ui, style);
    if (f) TTF_SizeUTF8(f, s, &w, &h);
    return w;
}

int dxl_ui_line_height(dxl_ui *ui, dxl_txt style) {
    TTF_Font *f = font_for(ui, style);
    return f ? TTF_FontLineSkip(f) : ui->m.line_gap;
}

void dxl_ui_fill(dxl_ui *ui, SDL_Rect r, SDL_Color c) {
    SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(ui->ren, &r);
}

static void outline(dxl_ui *ui, SDL_Rect r, SDL_Color c) {
    SDL_SetRenderDrawColor(ui->ren, c.r, c.g, c.b, c.a);
    SDL_RenderDrawRect(ui->ren, &r);
}

/* A small solid triangle pointing left (dir < 0) or right, centred on
 * (cx, cy). Drawn rather than typed: the device font's coverage of arrow
 * glyphs is unknown, and a missing glyph renders as a box. */
static void arrow(dxl_ui *ui, int cx, int cy, int size, int dir, SDL_Color c) {
    SDL_Vertex v[3];
    float h = (float)size, w = (float)size * 0.6f;
    float tip = (float)cx + (dir < 0 ? -w : w) / 2, base = (float)cx - (dir < 0 ? -w : w) / 2;
    v[0].position = (SDL_FPoint){ tip,  (float)cy };
    v[1].position = (SDL_FPoint){ base, (float)cy - h / 2 };
    v[2].position = (SDL_FPoint){ base, (float)cy + h / 2 };
    for (int i = 0; i < 3; i++) {
        v[i].color = c;
        v[i].tex_coord = (SDL_FPoint){ 0, 0 };
    }
    SDL_RenderGeometry(ui->ren, NULL, v, 3, NULL, 0);
}

int dxl_ui_paragraph_n(dxl_ui *ui, int x, int y, int max_w, const char *s,
                       dxl_txt style, SDL_Color c, int max_lines) {
    if (!s || !*s) return y;
    int lh = dxl_ui_line_height(ui, style);
    if (ui->headless) return y + lh;

    char line[512];
    size_t len = 0;
    int lines = 0;
    line[0] = '\0';

    const char *p = s;
    while (*p) {
        /* Explicit newlines break the line. */
        if (*p == '\n') {
            p++;
            if (max_lines > 0 && lines + 1 >= max_lines && *p) {
                snprintf(line + len, sizeof line - len, "...");
                break;
            }
            dxl_ui_text(ui, x, y, line, style, c);
            y += lh; lines++;
            line[0] = '\0'; len = 0;
            continue;
        }
        const char *ws = p;
        while (*p && *p != ' ' && *p != '\n') p++;
        size_t wlen = (size_t)(p - ws);
        while (*p == ' ') p++;

        char candidate[512];
        int n = snprintf(candidate, sizeof candidate, "%s%s%.*s",
                         line, len ? " " : "", (int)wlen, ws);
        if (n < 0) break;

        if (len && dxl_ui_text_width(ui, candidate, style) > max_w) {
            if (max_lines > 0 && lines + 1 >= max_lines) {
                snprintf(line + len, sizeof line - len, "...");
                len = strlen(line);
                p = "";   /* stop */
                break;
            }
            dxl_ui_text(ui, x, y, line, style, c);
            y += lh; lines++;
            snprintf(line, sizeof line, "%.*s", (int)wlen, ws);
        } else {
            snprintf(line, sizeof line, "%s", candidate);
        }
        len = strlen(line);
    }
    if (line[0]) { dxl_ui_text(ui, x, y, line, style, c); y += lh; }
    return y;
}

int dxl_ui_paragraph(dxl_ui *ui, int x, int y, int max_w, const char *s,
                     dxl_txt style, SDL_Color c) {
    return dxl_ui_paragraph_n(ui, x, y, max_w, s, style, c, 0);
}

int dxl_ui_header(dxl_ui *ui, const char *title) {
    int x = ui->m.pad, y = ui->m.pad;
    dxl_ui_text(ui, x, y, title, DXL_TXT_TITLE, DXL_PAL.accent);
    y += dxl_ui_line_height(ui, DXL_TXT_TITLE) + ui->m.pad / 4;
    dxl_ui_fill(ui, (SDL_Rect){ x, y, ui->w - 2 * ui->m.pad, 2 }, DXL_PAL.rule);
    return y + ui->m.pad / 2;
}

/* A button name in a filled chip, the way handheld UIs label controls.
 * Returns the x after it. */
static int chip(dxl_ui *ui, int x, int y, const char *button, SDL_Color bg) {
    int lh = dxl_ui_line_height(ui, DXL_TXT_FOOTER);
    int tw = dxl_ui_text_width(ui, button, DXL_TXT_FOOTER);
    int padx = lh / 3;
    dxl_ui_fill(ui, (SDL_Rect){ x, y, tw + 2 * padx, lh }, bg);
    dxl_ui_text(ui, x + padx, y, button, DXL_TXT_FOOTER, DXL_PAL.bg);
    return x + tw + 2 * padx;
}

int dxl_ui_tabbar(dxl_ui *ui, const char *const *labels, int count, int active) {
    int pad = ui->m.pad;
    int y = pad / 2;
    int lh = dxl_ui_line_height(ui, DXL_TXT_ITEM);
    int bar_h = lh + pad / 2;

    /* L1 / R1 at the ends, the tabs spread between them. */
    int flh = dxl_ui_line_height(ui, DXL_TXT_FOOTER);
    int cy = y + (bar_h - flh) / 2;
    int left_end = chip(ui, pad, cy, "L1", DXL_PAL.text_dim) + pad / 2;
    int r1w = dxl_ui_text_width(ui, "R1", DXL_TXT_FOOTER) + 2 * (flh / 3);
    int right_start = ui->w - pad - r1w;
    chip(ui, right_start, cy, "R1", DXL_PAL.text_dim);
    right_start -= pad / 2;

    int span = right_start - left_end;
    int slot = count > 0 ? span / count : span;
    for (int i = 0; i < count; i++) {
        int sx = left_end + i * slot;
        int tw = dxl_ui_text_width(ui, labels[i], DXL_TXT_ITEM);
        int tx = sx + (slot - tw) / 2;
        int ty = y + (bar_h - lh) / 2;
        if (i == active) {
            dxl_ui_fill(ui, (SDL_Rect){ sx + 4, y, slot - 8, bar_h }, DXL_PAL.panel_sel);
            dxl_ui_fill(ui, (SDL_Rect){ sx + 4, y + bar_h - 3, slot - 8, 3 }, DXL_PAL.accent);
        }
        dxl_ui_text(ui, tx, ty, labels[i], DXL_TXT_ITEM,
                    i == active ? DXL_PAL.accent : DXL_PAL.text_dim);
    }
    y += bar_h;
    dxl_ui_fill(ui, (SDL_Rect){ pad, y, ui->w - 2 * pad, 2 }, DXL_PAL.rule);
    return y + pad / 2;
}

int dxl_ui_footer(dxl_ui *ui, const char *const *pairs, int npairs) {
    int lh = dxl_ui_line_height(ui, DXL_TXT_FOOTER);
    int y  = ui->h - ui->m.pad / 2 - lh;
    int top = y - ui->m.pad / 4;
    dxl_ui_fill(ui, (SDL_Rect){ ui->m.pad, top, ui->w - 2 * ui->m.pad, 1 }, DXL_PAL.rule);
    int x = ui->m.pad;
    for (int i = 0; i + 1 < 2 * npairs; i += 2) {
        x = chip(ui, x, y, pairs[i], DXL_PAL.accent_dim) + lh / 3;
        dxl_ui_text(ui, x, y, pairs[i + 1], DXL_TXT_FOOTER, DXL_PAL.text_dim);
        x += dxl_ui_text_width(ui, pairs[i + 1], DXL_TXT_FOOTER) + lh;
    }
    return top;
}

void dxl_ui_row(dxl_ui *ui, int y, const char *label, const char *value,
                int selected, int flags) {
    int x = ui->m.pad, w = ui->w - 2 * ui->m.pad;
    int ih = ui->m.item_height;
    int disabled = flags & DXL_ROW_DISABLED;

    if (selected) {
        dxl_ui_fill(ui, (SDL_Rect){ x, y, w, ih }, DXL_PAL.panel_sel);
        dxl_ui_fill(ui, (SDL_Rect){ x, y, 4, ih }, DXL_PAL.accent);
    }
    SDL_Color c = disabled ? DXL_PAL.text_dim
                : selected ? DXL_PAL.accent : DXL_PAL.text;

    int lh  = dxl_ui_line_height(ui, DXL_TXT_ITEM);
    int ty  = y + (ih - lh) / 2;
    dxl_ui_text(ui, x + ui->m.pad / 2, ty, label, DXL_TXT_ITEM, c);

    if (value && *value) {
        int vw = dxl_ui_text_width(ui, value, DXL_TXT_ITEM);
        int right = x + w - ui->m.pad / 2;
        int show_arrows = selected && (flags & DXL_ROW_ARROWS) && !disabled;
        int asz = lh / 2;
        if (show_arrows) right -= asz + asz / 2;
        int vx = right - vw;
        dxl_ui_text(ui, vx, ty, value, DXL_TXT_ITEM,
                    disabled ? DXL_PAL.text_dim : selected ? DXL_PAL.text : DXL_PAL.text_dim);
        if (show_arrows) {
            arrow(ui, vx - asz, y + ih / 2, asz, -1, DXL_PAL.accent);
            arrow(ui, right + asz, y + ih / 2, asz, +1, DXL_PAL.accent);
        }
    }
}

void dxl_ui_slider_row(dxl_ui *ui, int y, const char *label, double frac,
                       const char *value_text, int selected, int flags) {
    dxl_ui_row(ui, y, label, NULL, selected, flags);
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;

    int ih = ui->m.item_height;
    int right = ui->w - ui->m.pad - ui->m.pad / 2;
    int vw = dxl_ui_text_width(ui, "000%", DXL_TXT_ITEM);
    int bar_w = ui->w / 5;
    int bar_h = ih / 5;
    int bar_x = right - vw - ui->m.pad / 2 - bar_w;
    int bar_y = y + (ih - bar_h) / 2;
    int disabled = flags & DXL_ROW_DISABLED;

    dxl_ui_fill(ui, (SDL_Rect){ bar_x, bar_y, bar_w, bar_h }, DXL_PAL.rule);
    dxl_ui_fill(ui, (SDL_Rect){ bar_x, bar_y, (int)(bar_w * frac + 0.5), bar_h },
                disabled ? DXL_PAL.text_dim : selected ? DXL_PAL.accent : DXL_PAL.accent_dim);
    if (selected && !disabled) {
        int asz = dxl_ui_line_height(ui, DXL_TXT_ITEM) / 2;
        arrow(ui, bar_x - asz, y + ih / 2, asz, -1, DXL_PAL.accent);
        arrow(ui, bar_x + bar_w + asz, y + ih / 2, asz, +1, DXL_PAL.accent);
    }
    if (value_text) {
        int tw = dxl_ui_text_width(ui, value_text, DXL_TXT_ITEM);
        int lh = dxl_ui_line_height(ui, DXL_TXT_ITEM);
        dxl_ui_text(ui, right - tw, y + (ih - lh) / 2, value_text, DXL_TXT_ITEM,
                    selected ? DXL_PAL.text : DXL_PAL.text_dim);
    }
}

void dxl_ui_overlay_panel(dxl_ui *ui, SDL_Rect r) {
    dxl_ui_fill(ui, (SDL_Rect){ 0, 0, ui->w, ui->h }, (SDL_Color){ 0, 0, 0, 170 });
    dxl_ui_fill(ui, r, DXL_PAL.panel);
    outline(ui, r, DXL_PAL.accent_dim);
}

void dxl_ui_frame_begin(dxl_ui *ui) {
    SDL_SetRenderDrawColor(ui->ren, DXL_PAL.bg.r, DXL_PAL.bg.g, DXL_PAL.bg.b, 255);
    SDL_RenderClear(ui->ren);
}

void dxl_ui_frame_end(dxl_ui *ui) { SDL_RenderPresent(ui->ren); }

/* ---- input ----------------------------------------------------------- */

static dxl_act from_button(SDL_GameControllerButton b) {
    switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       return DXL_ACT_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return DXL_ACT_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return DXL_ACT_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return DXL_ACT_RIGHT;
    /* The pad reports as an Xbox 360 controller, so SDL's A/B are already the
     * physical A/B on this shell -- confirmed with Port Ex Machina's tools/probes/probe-sdl.c. */
    case SDL_CONTROLLER_BUTTON_A:             return DXL_ACT_CONFIRM;
    case SDL_CONTROLLER_BUTTON_B:             return DXL_ACT_BACK;
    case SDL_CONTROLLER_BUTTON_X:             return DXL_ACT_ALT;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return DXL_ACT_TAB_PREV;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return DXL_ACT_TAB_NEXT;
    case SDL_CONTROLLER_BUTTON_START:         return DXL_ACT_START;
    case SDL_CONTROLLER_BUTTON_BACK:          return DXL_ACT_QUIT;   /* SELECT */
    case SDL_CONTROLLER_BUTTON_GUIDE:         return DXL_ACT_QUIT;   /* MENU   */
    default:                                  return DXL_ACT_NONE;
    }
}

static dxl_act from_key(SDL_Keycode k) {
    switch (k) {
    case SDLK_UP:     case SDLK_w: return DXL_ACT_UP;
    case SDLK_DOWN:   case SDLK_s: return DXL_ACT_DOWN;
    case SDLK_LEFT:   case SDLK_a: return DXL_ACT_LEFT;
    case SDLK_RIGHT:  case SDLK_d: return DXL_ACT_RIGHT;
    case SDLK_RETURN: case SDLK_SPACE: case SDLK_z: return DXL_ACT_CONFIRM;
    case SDLK_BACKSPACE: case SDLK_x: return DXL_ACT_BACK;
    case SDLK_r:                   return DXL_ACT_ALT;
    case SDLK_PAGEUP: case SDLK_COMMA:    return DXL_ACT_TAB_PREV;
    case SDLK_PAGEDOWN: case SDLK_PERIOD: case SDLK_TAB: return DXL_ACT_TAB_NEXT;
    case SDLK_p: case SDLK_F5:     return DXL_ACT_START;
    case SDLK_ESCAPE: case SDLK_q: return DXL_ACT_QUIT;
    default: return DXL_ACT_NONE;
    }
}

static int is_direction(dxl_act a) {
    return a == DXL_ACT_UP || a == DXL_ACT_DOWN ||
           a == DXL_ACT_LEFT || a == DXL_ACT_RIGHT;
}

dxl_act dxl_ui_poll(dxl_ui *ui) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            return DXL_ACT_QUIT;

        case SDL_CONTROLLERDEVICEADDED:
            if (!ui->pad) ui->pad = SDL_GameControllerOpen(e.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (ui->pad &&
                SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(ui->pad)) == e.cdevice.which) {
                SDL_GameControllerClose(ui->pad);
                ui->pad = NULL;
            }
            break;

        case SDL_CONTROLLERBUTTONDOWN: {
            dxl_act a = from_button((SDL_GameControllerButton)e.cbutton.button);
            if (is_direction(a)) {
                ui->held = a;
                ui->held_since = ui->last_repeat = SDL_GetTicks();
            }
            if (a != DXL_ACT_NONE) return a;
            break;
        }
        case SDL_CONTROLLERBUTTONUP: {
            dxl_act a = from_button((SDL_GameControllerButton)e.cbutton.button);
            if (a == ui->held) ui->held = DXL_ACT_NONE;
            break;
        }

        case SDL_CONTROLLERAXISMOTION: {
            /* Turn the continuous stick into discrete steps, one per crossing
             * of the threshold, so a held stick repeats at the same rate as
             * the d-pad instead of scrolling uncontrollably. */
            int vertical = (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY);
            int horizontal = (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX);
            if (!vertical && !horizontal) break;

            int v = e.caxis.value;
            int dir = (v > STICK_THRESHOLD) ? 1 : (v < -STICK_THRESHOLD) ? -1 : 0;
            int code = dir == 0 ? 0 : (vertical ? dir * 2 : dir);
            if (code == ui->stick_dir) break;
            ui->stick_dir = code;
            if (dir == 0) { ui->held = DXL_ACT_NONE; break; }

            dxl_act a = vertical ? (dir > 0 ? DXL_ACT_DOWN : DXL_ACT_UP)
                                 : (dir > 0 ? DXL_ACT_RIGHT : DXL_ACT_LEFT);
            ui->held = a;
            ui->held_since = ui->last_repeat = SDL_GetTicks();
            return a;
        }

        case SDL_KEYDOWN: {
            if (e.key.repeat) break;   /* we do our own repeat */
            dxl_act a = from_key(e.key.keysym.sym);
            if (is_direction(a)) {
                ui->held = a;
                ui->held_since = ui->last_repeat = SDL_GetTicks();
            }
            if (a != DXL_ACT_NONE) return a;
            break;
        }
        case SDL_KEYUP: {
            dxl_act a = from_key(e.key.keysym.sym);
            if (a == ui->held) ui->held = DXL_ACT_NONE;
            break;
        }
        default:
            break;
        }
    }

    if (ui->held != DXL_ACT_NONE) {
        Uint32 now = SDL_GetTicks();
        if (now - ui->held_since >= REPEAT_DELAY_MS &&
            now - ui->last_repeat >= REPEAT_RATE_MS) {
            ui->last_repeat = now;
            return ui->held;
        }
    }
    return DXL_ACT_NONE;
}

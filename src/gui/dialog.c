#include "gui/dialog.h"

#include <stdlib.h>
#include <string.h>

#define ITEM_H 13          /* a list row: the font's height */
#define DOUBLE_CLICK_MS 500

/* ---- the bitmaps Windows draws check boxes and radio buttons from --------
 * As the original's show under wine here, one character a pixel: s shadow,
 * D dark shadow, l light, w the window colour, . the dialog's face,
 * # the text colour. */

static const char *const check_box[13] = {
    "ssssssssssssw",
    "sDDDDDDDDDDlw",
    "sDwwwwwwwwwlw",
    "sDwwwwwwwwwlw",
    "sDwwwwwwwwwlw",
    "sDwwwwwwwwwlw",
    "sDwwwwwwwwwlw",
    "sDwwwwwwwwwlw",
    "sDwwwwwwwwwlw",
    "sDwwwwwwwwwlw",
    "sDwwwwwwwwwlw",
    "slllllllllllw",
    "wwwwwwwwwwwww",
};
/* The tick, over the box's inside from its third row and column. */
static const char *const check_mark[8] = {
    "........",
    ".......#",
    "......##",
    ".....###",
    ".#..###.",
    ".#####..",
    ".####...",
    "..##....",
};
static const char *const radio_off[12] = {
    "....ssss....",
    "..ssDDDDss..",
    ".ssDwwwwD.w.",
    ".sDwwwwwwlw.",
    "sDwwwwwwwwlw",
    "sDwwwwwwwwlw",
    "sDwwwwwwwwlw",
    "sDwwwwwwwwlw",
    ".sDwwwwwwlw.",
    ".wwlwwwwlww.",
    "..wwllllww..",
    "....wwww....",
};
static const char *const radio_on[12] = {
    "....ssss....",
    "..ssDDDDss..",
    ".ssDwwwwD.w.",
    ".sDwwwwwwlw.",
    "sDwww##wwwlw",
    "sDww####wwlw",
    "sDww####wwlw",
    "sDwww##wwwlw",
    ".sDwwwwwwlw.",
    ".wwlwwwwlww.",
    "..wwllllww..",
    "....wwww....",
};

static void pattern(SDL_Renderer *r, int x0, int y0, const char *const *rows, int n,
                    int transparent_face) {
    const dxl_palette *p = &dxl_colors;
    for (int y = 0; y < n; y++)
        for (int x = 0; rows[y][x]; x++) {
            dxl_rgb c;
            switch (rows[y][x]) {
            case 's': c = p->shadow; break;
            case 'D': c = p->dkshadow; break;
            case 'l': c = p->light; break;
            case 'w': c = p->window; break;
            case '#': c = p->text; break;
            default:
                if (transparent_face) continue;
                c = p->face;
            }
            SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
            SDL_RenderDrawPoint(r, x0 + x, y0 + y);
        }
}

/* ---- the dialog ---------------------------------------------------------- */

dxl_dialog *dxl_dialog_open(dxl_gui *g, const char *title, int w, int h, int offscreen,
                            dxl_err *err) {
    dxl_dialog *d = dxl_xmalloc(sizeof *d);
    memset(d, 0, sizeof *d);
    d->gui = g;
    d->font = g->font;
    d->line_h = g->line_h;
    d->w = w;
    d->h = h;
    d->focus = d->hover = d->pressed = d->last_click_ctl = -1;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (offscreen) {
        d->surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        d->ren = d->surface ? SDL_CreateSoftwareRenderer(d->surface) : NULL;
    } else {
        d->win = SDL_CreateWindow(title ? title : "", SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_HIDDEN);
        if (d->win) {
            d->ren = SDL_CreateRenderer(d->win, -1, SDL_RENDERER_ACCELERATED);
            if (!d->ren) d->ren = SDL_CreateRenderer(d->win, -1, SDL_RENDERER_SOFTWARE);
        }
    }
    if (!d->ren) {
        dxl_err_set(err, "cannot open a window: %s", SDL_GetError());
        dxl_dialog_close(d);
        return NULL;
    }
    d->dirty = 1;
    return d;
}

static void ctl_free(dxl_ctl *c) {
    free(c->text);
    for (int i = 0; i < c->item_count; i++) free(c->items[i]);
    free(c->items);
    if (c->bitmap) SDL_DestroyTexture(c->bitmap);
}

void dxl_dialog_close(dxl_dialog *d) {
    if (!d) return;
    for (int i = 0; i < d->count; i++) ctl_free(&d->ctls[i]);
    free(d->ctls);
    if (d->ren) SDL_DestroyRenderer(d->ren);
    if (d->win) SDL_DestroyWindow(d->win);
    if (d->surface) SDL_FreeSurface(d->surface);
    free(d);
}

void dxl_dialog_set_title(dxl_dialog *d, const char *title) {
    if (d->win) SDL_SetWindowTitle(d->win, title ? title : "");
}

void dxl_dialog_set_font(dxl_dialog *d, TTF_Font *font) {
    d->font = font;
    d->line_h = dxl_font_height(font);
    d->dirty = 1;
}

dxl_ctl *dxl_dialog_add(dxl_dialog *d, dxl_ctl_kind kind, int id, SDL_Rect rc,
                        const char *text, int layer) {
    if (d->count == d->cap) {
        d->cap = d->cap ? d->cap * 2 : 16;
        d->ctls = dxl_xrealloc(d->ctls, (size_t)d->cap * sizeof *d->ctls);
    }
    dxl_ctl *c = &d->ctls[d->count++];
    memset(c, 0, sizeof *c);
    c->kind = kind;
    c->id = id;
    c->rc = rc;
    c->text = dxl_xstrdup(text ? text : "");
    c->layer = layer;
    c->visible = 1;
    c->selected = -1;
    d->dirty = 1;
    return c;
}

dxl_ctl *dxl_dialog_find(dxl_dialog *d, int id, int layer) {
    for (int i = 0; i < d->count; i++)
        if (d->ctls[i].id == id && d->ctls[i].layer == layer) return &d->ctls[i];
    return NULL;
}

void dxl_dialog_remove_layer(dxl_dialog *d, int layer) {
    int o = 0;
    for (int i = 0; i < d->count; i++) {
        if (d->ctls[i].layer == layer) { ctl_free(&d->ctls[i]); continue; }
        d->ctls[o++] = d->ctls[i];
    }
    d->count = o;
    d->focus = d->hover = d->pressed = d->last_click_ctl = -1;
    d->dirty = 1;
}

void dxl_ctl_set_text(dxl_dialog *d, dxl_ctl *c, const char *text) {
    free(c->text);
    c->text = dxl_xstrdup(text ? text : "");
    d->dirty = 1;
}

void dxl_ctl_add_item(dxl_dialog *d, dxl_ctl *c, const char *item) {
    c->items = dxl_xrealloc(c->items, (size_t)(c->item_count + 1) * sizeof *c->items);
    c->items[c->item_count++] = dxl_xstrdup(item);
    d->dirty = 1;
}

void dxl_ctl_clear_items(dxl_dialog *d, dxl_ctl *c) {
    for (int i = 0; i < c->item_count; i++) free(c->items[i]);
    free(c->items);
    c->items = NULL;
    c->item_count = 0;
    c->selected = -1;
    d->dirty = 1;
}

void dxl_ctl_set_bitmap(dxl_dialog *d, dxl_ctl *c, const char *bmp_path) {
    if (c->bitmap) SDL_DestroyTexture(c->bitmap);
    c->bitmap = dxl_bitmap(d->ren, bmp_path, &c->bitmap_w, &c->bitmap_h);
    if (c->bitmap) {
        int frame = c->kind == DXL_CTL_BITMAP ? 4 : 0;
        c->rc.w = c->bitmap_w + frame;
        c->rc.h = c->bitmap_h + frame;
    }
    d->dirty = 1;
}

static int takes_focus(const dxl_ctl *c) { return c->visible && c->tabstop; }

void dxl_dialog_focus_first(dxl_dialog *d) {
    d->focus = -1;
    for (int i = 0; i < d->count; i++)
        if (takes_focus(&d->ctls[i])) { d->focus = i; break; }
    d->dirty = 1;
}

/* ---- drawing -------------------------------------------------------------- */

/* DrawText's DT_VCENTER: the line's top in a rectangle of height h. */
static int dt_vcenter(int top, int h, int cy) { return top + h / 2 - cy / 2; }

/* A button's label as wine places it: measured with DT_VCENTER, which
 * makes the measured rectangle taller than the text, centred, and centred
 * again inside that -- a default button's (in its outline) sits a pixel
 * higher than the others'. */
static int button_label_top(int top, int h, int cy) {
    int n = h / 2 - cy / 2 + cy;
    return dt_vcenter(top + (h - n) / 2, n, cy);
}

/* Whether a push button shows as the default: the focused one while a push
 * button has the focus, as the dialog manager moves the default with it;
 * else the one made default. */
static int shown_default(const dxl_dialog *d, int i) {
    if (d->focus >= 0 && d->ctls[d->focus].kind == DXL_CTL_PUSH) return d->focus == i;
    return d->ctls[i].is_default;
}

static void draw_ctl(dxl_dialog *d, int i) {
    dxl_ctl *c = &d->ctls[i];
    SDL_Renderer *r = d->ren;
    const dxl_palette *p = &dxl_colors;
    dxl_gui *g = d->gui;
    int focused = d->focus == i, pressed = d->pressed == i, hover = d->hover == i;
    SDL_Rect rc = c->rc;
    SDL_Point corner = { rc.x, rc.y };

    switch (c->kind) {
    case DXL_CTL_TEXT:
        dxl_text_wrapped(r, g, d->font, rc, c->text, p->text, NULL);
        break;
    case DXL_CTL_ETCHED:
        dxl_hline(r, rc.x, rc.x + rc.w, rc.y, p->shadow);
        dxl_hline(r, rc.x, rc.x + rc.w, rc.y + 1, p->highlight);
        break;
    case DXL_CTL_GROUP: {
        SDL_Rect f = { rc.x, rc.y + 5, rc.w, rc.h - 5 };
        dxl_edge(r, f, DXL_EDGE_ETCHED);
        if (*c->text) {
            int tw = dxl_text_width(g, d->font, c->text);
            SDL_Rect gap = { rc.x + 8, rc.y, tw + 2, d->line_h };
            dxl_fill(r, gap, p->face);
            dxl_text(r, g, d->font, rc.x + 9, rc.y, c->text, p->text);
        }
        break;
    }
    case DXL_CTL_BITMAP: {
        SDL_Rect in = dxl_edge(r, rc, DXL_EDGE_RAISED);
        if (c->bitmap) {
            SDL_Rect dst = { in.x, in.y, c->bitmap_w, c->bitmap_h };
            SDL_RenderCopy(r, c->bitmap, NULL, &dst);
        }
        break;
    }
    case DXL_CTL_COOL: {
        /* WCoolButton::OnDrawItem: flat, a thin raised edge; the full edge
         * under the mouse, sunk while pressed. */
        dxl_fill(r, rc, p->face);
        if (hover) dxl_edge(r, rc, pressed ? DXL_EDGE_SUNKEN : DXL_EDGE_RAISED);
        else dxl_edge(r, rc, pressed ? DXL_BDR_SUNKENINNER : DXL_BDR_RAISEDINNER);
        int tw = dxl_text_width(g, d->font, c->text);
        int x = rc.x + (rc.w - tw) / 2 + pressed, y = dt_vcenter(rc.y, rc.h, d->line_h) + pressed;
        dxl_text(r, g, d->font, x, y, c->text, p->text);
        break;
    }
    case DXL_CTL_URL:
        dxl_fill(r, rc, p->face);
        if (hover) dxl_edge(r, rc, pressed ? DXL_EDGE_SUNKEN : DXL_EDGE_RAISED);
        dxl_text(r, g, g->font_url, rc.x + 8 + pressed,
                 dt_vcenter(rc.y, rc.h, dxl_font_height(g->font_url)) + pressed, c->text, p->url);
        break;
    case DXL_CTL_PUSH: {
        /* The default button -- the focused one, while a button has the
         * focus -- is outlined; the bevel is DrawFrameControl's soft one,
         * flat while pressed. */
        dxl_fill(r, rc, p->face);
        SDL_Rect in = rc;
        if (shown_default(d, i)) {
            SDL_SetRenderDrawColor(r, p->frame.r, p->frame.g, p->frame.b, 255);
            SDL_RenderDrawRect(r, &in);
            in.x++; in.y++; in.w -= 2; in.h -= 2;
        }
        if (pressed) {
            SDL_SetRenderDrawColor(r, p->shadow.r, p->shadow.g, p->shadow.b, 255);
            SDL_RenderDrawRect(r, &in);
        } else {
            dxl_edge(r, in, DXL_EDGE_RAISED | DXL_BF_SOFT);
        }
        int tw = dxl_text_width(g, d->font, c->text);
        SDL_Rect lr = shown_default(d, i) ? (SDL_Rect){ rc.x + 1, rc.y + 1, rc.w - 2, rc.h - 2 } : rc;
        dxl_text(r, g, d->font, lr.x + (lr.w - tw) / 2 + pressed,
                 button_label_top(lr.y, lr.h, d->line_h) + pressed, c->text, p->text);
        if (focused) {
            SDL_Rect fr = { rc.x + 3, rc.y + 3, rc.w - 6, rc.h - 6 };
            dxl_focus_rect(r, fr, corner, p->face);
        }
        break;
    }
    case DXL_CTL_CHECK:
    case DXL_CTL_RADIO: {
        int radio = c->kind == DXL_CTL_RADIO;
        if (radio) pattern(r, rc.x + 1, rc.y + 1, c->checked ? radio_on : radio_off, 12, 1);
        else {
            pattern(r, rc.x, rc.y, check_box, 13, 1);
            if (c->checked) pattern(r, rc.x + 2, rc.y + 2, check_mark, 8, 1);
        }
        int x = rc.x + 17, y = button_label_top(rc.y, rc.h, d->line_h);
        dxl_text(r, g, d->font, x, y, c->text, p->text);
        if (focused) {
            SDL_Rect fr = { x - 1, y, dxl_text_width(g, d->font, c->text) + 2, d->line_h };
            dxl_focus_rect(r, fr, corner, p->face);
        }
        break;
    }
    case DXL_CTL_LIST: {
        SDL_Rect in = dxl_edge(r, rc, DXL_EDGE_SUNKEN);
        dxl_fill(r, in, p->window);
        SDL_RenderSetClipRect(r, &in);
        for (int k = 0; k < c->item_count; k++) {
            SDL_Rect row = { in.x, in.y + k * ITEM_H, in.w, ITEM_H };
            if (row.y >= in.y + in.h) break;
            int sel = k == c->selected;
            if (sel) dxl_fill(r, row, p->sel);
            dxl_text(r, g, d->font, row.x + 1, row.y, c->items[k], sel ? p->sel_text : p->text);
            if (sel && focused) {
                SDL_Point client = { in.x, in.y };
                dxl_focus_rect(r, row, client, p->sel);
            }
        }
        SDL_RenderSetClipRect(r, NULL);
        break;
    }
    case DXL_CTL_EDIT: {
        SDL_Rect in = dxl_edge(r, rc, DXL_EDGE_SUNKEN);
        dxl_fill(r, in, p->face);          /* read-only: the dialog's face */
        SDL_RenderSetClipRect(r, &in);
        int y = in.y + 1;
        const char *s = c->text;
        while (*s) {
            const char *e = s;
            while (*e && *e != '\r' && *e != '\n') e++;
            char *line = dxl_xstrndup(s, (size_t)(e - s));
            /* An edit shows & as it is: no mnemonics. */
            char *esc = dxl_xmalloc(strlen(line) * 2 + 1), *o = esc;
            for (char *q = line; *q; q++) { if (*q == '&') *o++ = '&'; *o++ = *q; }
            *o = '\0';
            dxl_text(r, g, d->font, in.x + 1, y, esc, p->text);
            free(esc);
            free(line);
            y += d->line_h;
            if (*e == '\r' && e[1] == '\n') e++;
            s = *e ? e + 1 : e;
        }
        SDL_RenderSetClipRect(r, NULL);
        break;
    }
    case DXL_CTL_PANE:
        dxl_fill(r, rc, p->face);
        break;
    case DXL_CTL_ICON: {
        /* The error icon: a red disc, a white cross. */
        int cx = rc.x + 16, cy = rc.y + 16;
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < 32; x++) {
                int dx = 2 * x - 31, dy = 2 * y - 31;
                if (dx * dx + dy * dy <= 30 * 30) {
                    SDL_SetRenderDrawColor(r, 0xd8, 0x1e, 0x06, 255);
                    SDL_RenderDrawPoint(r, rc.x + x, rc.y + y);
                }
            }
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        for (int t = -1; t <= 1; t++) {
            SDL_RenderDrawLine(r, cx - 7 + t, cy - 7, cx + 7 + t, cy + 7);
            SDL_RenderDrawLine(r, cx + 7 + t, cy - 7, cx - 7 + t, cy + 7);
        }
        break;
    }
    }
}

static void paint(dxl_dialog *d) {
    SDL_Rect all = { 0, 0, d->w, d->h };
    dxl_fill(d->ren, all, dxl_colors.face);
    for (int i = 0; i < d->count; i++)
        if (d->ctls[i].visible) draw_ctl(d, i);
}

void dxl_dialog_draw(dxl_dialog *d) {
    paint(d);
    SDL_RenderPresent(d->ren);
    if (d->win && !(SDL_GetWindowFlags(d->win) & SDL_WINDOW_SHOWN)) SDL_ShowWindow(d->win);
    d->dirty = 0;
}

int dxl_dialog_save(dxl_dialog *d, const char *path) {
    /* Read before it is presented: after, a window's back buffer is
     * undefined. */
    paint(d);
    if (d->surface) {
        SDL_RenderFlush(d->ren);
        return SDL_SaveBMP(d->surface, path);
    }
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, d->w, d->h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return -1;
    int rc = SDL_RenderReadPixels(d->ren, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    if (rc == 0) rc = SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    return rc;
}

/* ---- input ------------------------------------------------------------------ */

static void notify(dxl_dialog *d, int id, dxl_notify what) {
    if (d->on_command) d->on_command(d, id, what, d->ctx);
    d->dirty = 1;
}

void dxl_dialog_end(dxl_dialog *d, int result) {
    d->ended = 1;
    d->result = result;
}

static int inside(SDL_Rect rc, int x, int y) {
    return x >= rc.x && y >= rc.y && x < rc.x + rc.w && y < rc.y + rc.h;
}

static int hit(dxl_dialog *d, int x, int y) {
    for (int i = d->count - 1; i >= 0; i--) {
        dxl_ctl *c = &d->ctls[i];
        if (!c->visible) continue;
        switch (c->kind) {
        case DXL_CTL_COOL: case DXL_CTL_URL: case DXL_CTL_PUSH:
        case DXL_CTL_CHECK: case DXL_CTL_RADIO: case DXL_CTL_LIST:
            if (inside(c->rc, x, y)) return i;
            break;
        default:
            break;
        }
    }
    return -1;
}

/* A radio button's group: the radio buttons next to it in the list. */
static void radio_check(dxl_dialog *d, int i) {
    int a = i, b = i;
    while (a > 0 && d->ctls[a - 1].kind == DXL_CTL_RADIO) a--;
    while (b + 1 < d->count && d->ctls[b + 1].kind == DXL_CTL_RADIO) b++;
    for (int k = a; k <= b; k++) d->ctls[k].checked = k == i;
}

static void activate(dxl_dialog *d, int i) {
    dxl_ctl *c = &d->ctls[i];
    switch (c->kind) {
    case DXL_CTL_CHECK: c->checked = !c->checked; break;
    case DXL_CTL_RADIO: radio_check(d, i); break;
    default: break;
    }
    notify(d, c->id, DXL_CLICKED);
}

static void move_focus(dxl_dialog *d, int dir) {
    if (!d->count) return;
    int i = d->focus;
    for (int n = 0; n < d->count; n++) {
        i = (i + dir + d->count) % d->count;
        if (takes_focus(&d->ctls[i])) { d->focus = i; break; }
    }
    d->dirty = 1;
}

static void list_select(dxl_dialog *d, int i, int item) {
    dxl_ctl *c = &d->ctls[i];
    if (item < 0 || item >= c->item_count || item == c->selected) return;
    c->selected = item;
    notify(d, c->id, DXL_SELCHANGE);
}

static int visible_id(dxl_dialog *d, int id) {
    for (int i = d->count - 1; i >= 0; i--)
        if (d->ctls[i].id == id && d->ctls[i].visible) return i;
    return -1;
}

void dxl_dialog_click(dxl_dialog *d, int id) {
    int i = visible_id(d, id);
    if (i < 0) return;
    if (d->ctls[i].tabstop) d->focus = i;
    activate(d, i);
}

void dxl_dialog_choose(dxl_dialog *d, int id, int item, int twice) {
    int i = visible_id(d, id);
    if (i < 0 || d->ctls[i].kind != DXL_CTL_LIST) return;
    d->focus = i;
    list_select(d, i, item);
    if (twice && item >= 0 && item < d->ctls[i].item_count) notify(d, id, DXL_DBLCLK);
}

static void mnemonic(dxl_dialog *d, int letter) {
    for (int i = 0; i < d->count; i++) {
        dxl_ctl *c = &d->ctls[i];
        if (!c->visible || dxl_mnemonic(c->text) != letter) continue;
        if (c->kind == DXL_CTL_TEXT || c->kind == DXL_CTL_GROUP) {
            for (int k = i + 1; k < d->count; k++)
                if (takes_focus(&d->ctls[k])) { d->focus = k; d->dirty = 1; break; }
        } else {
            d->focus = takes_focus(c) ? i : d->focus;
            activate(d, i);
        }
        return;
    }
}

static void key(dxl_dialog *d, const SDL_KeyboardEvent *k) {
    SDL_Keycode sym = k->keysym.sym;
    int shift = (k->keysym.mod & KMOD_SHIFT) != 0, alt = (k->keysym.mod & KMOD_ALT) != 0;
    dxl_ctl *f = d->focus >= 0 ? &d->ctls[d->focus] : NULL;
    switch (sym) {
    case SDLK_TAB:
        move_focus(d, shift ? -1 : 1);
        return;
    case SDLK_ESCAPE:
        notify(d, DXL_IDCANCEL, DXL_CLICKED);
        return;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        if (f && (f->kind == DXL_CTL_COOL || f->kind == DXL_CTL_PUSH || f->kind == DXL_CTL_URL)) {
            activate(d, d->focus);
            return;
        }
        for (int i = 0; i < d->count; i++)
            if (d->ctls[i].visible && d->ctls[i].is_default) { activate(d, i); return; }
        notify(d, DXL_IDOK, DXL_CLICKED);
        return;
    case SDLK_SPACE:
        if (f && f->kind != DXL_CTL_LIST && f->kind != DXL_CTL_TEXT) activate(d, d->focus);
        return;
    case SDLK_UP: case SDLK_DOWN: case SDLK_LEFT: case SDLK_RIGHT:
    case SDLK_HOME: case SDLK_END:
        if (f && f->kind == DXL_CTL_LIST) {
            int s = f->selected;
            if (sym == SDLK_HOME) s = 0;
            else if (sym == SDLK_END) s = f->item_count - 1;
            else s += (sym == SDLK_UP || sym == SDLK_LEFT) ? -1 : 1;
            if (s < 0) s = 0;
            list_select(d, d->focus, s);
        } else if (f && f->kind == DXL_CTL_RADIO) {
            int step = (sym == SDLK_UP || sym == SDLK_LEFT) ? -1 : 1;
            int n = d->focus + step;
            if (n >= 0 && n < d->count && d->ctls[n].kind == DXL_CTL_RADIO) {
                d->focus = n;
                activate(d, n);
            }
        }
        return;
    default:
        break;
    }
    if (sym >= SDLK_a && sym <= SDLK_z && (alt || !f || f->kind != DXL_CTL_LIST))
        mnemonic(d, (int)sym);
}

static int our_window(dxl_dialog *d, Uint32 id) {
    return d->win && SDL_GetWindowID(d->win) == id;
}

static void handle(dxl_dialog *d, const SDL_Event *e) {
    switch (e->type) {
    case SDL_WINDOWEVENT:
        if (!our_window(d, e->window.windowID)) return;
        if (e->window.event == SDL_WINDOWEVENT_CLOSE) notify(d, DXL_IDCANCEL, DXL_CLICKED);
        else if (e->window.event == SDL_WINDOWEVENT_LEAVE) { d->hover = -1; d->dirty = 1; }
        else d->dirty = 1;
        return;
    case SDL_QUIT:
        notify(d, DXL_IDCANCEL, DXL_CLICKED);
        return;
    case SDL_MOUSEMOTION: {
        if (!our_window(d, e->motion.windowID)) return;
        int h = hit(d, e->motion.x, e->motion.y);
        if (h >= 0 && d->ctls[h].kind != DXL_CTL_COOL && d->ctls[h].kind != DXL_CTL_URL) h = -1;
        if (d->pressed >= 0) h = inside(d->ctls[d->pressed].rc, e->motion.x, e->motion.y) ? d->pressed : -1;
        if (h != d->hover) { d->hover = h; d->dirty = 1; }
        return;
    }
    case SDL_MOUSEBUTTONDOWN: {
        if (!our_window(d, e->button.windowID) || e->button.button != SDL_BUTTON_LEFT) return;
        int i = hit(d, e->button.x, e->button.y);
        if (i < 0) return;
        dxl_ctl *c = &d->ctls[i];
        if (c->tabstop) d->focus = i;
        if (c->kind == DXL_CTL_LIST) {
            SDL_Rect in = { c->rc.x + 2, c->rc.y + 2, c->rc.w - 4, c->rc.h - 4 };
            int item = (e->button.y - in.y) / ITEM_H;
            Uint32 now = SDL_GetTicks();
            int twice = d->last_click_ctl == i && item == c->selected &&
                        now - d->last_click < DOUBLE_CLICK_MS;
            list_select(d, i, item);
            d->last_click = now;
            d->last_click_ctl = i;
            if (twice && item < c->item_count) notify(d, c->id, DXL_DBLCLK);
        } else {
            d->pressed = i;
        }
        d->dirty = 1;
        return;
    }
    case SDL_MOUSEBUTTONUP: {
        if (!our_window(d, e->button.windowID) || e->button.button != SDL_BUTTON_LEFT) return;
        int i = d->pressed;
        d->pressed = -1;
        d->dirty = 1;
        if (i >= 0 && i < d->count && inside(d->ctls[i].rc, e->button.x, e->button.y))
            activate(d, i);
        return;
    }
    case SDL_KEYDOWN:
        if (!our_window(d, e->key.windowID)) return;
        key(d, &e->key);
        return;
    default:
        return;
    }
}

int dxl_dialog_step(dxl_dialog *d, int ms) {
    if (d->gui->quit && *d->gui->quit && !d->ended) notify(d, DXL_IDCANCEL, DXL_CLICKED);
    if (d->ended) return 1;
    if (d->dirty) dxl_dialog_draw(d);
    SDL_Event e;
    if (SDL_WaitEventTimeout(&e, ms)) {
        handle(d, &e);
        while (!d->ended && SDL_PollEvent(&e)) handle(d, &e);
    }
    if (d->dirty && !d->ended) dxl_dialog_draw(d);
    return d->ended;
}

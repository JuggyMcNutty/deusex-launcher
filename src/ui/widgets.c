#include "ui/widgets.h"
#include "ui/draw.h"
#include "ui/text.h"

#include <math.h>
#include <string.h>

#define BUTTON_CHAMFER 6
#define PANEL_CHAMFER  10
#define TITLE_BAR_H    32

void dxl_panel(dxl_ui *ui, dxl_rect r) {
    const dxl_theme *t = &ui->theme;
    dxl_bevel_style s = { t->panel_top, t->panel_bottom, t->panel_light, t->panel_shadow, PANEL_CHAMFER };
    dxl_bevel(ui, r, &s);
}

dxl_rect dxl_window(dxl_ui *ui, dxl_rect r, const char *title) {
    const dxl_theme *t = &ui->theme;
    dxl_panel(ui, r);
    dxl_rect bar = { r.x + 5, r.y + 5, r.w - 10, TITLE_BAR_H };
    dxl_bevel_style s = {
        dxl_rgba_mix(t->title_bar, t->title_text, 0.12f), dxl_rgba_mix(t->title_bar, t->panel_shadow, 0.35f),
        dxl_rgba_fade(t->title_text, 0.35f), t->panel_shadow, BUTTON_CHAMFER,
    };
    dxl_bevel(ui, bar, &s);
    dxl_rect text = { bar.x + 14, bar.y, bar.w - 28, bar.h };
    dxl_label(ui, text, DXL_FONT_BUTTON, title, t->title_text, DXL_W_LEFT);
    dxl_rect in = { r.x + 22, bar.y + bar.h + 16, r.w - 44, r.h - (bar.y + bar.h - r.y) - 16 - 20 };
    if (in.h < 0) in.h = 0;
    return in;
}

void dxl_well(dxl_ui *ui, dxl_rect r) {
    const dxl_theme *t = &ui->theme;
    dxl_bevel_style s = {
        dxl_rgba_fade(t->backdrop_bottom, 0.92f), dxl_rgba_fade(dxl_rgba_mix(t->backdrop_bottom, t->panel_bottom, 0.5f), 0.92f),
        t->panel_shadow, dxl_rgba_fade(t->panel_light, 0.45f), 4,
    };
    dxl_bevel(ui, r, &s);
}

void dxl_label(dxl_ui *ui, dxl_rect r, dxl_font_role role, const char *s, dxl_rgba c, int flags) {
    if (!s || !*s) return;
    char buf[1024];
    int cut = (flags & DXL_W_TAIL) ? dxl_text_fit_tail(ui, role, s, r.w, buf, sizeof buf)
                                   : dxl_text_fit(ui, role, s, r.w, buf, sizeof buf);
    if (cut && !(flags & DXL_W_MAY_CUT)) dxl_ui_diag(ui, "label cut short: \"%s\" in %.0f dp", s, r.w);
    SDL_FPoint size = dxl_text_size(ui, role, buf, 0);
    float x = r.x;
    if (flags & DXL_W_CENTER) x = r.x + (r.w - size.x) / 2;
    else if (flags & DXL_W_RIGHT) x = r.x + r.w - size.x;
    float y = r.y + (r.h - size.y) / 2;
    dxl_text_draw(ui, role, buf, x, y, 0, c, 1);
}

float dxl_paragraph(dxl_ui *ui, dxl_rect r, dxl_font_role role, const char *s, dxl_rgba c) {
    if (!s || !*s) return 0;
    SDL_FPoint size = dxl_text_size(ui, role, s, r.w);
    if (size.y > r.h + 0.5f) dxl_ui_diag(ui, "text taller than its %.0f dp: \"%.40s\"", r.h, s);
    dxl_text_draw(ui, role, s, r.x, r.y, r.w, c, 1);
    return size.y;
}

int dxl_button(dxl_ui *ui, const char *id, dxl_rect r, const char *label, int flags) {
    return dxl_button_id(ui, dxl_ui_id(ui, id), r, label, flags);
}

int dxl_button_id(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, int flags) {
    const dxl_theme *t = &ui->theme;
    int disabled = flags & DXL_W_DISABLED;
    int hits = dxl_ui_focusable(ui, id, r, disabled);
    if (!disabled) dxl_ui_name(ui, id, label);
    int lit = hits & (DXL_HIT_FOCUSED | DXL_HIT_HOVERED);
    dxl_bevel_style s;
    if (hits & DXL_HIT_HELD) {
        /* Pressed in: the light from below. */
        s = (dxl_bevel_style){ t->face_bottom, t->face_focus_top, t->panel_shadow, t->panel_light, BUTTON_CHAMFER };
    } else if (lit) {
        s = (dxl_bevel_style){ t->face_focus_top, t->face_focus_bottom, t->panel_light, t->panel_shadow, BUTTON_CHAMFER };
    } else {
        s = (dxl_bevel_style){ t->face_top, t->face_bottom, dxl_rgba_fade(t->panel_light, 0.8f), t->panel_shadow,
                               BUTTON_CHAMFER };
    }
    if (disabled) {
        s.top = dxl_rgba_fade(s.top, 0.45f);
        s.bottom = dxl_rgba_fade(s.bottom, 0.45f);
        s.light = dxl_rgba_fade(s.light, 0.4f);
    }
    dxl_bevel(ui, r, &s);

    float pad = 18;
    if (flags & DXL_W_PRIMARY) {
        /* The main action's mark: a bar of the accent down its left. */
        dxl_rect mark = { r.x + 6, r.y + 9, 3, r.h - 18 };
        dxl_fill(ui, mark, disabled ? t->text_disabled : t->accent);
        pad += 4;
    }
    dxl_rgba c = disabled ? t->text_disabled : (lit || (flags & DXL_W_PRIMARY)) ? t->text_focus : t->text;
    dxl_rect text = { r.x + pad, r.y, r.w - 2 * pad, r.h };
    int align = (flags & DXL_W_LEFT) ? DXL_W_LEFT : DXL_W_CENTER;
    dxl_label(ui, text, DXL_FONT_BUTTON, label, c, align | (flags & DXL_W_MAY_CUT));
    return (hits & DXL_HIT_ACTIVATED) != 0;
}

void dxl_field(dxl_ui *ui, dxl_rect *r, float h, float name_w, const char *name, const char *value,
               dxl_rgba c, int flags) {
    dxl_rect line = dxl_cut_top(r, h);
    dxl_rect n = dxl_cut_left(&line, name_w);
    dxl_label(ui, n, DXL_FONT_LABEL, name, ui->theme.help, DXL_W_LEFT);
    dxl_label(ui, line, DXL_FONT_BODY, value, c, DXL_W_LEFT | flags);
}

/* ---- lists ---------------------------------------------------------- */

/* A list's other values: a row asked for (dxl_list_focus), kept beside its
 * scroll; where the pointer holds its scroll bar's thumb. */
#define WANT_KEY 0x2545f491u
#define BAR_KEY  0x5c40bba7u
#define GRAB_KEY 0x0a7e11edu
#define BAR_W    10

/* The scroll bar's track and its thumb, for a list scrolled off dp. */
static void bar_of(const dxl_list *l, float off, dxl_rect *track, dxl_rect *thumb) {
    float total = l->count * l->row_h;
    *track = (dxl_rect){ l->area.x + l->area.w - BAR_W + 3, l->area.y + 2, 4, l->area.h - 4 };
    float h = track->h * l->area.h / total;
    if (h < 24) h = 24;
    float span = total - l->area.h;
    *thumb = (dxl_rect){ track->x, track->y + (track->h - h) * (span > 0 ? off / span : 0), track->w, h };
}

/* The pointer on the scroll bar: the thumb dragged, the track clicked a
 * page that way. */
static void bar_input(dxl_ui *ui, dxl_list *l, float *off) {
    dxl_rect track, thumb;
    bar_of(l, *off, &track, &thumb);
    dxl_rect hit = { l->area.x + l->area.w - BAR_W, l->area.y, BAR_W, l->area.h };
    uint32_t id = l->id ^ BAR_KEY;
    float *grab = dxl_ui_value(ui, l->id ^ GRAB_KEY, -1);
    int hits = dxl_ui_clickable(ui, id, hit);
    if (ui->in.pressed && (hits & DXL_HIT_HOVERED)) {
        int on_thumb = ui->in.py >= thumb.y && ui->in.py < thumb.y + thumb.h;
        *grab = on_thumb ? ui->in.py - thumb.y : -1;
        if (!on_thumb) *off += ui->in.py < thumb.y ? -l->area.h : l->area.h;
    }
    if (*grab >= 0 && ui->held == id) {
        float span = track.h - thumb.h, f = span > 0 ? (ui->in.py - *grab - track.y) / span : 0;
        if (f < 0) f = 0;
        if (f > 1) f = 1;
        *off = f * (l->count * l->row_h - l->area.h);
    } else {
        *grab = -1;
    }
}

uint32_t dxl_list_row_id(const dxl_list *l, int i) {
    uint32_t h = l->id ^ ((uint32_t)(i + 1) * 0x9e3779b1u);
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    return h ? h : 1;
}

#define COLS 4

uint32_t dxl_list_cell_id(const dxl_list *l, int i, int col) {
    uint32_t id = dxl_list_row_id(l, i);
    if (col > 0) id ^= (uint32_t)col * 0x632be5abu;
    return id ? id : 1;
}

void dxl_list_focus(dxl_ui *ui, dxl_list *l, int i) {
    if (i < 0 || i >= l->count) return;
    *dxl_ui_value(ui, l->id ^ WANT_KEY, -1) = (float)i;
    ui->focus = dxl_list_row_id(l, i);
}

void dxl_list_begin(dxl_ui *ui, dxl_list *l, const char *id, dxl_rect r, int count, float row_h, int flags) {
    memset(l, 0, sizeof *l);
    l->id = dxl_ui_id(ui, id);
    l->area = r;
    l->count = count;
    l->row_h = row_h;
    l->focused = -1;
    l->flags = flags;
    l->scrollable = count * row_h > r.h + 0.5f;
    float *off = dxl_ui_value(ui, l->id, 0);
    float *want = dxl_ui_value(ui, l->id ^ WANT_KEY, -1);
    int visible = (int)(r.h / row_h);
    if (visible < 1) visible = 1;

    /* Up and down from a row go along the rows, those out of view too; off
     * either end, the kit's move stands and takes the focus out of the
     * list. */
    int was = -1, col = 0, keep = 0;
    for (int i = 0; i < count && was < 0; i++)
        for (int c = 0; c < COLS && was < 0; c++)
            if (dxl_list_cell_id(l, i, c) == ui->focus_was) {
                was = i;
                col = c;
            }
    if (was >= 0) {
        int down = ui->in.nav[DXL_DIR_DOWN], up = ui->in.nav[DXL_DIR_UP];
        int to = was + down - up + (ui->in.page_down - ui->in.page_up) * visible;
        if (to < 0) to = 0;
        if (to > count - 1) to = count - 1;
        int off_end = (down > up && was == count - 1) || (up > down && was == 0);
        if ((down || up || ui->in.page_down || ui->in.page_up) && !off_end) {
            ui->focus = dxl_list_cell_id(l, to, col);
            ui->focus_moved = 1;
            keep = 1;
        }
        ui->in.page_down = ui->in.page_up = 0;
    }
    if (*want >= 0) {
        int i = (int)*want;
        if (i < count) ui->focus = dxl_list_row_id(l, i);
        *want = -1;
        keep = 1;
    }
    for (int i = 0; i < count && l->focused < 0; i++)
        for (int c = 0; c < COLS && l->focused < 0; c++)
            if (dxl_list_cell_id(l, i, c) == ui->focus) l->focused = i;

    if (l->focused >= 0 && (keep || ui->focus_moved)) {
        float top = l->focused * row_h, bottom = top + row_h;
        if (top < *off) *off = top;
        if (bottom > *off + r.h) *off = bottom - r.h;
    }
    int over = ui->in.px >= r.x && ui->in.px < r.x + r.w && ui->in.py >= r.y && ui->in.py < r.y + r.h;
    if (ui->in.wheel != 0 && over && dxl_ui_live(ui)) {
        *off -= ui->in.wheel * row_h * 2;
        ui->in.wheel = 0;
    }
    if (l->scrollable) bar_input(ui, l, off);
    float max = count * row_h - r.h;
    if (flags & DXL_LIST_WHOLE_ROWS) {
        /* A whole number of rows scrolled, the focused one all in view. */
        if (max > 0) max = ceilf(max / row_h) * row_h;
        float snapped = floorf(*off / row_h + 0.5f) * row_h;
        if (l->focused >= 0 && (keep || ui->focus_moved)) {
            float top = l->focused * row_h;
            if (top < snapped) snapped = top;
            while (top + row_h > snapped + r.h + 0.5f && snapped < top) snapped += row_h;
        }
        *off = snapped;
    }
    if (*off > max) *off = max;
    if (*off < 0) *off = 0;
    l->offset = *off;
    dxl_clip_push(ui, r);
}

int dxl_list_rect(const dxl_list *l, int i, dxl_rect *out) {
    dxl_rect row = { l->area.x, l->area.y + i * l->row_h - l->offset, l->area.w - (l->scrollable ? BAR_W : 0),
                     l->row_h };
    *out = row;
    return row.y >= l->area.y - 0.5f && row.y + row.h <= l->area.y + l->area.h + 0.5f;
}

int dxl_list_row(dxl_ui *ui, dxl_list *l, int i, const char *title, const char *detail, dxl_rgba detail_c,
                 int flags) {
    const dxl_theme *t = &ui->theme;
    dxl_rect row = { l->area.x, l->area.y + i * l->row_h - l->offset, l->area.w - (l->scrollable ? BAR_W : 0),
                     l->row_h };
    float top = l->area.y, bottom = l->area.y + l->area.h;
    if (row.y + row.h <= top + 0.5f || row.y >= bottom - 0.5f) return 0;
    /* What of it is in view takes the focus and the pointer. */
    dxl_rect seen = row;
    if (seen.y < top) { seen.h -= top - seen.y; seen.y = top; }
    if (seen.y + seen.h > bottom) seen.h = bottom - seen.y;

    int hits = dxl_ui_focusable(ui, dxl_list_row_id(l, i), seen, flags & DXL_W_DISABLED);
    if (!(flags & DXL_W_DISABLED)) dxl_ui_name(ui, dxl_list_row_id(l, i), title);
    if (hits & DXL_HIT_FOCUSED) dxl_fill(ui, row, t->row_focus);
    else if (hits & DXL_HIT_HOVERED) dxl_fill(ui, row, t->row_highlight);
    dxl_rect sep = { row.x + 8, row.y + row.h - 1, row.w - 16, 1 };
    dxl_fill(ui, sep, t->grid);

    dxl_rgba c = (flags & DXL_W_DISABLED) ? t->text_disabled : (hits & DXL_HIT_FOCUSED) ? t->text_focus : t->text;
    dxl_rect text = { row.x + 14, row.y, row.w - 28, row.h };
    int cut = DXL_W_LEFT | (flags & (DXL_W_MAY_CUT | DXL_W_TAIL));
    if (detail && *detail) {
        float lt = dxl_text_line(ui, DXL_FONT_BODY), ld = dxl_text_line(ui, DXL_FONT_SMALL);
        float y = row.y + (row.h - lt - ld) / 2;
        dxl_rect a = { text.x, y, text.w, lt }, b = { text.x, y + lt, text.w, ld };
        dxl_label(ui, a, DXL_FONT_BODY, title, c, cut);
        dxl_label(ui, b, DXL_FONT_SMALL, detail, detail_c, DXL_W_LEFT | DXL_W_MAY_CUT | (flags & DXL_W_TAIL));
    } else {
        dxl_label(ui, text, DXL_FONT_BODY, title, c, cut);
    }
    return hits;
}

void dxl_list_end(dxl_ui *ui, dxl_list *l) {
    dxl_clip_pop(ui);
    /* Moved up or down from a row's second control into a row of one: that
     * one, not a control nothing drew. */
    if (l->focused >= 0 && !dxl_focus_has(&ui->cur, ui->focus)) {
        uint32_t first = dxl_list_row_id(l, l->focused);
        if (dxl_focus_has(&ui->cur, first)) ui->focus = first;
    }
    if (!l->scrollable) return;
    const dxl_theme *t = &ui->theme;
    dxl_rect track, thumb;
    bar_of(l, l->offset, &track, &thumb);
    dxl_fill(ui, track, dxl_rgba_fade(t->panel_light, 0.18f));
    /* Lit under the pointer, or held. */
    int lit = ui->hot == (l->id ^ BAR_KEY) || ui->held == (l->id ^ BAR_KEY);
    dxl_fill(ui, thumb, lit ? t->accent : dxl_rgba_fade(t->accent, 0.7f));
}

/* ---- dialogs ---------------------------------------------------------- */

dxl_rect dxl_dialog_begin(dxl_ui *ui, dxl_rect over, float w, float h, const char *title) {
    dxl_fill(ui, over, dxl_rgba_fade(ui->theme.backdrop_bottom, 0.8f));
    if (w > over.w - 32) w = over.w - 32;
    if (h > over.h - 32) h = over.h - 32;
    dxl_rect r = dxl_centered(over, w, h);
    r.x = (float)(int)r.x;
    r.y = (float)(int)r.y;
    dxl_ui_scope(ui, 1);
    return dxl_window(ui, r, title);
}

void dxl_dialog_end(dxl_ui *ui) { dxl_ui_scope(ui, 0); }

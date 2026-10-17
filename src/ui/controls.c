#include "ui/controls.h"
#include "ui/draw.h"
#include "ui/glyphs.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PAD 16

/* Values the controls keep beside their own id. */
#define DRAG_KEY  0x51ab1e00u
#define LEFT_KEY  0x1ef7a000u
#define RIGHT_KEY 0x219b7000u

float dxl_control_label_w(float w) {
    float l = w * 0.42f;
    if (l < 180) l = 180;
    if (l > 380) l = 380;
    return l;
}

/* The row: the focusable, its background, its label. The rect left for
 * the control. */
static dxl_rect row(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, int flags, int *hits) {
    const dxl_theme *t = &ui->theme;
    *hits = dxl_ui_focusable(ui, id, r, flags & DXL_W_DISABLED);
    dxl_ui_name(ui, id, label);
    if (*hits & DXL_HIT_FOCUSED) dxl_fill(ui, r, t->row_focus);
    else if (*hits & DXL_HIT_HOVERED) dxl_fill(ui, r, t->row_highlight);
    float lw = dxl_control_label_w(r.w);
    dxl_rect lr = { r.x + PAD, r.y, lw - PAD, r.h };
    dxl_rgba c = (flags & DXL_W_DISABLED) ? t->text_disabled : (*hits & DXL_HIT_FOCUSED) ? t->text_focus : t->text;
    dxl_label(ui, lr, DXL_FONT_BODY, label, c, DXL_W_LEFT | (flags & DXL_W_MAY_CUT));
    dxl_rect area = { r.x + lw + 8, r.y, r.w - lw - 8 - PAD, r.h };
    return area;
}

static dxl_bevel_style knob_style(const dxl_theme *t, int lit) {
    dxl_bevel_style s = { lit ? t->face_focus_top : t->face_top, lit ? t->face_focus_bottom : t->face_bottom,
                          t->panel_light, t->panel_shadow, 3 };
    return s;
}

int dxl_toggle(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, int *on, int flags) {
    const dxl_theme *t = &ui->theme;
    int hits, was = *on;
    dxl_rect a = row(ui, id, r, label, flags, &hits);
    if (!(flags & DXL_W_DISABLED)) {
        if (hits & DXL_HIT_ACTIVATED) *on = !*on;
        int lr = dxl_ui_take_lr(ui, id);
        if (lr > 0) *on = 1;
        else if (lr < 0) *on = 0;
    }
    const float sw = 46, sh = 22;
    dxl_rect s = { a.x + a.w - sw, a.y + (a.h - sh) / 2, sw, sh };
    dxl_well(ui, s);
    if (*on) dxl_fill(ui, dxl_inset(s, 3), dxl_rgba_fade(t->accent, 0.6f));
    dxl_rect knob = { *on ? s.x + s.w - sh : s.x, s.y, sh, sh };
    dxl_bevel_style ks = knob_style(t, hits & DXL_HIT_FOCUSED);
    dxl_bevel(ui, dxl_inset(knob, 1), &ks);
    dxl_rect text = { a.x, a.y, a.w - sw - 12, a.h };
    dxl_label(ui, text, DXL_FONT_LABEL, *on ? "On" : "Off", *on ? t->text_focus : t->help, DXL_W_RIGHT);
    if (*on != was) ui->cues |= DXL_CUE_SLIDE;
    return *on != was;
}

int dxl_slider(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, double *v, double min, double max,
               double step, const char *shown, int flags) {
    const dxl_theme *t = &ui->theme;
    int hits;
    double was = *v;
    dxl_rect a = row(ui, id, r, label, flags, &hits);
    const float vw = 72;
    dxl_rect track = { a.x, a.y + a.h / 2 - 3, a.w - vw - 14, 6 };
    if (track.w < 40) track.w = 40;
    if (!(flags & DXL_W_DISABLED) && max > min) {
        int lr = dxl_ui_take_lr(ui, id);
        if (lr) *v += lr * step;
        /* The pointer drags it from a press on the track to the release. */
        float *drag = dxl_ui_value(ui, id ^ DRAG_KEY, 0);
        int on_track = ui->in.px >= track.x - 10 && ui->in.px <= track.x + track.w + 10 && ui->in.py >= r.y &&
                       ui->in.py < r.y + r.h;
        if (ui->in.pressed && on_track && (hits & DXL_HIT_HOVERED)) *drag = 1;
        if (*drag && (hits & (DXL_HIT_HELD | DXL_HIT_ACTIVATED))) {
            double f = (ui->in.px - track.x) / track.w;
            if (f < 0) f = 0;
            if (f > 1) f = 1;
            *v = min + f * (max - min);
            if (step > 0) *v = min + floor((*v - min) / step + 0.5) * step;
        }
        if (!(hits & DXL_HIT_HELD)) *drag = 0;
        if (ui->in.wheel != 0 && (hits & DXL_HIT_HOVERED)) {
            *v += (ui->in.wheel > 0 ? 1 : -1) * step;
            ui->in.wheel = 0;
        }
        if (*v < min) *v = min;
        if (*v > max) *v = max;
    }
    dxl_well(ui, track);
    float f = max > min ? (float)((*v - min) / (max - min)) : 0;
    dxl_rect fill = { track.x + 1, track.y + 1, (track.w - 2) * f, track.h - 2 };
    dxl_fill(ui, fill, dxl_rgba_fade(t->accent, (flags & DXL_W_DISABLED) ? 0.3f : 0.75f));
    dxl_rect knob = { track.x + track.w * f - 5, a.y + a.h / 2 - 10, 10, 20 };
    dxl_bevel_style ks = knob_style(t, hits & DXL_HIT_FOCUSED);
    dxl_bevel(ui, knob, &ks);
    dxl_rect text = { a.x + a.w - vw, a.y, vw, a.h };
    dxl_label(ui, text, DXL_FONT_LABEL, shown, (hits & DXL_HIT_FOCUSED) ? t->text_focus : t->text, DXL_W_RIGHT);
    if (*v != was) ui->cues |= DXL_CUE_SLIDE;
    return *v != was;
}

static void arrow(dxl_ui *ui, dxl_rect r, int right, dxl_rgba c) {
    float cx = r.x + r.w / 2, cy = r.y + r.h / 2, k = r.h * 0.22f;
    SDL_FPoint p[3];
    if (right) {
        p[0] = (SDL_FPoint){ cx - k * 0.6f, cy - k };
        p[1] = (SDL_FPoint){ cx + k, cy };
        p[2] = (SDL_FPoint){ cx - k * 0.6f, cy + k };
    } else {
        p[0] = (SDL_FPoint){ cx + k * 0.6f, cy - k };
        p[1] = (SDL_FPoint){ cx + k * 0.6f, cy + k };
        p[2] = (SDL_FPoint){ cx - k, cy };
    }
    dxl_poly(ui, p, 3, c);
}

int dxl_spinner(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, int *index, const char *const *labels,
                int n, int flags) {
    const dxl_theme *t = &ui->theme;
    int hits, was = *index, ret = 0;
    dxl_rect a = row(ui, id, r, label, flags, &hits);
    float gw = a.w < 300 ? a.w : 300;
    dxl_rect g = { a.x + a.w - gw, a.y, gw, a.h };
    const float box = 28;
    dxl_rect left = { g.x, g.y + (g.h - box) / 2, box, box }, right = { g.x + g.w - box, g.y + (g.h - box) / 2, box, box };
    if (!(flags & DXL_W_DISABLED) && n > 0) {
        int lr = dxl_ui_take_lr(ui, id);
        int cl = dxl_ui_clickable(ui, id ^ LEFT_KEY, left), cr = dxl_ui_clickable(ui, id ^ RIGHT_KEY, right);
        if (cl & DXL_HIT_ACTIVATED) lr--;
        if (cr & DXL_HIT_ACTIVATED) lr++;
        *index += lr;
        if (*index < 0) *index = 0;
        if (*index > n - 1) *index = n - 1;
        if (hits & DXL_HIT_ACTIVATED) ret = DXL_SPIN_LIST;
    }
    int focused = hits & DXL_HIT_FOCUSED;
    dxl_rgba on = focused ? t->text_focus : t->text;
    arrow(ui, left, 0, *index > 0 ? on : t->text_disabled);
    arrow(ui, right, 1, *index < n - 1 ? on : t->text_disabled);
    dxl_rect value = { left.x + box + 4, g.y, g.w - 2 * box - 8, g.h };
    if (n > 0 && *index >= 0 && *index < n)
        dxl_label(ui, value, DXL_FONT_BODY, labels[*index], (flags & DXL_W_DISABLED) ? t->text_disabled : on,
                  DXL_W_CENTER | (flags & DXL_W_MAY_CUT));
    if (*index != was) {
        ret = DXL_SPIN_CHANGED;
        ui->cues |= DXL_CUE_SLIDE;
    }
    return ret;
}

int dxl_value_row(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, const char *value, dxl_rgba c, int flags) {
    const dxl_theme *t = &ui->theme;
    int hits;
    dxl_rect a = row(ui, id, r, label, flags, &hits);
    const float box = 28;
    dxl_rect go = { a.x + a.w - box, a.y + (a.h - box) / 2, box, box };
    dxl_rect v = { a.x, a.y, a.w - box - 8, a.h };
    dxl_label(ui, v, DXL_FONT_BODY, value, (flags & DXL_W_DISABLED) ? t->text_disabled : c,
              DXL_W_RIGHT | DXL_W_TAIL | DXL_W_MAY_CUT);
    arrow(ui, go, 1, (flags & DXL_W_DISABLED) ? t->text_disabled : (hits & DXL_HIT_FOCUSED) ? t->text_focus : t->text);
    return !(flags & DXL_W_DISABLED) && (hits & DXL_HIT_ACTIVATED);
}

/* ---- text ----------------------------------------------------------- */

/* The field being edited -- one at a time -- as its edit began and where
 * it is: the caret and the selection's other end (bytes), how far its
 * text is scrolled (dp), whether the pointer drags a selection, when the
 * caret last moved (its blink starts again). Esc puts the text back. */
static struct {
    char   original[1024];
    size_t caret, anchor;
    float  scroll;
    int    dragging;
    double moved;
} ed;

#define BLINK_S 0.53

static size_t utf8_back(const char *s, size_t at) {
    if (at == 0) return 0;
    at--;
    while (at > 0 && ((unsigned char)s[at] & 0xc0) == 0x80) at--;
    return at;
}

static size_t utf8_ahead(const char *s, size_t at) {
    size_t n = strlen(s);
    if (at >= n) return n;
    at++;
    while (at < n && ((unsigned char)s[at] & 0xc0) == 0x80) at++;
    return at;
}

/* A word's character: a letter, a digit, or any of more than one byte. */
static int wordy(unsigned char c) { return c >= 0x80 || isalnum(c) || c == '_'; }

static size_t word_back(const char *s, size_t at) {
    while (at > 0 && !wordy((unsigned char)s[utf8_back(s, at)])) at = utf8_back(s, at);
    while (at > 0 && wordy((unsigned char)s[utf8_back(s, at)])) at = utf8_back(s, at);
    return at;
}

static size_t word_ahead(const char *s, size_t at) {
    size_t n = strlen(s);
    while (at < n && !wordy((unsigned char)s[at])) at = utf8_ahead(s, at);
    while (at < n && wordy((unsigned char)s[at])) at = utf8_ahead(s, at);
    return at;
}

static size_t sel_lo(void) { return ed.caret < ed.anchor ? ed.caret : ed.anchor; }
static size_t sel_hi(void) { return ed.caret < ed.anchor ? ed.anchor : ed.caret; }

/* The selection taken out of buf; the caret where it was. */
static void cut_selection(char *buf) {
    size_t lo = sel_lo(), hi = sel_hi(), len = strlen(buf);
    if (lo == hi) return;
    memmove(buf + lo, buf + hi, len - hi + 1);
    ed.caret = ed.anchor = lo;
}

static void copy_selection(const char *buf) {
    size_t lo = sel_lo(), hi = sel_hi();
    if (lo == hi) return;
    char part[1024];
    size_t n = hi - lo < sizeof part ? hi - lo : sizeof part - 1;
    memcpy(part, buf + lo, n);
    part[n] = '\0';
    SDL_SetClipboardText(part);
}

/* buf's bytes from..to taken out, the caret at from. */
static void erase_span(char *buf, size_t from, size_t to) {
    size_t len = strlen(buf);
    if (to > len) to = len;
    if (from >= to) return;
    memmove(buf + from, buf + to, len - to + 1);
    ed.caret = ed.anchor = from;
}

/* The frame's typing, in order: text in at the caret over the selection,
 * the editing keys as they came. 1 when the text or the caret changed. */
static int edit(dxl_ui *ui, char *buf, size_t size) {
    size_t len = strlen(buf);
    if (ed.caret > len) ed.caret = len;
    if (ed.anchor > len) ed.anchor = len;
    int changed = 0, select = 0;
    for (const char *p = ui->in.text; *p;) {
        if (DXL_EDIT_IS_OP(*p)) {
            char op = *p++;
            int sel = ed.caret != ed.anchor, move = 0;
            changed = 1;
            switch (op) {
            case DXL_EDIT_SELECT: select = 1; continue;
            case DXL_EDIT_ERASE:
                if (sel) cut_selection(buf);
                else erase_span(buf, utf8_back(buf, ed.caret), ed.caret);
                break;
            case DXL_EDIT_DELETE:
                if (sel) cut_selection(buf);
                else erase_span(buf, ed.caret, utf8_ahead(buf, ed.caret));
                break;
            case DXL_EDIT_ERASE_WORD:
                if (sel) cut_selection(buf);
                else erase_span(buf, word_back(buf, ed.caret), ed.caret);
                break;
            case DXL_EDIT_DELETE_WORD:
                if (sel) cut_selection(buf);
                else erase_span(buf, ed.caret, word_ahead(buf, ed.caret));
                break;
            /* A move without Shift from a selection goes to its end that
             * way; with Shift it grows the selection. */
            case DXL_EDIT_LEFT: ed.caret = sel && !select ? sel_lo() : utf8_back(buf, ed.caret); move = 1; break;
            case DXL_EDIT_RIGHT: ed.caret = sel && !select ? sel_hi() : utf8_ahead(buf, ed.caret); move = 1; break;
            case DXL_EDIT_WORD_LEFT: ed.caret = word_back(buf, ed.caret); move = 1; break;
            case DXL_EDIT_WORD_RIGHT: ed.caret = word_ahead(buf, ed.caret); move = 1; break;
            case DXL_EDIT_HOME: ed.caret = 0; move = 1; break;
            case DXL_EDIT_END: ed.caret = strlen(buf); move = 1; break;
            case DXL_EDIT_ALL: ed.anchor = 0; ed.caret = strlen(buf); break;
            case DXL_EDIT_COPY: copy_selection(buf); changed = 0; break;
            case DXL_EDIT_CUT: copy_selection(buf); cut_selection(buf); break;
            default: changed = 0; break;
            }
            if (move && !select) ed.anchor = ed.caret;
            select = 0;
            len = strlen(buf);
            continue;
        }
        /* A run of text, up to the next editing key, over the selection. */
        size_t add = 0;
        while (p[add] && !DXL_EDIT_IS_OP(p[add])) add++;
        cut_selection(buf);
        len = strlen(buf);
        size_t fits = add;
        if (len + fits >= size) {
            fits = size - 1 - len;
            while (fits > 0 && ((unsigned char)p[fits] & 0xc0) == 0x80) fits--;
        }
        if (fits) {
            memmove(buf + ed.caret + fits, buf + ed.caret, len - ed.caret + 1);
            memcpy(buf + ed.caret, p, fits);
            ed.caret += fits;
            ed.anchor = ed.caret;
            len += fits;
            changed = 1;
        }
        p += add;
        select = 0;
    }
    return changed;
}

/* The width of buf's first n bytes, dp. */
static float width_to(dxl_ui *ui, const char *buf, size_t n) {
    char part[1024];
    if (n >= sizeof part) n = sizeof part - 1;
    if (!n) return 0;
    memcpy(part, buf, n);
    part[n] = '\0';
    return dxl_text_size(ui, DXL_FONT_BODY, part, 0).x;
}

/* The character edge nearest x, dp from the text's start. */
static size_t edge_at(dxl_ui *ui, const char *buf, float x) {
    size_t best = 0, at = 0, len = strlen(buf);
    float best_d = x < 0 ? -x : x;
    while (at < len) {
        at = utf8_ahead(buf, at);
        float d = width_to(ui, buf, at) - x;
        if (d < 0) d = -d;
        if (d < best_d) {
            best_d = d;
            best = at;
        }
    }
    return best;
}

static void begin_edit(dxl_ui *ui, uint32_t id, dxl_rect box, const char *buf, size_t caret, size_t anchor) {
    /* Another field's edit under way: kept, as its field says when it is
     * drawn -- before this one, or after. */
    if (ui->text_id && ui->text_id != id) ui->text_lost = ui->text_id;
    snprintf(ed.original, sizeof ed.original, "%s", buf);
    ed.caret = caret;
    ed.anchor = anchor;
    ed.scroll = 0;
    ed.dragging = 0;
    ed.moved = ui->now;
    dxl_ui_text_begin(ui, id, box);
}

/* A press in the field at x (dp from its text's start): the caret there,
 * Shift growing the selection to it; a double click's word, a triple's
 * all of it. */
static void press_at(dxl_ui *ui, const char *buf, float x) {
    size_t at = edge_at(ui, buf, x);
    if (ui->in.clicks >= 3) {
        ed.anchor = 0;
        ed.caret = strlen(buf);
    } else if (ui->in.clicks == 2) {
        size_t lo = at, hi = at;
        while (lo > 0 && wordy((unsigned char)buf[utf8_back(buf, lo)])) lo = utf8_back(buf, lo);
        while (buf[hi] && wordy((unsigned char)buf[hi])) hi = utf8_ahead(buf, hi);
        ed.anchor = lo;
        ed.caret = hi;
    } else {
        ed.caret = at;
        if (!ui->in.shift_click) ed.anchor = at;
        ed.dragging = 1;
    }
    ed.moved = ui->now;
}

int dxl_text_field(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, char *buf, size_t size,
                   const char *placeholder, int flags) {
    const dxl_theme *t = &ui->theme;
    int ret = 0;
    int editing = ui->text_id == id;
    /* Enter on it as it is edited, before the row takes it as a choice. */
    int enter = editing && ui->in.activate && ui->focus == id;
    int hits;
    dxl_rect a = row(ui, id, r, label, flags, &hits);
    dxl_rect box = { a.x, a.y + 6, a.w, a.h - 12 };
    dxl_rect in = dxl_inset(box, 2);
    in.x += 8;
    in.w -= 16;
    float text_x = in.x - (editing ? ed.scroll : 0);
    int over = ui->in.px >= box.x && ui->in.px < box.x + box.w && ui->in.py >= box.y && ui->in.py < box.y + box.h;
    if (ui->text_lost == id) {
        /* Its edit ended as it was out of view: kept. */
        ui->text_lost = 0;
        ret = DXL_TEXT_DONE;
    }
    if (!(flags & DXL_W_DISABLED)) {
        int pressed = ui->in.pressed && over && dxl_ui_live(ui);
        if (!editing && pressed && ui->mode == DXL_INPUT_MOUSE) {
            /* A click puts the caret where it is. */
            begin_edit(ui, id, box, buf, 0, 0);
            editing = 1;
            text_x = in.x;
            press_at(ui, buf, ui->in.px - text_x);
            pressed = 0;
        } else if (!editing && (hits & DXL_HIT_ACTIVATED)) {
            if (ui->mode == DXL_INPUT_PAD) {
                ret = DXL_TEXT_OSK;
            } else {
                begin_edit(ui, id, box, buf, strlen(buf), strlen(buf));
                editing = 1;
            }
        } else if (!editing && (hits & DXL_HIT_FOCUSED) && ui->text_ready == id && ui->in.text[0]) {
            /* Typed on as it had the focus: what was typed in its place. */
            begin_edit(ui, id, box, buf, strlen(buf), 0);
            editing = 1;
        }
        if (editing) {
            if (pressed) press_at(ui, buf, ui->in.px - text_x);
            else if (ed.dragging && ui->held == id && !ui->in.pressed) {
                size_t at = edge_at(ui, buf, ui->in.px - text_x);
                if (at != ed.caret) {
                    ed.caret = at;
                    ed.moved = ui->now;
                }
            }
            if (ui->held != id) ed.dragging = 0;
            if (edit(ui, buf, size)) ed.moved = ui->now;
            int click_away = ui->in.pressed && !over;
            if (enter || click_away || ui->focus != id) {
                dxl_ui_text_end(ui);
                editing = 0;
                ret = DXL_TEXT_DONE;
            } else if (ui->in.back) {
                snprintf(buf, size, "%s", ed.original);
                dxl_ui_text_end(ui);
                ui->in.back = 0;
                editing = 0;
            }
        }
        /* With the focus and not edited -- an edit just kept too -- it takes
         * the keys' typing. */
        if (!editing && (hits & DXL_HIT_FOCUSED) && ui->mode != DXL_INPUT_PAD) dxl_ui_text_ready(ui, id, box);
    }
    dxl_well(ui, box);
    float lh = dxl_text_line(ui, DXL_FONT_BODY), ty = box.y + (box.h - lh) / 2;
    if (editing) {
        /* Scrolled to keep the caret in sight, and no room left after the
         * text's end. */
        float caret_x = width_to(ui, buf, ed.caret), full = width_to(ui, buf, strlen(buf)), room = in.w - 4;
        if (caret_x - ed.scroll > room) ed.scroll = caret_x - room;
        if (caret_x < ed.scroll) ed.scroll = caret_x;
        if (ed.scroll > 0 && full - ed.scroll < room) ed.scroll = full - room > 0 ? full - room : 0;
        text_x = in.x - ed.scroll;
        dxl_clip_push(ui, in);
        if (ed.caret != ed.anchor) {
            float x0 = width_to(ui, buf, sel_lo()), x1 = width_to(ui, buf, sel_hi());
            dxl_fill(ui, (dxl_rect){ text_x + x0, ty, x1 - x0, lh }, dxl_rgba_fade(t->accent, 0.45f));
        }
        if (*buf) dxl_text_draw(ui, DXL_FONT_BODY, buf, text_x, ty, 0, t->text_focus, 1);
        /* The caret, blinking from its last move; steady under reduced
         * motion. */
        double since = ui->now - ed.moved;
        long phase = since > 0 ? (long)(since / BLINK_S) : 0;
        if (ui->reduced_motion || phase % 2 == 0)
            dxl_fill(ui, (dxl_rect){ text_x + caret_x, ty + 2, 2, lh - 4 }, t->bracket);
        if (!ui->reduced_motion) dxl_ui_wake(ui, ed.moved + (double)(phase + 1) * BLINK_S);
        dxl_clip_pop(ui);
    } else {
        dxl_clip_push(ui, in);
        if (*buf) dxl_text_draw(ui, DXL_FONT_BODY, buf, in.x, ty, 0, t->text, 1);
        else if (placeholder) dxl_text_draw(ui, DXL_FONT_BODY, placeholder, in.x, ty, 0, t->text_disabled, 1);
        dxl_clip_pop(ui);
    }
    return ret;
}

/* ---- key slots -------------------------------------------------------- */

int dxl_key_slot(dxl_ui *ui, uint32_t id, dxl_rect r, const char *name, int flags) {
    const dxl_theme *t = &ui->theme;
    int hits = dxl_ui_focusable(ui, id, r, flags & DXL_W_DISABLED), ret = 0;
    int capturing = ui->capture_id == id;
    if (!(flags & DXL_W_DISABLED)) {
        if (capturing) {
            if (ui->in.back) {
                dxl_ui_capture_end(ui);
                ui->in.back = 0;
                capturing = 0;
                ret = DXL_SLOT_CANCELLED;
            } else if (ui->in.captured) {
                dxl_ui_capture_end(ui);
                capturing = 0;
                ret = DXL_SLOT_CAPTURED;
            }
        } else if (hits & DXL_HIT_ACTIVATED) {
            dxl_ui_capture_begin(ui, id);
            capturing = 1;
        } else if ((hits & DXL_HIT_FOCUSED) && ui->in.clear) {
            ui->in.clear = 0;
            ret = DXL_SLOT_CLEARED;
        }
    }
    int lit = hits & (DXL_HIT_FOCUSED | DXL_HIT_HOVERED);
    dxl_bevel_style s = { lit ? t->face_focus_top : t->face_top, lit ? t->face_focus_bottom : t->face_bottom,
                          dxl_rgba_fade(t->panel_light, 0.8f), t->panel_shadow, 4 };
    if (flags & DXL_W_DISABLED) {
        s.top = dxl_rgba_fade(s.top, 0.45f);
        s.bottom = dxl_rgba_fade(s.bottom, 0.45f);
    }
    dxl_bevel(ui, r, &s);
    if (capturing) {
        dxl_outline(ui, r, 2, t->accent);
        dxl_ui_animate(ui);
    }
    dxl_rect text = dxl_inset(r, 6);
    const char *shown = capturing ? "Press a key..." : name && *name ? name : "\xe2\x80\x94";
    dxl_rgba c = capturing ? t->accent
               : !(name && *name) ? t->text_disabled
               : (flags & DXL_W_WARN) ? t->warn
               : lit ? t->text_focus : t->text;
    dxl_label(ui, text, DXL_FONT_LABEL, shown, c, DXL_W_CENTER | DXL_W_MAY_CUT);
    return ret;
}

/* ---- tabs ------------------------------------------------------------- */

int dxl_tabs(dxl_ui *ui, const char *id, dxl_rect r, const char *const *labels, int n, int *current) {
    const dxl_theme *t = &ui->theme;
    if (n <= 0) return 0;
    int was = *current;
    int step = dxl_ui_live(ui) ? ui->in.shoulder_r - ui->in.shoulder_l + ui->in.tab_next - ui->in.tab_prev : 0;
    if (step) *current = ((*current + step) % n + n) % n;
    float x = r.x, gs = 22;
    if (ui->mode == DXL_INPUT_PAD) {
        x += dxl_glyph(ui, DXL_HINT_TAB_PREV, x, r.y + r.h / 2, gs, 1) + 12;
    }
    /* Each label and its padding, the padding less where all would not fit. */
    float text = 0;
    for (int i = 0; i < n; i++) text += dxl_text_size(ui, DXL_FONT_BUTTON, labels[i], 0).x;
    float room = r.x + r.w - x - (ui->mode == DXL_INPUT_PAD ? gs + 12 : 0) - 4 * (n - 1);
    float pad = (room - text) / n;
    if (pad > 32) pad = 32;
    if (pad < 8) pad = 8;
    uint32_t base = dxl_ui_id(ui, id);
    for (int i = 0; i < n; i++) {
        float w = dxl_text_size(ui, DXL_FONT_BUTTON, labels[i], 0).x + pad;
        dxl_rect tr = { x, r.y, w, r.h };
        uint32_t tid = base ^ ((uint32_t)(i + 1) * 0x9e3779b1u);
        int hits = dxl_ui_focusable(ui, tid, tr, 0);
        dxl_ui_name(ui, tid, labels[i]);
        if (hits & DXL_HIT_ACTIVATED) *current = i;
        if (hits & (DXL_HIT_FOCUSED | DXL_HIT_HOVERED)) dxl_fill(ui, tr, t->row_highlight);
        int on = i == *current;
        dxl_label(ui, tr, DXL_FONT_BUTTON, labels[i], on ? t->text_focus : t->help, DXL_W_CENTER);
        if (on) dxl_fill(ui, (dxl_rect){ tr.x + pad * 0.375f, tr.y + tr.h - 4, tr.w - pad * 0.75f, 3 }, t->accent);
        x += w + 4;
    }
    if (ui->mode == DXL_INPUT_PAD) dxl_glyph(ui, DXL_HINT_TAB_NEXT, x + 8, r.y + r.h / 2, gs, 1);
    dxl_fill(ui, (dxl_rect){ r.x, r.y + r.h - 1, r.w, 1 }, t->grid);
    return *current != was;
}

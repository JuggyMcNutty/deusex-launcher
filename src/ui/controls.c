#include "ui/controls.h"
#include "ui/draw.h"
#include "ui/glyphs.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define PAD 16

/* Values the controls keep beside their own id. */
#define DRAG_KEY  0x51ab1e00u
#define CARET_KEY 0xca9e7000u
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
    if (*index != was) ret = DXL_SPIN_CHANGED;
    return ret;
}

/* ---- text ----------------------------------------------------------- */

/* The text a field had as its edit began: Esc puts it back. One field is
 * edited at a time. */
static char edit_original[1024];

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

/* The frame's typing, in order: text in at the caret, the editing keys as
 * they came. */
static void edit(dxl_ui *ui, char *buf, size_t size, float *caret_v) {
    size_t len = strlen(buf), c = (size_t)*caret_v;
    if (c > len) c = len;
    for (const char *p = ui->in.text; *p;) {
        switch (*p) {
        case DXL_EDIT_ERASE:
            if (c > 0) {
                size_t b = utf8_back(buf, c);
                memmove(buf + b, buf + c, len - c + 1);
                len -= c - b;
                c = b;
            }
            p++;
            continue;
        case DXL_EDIT_DELETE:
            if (c < len) {
                size_t e = utf8_ahead(buf, c);
                memmove(buf + c, buf + e, len - e + 1);
                len -= e - c;
            }
            p++;
            continue;
        case DXL_EDIT_LEFT: c = utf8_back(buf, c); p++; continue;
        case DXL_EDIT_RIGHT: c = utf8_ahead(buf, c); p++; continue;
        case DXL_EDIT_HOME: c = 0; p++; continue;
        case DXL_EDIT_END: c = len; p++; continue;
        default: break;
        }
        /* A run of text, up to the next editing key. */
        size_t add = 0;
        while (p[add] && !strchr("\b\x7f\x02\x06\x01\x05", p[add])) add++;
        if (len + add < size) {
            memmove(buf + c + add, buf + c, len - c + 1);
            memcpy(buf + c, p, add);
            c += add;
            len += add;
        }
        p += add;
    }
    *caret_v = (float)c;
}

int dxl_text_field(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, char *buf, size_t size,
                   const char *placeholder, int flags) {
    const dxl_theme *t = &ui->theme;
    int hits, ret = 0;
    dxl_rect a = row(ui, id, r, label, flags, &hits);
    dxl_rect box = { a.x, a.y + 6, a.w, a.h - 12 };
    float *caret = dxl_ui_value(ui, id ^ CARET_KEY, 0);
    int editing = ui->text_id == id;
    if (!(flags & DXL_W_DISABLED)) {
        if (!editing && (hits & DXL_HIT_ACTIVATED)) {
            if (ui->mode == DXL_INPUT_PAD) {
                ret = DXL_TEXT_OSK;
            } else {
                snprintf(edit_original, sizeof edit_original, "%s", buf);
                dxl_ui_text_begin(ui, id, box);
                *caret = (float)strlen(buf);
                editing = 1;
            }
        } else if (editing) {
            edit(ui, buf, size, caret);
            int click_away = ui->in.pressed && !(hits & DXL_HIT_HOVERED);
            if ((hits & DXL_HIT_ACTIVATED) || click_away || ui->focus != id) {
                dxl_ui_text_end(ui);
                editing = 0;
                ret = DXL_TEXT_DONE;
            } else if (ui->in.back) {
                snprintf(buf, size, "%s", edit_original);
                dxl_ui_text_end(ui);
                ui->in.back = 0;
                editing = 0;
            }
        }
    }
    dxl_well(ui, box);
    dxl_rect in = dxl_inset(box, 2);
    in.x += 8;
    in.w -= 16;
    float lh = dxl_text_line(ui, DXL_FONT_BODY), ty = box.y + (box.h - lh) / 2;
    /* Scrolled to keep the caret in sight. */
    float caret_x = 0, shift = 0;
    if (editing) {
        char before[1024];
        size_t c = (size_t)*caret;
        if (c > strlen(buf)) c = strlen(buf);
        if (c >= sizeof before) c = sizeof before - 1;
        memcpy(before, buf, c);
        before[c] = '\0';
        caret_x = c ? dxl_text_size(ui, DXL_FONT_BODY, before, 0).x : 0;
        if (caret_x > in.w - 4) shift = caret_x - (in.w - 4);
    }
    dxl_clip_push(ui, in);
    if (*buf)
        dxl_text_draw(ui, DXL_FONT_BODY, buf, in.x - shift, ty, 0, editing ? t->text_focus : t->text, 1);
    else if (!editing && placeholder)
        dxl_text_draw(ui, DXL_FONT_BODY, placeholder, in.x, ty, 0, t->text_disabled, 1);
    if (editing) dxl_fill(ui, (dxl_rect){ in.x - shift + caret_x, ty + 2, 2, lh - 4 }, t->bracket);
    dxl_clip_pop(ui);
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
    uint32_t base = dxl_ui_id(ui, id);
    for (int i = 0; i < n; i++) {
        float w = dxl_text_size(ui, DXL_FONT_BUTTON, labels[i], 0).x + 32;
        dxl_rect tr = { x, r.y, w, r.h };
        uint32_t tid = base ^ ((uint32_t)(i + 1) * 0x9e3779b1u);
        int hits = dxl_ui_focusable(ui, tid, tr, 0);
        dxl_ui_name(ui, tid, labels[i]);
        if (hits & DXL_HIT_ACTIVATED) *current = i;
        if (hits & (DXL_HIT_FOCUSED | DXL_HIT_HOVERED)) dxl_fill(ui, tr, t->row_highlight);
        int on = i == *current;
        dxl_label(ui, tr, DXL_FONT_BUTTON, labels[i], on ? t->text_focus : t->help, DXL_W_CENTER);
        if (on) dxl_fill(ui, (dxl_rect){ tr.x + 12, tr.y + tr.h - 4, tr.w - 24, 3 }, t->accent);
        x += w + 4;
    }
    if (ui->mode == DXL_INPUT_PAD) dxl_glyph(ui, DXL_HINT_TAB_NEXT, x + 8, r.y + r.h / 2, gs, 1);
    dxl_fill(ui, (dxl_rect){ r.x, r.y + r.h - 1, r.w, 1 }, t->grid);
    return *current != was;
}

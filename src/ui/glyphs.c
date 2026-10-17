#include "ui/glyphs.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>

static dxl_rgba rgb(int r, int g, int b) {
    dxl_rgba c = { (uint8_t)r, (uint8_t)g, (uint8_t)b, 255 };
    return c;
}

/* The pad's button for a hint: a Nintendo pad confirms with its east
 * button, its A. */
static SDL_GamepadButton face_of(const dxl_ui *ui, dxl_hint h) {
    int nintendo = ui->pad_family == DXL_PADFAMILY_NINTENDO;
    if (h == DXL_HINT_CONFIRM) return nintendo ? SDL_GAMEPAD_BUTTON_EAST : SDL_GAMEPAD_BUTTON_SOUTH;
    return nintendo ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST;
}

/* What the button says on it: the pad's own label, or, with no pad open,
 * its family's. */
static SDL_GamepadButtonLabel label_of(const dxl_ui *ui, SDL_GamepadButton b) {
    SDL_Gamepad *g = ui->pad ? SDL_GetGamepadFromID(ui->pad) : NULL;
    if (g) return SDL_GetGamepadButtonLabel(g, b);
    static const SDL_GamepadButtonLabel xbox[4] = { SDL_GAMEPAD_BUTTON_LABEL_A, SDL_GAMEPAD_BUTTON_LABEL_B,
                                                    SDL_GAMEPAD_BUTTON_LABEL_X, SDL_GAMEPAD_BUTTON_LABEL_Y };
    static const SDL_GamepadButtonLabel ps[4] = { SDL_GAMEPAD_BUTTON_LABEL_CROSS, SDL_GAMEPAD_BUTTON_LABEL_CIRCLE,
                                                  SDL_GAMEPAD_BUTTON_LABEL_SQUARE, SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE };
    static const SDL_GamepadButtonLabel nin[4] = { SDL_GAMEPAD_BUTTON_LABEL_B, SDL_GAMEPAD_BUTTON_LABEL_A,
                                                   SDL_GAMEPAD_BUTTON_LABEL_Y, SDL_GAMEPAD_BUTTON_LABEL_X };
    if (b > SDL_GAMEPAD_BUTTON_NORTH) return SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
    switch (ui->pad_family) {
    case DXL_PADFAMILY_XBOX: return xbox[b];
    case DXL_PADFAMILY_PLAYSTATION: return ps[b];
    case DXL_PADFAMILY_NINTENDO: return nin[b];
    default: return SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
    }
}

static void centred_text(dxl_ui *ui, dxl_font_role role, const char *s, float cx, float cy, dxl_rgba c) {
    SDL_FPoint size = dxl_text_size(ui, role, s, 0);
    dxl_text_draw(ui, role, s, cx - size.x / 2, cy - size.y / 2, 0, c, 1);
}

static void triangle(dxl_ui *ui, float cx, float cy, float r, int dir, dxl_rgba c) {
    SDL_FPoint p[3];
    switch (dir) {
    case DXL_DIR_UP:    p[0] = (SDL_FPoint){ cx, cy - r }; p[1] = (SDL_FPoint){ cx + r, cy + r * 0.6f }; p[2] = (SDL_FPoint){ cx - r, cy + r * 0.6f }; break;
    case DXL_DIR_DOWN:  p[0] = (SDL_FPoint){ cx - r, cy - r * 0.6f }; p[1] = (SDL_FPoint){ cx + r, cy - r * 0.6f }; p[2] = (SDL_FPoint){ cx, cy + r }; break;
    case DXL_DIR_LEFT:  p[0] = (SDL_FPoint){ cx - r, cy }; p[1] = (SDL_FPoint){ cx + r * 0.6f, cy - r }; p[2] = (SDL_FPoint){ cx + r * 0.6f, cy + r }; break;
    default:            p[0] = (SDL_FPoint){ cx - r * 0.6f, cy - r }; p[1] = (SDL_FPoint){ cx + r, cy }; p[2] = (SDL_FPoint){ cx - r * 0.6f, cy + r }; break;
    }
    dxl_poly(ui, p, 3, c);
}

/* ---- the keyboard ------------------------------------------------------ */

static float keycap(dxl_ui *ui, const char *text, float x, float cy, float s, int draw) {
    const dxl_theme *t = &ui->theme;
    float w = s;
    if (text) {
        float tw = dxl_text_size(ui, DXL_FONT_SMALL, text, 0).x + 14;
        if (tw > w) w = tw;
    }
    if (!draw) return w;
    dxl_rect r = { x, cy - s / 2, w, s };
    dxl_bevel_style st = { t->face_top, t->face_bottom, t->panel_light, t->panel_shadow, 3 };
    dxl_bevel(ui, r, &st);
    if (text) {
        centred_text(ui, DXL_FONT_SMALL, text, x + w / 2, cy, t->text_focus);
    } else {
        /* The arrow keys, as one cap with the four arrows on it. */
        float cx = x + w / 2, d = s * 0.25f, a = s * 0.11f;
        triangle(ui, cx, cy - d, a, DXL_DIR_UP, t->text_focus);
        triangle(ui, cx, cy + d, a, DXL_DIR_DOWN, t->text_focus);
        triangle(ui, cx - d, cy, a, DXL_DIR_LEFT, t->text_focus);
        triangle(ui, cx + d, cy, a, DXL_DIR_RIGHT, t->text_focus);
    }
    return w;
}

/* ---- the pad --------------------------------------------------------- */

static void ring(dxl_ui *ui, float cx, float cy, float r, float w, dxl_rgba c, dxl_rgba inside) {
    dxl_disc(ui, cx, cy, r, c);
    dxl_disc(ui, cx, cy, r - w, inside);
}

static float face_button(dxl_ui *ui, SDL_GamepadButton b, float x, float cy, float s, int draw) {
    if (!draw) return s;
    const dxl_theme *t = &ui->theme;
    float r = s / 2, cx = x + r;
    dxl_rgba face = dxl_rgba_mix(t->face_bottom, t->panel_shadow, 0.3f);
    ring(ui, cx, cy, r, 1.5f, t->panel_light, face);
    int xbox = ui->pad_family == DXL_PADFAMILY_XBOX;
    dxl_rgba ink = t->text_focus;
    float k = s * 0.26f;
    switch (label_of(ui, b)) {
    case SDL_GAMEPAD_BUTTON_LABEL_A: centred_text(ui, DXL_FONT_LABEL, "A", cx, cy, xbox ? rgb(120, 205, 110) : ink); break;
    case SDL_GAMEPAD_BUTTON_LABEL_B: centred_text(ui, DXL_FONT_LABEL, "B", cx, cy, xbox ? rgb(235, 100, 90) : ink); break;
    case SDL_GAMEPAD_BUTTON_LABEL_X: centred_text(ui, DXL_FONT_LABEL, "X", cx, cy, xbox ? rgb(95, 155, 240) : ink); break;
    case SDL_GAMEPAD_BUTTON_LABEL_Y: centred_text(ui, DXL_FONT_LABEL, "Y", cx, cy, xbox ? rgb(240, 205, 85) : ink); break;
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS: {
        dxl_rgba c = rgb(140, 175, 235);
        dxl_line(ui, cx - k, cy - k, cx + k, cy + k, 2, c);
        dxl_line(ui, cx - k, cy + k, cx + k, cy - k, 2, c);
        break;
    }
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
        ring(ui, cx, cy, k * 1.05f, 2, rgb(235, 110, 110), face);
        break;
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: {
        dxl_rect sq = { cx - k * 0.85f, cy - k * 0.85f, k * 1.7f, k * 1.7f };
        dxl_outline(ui, sq, 2, rgb(225, 145, 205));
        break;
    }
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: {
        dxl_rgba c = rgb(100, 205, 175);
        float ty = cy - k, by = cy + k * 0.7f;
        dxl_line(ui, cx, ty, cx + k, by, 2, c);
        dxl_line(ui, cx + k, by, cx - k, by, 2, c);
        dxl_line(ui, cx - k, by, cx, ty, 2, c);
        break;
    }
    default: {
        /* No label known: the four face buttons as dots, this one lit. */
        static const float at[4][2] = { { 0, 1 }, { 1, 0 }, { -1, 0 }, { 0, -1 } };
        for (int i = 0; i < 4; i++) {
            float dx = at[i][0] * k, dy = at[i][1] * k;
            dxl_disc(ui, cx + dx, cy + dy, s * 0.1f, i == (int)b ? ink : dxl_rgba_fade(t->text_disabled, 0.9f));
        }
        break;
    }
    }
    return s;
}

static float start_button(dxl_ui *ui, float x, float cy, float s, int draw) {
    float w = s * 1.5f, h = s * 0.72f;
    if (!draw) return w;
    const dxl_theme *t = &ui->theme;
    dxl_rect r = { x, cy - h / 2, w, h };
    dxl_bevel_style st = { t->face_top, t->face_bottom, t->panel_light, t->panel_shadow, h / 2 - 1 };
    dxl_bevel(ui, r, &st);
    float cx = x + w / 2, l = s * 0.24f;
    if (ui->pad_family == DXL_PADFAMILY_NINTENDO) {
        dxl_line(ui, cx - l, cy, cx + l, cy, 2, t->text_focus);
        dxl_line(ui, cx, cy - l, cx, cy + l, 2, t->text_focus);
    } else {
        for (int i = -1; i <= 1; i++) {
            float y = cy + i * s * 0.12f;
            dxl_line(ui, cx - l, y, cx + l, y, 1.5f, t->text_focus);
        }
    }
    return w;
}

static float dpad(dxl_ui *ui, float x, float cy, float s, int draw) {
    if (!draw) return s;
    const dxl_theme *t = &ui->theme;
    float cx = x + s / 2, arm = s * 0.34f;
    dxl_rect hz = { x, cy - arm / 2, s, arm }, vt = { cx - arm / 2, cy - s / 2, arm, s };
    dxl_rect hz2 = hz, vt2 = vt;
    hz2.x -= 1; hz2.y -= 1; hz2.w += 2; hz2.h += 2;
    vt2.x -= 1; vt2.y -= 1; vt2.w += 2; vt2.h += 2;
    dxl_fill(ui, hz2, t->panel_light);
    dxl_fill(ui, vt2, t->panel_light);
    dxl_fill(ui, hz, t->face_bottom);
    dxl_fill(ui, vt, t->face_bottom);
    float a = s * 0.1f, d = s * 0.33f;
    triangle(ui, cx, cy - d, a, DXL_DIR_UP, t->text_focus);
    triangle(ui, cx, cy + d, a, DXL_DIR_DOWN, t->text_focus);
    triangle(ui, cx - d, cy, a, DXL_DIR_LEFT, t->text_focus);
    triangle(ui, cx + d, cy, a, DXL_DIR_RIGHT, t->text_focus);
    return s;
}

/* A shoulder: a tab with its family's name for it. */
static float shoulder(dxl_ui *ui, int right, float x, float cy, float s, int draw) {
    static const char *const names[4][2] = { { "LB", "RB" }, { "L1", "R1" }, { "L", "R" }, { "L1", "R1" } };
    const char *name = names[ui->pad_family][right];
    float w = dxl_text_size(ui, DXL_FONT_SMALL, name, 0).x + 14;
    if (w < s * 1.3f) w = s * 1.3f;
    if (!draw) return w;
    const dxl_theme *t = &ui->theme;
    float h = s * 0.78f;
    dxl_rect r = { x, cy - h / 2, w, h };
    dxl_bevel_style st = { t->face_top, t->face_bottom, t->panel_light, t->panel_shadow, h * 0.32f };
    dxl_bevel(ui, r, &st);
    centred_text(ui, DXL_FONT_SMALL, name, x + w / 2, cy, t->text_focus);
    return w;
}

float dxl_glyph(dxl_ui *ui, dxl_hint h, float x, float cy, float size, int draw) {
    if (ui->mode == DXL_INPUT_PAD) {
        switch (h) {
        case DXL_HINT_CONFIRM:
        case DXL_HINT_BACK: return face_button(ui, face_of(ui, h), x, cy, size, draw);
        case DXL_HINT_PLAY: return start_button(ui, x, cy, size, draw);
        case DXL_HINT_MOVE: return dpad(ui, x, cy, size, draw);
        case DXL_HINT_TAB_PREV: return shoulder(ui, 0, x, cy, size, draw);
        case DXL_HINT_TAB_NEXT: return shoulder(ui, 1, x, cy, size, draw);
        case DXL_HINT_CLEAR: return face_button(ui, SDL_GAMEPAD_BUTTON_WEST, x, cy, size, draw);
        default: return 0;
        }
    }
    switch (h) {
    case DXL_HINT_CONFIRM: return keycap(ui, "Enter", x, cy, size, draw);
    case DXL_HINT_BACK: return keycap(ui, "Esc", x, cy, size, draw);
    case DXL_HINT_PLAY: return keycap(ui, "F5", x, cy, size, draw);
    case DXL_HINT_MOVE: return keycap(ui, NULL, x, cy, size, draw);
    case DXL_HINT_TAB_PREV: return keycap(ui, "Ctrl+Shift+Tab", x, cy, size, draw);
    case DXL_HINT_TAB_NEXT: return keycap(ui, "Ctrl+Tab", x, cy, size, draw);
    case DXL_HINT_CLEAR: return keycap(ui, "Delete", x, cy, size, draw);
    default: return 0;
    }
}

int dxl_hints(dxl_ui *ui, dxl_rect r, const dxl_hint_item *items, int n) {
    const dxl_theme *t = &ui->theme;
    const float gs = 22, gap = 8, between = 26;
    float widths[8], total = 0;
    if (n > 8) n = 8;
    for (int i = 0; i < n; i++) {
        widths[i] = dxl_glyph(ui, items[i].hint, 0, 0, gs, 0) + gap +
                    dxl_text_size(ui, DXL_FONT_LABEL, items[i].label, 0).x;
        total += widths[i] + (i ? between : 0);
    }
    float x = r.x + r.w - total, cy = r.y + r.h / 2;
    int clicked = -1;
    for (int i = 0; i < n; i++) {
        char name[32];
        snprintf(name, sizeof name, "hint %d", (int)items[i].hint);
        dxl_rect area = { x - 8, r.y, widths[i] + 16, r.h };
        int hits = dxl_ui_clickable(ui, dxl_ui_id(ui, name), area);
        float gw = dxl_glyph(ui, items[i].hint, x, cy, gs, 1);
        dxl_rect label = { x + gw + gap, r.y, widths[i] - gw - gap + 1, r.h };
        dxl_label(ui, label, DXL_FONT_LABEL, items[i].label, (hits & DXL_HIT_HOVERED) ? t->text_focus : t->help,
                  DXL_W_LEFT | DXL_W_MAY_CUT);
        if (hits & DXL_HIT_ACTIVATED) clicked = (int)items[i].hint;
        x += widths[i] + between;
    }
    return clicked;
}

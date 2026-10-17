/* Settings' Controller tab. On the left the controllers SDL sees and the
 * one chosen as a model (ui/pad3d.h), live, each control naming what it
 * does on a leader line, the sticks and the triggers under it as the engine
 * reads them (core/padcurve.h); on the right its rows: the layout's preset,
 * the controller's test, each button's action (core/bindings.h, User.ini's
 * Joy lines), the tuning's rows (Settings.json's Gamepad), and the mapping
 * wizard for a controller SDL has no mapping for (screens/padwizard.c).
 *
 * The test is a dialog of the same model, larger, while which every pad's
 * input drives it alone (the kit's pads_off): the controls pressed lit and
 * their leaders' labels with them, the triggers rumbling the controller as
 * far as they are pulled; its back button held a second, Esc or Stop ends
 * it. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/bindings.h"
#include "core/log.h"
#include "core/padcurve.h"
#include "core/padmap.h"
#include "ui/controls.h"
#include "ui/draw.h"
#include "ui/pad3d.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROW_H 46
#define STRIP_H 84
#define LEADER_GAP 17       /* a label's height and the room between two */

/* ---- the controllers --------------------------------------------------- */

typedef struct {
    SDL_JoystickID id;
    int  gamepad;               /* SDL has a mapping for it */
    char name[96];
} pad_entry;

static int pads_now(pad_entry *out, int max) {
    int count = 0, n = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&count);
    for (int i = 0; ids && i < count && n < max; i++) {
        out[n].id = ids[i];
        out[n].gamepad = SDL_IsGamepad(ids[i]);
        const char *name = out[n].gamepad ? SDL_GetGamepadNameForID(ids[i]) : SDL_GetJoystickNameForID(ids[i]);
        snprintf(out[n].name, sizeof out[n].name, "%s", name && *name ? name : "A controller");
        n++;
    }
    SDL_free(ids);
    return n;
}

/* Its labels' family: one SDL has no mapping for, the plain names. */
static dxl_pad_kind kind_of(const pad_entry *p) {
    SDL_Gamepad *g = p->gamepad ? SDL_GetGamepadFromID(p->id) : NULL;
    return g ? (dxl_pad_kind)dxl_ui_pad_family(g) : DXL_PAD_KIND_GENERIC;
}

/* The control of a part, as the game has it: its Joy key; NULL for one that
 * is none of the game's (the guide button, the sticks' axes). */
static const char *key_of(int part) {
    switch (part) {
    case DXL_PADPART_SOUTH: return "Joy1";
    case DXL_PADPART_EAST: return "Joy2";
    case DXL_PADPART_WEST: return "Joy3";
    case DXL_PADPART_NORTH: return "Joy4";
    case DXL_PADPART_LSHOULDER: return "Joy5";
    case DXL_PADPART_RSHOULDER: return "Joy6";
    case DXL_PADPART_BACK: return "Joy7";
    case DXL_PADPART_START: return "Joy8";
    case DXL_PADPART_LSTICK: return "Joy9";
    case DXL_PADPART_RSTICK: return "Joy10";
    case DXL_PADPART_LTRIGGER: return "Joy11";
    case DXL_PADPART_RTRIGGER: return "Joy12";
    case DXL_PADPART_DPUP: return "JoyPovUp";
    case DXL_PADPART_DPDOWN: return "JoyPovDown";
    case DXL_PADPART_DPLEFT: return "JoyPovLeft";
    case DXL_PADPART_DPRIGHT: return "JoyPovRight";
    default: return NULL;
    }
}

static int part_of(const char *key) {
    for (int p = 0; p < DXL_PADPART_COUNT; p++)
        if (key_of(p) && !dxl_stricmp(key_of(p), key)) return p;
    return -1;
}

/* What a stick does, by what its across axis is bound to. */
static const char *stick_does(const dxl_ini *user, int left) {
    const char *b = user ? dxl_ini_get(user, DXL_INPUT_SECTION, left ? "JoyX" : "JoyU") : NULL;
    if (!b || !*b) return "Nothing";
    if (dxl_stristr(b, "aStrafe")) return "Moves";
    if (dxl_stristr(b, "aBaseX") || dxl_stristr(b, "aTurn")) return "Looks";
    return "Bound by hand";
}

static const char *binding_of(const dxl_ini *user, const char *key) {
    const char *b = user ? dxl_ini_get(user, DXL_INPUT_SECTION, key) : NULL;
    return b ? b : "";
}

/* The shown controller's state, for the model and the meters. */
static void pad_state(SDL_Gamepad *g, dxl_pad3d_state *st) {
    if (!g) return;
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT && b + 1 < DXL_PADPART_LTRIGGER; b++)
        st->down[DXL_PADPART_OF_BUTTON(b)] = SDL_GetGamepadButton(g, (SDL_GamepadButton)b);
    st->lx = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    st->ly = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    st->rx = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
    st->ry = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;
    st->lt = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
    st->rt = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
}

/* Where its mapping comes from: ours, which SDL takes over Steam's
 * (dxl_pad_mappings), Steam's, SDL's own. */
static const char *mapping_source(dxl_shell *s, SDL_JoystickID id) {
    char guid[33], bare[33];
    SDL_GUIDToString(SDL_GetJoystickGUIDForID(id), guid, sizeof guid);
    if (!s->pad_db) {
        char *db = dxl_run_pad_db(s->run);
        s->pad_db = dxl_padmap_db_read(db);
        free(db);
    }
    if (dxl_padmap_db_has(s->pad_db, guid)) return "the launcher's mapping";
    const char *steam = getenv("SDL_GAMECONTROLLERCONFIG");
    if (steam && (dxl_stristr(steam, guid) || (dxl_padmap_guid_no_crc(guid, bare) && dxl_stristr(steam, bare))))
        return "Steam's mapping";
    return "SDL's mapping";
}

/* The line under the controller's name: what it is, how it is joined,
 * its battery, whose mapping, whether the game takes it. */
static void details(dxl_shell *s, const pad_entry *p, int game_pad, char *out, size_t size) {
    SDL_Gamepad *g = p->gamepad ? SDL_GetGamepadFromID(p->id) : NULL;
    dxl_buf b;
    dxl_buf_init(&b);
    if (!p->gamepad) {
        dxl_buf_puts(&b, "SDL knows no mapping for it: map it below, and the game can use it");
    } else {
        if (g) {
            SDL_JoystickConnectionState c = SDL_GetGamepadConnectionState(g);
            if (c == SDL_JOYSTICK_CONNECTION_WIRED) dxl_buf_word(&b, "Wired");
            else if (c == SDL_JOYSTICK_CONNECTION_WIRELESS) dxl_buf_word(&b, "Wireless");
            int percent = -1;
            SDL_PowerState power = SDL_GetGamepadPowerInfo(g, &percent);
            if (power == SDL_POWERSTATE_ON_BATTERY && percent >= 0) dxl_buf_printf(&b, "%sbattery %d%%", b.len ? " · " : "", percent);
            else if (power == SDL_POWERSTATE_CHARGING) dxl_buf_printf(&b, "%scharging", b.len ? " · " : "");
        }
        dxl_buf_printf(&b, "%s%s", b.len ? " · " : "", mapping_source(s, p->id));
        dxl_buf_printf(&b, " · %s", game_pad ? "the game's" : "not the game's: it takes the first");
        if (SDL_GetJoystickVendorForID(p->id) == 0x28de)
            dxl_buf_puts(&b, " · Steam Input is in charge: Steam's own layout comes first, these bind what it gives");
    }
    snprintf(out, size, "%s", b.data ? b.data : "");
    dxl_buf_free(&b);
}

/* ---- the model -------------------------------------------------------- */

static void ring(dxl_ui *ui, float cx, float cy, float r, float w, dxl_rgba c) {
    enum { SEG = 36 };
    for (int i = 0; i < SEG; i++) {
        float a0 = (float)i / SEG * 6.2831853f, a1 = (float)(i + 1) / SEG * 6.2831853f;
        dxl_line(ui, cx + cosf(a0) * r, cy + sinf(a0) * r, cx + cosf(a1) * r, cy + sinf(a1) * r, w, c);
    }
}

/* A stick's circle: its travel, the dead zone's ring, where it is (the dim
 * dot) and what the game gets of it (the bright one), up positive. */
static void stick_meter(dxl_ui *ui, float cx, float cy, float r, float x, float y, float gx, float gy, float dz,
                        const char *label) {
    const dxl_theme *t = &ui->theme;
    dxl_disc(ui, cx, cy, r, dxl_rgba_fade(t->panel_bottom, 0.9f));
    ring(ui, cx, cy, r, 1, dxl_rgba_fade(t->accent, 0.5f));
    ring(ui, cx, cy, r * dz, 1, dxl_rgba_fade(t->help, 0.7f));
    dxl_disc(ui, cx + x * r, cy + y * r, 3, dxl_rgba_fade(t->text, 0.6f));
    dxl_disc(ui, cx + gx * r, cy - gy * r, 4, t->accent);
    dxl_rect lr = { cx - 60, cy + r + 2, 120, 16 };
    dxl_label(ui, lr, DXL_FONT_SMALL, label, t->help, DXL_W_CENTER | DXL_W_MAY_CUT);
}

/* A trigger's bar, its press and release marks. */
static void trigger_meter(dxl_ui *ui, dxl_rect r, float v, int down, const char *label) {
    const dxl_theme *t = &ui->theme;
    dxl_rect bar = { r.x, r.y, r.w, r.h - 18 };
    dxl_fill(ui, bar, dxl_rgba_fade(t->panel_bottom, 0.9f));
    dxl_rect fill = { bar.x, bar.y + bar.h * (1 - v), bar.w, bar.h * v };
    dxl_fill(ui, fill, down ? t->accent : dxl_rgba_fade(t->accent, 0.45f));
    float press = bar.y + bar.h * (1 - DXL_PAD_TRIGGER_PRESS), release = bar.y + bar.h * (1 - DXL_PAD_TRIGGER_RELEASE);
    dxl_line(ui, bar.x - 3, press, bar.x + bar.w + 3, press, 1, t->text);
    dxl_line(ui, bar.x - 3, release, bar.x + bar.w + 3, release, 1, t->help);
    dxl_rect lr = { r.x - 20, bar.y + bar.h + 2, r.w + 40, 16 };
    dxl_label(ui, lr, DXL_FONT_SMALL, label, t->help, DXL_W_CENTER | DXL_W_MAY_CUT);
}

typedef struct {
    int part;
    float y;
    char text[64];
} leader;

static int by_y(const void *a, const void *b) {
    float x = ((const leader *)a)->y, y = ((const leader *)b)->y;
    return x < y ? -1 : x > y;
}

/* Each control's line out to what it does, the labels down each side,
 * apart; the chosen control's lit, and under a test those pressed. */
static void leaders(dxl_shell *s, dxl_rect r, float model_l, float model_r, const dxl_pad3d_spot *spots, int chosen,
                    const int *pressed) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    leader side[2][DXL_PADPART_COUNT];
    int count[2] = { 0, 0 };
    float mid = (model_l + model_r) / 2;
    for (int p = 0; p < DXL_PADPART_COUNT; p++) {
        if (!spots[p].seen || p == DXL_PADPART_GUIDE) continue;
        int right = spots[p].x > mid;
        leader *l = &side[right][count[right]++];
        l->part = p;
        l->y = spots[p].y;
        char what[64];
        const char *b = binding_of(s->stores.user, key_of(p));
        if (*b) dxl_binding_describe(b, what, sizeof what);
        else snprintf(what, sizeof what, "\xe2\x80\x94");
        if (p == DXL_PADPART_LSTICK || p == DXL_PADPART_RSTICK)
            snprintf(l->text, sizeof l->text, "%s; %.40s", stick_does(s->stores.user, p == DXL_PADPART_LSTICK),
                     *b ? what : "no click");
        else
            snprintf(l->text, sizeof l->text, "%s", what);
    }
    for (int k = 0; k < 2; k++) {
        /* A side's labels taller than the panel: the lit controls' alone. */
        if (count[k] && (count[k] - 1) * LEADER_GAP + 16 > r.h) {
            int kept = 0;
            for (int i = 0; i < count[k]; i++)
                if (side[k][i].part == chosen || (pressed && pressed[side[k][i].part])) side[k][kept++] = side[k][i];
            count[k] = kept;
        }
        qsort(side[k], (size_t)count[k], sizeof side[k][0], by_y);
        for (int i = 0; i < count[k]; i++) {
            if (side[k][i].y < r.y + 8) side[k][i].y = r.y + 8;
            if (i > 0 && side[k][i].y < side[k][i - 1].y + LEADER_GAP) side[k][i].y = side[k][i - 1].y + LEADER_GAP;
        }
        /* Off the bottom: the whole side moved up as far as it must. */
        if (count[k] && side[k][count[k] - 1].y > r.y + r.h - 8) {
            float up = side[k][count[k] - 1].y - (r.y + r.h - 8);
            for (int i = 0; i < count[k]; i++) side[k][i].y -= up;
        }
        for (int i = 0; i < count[k]; i++) {
            const leader *l = &side[k][i];
            int lit = l->part == chosen || (pressed && pressed[l->part]);
            dxl_rgba c = lit ? t->text_focus : dxl_rgba_fade(t->help, 0.9f);
            float edge = k ? model_r : model_l;
            dxl_line(ui, spots[l->part].x, spots[l->part].y, edge, l->y, 1, dxl_rgba_fade(c, 0.7f));
            dxl_disc(ui, spots[l->part].x, spots[l->part].y, 2.5f, c);
            dxl_rect lr = k ? (dxl_rect){ model_r + 4, l->y - 8, r.x + r.w - model_r - 4, 16 }
                            : (dxl_rect){ r.x, l->y - 8, model_l - r.x - 4, 16 };
            dxl_label(ui, lr, DXL_FONT_SMALL, l->text, c, (k ? DXL_W_LEFT : DXL_W_RIGHT) | DXL_W_MAY_CUT);
        }
    }
}

/* The model and its meters, the controller chosen above them; testing,
 * the one tried, its controls pressed lit. */
static void left_panel(dxl_shell *s, dxl_rect r, const pad_entry *pads, int n, int shown, int game,
                       dxl_pad_kind kind, int chosen, int testing) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_well(ui, r);
    dxl_rect in = dxl_inset(r, 10);

    /* Which controller: a choice when there are several. */
    dxl_rect top = dxl_cut_top(&in, 30);
    if (n > 1 && !testing) {
        static const char *names[16];
        int count = n < 16 ? n : 16, index = shown;
        for (int i = 0; i < count; i++) names[i] = pads[i].name;
        if (dxl_spinner(ui, dxl_ui_id(ui, "pad which"), top, "Controller", &index, names, count, DXL_W_MAY_CUT) ==
            DXL_SPIN_CHANGED) {
            /* The rest of the panel the one chosen now; the rows follow
             * on the next frame. */
            s->pad_id = pads[index].id;
            shown = index;
            kind = kind_of(&pads[shown]);
            dxl_ui_animate(ui);
        }
    } else {
        dxl_label(ui, top, DXL_FONT_LABEL, n ? pads[shown].name : "No controller connected", n ? t->text : t->help,
                  DXL_W_LEFT | DXL_W_MAY_CUT);
    }
    char line[400] = "The layout as an Xbox pad's: connect a controller to see its own.";
    if (n) details(s, &pads[shown], shown == game, line, sizeof line);
    dxl_label(ui, dxl_cut_top(&in, 20), DXL_FONT_SMALL, line, n && !pads[shown].gamepad ? t->warn : t->help,
              DXL_W_LEFT | DXL_W_MAY_CUT);
    dxl_cut_top(&in, 4);

    SDL_Gamepad *g = n && pads[shown].gamepad ? SDL_GetGamepadFromID(pads[shown].id) : NULL;
    dxl_pad3d_state st;
    memset(&st, 0, sizeof st);
    st.chosen = chosen;
    pad_state(g, &st);

    dxl_rect strip = dxl_cut_bottom(&in, STRIP_H);
    /* A panel taller than the model and its labels need: they and the
     * meters together in its middle. */
    float label_w = in.w * 0.22f;
    float need = fmaxf(dxl_pad3d_fit_height(in.w - 2 * label_w), 8 * LEADER_GAP + 16) + 16;
    if (in.h > need) {
        float spare = in.h - need;
        in.y += spare / 2;
        in.h = need;
        strip.y -= spare / 2;
    }
    /* The view: turned a little, swaying, dragged round, or by the right
     * stick -- but under a test, whose stick is the model's. */
    float sway = ui->reduced_motion ? 0 : (float)sin(ui->now * 0.6) * 0.14f;
    if (!ui->reduced_motion) dxl_ui_animate(ui);
    uint32_t model_id = dxl_ui_id(ui, testing ? "pad test model" : "pad model");
    int hits = dxl_ui_clickable(ui, model_id, in);
    if (ui->held == model_id) {
        s->pad_yaw += (ui->in.px - s->pad_drag_x) * 0.01f;
        s->pad_pitch += (ui->in.py - s->pad_drag_y) * 0.01f;
    }
    s->pad_drag_x = ui->in.px;
    s->pad_drag_y = ui->in.py;
    if (g && !ui->capture_id && !ui->text_id && !ui->pads_off) {
        float rx = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
        if (fabsf(rx) > 0.3f) {
            s->pad_yaw += rx * ui->dt * 1.5f;
            dxl_ui_animate(ui);
        }
    }
    if (s->pad_pitch < -0.4f) s->pad_pitch = -0.4f;
    if (s->pad_pitch > 0.6f) s->pad_pitch = 0.6f;
    st.yaw = s->pad_yaw + sway;
    st.pitch = 0.38f + s->pad_pitch;

    /* The model in the middle, the labels either side of it. */
    dxl_rect model = { in.x + label_w, in.y, in.w - 2 * label_w, in.h };
    if (s->pad_mesh_family != (int)kind) {
        dxl_padmesh_free(&s->pad_mesh);
        dxl_padmesh_build(&s->pad_mesh, (int)kind);
        s->pad_mesh_family = (int)kind;
    }
    dxl_pad3d_spot spots[DXL_PADPART_COUNT];
    dxl_pad3d_draw(ui, &s->pad_mesh, model, &st, spots);
    /* Under a test, what is pressed, pulled or pushed. */
    int pressed[DXL_PADPART_COUNT];
    memcpy(pressed, st.down, sizeof pressed);
    pressed[DXL_PADPART_LTRIGGER] = st.lt > DXL_PAD_TRIGGER_RELEASE;
    pressed[DXL_PADPART_RTRIGGER] = st.rt > DXL_PAD_TRIGGER_RELEASE;
    pressed[DXL_PADPART_LSTICK] |= fabsf(st.lx) > 0.3f || fabsf(st.ly) > 0.3f;
    pressed[DXL_PADPART_RSTICK] |= fabsf(st.rx) > 0.3f || fabsf(st.ry) > 0.3f;
    leaders(s, in, model.x, model.x + model.w, spots, chosen, testing ? pressed : NULL);
    /* A control clicked on the model: its row. */
    if (hits & DXL_HIT_ACTIVATED) {
        int p = dxl_pad3d_hit(&s->pad_mesh, spots, ui->in.px, ui->in.py);
        if (p >= 0 && key_of(p)) s->pad_find = p;
    }

    /* The sticks and triggers as the engine reads them. */
    dxl_pad_tuning tune = { 0.2f, 1, 1, 0 };
    if (s->stores.engine) {
        tune.dead_zone = (float)dxl_es_number(s->stores.engine, DXL_ES_PAD_DEADZONE);
        tune.look_x = (float)dxl_es_number(s->stores.engine, DXL_ES_PAD_LOOK_X);
        tune.look_y = (float)dxl_es_number(s->stores.engine, DXL_ES_PAD_LOOK_Y);
        tune.invert_y = dxl_es_bool(s->stores.engine, DXL_ES_PAD_INVERT_Y);
    }
    float mx, my, lx2, ly2;
    dxl_pad_move(st.lx, st.ly, &tune, &mx, &my);
    dxl_pad_look(st.rx, st.ry, &tune, &lx2, &ly2);
    float cr = (strip.h - 22) / 2, cy = strip.y + cr + 2;
    stick_meter(ui, strip.x + strip.w * 0.18f, cy, cr, st.lx, st.ly, mx, my, tune.dead_zone, "Left stick");
    stick_meter(ui, strip.x + strip.w * 0.82f, cy, cr, st.rx, st.ry, fmaxf(-1, fminf(1, lx2)), fmaxf(-1, fminf(1, ly2)),
                tune.dead_zone, "Right stick");
    static int lt_down, rt_down;
    lt_down = dxl_pad_trigger_down(st.lt, lt_down);
    rt_down = dxl_pad_trigger_down(st.rt, rt_down);
    trigger_meter(ui, (dxl_rect){ strip.x + strip.w * 0.42f - 8, strip.y + 2, 16, strip.h - 4 }, st.lt, lt_down,
                  dxl_pad_control_label(kind, "Joy11"));
    trigger_meter(ui, (dxl_rect){ strip.x + strip.w * 0.58f - 8, strip.y + 2, 16, strip.h - 4 }, st.rt, rt_down,
                  dxl_pad_control_label(kind, "Joy12"));
}

/* ---- the rows ---------------------------------------------------------- */

/* A button's row: its name on this controller, what it does; chosen, the
 * actions to pick from. */
static void button_row(dxl_shell *s, dxl_list *l, int i, const char *key, dxl_pad_kind kind, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    uint32_t id = dxl_list_row_id(l, i);
    const char *label = dxl_pad_control_label(kind, key);
    int hits = dxl_ui_focusable(ui, id, r, 0);
    dxl_ui_name(ui, id, label);
    int lit = hits & DXL_HIT_FOCUSED;
    if (lit) dxl_fill(ui, r, t->row_focus);
    else if (hits & DXL_HIT_HOVERED) dxl_fill(ui, r, t->row_highlight);
    float lw = dxl_control_label_w(r.w);
    dxl_label(ui, (dxl_rect){ r.x + 16, r.y, lw - 16, r.h }, DXL_FONT_BODY, label, lit ? t->text_focus : t->text,
              DXL_W_LEFT | DXL_W_MAY_CUT);
    char what[64];
    dxl_binding_describe(binding_of(s->stores.user, key), what, sizeof what);
    dxl_label(ui, (dxl_rect){ r.x + lw + 8, r.y, r.w - lw - 24, r.h }, DXL_FONT_BODY, what,
              lit ? t->text_focus : t->help, DXL_W_RIGHT | DXL_W_MAY_CUT);

    /* The actions, by group: the catalogue's, but its first, "Nothing", too. */
    const char *picked = dxl_shell_picked(s, dxl_shell_answer(s, id));
    for (size_t a = 0; picked && a < dxl_pad_action_count(); a++)
        if (!strcmp(dxl_pad_action_at(a)->label, picked)) {
            dxl_bindings_set(s->stores.user, key, dxl_pad_action_at(a)->command);
            dxl_log("pad: %s for %s", key, dxl_pad_action_at(a)->command);
        }
    if (hits & DXL_HIT_ACTIVATED) {
        static const char *labels[DXL_PICK_MAX], *groups[DXL_PICK_MAX];
        int n = 0;
        for (size_t a = 0; a < dxl_pad_action_count() && n < DXL_PICK_MAX; a++, n++) {
            labels[n] = dxl_pad_action_at(a)->label;
            groups[n] = dxl_pad_action_at(a)->group;
        }
        int now = dxl_pad_action_find(binding_of(s->stores.user, key));
        dxl_shell_pick_with(s, id, label, labels, groups, n, now < 0 ? 0 : now);
    }
}

void dxl_page_controller(dxl_shell *s, dxl_rect area, const char **why, const dxl_setting **focused) {
    dxl_ui *ui = &s->ui;
    pad_entry pads[16];
    int n = pads_now(pads, 16), shown = 0, game = -1;
    /* The game takes the first that SDL has a mapping for. */
    for (int i = 0; i < n && game < 0; i++)
        if (pads[i].gamepad) game = i;
    if (game >= 0) shown = game;
    for (int i = 0; i < n; i++)
        if (pads[i].id == s->pad_id) shown = i;
    if (n && pads[shown].id != s->pad_id) s->pad_id = pads[shown].id;
    SDL_Gamepad *g = n && pads[shown].gamepad ? SDL_GetGamepadFromID(pads[shown].id) : NULL;
    dxl_pad_kind kind = n ? kind_of(&pads[shown]) : DXL_PAD_KIND_XBOX;

    float lw = area.w * 0.56f;
    dxl_rect left = { area.x, area.y, lw, area.h }, right = { area.x + lw + 12, area.y, area.w - lw - 12, area.h };

    /* The rows: the layout, the test, finding a button, the buttons, the
     * tuning, the wizard. */
    const dxl_setting *rows[16];
    int tuning = dxl_settings_rows("Controller", rows, 16);
    int buttons = (int)dxl_pad_button_count(), first = 3, count = first + buttons + tuning + 1;
    dxl_well(ui, right);
    dxl_list l;
    dxl_list_begin(ui, &l, "pad rows", dxl_inset(right, 4), count, ROW_H, DXL_LIST_WHOLE_ROWS);
    /* A control found on the model or by pressing it: its row. */
    if (s->pad_find >= 0) {
        for (int b = 0; b < buttons; b++)
            if (part_of(dxl_pad_button_at((size_t)b)) == s->pad_find) dxl_list_focus(ui, &l, first + b);
        s->pad_find = -1;
    }
    int chosen = -1;
    for (int i = 0; i < count; i++) {
        dxl_rect rr;
        if (!dxl_list_rect(&l, i, &rr)) continue;
        uint32_t id = dxl_list_row_id(&l, i);
        if (i == 0) {
            static const char *names[8];
            int presets = (int)dxl_pad_preset_count(), index = presets;
            for (int p = 0; p < presets && p < 7; p++) names[p] = dxl_pad_preset_at((size_t)p)->label;
            names[presets] = "Custom";
            const dxl_pad_preset *now = s->stores.user ? dxl_bindings_detect(s->stores.user) : NULL;
            for (int p = 0; p < presets; p++)
                if (dxl_pad_preset_at((size_t)p) == now) index = p;
            int was = index;
            int r2 = dxl_spinner(ui, id, rr, "Layout", &index, names, presets + 1, 0);
            if (r2 == DXL_SPIN_CHANGED && index < presets && index != was && s->stores.user) {
                dxl_bindings_apply(s->stores.user, dxl_pad_preset_at((size_t)index));
                dxl_log("pad: layout %s", dxl_pad_preset_at((size_t)index)->id);
            }
            if (ui->focus == id)
                *why = now ? now->description : "A layout of the controller's own: changed button by button.";
        } else if (i == 1) {
            /* Never disabled: a row in a list's middle that takes no focus
             * would break the rows' up and down. */
            if (dxl_button_id(ui, id, dxl_inset(rr, 4), "Test the controller...", 0)) {
                if (g) dxl_padtest_open(s, pads[shown].id);
                else if (n) dxl_shell_toast(s, "SDL knows no mapping for %s: map it first, at the end of the rows.",
                                            pads[shown].name);
                else dxl_shell_toast(s, "No controller is connected.");
            }
            if (ui->focus == id)
                *why = "Every button, stick and trigger on the model, and nothing else in the launcher moving with "
                       "them; the triggers rumble it.";
        } else if (i == 2) {
            /* The next button pressed, or trigger pulled, on a controller:
             * its row. */
            int capturing = ui->capture_id == id;
            if (capturing && ui->in.captured == DXL_CAPTURED_PAD) {
                int b = ui->in.captured_code;
                int part = b >= 0 && b < SDL_GAMEPAD_BUTTON_MISC1 ? (int)DXL_PADPART_OF_BUTTON(b) : -1;
                if (part >= 0 && key_of(part)) s->pad_find = part;
                else dxl_shell_toast(s, "The game does not read that button.");
                dxl_ui_capture_end(ui);
            } else if (capturing && ui->in.captured == DXL_CAPTURED_TRIGGER) {
                s->pad_find = ui->in.captured_code ? DXL_PADPART_RTRIGGER : DXL_PADPART_LTRIGGER;
                dxl_ui_capture_end(ui);
            } else if (capturing && (ui->in.captured || ui->in.back)) {
                dxl_ui_capture_end(ui);
                ui->in.back = 0;
            }
            int hits = dxl_ui_focusable(ui, id, rr, 0);
            dxl_ui_name(ui, id, "Find a button");
            if (hits & DXL_HIT_FOCUSED) dxl_fill(ui, rr, ui->theme.row_focus);
            dxl_label(ui, (dxl_rect){ rr.x + 16, rr.y, rr.w - 32, rr.h }, DXL_FONT_BODY,
                      ui->capture_id == id ? "Press a button on the controller..." : "Find a button: press it",
                      ui->capture_id == id ? ui->theme.accent : (hits & DXL_HIT_FOCUSED) ? ui->theme.text_focus : ui->theme.text,
                      DXL_W_LEFT | DXL_W_MAY_CUT);
            if ((hits & DXL_HIT_ACTIVATED) && !capturing) dxl_ui_capture_begin(ui, id);
            if (ui->focus == id)
                *why = "The next button pressed, or trigger pulled, on a controller is found in the rows below; "
                       "Esc stops.";
        } else if (i < first + buttons) {
            const char *key = dxl_pad_button_at((size_t)(i - first));
            button_row(s, &l, i, key, kind, rr);
            if (ui->focus == id) {
                chosen = part_of(key);
                *why = "Enter, a click or the pad's confirm chooses what the button does.";
            }
        } else if (i < first + buttons + tuning) {
            dxl_settings_row(s, rows[i - first - buttons], id, rr);
            if (ui->focus == id) *focused = rows[i - first - buttons];
        } else {
            int wizard = n && (!pads[shown].gamepad || g);
            if (dxl_button_id(ui, id, dxl_inset(rr, 4), "Map this controller...",
                              n && !pads[shown].gamepad ? DXL_W_PRIMARY : (wizard ? 0 : DXL_W_DISABLED)))
                dxl_padwizard_open(s, pads[shown].id);
            if (ui->focus == id)
                *why = "Press each of the controller's buttons and move each stick in turn, as asked: SDL, "
                       "the launcher and the game then know it.";
        }
    }
    dxl_list_end(ui, &l);

    /* A short rumble as a control is chosen, on the controller shown. */
    if (chosen != s->pad_chosen) {
        s->pad_chosen = chosen;
        if (g && chosen >= 0) SDL_RumbleGamepad(g, 0x2400, 0x4800, 90);
    }
    left_panel(s, left, pads, n, shown, game, kind, chosen, 0);
}

/* ---- the test --------------------------------------------------------- */

#define HOLD_S 1.0          /* the back button held this long ends the test */

/* The pad's back button: east, but south on a Nintendo pad, whose A is
 * east; and its name. */
static SDL_GamepadButton back_button(SDL_Gamepad *g) {
    return dxl_ui_pad_family(g) == DXL_PADFAMILY_NINTENDO ? SDL_GAMEPAD_BUTTON_SOUTH : SDL_GAMEPAD_BUTTON_EAST;
}

static const char *back_name(SDL_Gamepad *g, dxl_pad_kind kind) {
    return dxl_pad_control_label(kind, back_button(g) == SDL_GAMEPAD_BUTTON_SOUTH ? "Joy1" : "Joy2");
}

static void test_close(dxl_shell *s) {
    SDL_Gamepad *g = SDL_GetGamepadFromID(s->pad_test.id);
    if (g && s->pad_test.rumbling) SDL_RumbleGamepad(g, 0, 0, 0);
    s->pad_test.rumbling = 0;
    s->ui.pads_off = 0;
    dxl_log("pad test: done");
    dxl_shell_close_dialog(s);
}

void dxl_padtest_open(dxl_shell *s, SDL_JoystickID id) {
    s->pad_test.id = id;
    s->pad_test.held_since = -1;
    s->pad_test.rumbling = 0;
    if (s->dialog == DXL_DIALOG_NONE) s->dialog_return = s->ui.focus;
    s->dialog = DXL_DIALOG_PADTEST;
    /* The pads are the model's now, not the window's. */
    s->ui.pads_off = 1;
    dxl_log("pad test: %s", SDL_GetGamepadNameForID(id));
}

void dxl_padtest(dxl_shell *s, dxl_rect over, int back) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    SDL_Gamepad *g = SDL_GetGamepadFromID(s->pad_test.id);
    if (!g) {
        test_close(s);
        dxl_shell_toast(s, "The controller went as it was tried.");
        return;
    }
    /* Its back held: the hold's share of a second; the whole, the end. */
    float hold = 0;
    if (SDL_GetGamepadButton(g, back_button(g))) {
        if (s->pad_test.held_since < 0) s->pad_test.held_since = ui->now;
        hold = (float)((ui->now - s->pad_test.held_since) / HOLD_S);
    } else {
        s->pad_test.held_since = -1;
    }
    if (hold >= 1) {
        test_close(s);
        return;
    }
    /* The triggers rumble it, each its own motor, as far as they are
     * pulled. */
    float lt = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32767.0f;
    float rt = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0f;
    if (lt > 0.05f || rt > 0.05f) {
        SDL_RumbleGamepad(g, (Uint16)(fminf(lt, 1) * 0xffff), (Uint16)(fminf(rt, 1) * 0xffff), 250);
        s->pad_test.rumbling = 1;
    } else if (s->pad_test.rumbling) {
        SDL_RumbleGamepad(g, 0, 0, 0);
        s->pad_test.rumbling = 0;
    }
    dxl_ui_animate(ui);

    pad_entry pads[16];
    int n = pads_now(pads, 16), shown = 0, game = -1;
    for (int i = 0; i < n; i++) {
        if (pads[i].id == s->pad_test.id) shown = i;
        if (game < 0 && pads[i].gamepad) game = i;
    }
    dxl_pad_kind kind = kind_of(&pads[shown]);
    dxl_rect in = dxl_dialog_begin(ui, over, 1000, 680, "Testing the controller");
    char line[300];
    snprintf(line, sizeof line,
             "Every button, stick and trigger shows on the model, and moves nothing else; the triggers rumble it. "
             "Hold %s a second, or press Esc, to stop.",
             back_name(g, kind));
    float h = dxl_paragraph(ui, in, DXL_FONT_SMALL, line, t->help);
    dxl_cut_top(&in, h + 6);
    /* The hold, as it fills. */
    dxl_rect bar = dxl_cut_top(&in, 4);
    if (hold > 0) {
        dxl_fill(ui, bar, dxl_rgba_fade(t->panel_light, 0.2f));
        dxl_fill(ui, (dxl_rect){ bar.x, bar.y, bar.w * hold, bar.h }, t->accent);
    }
    dxl_cut_top(&in, 8);
    dxl_rect buttons = dxl_cut_bottom(&in, 46);
    dxl_cut_bottom(&in, 10);
    left_panel(s, in, pads, n, shown, game, kind, -1, 1);
    int stop = dxl_button(ui, "test stop", dxl_cut_right(&buttons, 170), "Stop testing", DXL_W_PRIMARY);
    dxl_dialog_end(ui);
    if (stop || back) test_close(s);
}

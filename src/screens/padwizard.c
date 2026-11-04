/* The mapping wizard: a controller SDL has no mapping for -- or one it maps
 * wrongly -- learnt control by control, each of SDL's asked for in turn and
 * pressed or moved, or skipped where the controller has none of it; then
 * its mapping (core/padmap.h) given to SDL at once, so the launcher and its
 * pages know the controller, and kept in the launcher's gamecontrollerdb.txt
 * for its next start and the engine (app/run.h). A dialog over Settings;
 * the controller's own input drives nothing of the window while it is up. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/log.h"
#include "core/padmap.h"
#include "ui/draw.h"
#include "ui/pad3d.h"
#include "ui/widgets.h"

#include "core/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* An axis pushed past this from where it rests, of 32767, is taken. */
#define TAKEN 16000
/* And back within this, the controller is at rest again. */
#define REST 8000
/* How long an axis SDL has not read yet is waited for. */
#define FIRST_READ 0.5
/* Still this long and not where SDL first read it, before the first step:
 * where it rests. */
#define SETTLE 1.0
/* A face button the controller has given, held this long: its own Skip,
 * Back or Cancel (the test's back button is held as long,
 * screens/controller.c). */
#define HOLD_S 1.0

static const struct {
    int ctl, part, axis;    /* axis: a stick's step, which wants an axis */
    const char *ask;
} steps[] = {
    { DXL_PADCTL_A, DXL_PADPART_SOUTH, 0, "Press the bottom face button" },
    { DXL_PADCTL_B, DXL_PADPART_EAST, 0, "Press the right face button" },
    { DXL_PADCTL_X, DXL_PADPART_WEST, 0, "Press the left face button" },
    { DXL_PADCTL_Y, DXL_PADPART_NORTH, 0, "Press the top face button" },
    { DXL_PADCTL_LEFTSHOULDER, DXL_PADPART_LSHOULDER, 0, "Press the left shoulder button" },
    { DXL_PADCTL_RIGHTSHOULDER, DXL_PADPART_RSHOULDER, 0, "Press the right shoulder button" },
    { DXL_PADCTL_LEFTTRIGGER, DXL_PADPART_LTRIGGER, 0, "Pull the left trigger all the way" },
    { DXL_PADCTL_RIGHTTRIGGER, DXL_PADPART_RTRIGGER, 0, "Pull the right trigger all the way" },
    { DXL_PADCTL_BACK, DXL_PADPART_BACK, 0, "Press the left middle button (View, Create, Minus, Select)" },
    { DXL_PADCTL_START, DXL_PADPART_START, 0, "Press the right middle button (Menu, Options, Plus, Start)" },
    { DXL_PADCTL_GUIDE, DXL_PADPART_GUIDE, 0, "Press the button with the maker's logo" },
    { DXL_PADCTL_LEFTSTICK, DXL_PADPART_LSTICK, 0, "Press the left stick in" },
    { DXL_PADCTL_RIGHTSTICK, DXL_PADPART_RSTICK, 0, "Press the right stick in" },
    { DXL_PADCTL_DPUP, DXL_PADPART_DPUP, 0, "Press the d-pad up" },
    { DXL_PADCTL_DPDOWN, DXL_PADPART_DPDOWN, 0, "Press the d-pad down" },
    { DXL_PADCTL_DPLEFT, DXL_PADPART_DPLEFT, 0, "Press the d-pad left" },
    { DXL_PADCTL_DPRIGHT, DXL_PADPART_DPRIGHT, 0, "Press the d-pad right" },
    { DXL_PADCTL_LEFTX, DXL_PADPART_LSTICK, 1, "Push the left stick right" },
    { DXL_PADCTL_LEFTY, DXL_PADPART_LSTICK, 1, "Push the left stick down" },
    { DXL_PADCTL_RIGHTX, DXL_PADPART_RSTICK, 1, "Push the right stick right" },
    { DXL_PADCTL_RIGHTY, DXL_PADPART_RSTICK, 1, "Push the right stick down" },
};
#define STEPS ((int)(sizeof steps / sizeof *steps))

/* The controller's own Skip, Back and Cancel, for a controller with no keys
 * or mouse beside it (the Smart Pro; bugs.md LUI.6): the face buttons it
 * has given, held -- the bottom one as a pad's confirm presses the dialog's
 * first button, the left as its clear, the right as its back. */
enum { PAD_SKIP, PAD_BACK, PAD_CANCEL, PAD_COMMANDS };
static const int command_ctl[PAD_COMMANDS] = { DXL_PADCTL_A, DXL_PADCTL_X, DXL_PADCTL_B };
static const char *const command_does[PAD_COMMANDS] = {
    "the bottom face button skips", "the left face button goes back", "the right face button cancels",
};

/* Where everything rests, the moment a step begins. */
static void rest(dxl_padwizard_state *w) {
    for (int a = 0; a < w->naxes && a < 16; a++) w->axis_rest[a] = SDL_GetJoystickAxis(w->joy, a);
    for (int h = 0; h < w->nhats && h < 4; h++) w->hat_rest[h] = SDL_GetJoystickHat(w->joy, h);
}

/* Where it rests before the first step: each axis as SDL first read it,
 * the controller as it was before anyone took hold of it -- a stick held
 * as the wizard opened is not its rest -- and the hats centred. 0 while an
 * axis is still to be read, for a while: one never moved is read as it is. */
static int learn_rest(dxl_padwizard_state *w, double now) {
    int ready = 1;
    for (int a = 0; a < w->naxes && a < 16; a++) {
        Sint16 v;
        if (SDL_GetJoystickAxisInitialState(w->joy, a, &v)) w->axis_rest[a] = v;
        else if (now - w->opened < FIRST_READ) ready = 0;
        else w->axis_rest[a] = SDL_GetJoystickAxis(w->joy, a);
    }
    for (int h = 0; h < w->nhats && h < 4; h++) w->hat_rest[h] = SDL_HAT_CENTERED;
    return ready;
}

/* Before the first step, a first reading that was not its rest -- a
 * trigger read before it was at rest, an axis that reads as it likes --
 * given up once the controller has been still a while: where it is now.
 * Still is no button down, the hats as they were, no axis moved much. */
static void settle(dxl_padwizard_state *w, double now) {
    int moved = 0;
    for (int b = 0; b < w->nbuttons && !moved; b++) moved = SDL_GetJoystickButton(w->joy, b);
    for (int h = 0; h < w->nhats && h < 4; h++) {
        Uint8 v = SDL_GetJoystickHat(w->joy, h);
        if (v != w->still_hat[h]) moved = 1;
        w->still_hat[h] = v;
    }
    for (int a = 0; a < w->naxes && a < 16; a++) {
        int v = SDL_GetJoystickAxis(w->joy, a);
        if (v - w->still_axis[a] > REST / 4 || v - w->still_axis[a] < -REST / 4) {
            moved = 1;
            w->still_axis[a] = (Sint16)v;
        }
    }
    if (moved) w->still_since = now;
    else if (now - w->still_since > SETTLE) rest(w);
}

static int at_rest(const dxl_padwizard_state *w) {
    for (int b = 0; b < w->nbuttons; b++)
        if (SDL_GetJoystickButton(w->joy, b)) return 0;
    for (int h = 0; h < w->nhats; h++)
        if (SDL_GetJoystickHat(w->joy, h) != w->hat_rest[h]) return 0;
    for (int a = 0; a < w->naxes; a++) {
        int d = SDL_GetJoystickAxis(w->joy, a) - w->axis_rest[a];
        if (d > REST || d < -REST) return 0;
    }
    return 1;
}

static int button_taken(const dxl_padwizard_state *w, int b) {
    for (int c = 0; c < DXL_PADCTL_COUNT; c++)
        if (w->map.src[c].kind == DXL_PADSRC_BUTTON && w->map.src[c].index == b) return 1;
    return 0;
}

static int hat_taken(const dxl_padwizard_state *w, int h, int mask) {
    for (int c = 0; c < DXL_PADCTL_COUNT; c++) {
        const dxl_padsrc *s = &w->map.src[c];
        if (s->kind == DXL_PADSRC_HAT && s->index == h && s->hat_mask == mask) return 1;
    }
    return 0;
}

static int axis_taken(const dxl_padwizard_state *w, int a, int half) {
    for (int c = 0; c < DXL_PADCTL_COUNT; c++) {
        const dxl_padsrc *s = &w->map.src[c];
        if (s->kind == DXL_PADSRC_AXIS && s->index == a && (s->half == 0 || half == 0 || s->half == half)) return 1;
    }
    return 0;
}

/* What the controller does now for the step asked: 1 when it took it. */
static int take(dxl_padwizard_state *w) {
    dxl_padsrc *src = &w->map.src[steps[w->step].ctl];
    if (!steps[w->step].axis) {
        for (int b = 0; b < w->nbuttons; b++)
            if (SDL_GetJoystickButton(w->joy, b) && !button_taken(w, b)) {
                *src = (dxl_padsrc){ DXL_PADSRC_BUTTON, b, 0, 0, 0 };
                return 1;
            }
        for (int h = 0; h < w->nhats; h++) {
            Uint8 v = SDL_GetJoystickHat(w->joy, h);
            if (v != w->hat_rest[h] && (v == 1 || v == 2 || v == 4 || v == 8) && !hat_taken(w, h, v)) {
                *src = (dxl_padsrc){ DXL_PADSRC_HAT, h, v, 0, 0 };
                return 1;
            }
        }
    }
    for (int a = 0; a < w->naxes; a++) {
        int d = SDL_GetJoystickAxis(w->joy, a) - w->axis_rest[a];
        if (d < TAKEN && d > -TAKEN) continue;
        if (steps[w->step].axis) {
            /* A stick's axis whole, turned so right and down are positive. */
            if (axis_taken(w, a, 0)) continue;
            *src = (dxl_padsrc){ DXL_PADSRC_AXIS, a, 0, 0, d < 0 };
            return 1;
        }
        /* A trigger resting at either end is its whole axis, turned when it
         * rests at the top; else, and a d-pad on an axis, the half it moved
         * into. */
        int trigger = steps[w->step].ctl == DXL_PADCTL_LEFTTRIGGER || steps[w->step].ctl == DXL_PADCTL_RIGHTTRIGGER;
        int whole = trigger && (w->axis_rest[a] < -TAKEN || w->axis_rest[a] > TAKEN);
        int half = whole ? 0 : d > 0 ? 1 : -1;
        if (axis_taken(w, a, half)) continue;
        *src = (dxl_padsrc){ DXL_PADSRC_AXIS, a, 0, half, whole && w->axis_rest[a] > 0 };
        return 1;
    }
    return 0;
}

/* Whether the source a control was given is held now. */
static int held(const dxl_padwizard_state *w, int ctl) {
    const dxl_padsrc *s = &w->map.src[ctl];
    if (s->kind == DXL_PADSRC_BUTTON) return SDL_GetJoystickButton(w->joy, s->index);
    if (s->kind == DXL_PADSRC_HAT) return (SDL_GetJoystickHat(w->joy, s->index) & s->hat_mask) != 0;
    if (s->kind != DXL_PADSRC_AXIS || s->index < 0 || s->index >= 16) return 0;
    int d = SDL_GetJoystickAxis(w->joy, s->index) - w->axis_rest[s->index];
    return s->half > 0 ? d >= TAKEN : s->half < 0 ? d <= -TAKEN : (d >= TAKEN || d <= -TAKEN);
}

/* The line under the step: let go first; then Skip's use, and the
 * controller's own commands by the face buttons it has given. */
static void help_line(const dxl_padwizard_state *w, char *out, size_t size) {
    if (!w->armed) {
        snprintf(out, size, "Let go of it...");
        return;
    }
    size_t n = (size_t)snprintf(out, size, "Skip what the controller has none of.");
    int any = 0;
    for (int c = 0; c < PAD_COMMANDS && n < size; c++) {
        if (w->map.src[command_ctl[c]].kind == DXL_PADSRC_NONE || (c == PAD_BACK && !w->step)) continue;
        n += (size_t)snprintf(out + n, size - n, "%s%s", any++ ? ", " : " Held a second, ", command_does[c]);
    }
    if (any && n < size) snprintf(out + n, size - n, ".");
}

static void close_wizard(dxl_shell *s) {
    dxl_padwizard_state *w = &s->wizard;
    if (w->joy) SDL_CloseJoystick(w->joy);
    w->joy = NULL;
    dxl_padmesh_free(&w->mesh);
    w->open = 0;
    s->ui.pads_off = 0;
    dxl_shell_close_dialog(s);
}

int dxl_pad_mappings(const dxl_run *r, int start) {
    char *db = dxl_run_pad_db(r);
    /* Over the environment's, which SDL takes over any added. */
    char *theirs = dxl_padmap_without_ours(getenv("SDL_GAMECONTROLLERCONFIG"), db);
    if (theirs) SDL_SetHintWithPriority(SDL_HINT_GAMECONTROLLERCONFIG, theirs, SDL_HINT_OVERRIDE);
    free(theirs);
    int ok = start ? SDL_InitSubSystem(SDL_INIT_GAMEPAD) : SDL_ReloadGamepadMappings();
    if (ok && dxl_path_exists(db) && SDL_AddGamepadMappingsFromFile(db) < 0)
        dxl_log("warning: the pad mappings in %s: %s", db, SDL_GetError());
    free(db);
    return ok;
}

/* Whether SDL maps the controller as the wizard did. */
static int sdl_has(const dxl_padwizard_state *w) {
    char *now = SDL_GetGamepadMappingForID(w->id);
    dxl_padmap m;
    int same = now && dxl_padmap_parse(now, &m) == 0 && dxl_padmap_same(&m, &w->map);
    SDL_free(now);
    return same;
}

/* The mapping made: SDL's at once, kept for the next start and the game. */
static void finish(dxl_shell *s) {
    dxl_padwizard_state *w = &s->wizard;
    if (!dxl_padmap_controls(&w->map)) {
        dxl_shell_toast(s, "Nothing was mapped: the controller is as it was.");
        close_wizard(s);
        return;
    }
    SDL_GUIDToString(SDL_GetJoystickGUIDForID(w->id), w->map.guid, sizeof w->map.guid);
    const char *name = SDL_GetJoystickNameForID(w->id);
    snprintf(w->map.name, sizeof w->map.name, "%s", name && *name ? name : "A controller");
    char text[2048];
    dxl_padmap_format(&w->map, SDL_GetPlatform(), text, sizeof text);
    dxl_log("pad mapping: %s", text);
    char *db = dxl_run_pad_db(s->run);
    dxl_err e;
    int saved = dxl_padmap_db_put(db, text, &e);
    free(db);
    free(s->pad_db);
    s->pad_db = NULL;
    int added = SDL_AddGamepadMapping(text) >= 0;
    /* A mapping of it the environment gave, Steam's, holds over one added:
     * SDL's made again without that one. */
    if (added && saved == 0 && !sdl_has(w)) {
        dxl_log("pad mapping: the environment's held, SDL's mappings made again");
        dxl_pad_mappings(s->run, 0);
    }
    if (!added) dxl_shell_toast(s, "SDL did not take the mapping: %s", SDL_GetError());
    else if (saved != 0) dxl_shell_toast(s, "The mapping was not kept: %s", dxl_err_msg(&e));
    else if (!sdl_has(w)) dxl_shell_toast(s, "%s is mapped for the game; the launcher keeps another mapping of it.", w->map.name);
    else dxl_shell_toast(s, "%s is mapped: the launcher and the game know it.", w->map.name);
    close_wizard(s);
}

void dxl_padwizard_open(dxl_shell *s, SDL_JoystickID id) {
    dxl_padwizard_state *w = &s->wizard;
    memset(w, 0, sizeof *w);
    w->joy = SDL_OpenJoystick(id);
    if (!w->joy) {
        dxl_shell_message(s, "No controller", "It cannot be opened: %s", SDL_GetError());
        return;
    }
    w->id = id;
    w->nbuttons = SDL_GetNumJoystickButtons(w->joy);
    w->naxes = SDL_GetNumJoystickAxes(w->joy);
    w->nhats = SDL_GetNumJoystickHats(w->joy);
    if (w->naxes > 16) w->naxes = 16;
    if (w->nhats > 4) w->nhats = 4;
    w->open = 1;
    /* Armed once it is let go of: a pad's confirm that opened it is still
     * down. */
    w->opened = s->ui.now;
    dxl_padmesh_build(&w->mesh, DXL_PADFAMILY_GENERIC);
    if (s->dialog == DXL_DIALOG_NONE) s->dialog_return = s->ui.focus;
    s->dialog = DXL_DIALOG_PADWIZARD;
    /* Its own buttons are the ones asked for, not the window's. */
    s->ui.pads_off = 1;
    dxl_log("pad wizard: %s, %d buttons, %d axes, %d hats", SDL_GetJoystickNameForID(id), w->nbuttons, w->naxes,
            w->nhats);
}

void dxl_padwizard(dxl_shell *s, dxl_rect over, int back) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_padwizard_state *w = &s->wizard;
    if (!w->open) return;
    if (!SDL_JoystickConnected(w->joy)) {
        close_wizard(s);
        dxl_shell_toast(s, "The controller went before it was mapped.");
        return;
    }
    /* The controller back at rest, the step asked for armed; taken, the next. */
    if (!w->learnt && (w->learnt = learn_rest(w, ui->now))) w->still_since = ui->now;
    if (!w->learnt) {
        /* SDL still to read it. */
    } else if (!w->armed && !w->primed && !at_rest(w)) {
        settle(w, ui->now);
    } else if (!w->armed && at_rest(w)) {
        w->armed = w->primed = 1;
        rest(w);
    } else if (w->armed && take(w)) {
        char got[16];
        dxl_padsrc_format(&w->map.src[steps[w->step].ctl], got, sizeof got);
        dxl_log("pad wizard: %s:%s", dxl_padctl_names[steps[w->step].ctl], got);
        w->armed = 0;
        w->step++;
    }
    /* The controller's own command: a face button it has given, held a
     * second while the step waits -- armed, so the press that gave it, or
     * opened the wizard, is let go of first. A control given is no step's
     * answer again (take), so its hold is never one. */
    int holding = 0;
    for (int c = 0; c < PAD_COMMANDS && w->armed && !holding && w->step < STEPS; c++)
        if (held(w, command_ctl[c])) holding = c + 1;
    if (holding != w->holding) w->held_since = ui->now;
    w->holding = holding;
    float hold = holding ? (float)((ui->now - w->held_since) / HOLD_S) : 0;
    int pad = hold >= 1 ? holding - 1 : -1;
    dxl_ui_animate(ui);
    if (w->step >= STEPS) {
        finish(s);
        return;
    }

    dxl_rect in = dxl_dialog_begin(ui, over, 640, 440, "Map the controller");
    char line[200];
    snprintf(line, sizeof line, "%d of %d: %s.", w->step + 1, STEPS, steps[w->step].ask);
    dxl_label(ui, dxl_cut_top(&in, 30), DXL_FONT_LABEL, line, t->text_focus, DXL_W_LEFT | DXL_W_MAY_CUT);
    char help[240];
    help_line(w, help, sizeof help);
    float h = dxl_paragraph(ui, in, DXL_FONT_SMALL, help, t->help);
    dxl_cut_top(&in, h + 4);
    /* The hold, as it fills. */
    dxl_rect bar = dxl_cut_top(&in, 4);
    if (hold > 0) {
        dxl_fill(ui, bar, dxl_rgba_fade(t->panel_light, 0.2f));
        dxl_fill(ui, (dxl_rect){ bar.x, bar.y, bar.w * (hold < 1 ? hold : 1), bar.h }, t->accent);
    }
    dxl_rect buttons = dxl_cut_bottom(&in, 46);
    dxl_cut_bottom(&in, 10);
    dxl_pad3d_state st;
    memset(&st, 0, sizeof st);
    st.pitch = 0.38f;
    st.chosen = steps[w->step].part;
    dxl_pad3d_spot spots[DXL_PADPART_COUNT];
    dxl_pad3d_draw(ui, &w->mesh, in, &st, spots);

    int skip = dxl_button(ui, "wizard skip", dxl_cut_left(&buttons, 150), "Skip", DXL_W_PRIMARY);
    dxl_cut_left(&buttons, 12);
    int prev = dxl_button(ui, "wizard back", dxl_cut_left(&buttons, 150), "Back", w->step ? 0 : DXL_W_DISABLED);
    int cancel = dxl_button(ui, "wizard cancel", dxl_cut_right(&buttons, 150), "Cancel", 0);
    dxl_dialog_end(ui);
    if (pad >= 0) {
        dxl_log("pad wizard: the controller's %s", pad == PAD_SKIP ? "Skip" : pad == PAD_BACK ? "Back" : "Cancel");
        w->holding = 0;
    }
    if (skip || pad == PAD_SKIP) {
        w->map.src[steps[w->step].ctl].kind = DXL_PADSRC_NONE;
        w->step++;
        w->armed = 0;
        if (w->step >= STEPS) finish(s);
    } else if ((prev || pad == PAD_BACK) && w->step) {
        w->step--;
        w->map.src[steps[w->step].ctl].kind = DXL_PADSRC_NONE;
        w->armed = 0;
    } else if (cancel || back || pad == PAD_CANCEL) {
        dxl_log("pad wizard: put back");
        close_wizard(s);
    }
}

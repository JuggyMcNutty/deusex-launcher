#include "ui/kit.h"
#include "ui/draw.h"
#include "ui/text.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* The brackets' tween to a new focus. */
#define BRACKET_S 0.15
/* A pad's held direction: the first repeat after 350 ms, then every 90. */
#define REPEAT_FIRST 0.35
#define REPEAT_NEXT  0.09
/* A stick's push counts past this, and lets go under the other. */
#define STICK_ON  0.55f
#define STICK_OFF 0.35f

static void text_input_sync(dxl_ui *ui);

/* ---- the pads ---------------------------------------------------------- */

#define PADS 8
static SDL_Gamepad *pads[PADS];

static void pad_open(SDL_JoystickID id) {
    for (int i = 0; i < PADS; i++) if (pads[i] && SDL_GetGamepadID(pads[i]) == id) return;
    for (int i = 0; i < PADS; i++) {
        if (!pads[i]) { pads[i] = SDL_OpenGamepad(id); return; }
    }
}

static void pad_close(SDL_JoystickID id) {
    for (int i = 0; i < PADS; i++) {
        if (pads[i] && SDL_GetGamepadID(pads[i]) == id) {
            SDL_CloseGamepad(pads[i]);
            pads[i] = NULL;
        }
    }
}

/* The kit's own, or one opened elsewhere (dxl-shots' virtual pads). */
static SDL_Gamepad *pad_of(SDL_JoystickID id) {
    return SDL_GetGamepadFromID(id);
}

/* Whose labels the pad's face buttons carry: where confirm and back are. */
dxl_pad_family dxl_ui_pad_family(SDL_Gamepad *g) {
    if (!g) return DXL_PADFAMILY_GENERIC;
    switch (SDL_GetGamepadButtonLabel(g, SDL_GAMEPAD_BUTTON_SOUTH)) {
    case SDL_GAMEPAD_BUTTON_LABEL_A: return DXL_PADFAMILY_XBOX;
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return DXL_PADFAMILY_PLAYSTATION;
    case SDL_GAMEPAD_BUTTON_LABEL_B: return DXL_PADFAMILY_NINTENDO;
    default: return DXL_PADFAMILY_GENERIC;
    }
}

/* The held direction a pad gives, for its repeats: the d-pad's or the
 * left stick's, the last pressed. Its first repeat counts from the frame
 * that takes the press: the events come before the frame sets the clock,
 * which until then is the last frame's -- seconds old once the window has
 * waited a while. */
static int    held_dir = -1;
static int    held_by_stick;  /* the stick holds it, not the d-pad */
static int    held_new;       /* pressed since the last frame began */
static double held_next;
static int    stick_dir = -1;

static void press_dir(dxl_ui *ui, int dir, int stick) {
    ui->in.nav[dir]++;
    held_dir = dir;
    held_by_stick = stick;
    held_new = 1;
}

/* The direction of a d-pad button; -1 for any other. */
static int dpad_dir(int button) {
    switch (button) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return DXL_DIR_UP;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return DXL_DIR_DOWN;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return DXL_DIR_LEFT;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return DXL_DIR_RIGHT;
    default: return -1;
    }
}

/* ---- setup ------------------------------------------------------------- */

static void measure(dxl_ui *ui) {
    int w = 0, h = 0;
    float scale = ui->user_scale;
    if (ui->window) {
        SDL_GetWindowSizeInPixels(ui->window, &w, &h);
        float ds = SDL_GetWindowDisplayScale(ui->window);
        scale = (ds > 0 ? ds : 1) * ui->user_scale;
    } else if (ui->renderer) {
        SDL_GetCurrentRenderOutputSize(ui->renderer, &w, &h);
    }
    if (scale < 0.5f) scale = 0.5f;
    ui->scale = scale;
    ui->w_px = w;
    ui->h_px = h;
    ui->w = w / scale;
    ui->h = h / scale;
    if (fabsf(ui->scale - ui->font_scale) > 0.001f) dxl_text_open(ui);
}

int dxl_ui_init(dxl_ui *ui, SDL_Window *window, SDL_Renderer *renderer, const char *fonts_dir,
                float user_scale) {
    memset(ui, 0, sizeof *ui);
    snprintf(ui->fonts_dir, sizeof ui->fonts_dir, "%s", fonts_dir);
    ui->user_scale = user_scale > 0 ? user_scale : 1;
    dxl_theme_default(&ui->theme);
    ui->mode = DXL_INPUT_MOUSE;
    return dxl_ui_attach(ui, window, renderer);
}

int dxl_ui_attach(dxl_ui *ui, SDL_Window *window, SDL_Renderer *renderer) {
    ui->window = window;
    ui->renderer = renderer;
    ui->engine = TTF_CreateRendererTextEngine(renderer);
    if (!ui->engine) return -1;
    ui->font_scale = 0;
    measure(ui);
    if (!ui->fonts[0] && dxl_text_open(ui) != 0) return -1;
    return ui->fonts[0] ? 0 : -1;
}

void dxl_ui_detach(dxl_ui *ui) {
    if (ui->text_on && ui->window) SDL_StopTextInput(ui->window);
    ui->text_on = 0;
    ui->text_id = ui->capture_id = 0;
    ui->text_ready = ui->text_ready_now = ui->text_lost = 0;
    if (ui->backdrop) SDL_DestroyTexture(ui->backdrop);
    ui->backdrop = NULL;
    dxl_text_flush(ui);
    if (ui->engine) TTF_DestroyRendererTextEngine(ui->engine);
    ui->engine = NULL;
    ui->renderer = NULL;
    ui->window = NULL;
    for (int i = 0; i < PADS; i++) {
        if (pads[i]) SDL_CloseGamepad(pads[i]);
        pads[i] = NULL;
    }
    held_dir = stick_dir = -1;
}

void dxl_ui_reset(dxl_ui *ui) {
    ui->focus = ui->focus_was = ui->hot = ui->held = 0;
    dxl_focus_clear(&ui->prev);
    dxl_focus_clear(&ui->cur);
    ui->br_valid = 0;
    ui->now = ui->dt = 0;
    ui->frame = 0;
    ui->forget = 0;
    ui->mode = DXL_INPUT_MOUSE;
    ui->pad = 0;
    ui->pad_family = DXL_PADFAMILY_XBOX;
    memset(ui->values, 0, sizeof ui->values);
    memset(&ui->in, 0, sizeof ui->in);
    ui->text_id = ui->capture_id = 0;
    ui->text_ready = ui->text_ready_now = ui->text_lost = 0;
    ui->swallow_space = 0;
    ui->wake = 0;
    held_dir = stick_dir = -1;
}

void dxl_ui_free(dxl_ui *ui) {
    dxl_ui_detach(ui);
    dxl_text_close(ui);
}

/* ---- input ------------------------------------------------------------- */

void dxl_ui_input_begin(dxl_ui *ui) {
    float px = ui->in.px, py = ui->in.py;
    memset(&ui->in, 0, sizeof ui->in);
    text_input_sync(ui);
    ui->in.px = px;
    ui->in.py = py;
}

/* The bytes of the UTF-8 character at p; 0 when it is none whole. */
static size_t utf8_len(const unsigned char *p) {
    size_t len = *p < 0x80 ? 1 : (*p & 0xe0) == 0xc0 ? 2 : (*p & 0xf0) == 0xe0 ? 3 : (*p & 0xf8) == 0xf0 ? 4 : 0;
    for (size_t k = 1; k < len; k++)
        if ((p[k] & 0xc0) != 0x80) return 0;
    return len;
}

/* Text into the frame's typing: what fits, cut at a character's edge. */
static void typed(dxl_ui *ui, const char *s) {
    size_t have = strlen(ui->in.text), add = strlen(s), room = sizeof ui->in.text - 1 - have;
    if (add > room) {
        add = room;
        while (add > 0 && ((unsigned char)s[add] & 0xc0) == 0x80) add--;
    }
    memcpy(ui->in.text + have, s, add);
    ui->in.text[have + add] = '\0';
}

static void typed_op(dxl_ui *ui, char op, int select) {
    char ops[3] = { select ? DXL_EDIT_SELECT : op, select ? op : '\0', '\0' };
    typed(ui, ops);
}

/* The clipboard's text as typed: one line, its line ends and other
 * control characters spaces, the last line end dropped. */
static void paste(dxl_ui *ui) {
    char *clip = SDL_GetClipboardText();
    if (clip) {
        size_t n = strlen(clip);
        while (n && (clip[n - 1] == '\n' || clip[n - 1] == '\r')) clip[--n] = '\0';
        for (char *p = clip; *p; p++)
            if (DXL_EDIT_IS_OP(*p)) *p = ' ';
        typed(ui, clip);
    }
    SDL_free(clip);
}

/* A key while a field is edited: the editing keys; the rest is text, which
 * comes as SDL_EVENT_TEXT_INPUT. Enter keeps the edit, Esc puts it back;
 * Tab, Up and Down keep it and move on. */
static void edit_key(dxl_ui *ui, const SDL_KeyboardEvent *k) {
    int ctrl = (k->mod & SDL_KMOD_CTRL) != 0, shift = (k->mod & SDL_KMOD_SHIFT) != 0;
    switch (k->key) {
    case SDLK_BACKSPACE: typed_op(ui, ctrl ? DXL_EDIT_ERASE_WORD : DXL_EDIT_ERASE, 0); break;
    case SDLK_DELETE:
        if (shift) typed_op(ui, DXL_EDIT_CUT, 0);
        else typed_op(ui, ctrl ? DXL_EDIT_DELETE_WORD : DXL_EDIT_DELETE, 0);
        break;
    case SDLK_LEFT:      typed_op(ui, ctrl ? DXL_EDIT_WORD_LEFT : DXL_EDIT_LEFT, shift); break;
    case SDLK_RIGHT:     typed_op(ui, ctrl ? DXL_EDIT_WORD_RIGHT : DXL_EDIT_RIGHT, shift); break;
    case SDLK_HOME:      typed_op(ui, DXL_EDIT_HOME, shift); break;
    case SDLK_END:       typed_op(ui, DXL_EDIT_END, shift); break;
    case SDLK_A:         if (!ctrl) return; typed_op(ui, DXL_EDIT_ALL, 0); break;
    case SDLK_C:         if (!ctrl) return; typed_op(ui, DXL_EDIT_COPY, 0); break;
    case SDLK_X:         if (!ctrl) return; typed_op(ui, DXL_EDIT_CUT, 0); break;
    case SDLK_V:         if (!ctrl) return; paste(ui); break;
    case SDLK_INSERT:
        if (shift) paste(ui);
        else if (ctrl) typed_op(ui, DXL_EDIT_COPY, 0);
        else return;
        break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:  if (!k->repeat) ui->in.activate++; break;
    case SDLK_ESCAPE:    if (!k->repeat) ui->in.back++; break;
    case SDLK_TAB:       if (shift) ui->in.untab++; else ui->in.tab++; break;
    case SDLK_UP:        ui->in.nav[DXL_DIR_UP]++; break;
    case SDLK_DOWN:      ui->in.nav[DXL_DIR_DOWN]++; break;
    default: return;
    }
    ui->mode = DXL_INPUT_KEYBOARD;
    ui->in.any = 1;
}

/* A key on a field that takes typing (dxl_ui_text_ready), not edited yet:
 * a letter types -- its SDL_EVENT_TEXT_INPUT follows -- even W, A, S and
 * D, which move the focus elsewhere; Backspace, Delete, Ctrl+A and Ctrl+V
 * begin the edit with what they do; Space chooses the field, its own text
 * not typed. 1 when the field took the key. */
static int ready_key(dxl_ui *ui, const SDL_KeyboardEvent *k) {
    if (!ui->text_ready || ui->text_ready != ui->focus || ui->text_id) return 0;
    int ctrl = (k->mod & SDL_KMOD_CTRL) != 0;
    switch (k->key) {
    case SDLK_A:
        if (ctrl) { typed_op(ui, DXL_EDIT_ALL, 0); break; }
        /* fall through */
    case SDLK_W:
    case SDLK_S:
    case SDLK_D:
        if (ctrl) return 0;
        break;
    case SDLK_BACKSPACE: typed_op(ui, ctrl ? DXL_EDIT_ERASE_WORD : DXL_EDIT_ERASE, 0); break;
    case SDLK_DELETE:    typed_op(ui, ctrl ? DXL_EDIT_DELETE_WORD : DXL_EDIT_DELETE, 0); break;
    case SDLK_V:         if (!ctrl) return 0; paste(ui); break;
    case SDLK_SPACE:     ui->swallow_space = !(k->mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)); return 0;
    default: return 0;
    }
    ui->mode = DXL_INPUT_KEYBOARD;
    ui->in.any = 1;
    return 1;
}

static void key(dxl_ui *ui, const SDL_KeyboardEvent *k) {
    /* Space's own text follows its key at once, or never (Ctrl+Space). */
    ui->swallow_space = 0;
    if (ui->capture_id) {
        if (k->repeat) return;
        if (k->key == SDLK_ESCAPE) ui->in.back++;
        else if (!ui->in.captured) {
            ui->in.captured = DXL_CAPTURED_KEY;
            ui->in.captured_code = (int)k->scancode;
        }
        ui->mode = DXL_INPUT_KEYBOARD;
        ui->in.any = 1;
        return;
    }
    if (ui->text_id) {
        edit_key(ui, k);
        return;
    }
    if (ready_key(ui, k)) return;
    if (k->key == SDLK_TAB && (k->mod & SDL_KMOD_CTRL)) {
        if (k->mod & SDL_KMOD_SHIFT) ui->in.tab_prev++;
        else ui->in.tab_next++;
        ui->mode = DXL_INPUT_KEYBOARD;
        ui->in.any = 1;
        return;
    }
    switch (k->key) {
    case SDLK_UP:       ui->in.nav[DXL_DIR_UP]++; break;
    case SDLK_DOWN:     ui->in.nav[DXL_DIR_DOWN]++; break;
    case SDLK_LEFT:     ui->in.nav[DXL_DIR_LEFT]++; break;
    case SDLK_RIGHT:    ui->in.nav[DXL_DIR_RIGHT]++; break;
    case SDLK_W:        ui->in.nav[DXL_DIR_UP]++; break;
    case SDLK_S:        ui->in.nav[DXL_DIR_DOWN]++; break;
    case SDLK_A:        ui->in.nav[DXL_DIR_LEFT]++; break;
    case SDLK_D:        ui->in.nav[DXL_DIR_RIGHT]++; break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
    case SDLK_SPACE:    if (!k->repeat) ui->in.activate++; break;
    case SDLK_ESCAPE:
    case SDLK_BACKSPACE: if (!k->repeat) ui->in.back++; break;
    case SDLK_TAB:      if (k->mod & SDL_KMOD_SHIFT) ui->in.untab++; else ui->in.tab++; break;
    case SDLK_PAGEUP:   ui->in.page_up++; break;
    case SDLK_PAGEDOWN: ui->in.page_down++; break;
    case SDLK_F5:       if (!k->repeat) ui->in.start++; break;
    case SDLK_DELETE:   if (!k->repeat) ui->in.clear++; break;
    case SDLK_F11:      if (!k->repeat) ui->in.toggle_big++; break;
    default: return;
    }
    ui->mode = DXL_INPUT_KEYBOARD;
    ui->in.any = 1;
}

static void pad_button(dxl_ui *ui, const SDL_GamepadButtonEvent *b) {
    SDL_Gamepad *g = pad_of(b->which);
    ui->mode = DXL_INPUT_PAD;
    ui->pad = b->which;
    ui->pad_family = dxl_ui_pad_family(g);
    ui->in.any = 1;
    if (ui->capture_id) {
        if (!ui->in.captured) {
            ui->in.captured = DXL_CAPTURED_PAD;
            ui->in.captured_code = b->button;
        }
        return;
    }
    /* Confirm and back where the pad's own labels put them: a Nintendo pad
     * confirms with its A, on the east. */
    int south_confirms = ui->pad_family != DXL_PADFAMILY_NINTENDO;
    if (dpad_dir(b->button) >= 0) {
        press_dir(ui, dpad_dir(b->button), 0);
        return;
    }
    switch (b->button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: if (south_confirms) ui->in.activate++; else ui->in.back++; break;
    case SDL_GAMEPAD_BUTTON_EAST:  if (south_confirms) ui->in.back++; else ui->in.activate++; break;
    case SDL_GAMEPAD_BUTTON_START: ui->in.start++; break;
    case SDL_GAMEPAD_BUTTON_WEST:  ui->in.clear++; break;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  ui->in.shoulder_l++; break;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: ui->in.shoulder_r++; break;
    default: break;
    }
}

/* A trigger pulled past half, as the game presses it, while a slot captures. */
static void pad_trigger(dxl_ui *ui, const SDL_GamepadAxisEvent *a) {
    if (a->axis != SDL_GAMEPAD_AXIS_LEFT_TRIGGER && a->axis != SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) return;
    if (a->value <= 16383 || ui->in.captured) return;
    ui->mode = DXL_INPUT_PAD;
    ui->pad = a->which;
    ui->pad_family = dxl_ui_pad_family(pad_of(a->which));
    ui->in.any = 1;
    ui->in.captured = DXL_CAPTURED_TRIGGER;
    ui->in.captured_code = a->axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
}

static void pad_axis(dxl_ui *ui, const SDL_GamepadAxisEvent *a) {
    if (a->axis != SDL_GAMEPAD_AXIS_LEFTX && a->axis != SDL_GAMEPAD_AXIS_LEFTY) return;
    SDL_Gamepad *g = pad_of(a->which);
    if (!g) return;
    float x = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
    float y = SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
    float m = fmaxf(fabsf(x), fabsf(y));
    if (stick_dir >= 0 && m < STICK_OFF) {
        if (held_by_stick) held_dir = -1;
        stick_dir = -1;
        return;
    }
    if (stick_dir < 0 && m > STICK_ON) {
        int dir = fabsf(x) > fabsf(y) ? (x < 0 ? DXL_DIR_LEFT : DXL_DIR_RIGHT) : (y < 0 ? DXL_DIR_UP : DXL_DIR_DOWN);
        stick_dir = dir;
        ui->mode = DXL_INPUT_PAD;
        ui->pad = a->which;
        ui->pad_family = dxl_ui_pad_family(g);
        ui->in.any = 1;
        press_dir(ui, dir, 1);
    }
}

void dxl_ui_event(dxl_ui *ui, const SDL_Event *e) {
    switch (e->type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        ui->in.quit = 1;
        ui->in.any = 1;
        break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
    case SDL_EVENT_WINDOW_EXPOSED:
        ui->in.resized = 1;
        ui->in.any = 1;
        break;
    case SDL_EVENT_KEY_DOWN:
        key(ui, &e->key);
        break;
    case SDL_EVENT_MOUSE_MOTION: {
        float density = ui->window ? SDL_GetWindowPixelDensity(ui->window) : 1;
        ui->in.px = e->motion.x * density / ui->scale;
        ui->in.py = e->motion.y * density / ui->scale;
        ui->in.pointer_moved = 1;
        ui->in.any = 1;
        ui->mode = DXL_INPUT_MOUSE;
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        float density = ui->window ? SDL_GetWindowPixelDensity(ui->window) : 1;
        ui->in.px = e->button.x * density / ui->scale;
        ui->in.py = e->button.y * density / ui->scale;
        if (ui->capture_id) {
            if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && !ui->in.captured) {
                ui->in.captured = DXL_CAPTURED_MOUSE;
                ui->in.captured_code = e->button.button;
            }
            ui->in.any = 1;
            break;
        }
        if (e->button.button == SDL_BUTTON_LEFT) {
            if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                ui->in.pressed = 1;
                ui->in.clicks = e->button.clicks > 1 ? e->button.clicks : 1;
                ui->in.shift_click = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
            } else {
                ui->in.released = 1;
            }
        } else if (e->button.button == SDL_BUTTON_X1 && e->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            /* The mouse's back button, as Esc. */
            ui->in.back++;
        }
        ui->in.any = 1;
        ui->mode = DXL_INPUT_MOUSE;
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        /* Nothing scrolls under an edit: its field would go from view. */
        if (ui->text_id) break;
        if (ui->capture_id && !ui->in.captured && e->wheel.y != 0) {
            float y = e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e->wheel.y : e->wheel.y;
            ui->in.captured = DXL_CAPTURED_WHEEL;
            ui->in.captured_code = y > 0 ? 1 : -1;
            ui->in.any = 1;
            break;
        }
        ui->in.wheel += e->wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e->wheel.y : e->wheel.y;
        ui->in.any = 1;
        ui->mode = DXL_INPUT_MOUSE;
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        pad_open(e->gdevice.which);
        ui->in.any = 1;
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        pad_close(e->gdevice.which);
        if (ui->pad == e->gdevice.which) ui->pad = 0;
        ui->in.any = 1;
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        if (!ui->pads_off) pad_button(ui, &e->gbutton);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        /* The d-pad let go of: its own direction's repeat, not another's
         * pressed since. */
        if (!held_by_stick && held_dir >= 0 && dpad_dir(e->gbutton.button) == held_dir) held_dir = -1;
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (ui->pads_off) break;
        if (ui->capture_id) pad_trigger(ui, &e->gaxis);
        else pad_axis(ui, &e->gaxis);
        break;
    case SDL_EVENT_TEXT_INPUT:
        if ((ui->text_id || (ui->text_ready && ui->text_ready == ui->focus)) && e->text.text) {
            int swallow = ui->swallow_space;
            ui->swallow_space = 0;
            if (swallow && !strcmp(e->text.text, " ")) break;
            /* Text, never the editing keys' codes, whole characters alone:
             * as many as fit. */
            char clean[sizeof ui->in.text];
            size_t n = 0;
            for (const unsigned char *p = (const unsigned char *)e->text.text; *p;) {
                size_t len = utf8_len(p);
                if (!len || (len == 1 && DXL_EDIT_IS_OP(*p))) {
                    p++;
                    continue;
                }
                if (n + len >= sizeof clean) break;
                memcpy(clean + n, p, len);
                n += len;
                p += len;
            }
            clean[n] = '\0';
            typed(ui, clean);
            ui->mode = DXL_INPUT_KEYBOARD;
            ui->in.any = 1;
        }
        break;
    default:
        break;
    }
}

/* ---- the frame -------------------------------------------------------- */

static dxl_box box_of(dxl_rect r) {
    dxl_box b = { r.x, r.y, r.w, r.h };
    return b;
}

void dxl_ui_begin(dxl_ui *ui, double now) {
    ui->dt = ui->now > 0 ? (float)(now - ui->now) : 0;
    ui->now = now;
    ui->animating = 0;
    ui->wake = 0;
    ui->text_ready_now = 0;
    ui->cues = ui->in.back ? DXL_CUE_BACK : 0;
    /* A key or a click shows what is typing whole. */
    if (ui->in.activate || ui->in.back || ui->in.pressed || ui->in.start) ui->typed_until = now;
    ui->diag_count = 0;
    ui->focus_moved = 0;
    ui->id_seed = 0;
    ui->scope = 0;
    ui->frame++;
    ui->clip_count = 0;
    measure(ui);

    /* A pad's held direction repeats; none while the pads are another's,
     * whose letting go it would not see. */
    if (ui->pads_off) held_dir = stick_dir = -1;
    if (held_dir >= 0 && held_new) {
        held_next = now + REPEAT_FIRST;
    } else if (held_dir >= 0 && now >= held_next) {
        ui->in.nav[held_dir]++;
        held_next = now + REPEAT_NEXT;
    }
    held_new = 0;
    if (held_dir >= 0) dxl_ui_animate(ui);

    if (ui->forget) {
        dxl_focus_clear(&ui->prev);
        ui->forget = 0;
    } else {
        ui->prev = ui->cur;
    }
    dxl_focus_clear(&ui->cur);
    ui->names.count = 0;
    ui->hot = 0;

    /* The focus where it can be: the screen's first when it has none, or
     * when what had it is gone (or under a dialog now). */
    if (ui->prev.count && !dxl_focus_has(&ui->prev, ui->focus)) ui->focus = dxl_focus_first(&ui->prev);
    ui->focus_was = ui->focus;
    for (int d = 0; d < 4 && ui->prev.count; d++) {
        for (int n = 0; n < ui->in.nav[d]; n++) {
            uint32_t to = dxl_focus_move(&ui->prev, ui->focus, (dxl_dir)d);
            if (to != ui->focus) {
                ui->focus_moved = 1;
                ui->cues |= DXL_CUE_FOCUS;
            }
            ui->focus = to;
        }
    }
    for (int n = 0; n < ui->in.tab && ui->prev.count; n++) {
        ui->focus = dxl_focus_step(&ui->prev, ui->focus, 1);
        ui->focus_moved = 1;
        ui->cues |= DXL_CUE_FOCUS;
    }
    for (int n = 0; n < ui->in.untab && ui->prev.count; n++) {
        ui->focus = dxl_focus_step(&ui->prev, ui->focus, -1);
        ui->focus_moved = 1;
        ui->cues |= DXL_CUE_FOCUS;
    }
}

static float ease(float t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;
    return 1 - (1 - t) * (1 - t) * (1 - t);
}

static dxl_box lerp_box(dxl_box a, dxl_box b, float t) {
    dxl_box r = { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.w + (b.w - a.w) * t, a.h + (b.h - a.h) * t };
    return r;
}

void dxl_ui_end(dxl_ui *ui) {
    if (ui->in.released) ui->held = 0;
    if (ui->focus != ui->focus_seen) {
        ui->focus_seen = ui->focus;
        ui->focus_since = ui->now;
    }

    /* The field that takes typing, for the keys before the next frame. An
     * edit or a capture whose control this frame did not draw -- scrolled
     * from view, its page gone -- ends: the edit kept, as its field says
     * when it is drawn again. */
    ui->text_ready = ui->text_ready_now;
    if (ui->text_id && !dxl_focus_has(&ui->cur, ui->text_id)) {
        ui->text_lost = ui->text_id;
        dxl_ui_text_end(ui);
    }
    if (ui->capture_id && !dxl_focus_has(&ui->cur, ui->capture_id)) dxl_ui_capture_end(ui);

    /* The brackets round the focus, moved to it over 150 ms. */
    const dxl_focusable *f = NULL;
    for (int i = 0; i < ui->cur.count; i++)
        if (ui->cur.items[i].id == ui->focus) f = &ui->cur.items[i];
    if (f) {
        dxl_box to = { f->box.x - 4, f->box.y - 4, f->box.w + 8, f->box.h + 8 };
        if (!ui->br_valid) {
            ui->br_from = ui->br_to = ui->br_now = to;
            ui->br_valid = 1;
        } else if (memcmp(&to, &ui->br_to, sizeof to) != 0) {
            ui->br_from = ui->br_now;
            ui->br_to = to;
            ui->br_start = ui->now;
        }
        float t = ui->reduced_motion ? 1 : (float)((ui->now - ui->br_start) / BRACKET_S);
        ui->br_now = lerp_box(ui->br_from, ui->br_to, ease(t));
        if (t < 1) dxl_ui_animate(ui);
        dxl_rect r = { ui->br_now.x, ui->br_now.y, ui->br_now.w, ui->br_now.h };
        dxl_brackets(ui, r, 10, 2, ui->theme.bracket);
    } else {
        ui->br_valid = 0;
    }

    /* Diagnostics: focusables off the window or over one another. */
    if (ui->cur.overflowed) dxl_ui_diag(ui, "more than %d focusables", DXL_FOCUS_MAX);
    for (int i = 0; i < ui->cur.count; i++) {
        const dxl_focusable *a = &ui->cur.items[i];
        if (a->box.x < -0.5f || a->box.y < -0.5f || a->box.x + a->box.w > ui->w + 0.5f ||
            a->box.y + a->box.h > ui->h + 0.5f)
            dxl_ui_diag(ui, "focusable %08x off the window (%.0f,%.0f %.0fx%.0f in %.0fx%.0f)", a->id,
                        a->box.x, a->box.y, a->box.w, a->box.h, ui->w, ui->h);
        for (int j = i + 1; j < ui->cur.count; j++) {
            const dxl_focusable *b = &ui->cur.items[j];
            if (a->scope != b->scope) continue;
            if (a->box.x < b->box.x + b->box.w - 0.5f && b->box.x < a->box.x + a->box.w - 0.5f &&
                a->box.y < b->box.y + b->box.h - 0.5f && b->box.y < a->box.y + a->box.h - 0.5f)
                dxl_ui_diag(ui, "focusables %08x and %08x overlap", a->id, b->id);
        }
    }
}

uint32_t dxl_ui_id(const dxl_ui *ui, const char *name) {
    uint32_t h = 2166136261u ^ ui->id_seed;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        h ^= *p;
        h *= 16777619u;
    }
    return h ? h : 1;
}

void dxl_ui_seed(dxl_ui *ui, uint32_t seed) { ui->id_seed = seed * 2654435761u; }

void dxl_ui_scope(dxl_ui *ui, int scope) { ui->scope = scope; }

void dxl_ui_focus(dxl_ui *ui, uint32_t id) { ui->focus = id; }

void dxl_ui_forget(dxl_ui *ui) { ui->forget = 1; }

int dxl_ui_take_lr(dxl_ui *ui, uint32_t id) {
    int steps = ui->in.nav[DXL_DIR_RIGHT] - ui->in.nav[DXL_DIR_LEFT];
    if (ui->focus_was != id || (!ui->in.nav[DXL_DIR_RIGHT] && !ui->in.nav[DXL_DIR_LEFT])) return 0;
    ui->focus = id;
    return steps;
}

int dxl_ui_take_ud(dxl_ui *ui, uint32_t id) {
    int steps = ui->in.nav[DXL_DIR_DOWN] - ui->in.nav[DXL_DIR_UP];
    if (ui->focus_was != id || (!ui->in.nav[DXL_DIR_DOWN] && !ui->in.nav[DXL_DIR_UP])) return 0;
    ui->focus = id;
    return steps;
}

/* SDL's text input, started and stopped with the kit's, its window for
 * the input method placed by the field. */
static void text_input_sync(dxl_ui *ui) {
    if (!ui->window) return;
    int wanted = ui->text_id || ui->text_ready;
    if (wanted && !ui->text_on) {
        ui->text_on = SDL_StartTextInput(ui->window);
    } else if (!wanted && ui->text_on) {
        SDL_StopTextInput(ui->window);
        ui->text_on = 0;
    }
    if (ui->text_on) {
        dxl_rect p = dxl_px(ui, ui->text_area);
        float density = SDL_GetWindowPixelDensity(ui->window);
        if (density <= 0) density = 1;
        SDL_Rect r = { (int)(p.x / density), (int)(p.y / density), (int)(p.w / density), (int)(p.h / density) };
        SDL_SetTextInputArea(ui->window, &r, 0);
    }
}

void dxl_ui_text_begin(dxl_ui *ui, uint32_t id, dxl_rect area) {
    ui->text_id = id;
    ui->text_area = area;
    text_input_sync(ui);
}

void dxl_ui_text_end(dxl_ui *ui) {
    ui->text_id = 0;
    text_input_sync(ui);
}

void dxl_ui_text_ready(dxl_ui *ui, uint32_t id, dxl_rect area) {
    ui->text_ready_now = id;
    if (!ui->text_id) ui->text_area = area;
}

void dxl_ui_capture_begin(dxl_ui *ui, uint32_t id) { ui->capture_id = id; }

void dxl_ui_capture_end(dxl_ui *ui) { ui->capture_id = 0; }

/* Whether input reaches the scope focusables are added to now: only the
 * topmost, as last frame drew it. */
static int scope_live(const dxl_ui *ui) {
    return !ui->prev.count || ui->scope >= dxl_focus_top_scope(&ui->prev);
}

int dxl_ui_live(const dxl_ui *ui) { return scope_live(ui); }

static int pointer_hits(dxl_ui *ui, uint32_t id, dxl_rect r, int focuses) {
    int hits = 0;
    int inside = ui->in.px >= r.x && ui->in.px < r.x + r.w && ui->in.py >= r.y && ui->in.py < r.y + r.h;
    if (inside && ui->mode == DXL_INPUT_MOUSE) {
        hits |= DXL_HIT_HOVERED;
        ui->hot = id;
        /* Focus follows the pointer only as it moves, so it does not take
         * the keys' focus from where they put it; and not from a field
         * being edited. */
        if (focuses && ui->in.pointer_moved && !ui->text_id && ui->focus != id) {
            ui->focus = id;
            ui->cues |= DXL_CUE_FOCUS;
        }
    }
    if (inside && ui->in.pressed) {
        ui->held = id;
        if (focuses) ui->focus = id;
    }
    if (ui->held == id) {
        if (ui->in.released) {
            if (inside) {
                hits |= DXL_HIT_ACTIVATED;
                ui->cues |= DXL_CUE_PRESS;
            }
        } else {
            hits |= DXL_HIT_HELD;
        }
    }
    return hits;
}

int dxl_ui_focusable(dxl_ui *ui, uint32_t id, dxl_rect r, int disabled) {
    if (disabled) return 0;
    dxl_focus_add(&ui->cur, id, box_of(r), ui->scope);
    if (!scope_live(ui)) return 0;
    int hits = pointer_hits(ui, id, r, 1);
    if (ui->focus == id) {
        hits |= DXL_HIT_FOCUSED;
        if (ui->in.activate) {
            hits |= DXL_HIT_ACTIVATED;
            ui->cues |= DXL_CUE_PRESS;
            ui->in.activate = 0;   /* one widget takes it */
        }
    }
    return hits;
}

void dxl_ui_name(dxl_ui *ui, uint32_t id, const char *name) {
    int n = ui->names.count;
    if (!name || n >= (int)(sizeof ui->names.at / sizeof *ui->names.at)) return;
    ui->names.at[n].id = id;
    snprintf(ui->names.at[n].name, sizeof ui->names.at[n].name, "%s", name);
    ui->names.count++;
}

uint32_t dxl_ui_named(const dxl_ui *ui, const char *name) {
    /* The last drawn of the name: a dialog's, over the page under it. */
    uint32_t id = 0;
    for (int i = 0; i < ui->names.count; i++)
        if (!strcmp(ui->names.at[i].name, name)) id = ui->names.at[i].id;
    return id;
}

int dxl_ui_clickable(dxl_ui *ui, uint32_t id, dxl_rect r) {
    if (!scope_live(ui)) return 0;
    return pointer_hits(ui, id, r, 0);
}

float *dxl_ui_value(dxl_ui *ui, uint32_t id, float init) {
    int n = (int)(sizeof ui->values / sizeof *ui->values), oldest = 0;
    for (int i = 0; i < n; i++) {
        if (ui->values[i].id == id) {
            ui->values[i].used = ui->frame;
            return &ui->values[i].v;
        }
        if (ui->values[i].used < ui->values[oldest].used) oldest = i;
    }
    ui->values[oldest].id = id;
    ui->values[oldest].v = init;
    ui->values[oldest].used = ui->frame;
    return &ui->values[oldest].v;
}

void dxl_clip_push(dxl_ui *ui, dxl_rect r) {
    dxl_rect p = dxl_px(ui, r);
    SDL_Rect c = { (int)floorf(p.x + 0.5f), (int)floorf(p.y + 0.5f), 0, 0 };
    c.w = (int)floorf(p.x + p.w + 0.5f) - c.x;
    c.h = (int)floorf(p.y + p.h + 0.5f) - c.y;
    if (c.w < 0) c.w = 0;
    if (c.h < 0) c.h = 0;
    if (ui->clip_count > 0) {
        SDL_Rect both;
        if (!SDL_GetRectIntersection(&c, &ui->clips[ui->clip_count - 1], &both)) both = (SDL_Rect){ c.x, c.y, 0, 0 };
        c = both;
    }
    if (ui->clip_count < (int)(sizeof ui->clips / sizeof *ui->clips)) ui->clips[ui->clip_count] = c;
    ui->clip_count++;
    SDL_SetRenderClipRect(ui->renderer, &c);
}

void dxl_clip_pop(dxl_ui *ui) {
    if (ui->clip_count > 0) ui->clip_count--;
    int n = ui->clip_count;
    int max = (int)(sizeof ui->clips / sizeof *ui->clips);
    SDL_SetRenderClipRect(ui->renderer, n > 0 ? &ui->clips[(n < max ? n : max) - 1] : NULL);
}

/* ---- geometry ---------------------------------------------------------- */

dxl_rect dxl_px(const dxl_ui *ui, dxl_rect r) {
    dxl_rect p = { r.x * ui->scale, r.y * ui->scale, r.w * ui->scale, r.h * ui->scale };
    return p;
}

float dxl_pxf(const dxl_ui *ui, float dp) { return dp * ui->scale; }

dxl_rect dxl_cut_top(dxl_rect *r, float h) {
    if (h > r->h) h = r->h;
    dxl_rect c = { r->x, r->y, r->w, h };
    r->y += h;
    r->h -= h;
    return c;
}

dxl_rect dxl_cut_bottom(dxl_rect *r, float h) {
    if (h > r->h) h = r->h;
    dxl_rect c = { r->x, r->y + r->h - h, r->w, h };
    r->h -= h;
    return c;
}

dxl_rect dxl_cut_left(dxl_rect *r, float w) {
    if (w > r->w) w = r->w;
    dxl_rect c = { r->x, r->y, w, r->h };
    r->x += w;
    r->w -= w;
    return c;
}

dxl_rect dxl_cut_right(dxl_rect *r, float w) {
    if (w > r->w) w = r->w;
    dxl_rect c = { r->x + r->w - w, r->y, w, r->h };
    r->w -= w;
    return c;
}

dxl_rect dxl_inset(dxl_rect r, float d) {
    dxl_rect c = { r.x + d, r.y + d, r.w - 2 * d, r.h - 2 * d };
    if (c.w < 0) c.w = 0;
    if (c.h < 0) c.h = 0;
    return c;
}

dxl_rect dxl_centered(dxl_rect within, float w, float h) {
    dxl_rect c = { within.x + (within.w - w) / 2, within.y + (within.h - h) / 2, w, h };
    return c;
}

void dxl_ui_diag(dxl_ui *ui, const char *fmt, ...) {
    if (ui->diag_count < (int)(sizeof ui->diag / sizeof *ui->diag)) {
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(ui->diag[ui->diag_count], sizeof ui->diag[0], fmt, ap);
        va_end(ap);
    }
    ui->diag_count++;
}

/* The typewriter's pace. */
#define TYPE_CPS 220.0

float dxl_ui_typed(dxl_ui *ui, double since, const char *text) {
    size_t n = text ? strlen(text) : 0;
    /* Typed from the focus's coming, in the frame the focus moved: the new
     * control's, from now; focus_since is the old one's until the frame
     * ends. */
    if (since == ui->focus_since && ui->focus != ui->focus_seen) since = ui->now;
    if (ui->reduced_motion || n == 0 || since < ui->typed_until) return 1;
    double t = (ui->now - since) * TYPE_CPS / (double)n;
    if (t >= 1) return 1;
    dxl_ui_animate(ui);
    return t <= 0 ? 0 : (float)t;
}

void dxl_ui_set_theme(dxl_ui *ui, const dxl_theme *t) {
    ui->theme = *t;
    if (ui->backdrop) SDL_DestroyTexture(ui->backdrop);
    ui->backdrop = NULL;
}

void dxl_ui_animate(dxl_ui *ui) { ui->animating = 1; }

void dxl_ui_wake(dxl_ui *ui, double at) {
    if (ui->wake <= 0 || at < ui->wake) ui->wake = at;
}

float dxl_ui_tween(dxl_ui *ui, double start, float dur) {
    if (ui->reduced_motion || dur <= 0) return 1;
    float t = (float)((ui->now - start) / dur);
    if (t < 1) dxl_ui_animate(ui);
    return ease(t);
}

#include "test.h"
#include "ui_offscreen.h"
#include "ui/controls.h"
#include "ui/glyphs.h"
#include "ui/osk.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <math.h>

static dxl_ui ui;
static double clock_s = 10;

static void key(SDL_Keycode k, SDL_Scancode sc) {
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    e.key.scancode = sc;
    dxl_ui_event(&ui, &e);
}

static void key_mod(SDL_Keycode k, SDL_Keymod mod) {
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    e.key.scancode = SDL_GetScancodeFromKey(k, NULL);
    e.key.mod = mod;
    dxl_ui_event(&ui, &e);
}

/* The pointer at x, y dp (the scale is 1): moved, or its left button
 * pressed (clicks: 2 a double click) or let go. */
static void mouse(Uint32 type, float x, float y, int clicks) {
    SDL_Event e;
    SDL_zero(e);
    e.type = type;
    if (type == SDL_EVENT_MOUSE_MOTION) {
        e.motion.x = x;
        e.motion.y = y;
    } else {
        e.button.button = SDL_BUTTON_LEFT;
        e.button.x = x;
        e.button.y = y;
        e.button.clicks = (Uint8)clicks;
    }
    dxl_ui_event(&ui, &e);
}

static void text(const char *s) {
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_TEXT_INPUT;
    e.text.text = s;
    dxl_ui_event(&ui, &e);
}

static void begin(void) { dxl_ui_begin(&ui, clock_s += 0.016); }
static void end(void) {
    dxl_ui_end(&ui);
    dxl_ui_input_begin(&ui);
}

/* Left and right are a focused control's own; up and down move on. */
static void test_toggle_slider_spinner(void) {
    int on = 0, idx = 1, changed;
    double v = 0.5;
    static const char *const labels[] = { "Low", "Medium", "High" };
    uint32_t a = dxl_ui_id(&ui, "toggle"), b = dxl_ui_id(&ui, "slider"), c = dxl_ui_id(&ui, "spinner");
    for (int frame = 0; frame < 2; frame++) {
        begin();
        dxl_toggle(&ui, a, (dxl_rect){ 20, 20, 600, 44 }, "Toggle", &on, 0);
        dxl_slider(&ui, b, (dxl_rect){ 20, 70, 600, 44 }, "Slider", &v, 0, 1, 0.1, "x", 0);
        dxl_spinner(&ui, c, (dxl_rect){ 20, 120, 600, 44 }, "Spinner", &idx, labels, 3, 0);
        end();
    }
    CHECK_INT(ui.focus, a);
    key(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
    begin();
    changed = dxl_toggle(&ui, a, (dxl_rect){ 20, 20, 600, 44 }, "Toggle", &on, 0);
    dxl_slider(&ui, b, (dxl_rect){ 20, 70, 600, 44 }, "Slider", &v, 0, 1, 0.1, "x", 0);
    dxl_spinner(&ui, c, (dxl_rect){ 20, 120, 600, 44 }, "Spinner", &idx, labels, 3, 0);
    end();
    CHECK_INT(changed, 1);
    CHECK_INT(on, 1);
    CHECK_INT(ui.focus, a);
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    begin();
    dxl_toggle(&ui, a, (dxl_rect){ 20, 20, 600, 44 }, "Toggle", &on, 0);
    dxl_slider(&ui, b, (dxl_rect){ 20, 70, 600, 44 }, "Slider", &v, 0, 1, 0.1, "x", 0);
    dxl_spinner(&ui, c, (dxl_rect){ 20, 120, 600, 44 }, "Spinner", &idx, labels, 3, 0);
    end();
    CHECK_INT(on, 0);
    key(SDLK_DOWN, SDL_SCANCODE_DOWN);
    begin();
    dxl_toggle(&ui, a, (dxl_rect){ 20, 20, 600, 44 }, "Toggle", &on, 0);
    dxl_slider(&ui, b, (dxl_rect){ 20, 70, 600, 44 }, "Slider", &v, 0, 1, 0.1, "x", 0);
    dxl_spinner(&ui, c, (dxl_rect){ 20, 120, 600, 44 }, "Spinner", &idx, labels, 3, 0);
    end();
    CHECK_INT(ui.focus, b);
    key(SDLK_LEFT, SDL_SCANCODE_LEFT);
    key(SDLK_LEFT, SDL_SCANCODE_LEFT);
    begin();
    dxl_toggle(&ui, a, (dxl_rect){ 20, 20, 600, 44 }, "Toggle", &on, 0);
    changed = dxl_slider(&ui, b, (dxl_rect){ 20, 70, 600, 44 }, "Slider", &v, 0, 1, 0.1, "x", 0);
    dxl_spinner(&ui, c, (dxl_rect){ 20, 120, 600, 44 }, "Spinner", &idx, labels, 3, 0);
    end();
    CHECK_INT(changed, 1);
    CHECK(v > 0.29 && v < 0.31);
    key(SDLK_DOWN, SDL_SCANCODE_DOWN);
    begin();
    dxl_toggle(&ui, a, (dxl_rect){ 20, 20, 600, 44 }, "Toggle", &on, 0);
    dxl_slider(&ui, b, (dxl_rect){ 20, 70, 600, 44 }, "Slider", &v, 0, 1, 0.1, "x", 0);
    dxl_spinner(&ui, c, (dxl_rect){ 20, 120, 600, 44 }, "Spinner", &idx, labels, 3, 0);
    end();
    CHECK_INT(ui.focus, c);
    key(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
    key(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
    begin();
    dxl_toggle(&ui, a, (dxl_rect){ 20, 20, 600, 44 }, "Toggle", &on, 0);
    dxl_slider(&ui, b, (dxl_rect){ 20, 70, 600, 44 }, "Slider", &v, 0, 1, 0.1, "x", 0);
    changed = dxl_spinner(&ui, c, (dxl_rect){ 20, 120, 600, 44 }, "Spinner", &idx, labels, 3, 0);
    end();
    CHECK_INT(changed, DXL_SPIN_CHANGED);
    CHECK_INT(idx, 2);   /* the last: no further */
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    begin();
    dxl_toggle(&ui, a, (dxl_rect){ 20, 20, 600, 44 }, "Toggle", &on, 0);
    dxl_slider(&ui, b, (dxl_rect){ 20, 70, 600, 44 }, "Slider", &v, 0, 1, 0.1, "x", 0);
    changed = dxl_spinner(&ui, c, (dxl_rect){ 20, 120, 600, 44 }, "Spinner", &idx, labels, 3, 0);
    end();
    CHECK_INT(changed, DXL_SPIN_LIST);
}

/* A value outside the slider's range, or none (a NaN), shows at its end and
 * is kept as it is, no change and no sound, until the player moves the
 * slider, from where it shows. */
static void test_slider_out_of_range(void) {
    uint32_t fov = dxl_ui_id(&ui, "fov"), none = dxl_ui_id(&ui, "none");
    dxl_rect a = { 20, 20, 600, 44 }, b = { 20, 70, 600, 44 };
    double v = 130, n = NAN;
    int changed = 0, cues = 0;
    dxl_ui_forget(&ui);
    dxl_ui_focus(&ui, fov);
    for (int frame = 0; frame < 3; frame++) {
        begin();
        changed |= dxl_slider(&ui, fov, a, "Field of view", &v, 60, 120, 5, "130", 0);
        changed |= dxl_slider(&ui, none, b, "None", &n, 0, 1, 0.1, "nan", 0);
        cues |= ui.cues;
        end();
    }
    CHECK_INT(changed, 0);
    CHECK(v == 130);
    CHECK(isnan(n));
    CHECK(!(cues & DXL_CUE_SLIDE));
    /* Right at the end it shows at: no further, and no change. */
    key(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
    begin();
    changed = dxl_slider(&ui, fov, a, "Field of view", &v, 60, 120, 5, "130", 0);
    dxl_slider(&ui, none, b, "None", &n, 0, 1, 0.1, "nan", 0);
    end();
    CHECK_INT(changed, 0);
    CHECK(v == 130);
    key(SDLK_LEFT, SDL_SCANCODE_LEFT);
    begin();
    changed = dxl_slider(&ui, fov, a, "Field of view", &v, 60, 120, 5, "130", 0);
    dxl_slider(&ui, none, b, "None", &n, 0, 1, 0.1, "nan", 0);
    end();
    CHECK_INT(changed, 1);
    CHECK(v == 115);
    /* The NaN's slider, moved, from its start. */
    key(SDLK_DOWN, SDL_SCANCODE_DOWN);
    begin();
    dxl_slider(&ui, fov, a, "Field of view", &v, 60, 120, 5, "115", 0);
    changed = dxl_slider(&ui, none, b, "None", &n, 0, 1, 0.1, "nan", 0);
    end();
    CHECK_INT(ui.focus, none);
    CHECK_INT(changed, 0);
    key(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
    begin();
    dxl_slider(&ui, fov, a, "Field of view", &v, 60, 120, 5, "115", 0);
    changed = dxl_slider(&ui, none, b, "None", &n, 0, 1, 0.1, "nan", 0);
    end();
    CHECK_INT(changed, 1);
    CHECK(n > 0.09 && n < 0.11);
}

/* Edited in place: typed, erased, the caret moved; Enter keeps it, Esc
 * puts it back. */
static void test_text_field(void) {
    char buf[32] = "JC";
    uint32_t id = dxl_ui_id(&ui, "name");
    dxl_ui_forget(&ui);
    dxl_ui_focus(&ui, id);
    int r = 0;
    for (int frame = 0; frame < 2; frame++) {
        begin();
        r = dxl_text_field(&ui, id, (dxl_rect){ 20, 20, 600, 44 }, "Name", buf, sizeof buf, NULL, 0);
        end();
    }
    ui.mode = DXL_INPUT_KEYBOARD;
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    begin();
    r = dxl_text_field(&ui, id, (dxl_rect){ 20, 20, 600, 44 }, "Name", buf, sizeof buf, NULL, 0);
    end();
    CHECK_INT(ui.text_id, id);
    text(" Denton");
    key(SDLK_BACKSPACE, SDL_SCANCODE_BACKSPACE);
    begin();
    r = dxl_text_field(&ui, id, (dxl_rect){ 20, 20, 600, 44 }, "Name", buf, sizeof buf, NULL, 0);
    end();
    CHECK_STR(buf, "JC Dento");
    key(SDLK_HOME, SDL_SCANCODE_HOME);
    text("\xc3\x89");   /* a character of two bytes */
    begin();
    r = dxl_text_field(&ui, id, (dxl_rect){ 20, 20, 600, 44 }, "Name", buf, sizeof buf, NULL, 0);
    end();
    CHECK_STR(buf, "\xc3\x89JC Dento");
    key(SDLK_DELETE, SDL_SCANCODE_DELETE);
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    begin();
    r = dxl_text_field(&ui, id, (dxl_rect){ 20, 20, 600, 44 }, "Name", buf, sizeof buf, NULL, 0);
    end();
    CHECK_INT(r, DXL_TEXT_DONE);
    CHECK_STR(buf, "\xc3\x89" "C Dento");
    CHECK_INT(ui.text_id, 0);
    /* Again, and Esc: as it was. */
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    begin();
    dxl_text_field(&ui, id, (dxl_rect){ 20, 20, 600, 44 }, "Name", buf, sizeof buf, NULL, 0);
    end();
    text("xyz");
    key(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    begin();
    dxl_text_field(&ui, id, (dxl_rect){ 20, 20, 600, 44 }, "Name", buf, sizeof buf, NULL, 0);
    end();
    CHECK_STR(buf, "\xc3\x89" "C Dento");
    CHECK_INT(ui.text_id, 0);
    /* From a pad: the on-screen keyboard is the caller's. */
    ui.mode = DXL_INPUT_PAD;
    ui.in.activate = 1;
    begin();
    r = dxl_text_field(&ui, id, (dxl_rect){ 20, 20, 600, 44 }, "Name", buf, sizeof buf, NULL, 0);
    end();
    CHECK_INT(r, DXL_TEXT_OSK);
}

/* A frame of the field id over a button below it; the field's
 * DXL_TEXT_*, and the button's DXL_HIT_*s. */
static int field_frame(uint32_t id, char *buf, size_t size, int *below) {
    begin();
    int r = dxl_text_field(&ui, id, (dxl_rect){ 20, 20, 600, 44 }, "Name", buf, size, NULL, 0);
    int h = dxl_ui_focusable(&ui, dxl_ui_id(&ui, "below"), (dxl_rect){ 20, 80, 600, 44 }, 0);
    if (below) *below = h;
    end();
    return r;
}

/* Shift selects, Ctrl goes by words, and the clipboard's three. */
static void test_text_select(void) {
    char buf[64] = "JC Denton";
    uint32_t id = dxl_ui_id(&ui, "select");
    dxl_ui_forget(&ui);
    dxl_ui_focus(&ui, id);
    ui.mode = DXL_INPUT_KEYBOARD;
    field_frame(id, buf, sizeof buf, NULL);
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_INT(ui.text_id, id);
    key_mod(SDLK_LEFT, SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT);   /* "Denton" selected */
    key_mod(SDLK_C, SDL_KMOD_LCTRL);
    text("Paul");
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_STR(buf, "JC Paul");
    key_mod(SDLK_HOME, 0);
    key_mod(SDLK_V, SDL_KMOD_LCTRL);
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_STR(buf, "DentonJC Paul");
    key_mod(SDLK_BACKSPACE, SDL_KMOD_LCTRL);   /* the word before the caret */
    key_mod(SDLK_RIGHT, SDL_KMOD_LCTRL);
    key_mod(SDLK_DELETE, SDL_KMOD_LCTRL);      /* the space and the word after it */
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_STR(buf, "JC");
    key_mod(SDLK_A, SDL_KMOD_LCTRL);
    key_mod(SDLK_X, SDL_KMOD_LCTRL);
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_STR(buf, "");
    key_mod(SDLK_V, SDL_KMOD_LCTRL);
    key_mod(SDLK_V, SDL_KMOD_LCTRL);
    key_mod(SDLK_LEFT, SDL_KMOD_LSHIFT);
    key_mod(SDLK_LEFT, SDL_KMOD_LSHIFT);
    key_mod(SDLK_LEFT, 0);                     /* to the selection's start */
    text("-");
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    CHECK_INT(field_frame(id, buf, sizeof buf, NULL), DXL_TEXT_DONE);
    CHECK_STR(buf, "JC-JC");
}

/* A field with the focus takes typing as it is: W, A, S and D type there,
 * the text typed in its place; Space chooses it; Tab keeps it and moves. */
static void test_text_typed_on(void) {
    char buf[64] = "Player";
    uint32_t id = dxl_ui_id(&ui, "typed on");
    dxl_ui_forget(&ui);
    dxl_ui_focus(&ui, id);
    ui.mode = DXL_INPUT_KEYBOARD;
    int below = 0;
    field_frame(id, buf, sizeof buf, NULL);
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_INT(ui.text_ready, id);
    key(SDLK_S, SDL_SCANCODE_S);
    text("s");
    key(SDLK_D, SDL_SCANCODE_D);
    text("d");
    field_frame(id, buf, sizeof buf, &below);
    CHECK_INT(ui.text_id, id);
    CHECK_STR(buf, "sd");
    CHECK(!(below & DXL_HIT_FOCUSED));
    key(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_STR(buf, "Player");
    CHECK_INT(ui.text_id, 0);
    key(SDLK_SPACE, SDL_SCANCODE_SPACE);
    text(" ");
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_INT(ui.text_id, id);
    CHECK_STR(buf, "Player");
    text("!");
    key(SDLK_TAB, SDL_SCANCODE_TAB);
    CHECK_INT(field_frame(id, buf, sizeof buf, &below), DXL_TEXT_DONE);
    CHECK_STR(buf, "Player!");
    CHECK(below & DXL_HIT_FOCUSED);
    CHECK_INT(ui.text_id, 0);
    /* Backspace on it as it has the focus: its edit, not the page's back. */
    dxl_ui_focus(&ui, id);
    field_frame(id, buf, sizeof buf, NULL);
    field_frame(id, buf, sizeof buf, NULL);
    key(SDLK_BACKSPACE, SDL_SCANCODE_BACKSPACE);
    CHECK_INT(ui.in.back, 0);
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_INT(ui.text_id, id);
    CHECK_STR(buf, "");
    key(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_STR(buf, "Player!");
}

/* A click puts the caret where it is, a double click takes a word, the
 * pointer moved elsewhere leaves the edit be, a click away keeps it. An
 * edit whose field is not drawn ends, and is kept as it is drawn again. */
static void test_text_mouse(void) {
    char buf[64] = "JC Denton";
    uint32_t id = dxl_ui_id(&ui, "mouse");
    dxl_ui_forget(&ui);
    ui.mode = DXL_INPUT_MOUSE;
    field_frame(id, buf, sizeof buf, NULL);
    /* The text starts 10 dp into the field's box, itself after the label's
     * column (controls.c). */
    float x0 = 20 + dxl_control_label_w(600) + 8 + 10, y = 42;
    float x = x0 + dxl_text_size(&ui, DXL_FONT_BODY, "JC", 0).x + 0.4f;
    mouse(SDL_EVENT_MOUSE_MOTION, x, y, 0);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, x, y, 1);
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_INT(ui.text_id, id);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, x, y, 1);
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_INT(ui.text_id, id);
    text("!");
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_STR(buf, "JC! Denton");
    int below = 0;
    mouse(SDL_EVENT_MOUSE_MOTION, 100, 100, 0);
    field_frame(id, buf, sizeof buf, &below);
    CHECK(!(below & DXL_HIT_FOCUSED));
    CHECK_INT(ui.text_id, id);
    x = x0 + dxl_text_size(&ui, DXL_FONT_BODY, "JC! Den", 0).x;
    mouse(SDL_EVENT_MOUSE_MOTION, x, y, 0);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, x, y, 2);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, x, y, 2);
    text("Paul");
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_STR(buf, "JC! Paul");
    /* Out of view a frame: ended, kept. */
    begin();
    dxl_ui_focusable(&ui, dxl_ui_id(&ui, "below"), (dxl_rect){ 20, 80, 600, 44 }, 0);
    end();
    CHECK_INT(ui.text_id, 0);
    CHECK_INT(field_frame(id, buf, sizeof buf, NULL), DXL_TEXT_DONE);
    /* Again by a click, and a click away. */
    mouse(SDL_EVENT_MOUSE_MOTION, x0 + 2, y, 0);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, x0 + 2, y, 1);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, x0 + 2, y, 1);
    field_frame(id, buf, sizeof buf, NULL);
    text(">");
    field_frame(id, buf, sizeof buf, NULL);
    CHECK_STR(buf, ">JC! Paul");
    mouse(SDL_EVENT_MOUSE_MOTION, 100, 100, 0);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, 100, 100, 1);
    CHECK_INT(field_frame(id, buf, sizeof buf, &below), DXL_TEXT_DONE);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, 100, 100, 1);
    field_frame(id, buf, sizeof buf, &below);
    CHECK(below & DXL_HIT_ACTIVATED);
    CHECK_INT(ui.text_id, 0);
    CHECK_INT(ui.diag_count, 0);
}

/* Two fields, the second under the first; each's DXL_TEXT_*. */
static void two_fields(uint32_t a, char *x, uint32_t b, char *y, size_t size, int *ra, int *rb) {
    begin();
    *ra = dxl_text_field(&ui, a, (dxl_rect){ 20, 20, 600, 44 }, "First", x, size, NULL, 0);
    *rb = dxl_text_field(&ui, b, (dxl_rect){ 20, 80, 600, 44 }, "Second", y, size, NULL, 0);
    end();
}

/* A click on another field keeps the edit under way -- the field below
 * it or above -- and that field's edit begins; a key right after an edit
 * kept with Enter is still the field's, Backspace no page's back. */
static void test_text_two_fields(void) {
    char x[32] = "one", y[32] = "two";
    uint32_t a = dxl_ui_id(&ui, "first"), b = dxl_ui_id(&ui, "second");
    int ra, rb;
    dxl_ui_forget(&ui);
    ui.mode = DXL_INPUT_MOUSE;
    float tx = 20 + dxl_control_label_w(600) + 8 + 10 + 2;
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    /* The second, by a click, typed in. */
    mouse(SDL_EVENT_MOUSE_MOTION, tx, 102, 0);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, tx, 102, 1);
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, tx, 102, 1);
    text("x");
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    CHECK_INT(ui.text_id, b);
    CHECK_STR(y, "xtwo");
    /* The first, drawn before it, clicked: the second's edit kept. */
    mouse(SDL_EVENT_MOUSE_MOTION, tx, 42, 0);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, tx, 42, 1);
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    CHECK_INT(rb, DXL_TEXT_DONE);
    CHECK_INT(ui.text_id, a);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, tx, 42, 1);
    text("y");
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    CHECK_STR(x, "yone");
    /* And back to the second, drawn after it: the first's kept. */
    mouse(SDL_EVENT_MOUSE_MOTION, tx, 102, 0);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, tx, 102, 1);
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    CHECK_INT(ra, DXL_TEXT_DONE);
    CHECK_INT(ui.text_id, b);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, tx, 102, 1);
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    /* Kept with Enter, then Backspace at once: the field's edit again. */
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    CHECK_INT(rb, DXL_TEXT_DONE);
    CHECK_INT(ui.text_ready, b);
    key(SDLK_BACKSPACE, SDL_SCANCODE_BACKSPACE);
    CHECK_INT(ui.in.back, 0);
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    CHECK_INT(ui.text_id, b);
    key(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    CHECK_STR(y, "xtwo");
    /* Ctrl+Space, which types nothing, begins the edit; a space typed
     * after is kept. A frame for each key, as a player types. */
    dxl_ui_focus(&ui, a);
    ui.mode = DXL_INPUT_KEYBOARD;
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    key_mod(SDLK_SPACE, SDL_KMOD_LCTRL);
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    CHECK_INT(ui.text_id, a);
    key(SDLK_A, SDL_SCANCODE_A);
    text("a");
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    key(SDLK_SPACE, SDL_SCANCODE_SPACE);
    text(" ");
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    key(SDLK_B, SDL_SCANCODE_B);
    text("b");
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    CHECK_STR(x, "yonea b");
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    two_fields(a, x, b, y, sizeof x, &ra, &rb);
    CHECK_INT(ra, DXL_TEXT_DONE);
    CHECK_INT(ui.diag_count, 0);
}

/* A slot takes the next key as it is, Esc cancels, Delete empties. */
/* A key captured is named as the engine's window names it (bugs.md
 * LCORE.6): the keypad by Num Lock as the key went down, the ISO key,
 * Windows' code for a typing key alone; SDL's keymap here the default, US. */
static void test_key_names(void) {
    uint32_t id = dxl_ui_id(&ui, "names slot");
    dxl_ui_forget(&ui);
    dxl_ui_focus(&ui, id);
    ui.mode = DXL_INPUT_KEYBOARD;
    begin();
    dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0);
    end();
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    begin();
    dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0);
    end();
    key_mod(SDLK_KP_8, SDL_KMOD_NUM);
    begin();
    CHECK_INT(dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0), DXL_SLOT_CAPTURED);
    CHECK_STR(dxl_ui_key_name(ui.in.captured_code, ui.in.captured_mod, 0), "NumPad8");
    end();
    CHECK_STR(dxl_ui_key_name(SDL_SCANCODE_KP_8, SDL_KMOD_NONE, 0), "Up");
    CHECK_STR(dxl_ui_key_name(SDL_SCANCODE_KP_5, SDL_KMOD_NONE, 0), "Unknown0C");
    CHECK_STR(dxl_ui_key_name(SDL_SCANCODE_NONUSBACKSLASH, SDL_KMOD_NUM, 0), "JoyZ");
    CHECK_STR(dxl_ui_key_name(SDL_SCANCODE_W, SDL_KMOD_NUM, 'Z'), "Z");
    CHECK_STR(dxl_ui_key_name(SDL_SCANCODE_F1, SDL_KMOD_NUM, 'Z'), "F1");
}

static void test_key_slot(void) {
    uint32_t id = dxl_ui_id(&ui, "slot");
    dxl_ui_forget(&ui);
    dxl_ui_focus(&ui, id);
    ui.mode = DXL_INPUT_KEYBOARD;
    int r;
    begin();
    dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0);
    end();
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    begin();
    dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0);
    end();
    CHECK_INT(ui.capture_id, id);
    key(SDLK_DOWN, SDL_SCANCODE_DOWN);   /* not a move: the key captured */
    begin();
    r = dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0);
    CHECK_INT(r, DXL_SLOT_CAPTURED);
    CHECK_INT(ui.in.captured, DXL_CAPTURED_KEY);
    CHECK_INT(ui.in.captured_code, SDL_SCANCODE_DOWN);
    end();
    CHECK_INT(ui.capture_id, 0);
    key(SDLK_RETURN, SDL_SCANCODE_RETURN);
    begin();
    dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0);
    end();
    key(SDLK_ESCAPE, SDL_SCANCODE_ESCAPE);
    begin();
    r = dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0);
    end();
    CHECK_INT(r, DXL_SLOT_CANCELLED);
    key(SDLK_DELETE, SDL_SCANCODE_DELETE);
    begin();
    r = dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0);
    end();
    CHECK_INT(r, DXL_SLOT_CLEARED);
    /* While it captures, the foot's Cancel hint clicked is Esc: cancelled;
     * the left button anywhere else is captured. */
    static const dxl_hint_item foot[] = { { DXL_HINT_BACK, "Cancel" } };
    const dxl_rect bar = { 0, 600, 1280, 60 };
    for (int on_hint = 1; on_hint >= 0; on_hint--) {
        key(SDLK_RETURN, SDL_SCANCODE_RETURN);
        begin();
        dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0);
        dxl_hints(&ui, bar, foot, 1);
        end();
        CHECK_INT(ui.capture_id, id);
        float x = on_hint ? 1270 : 640, y = on_hint ? 630 : 300;
        mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, x, y, 1);
        begin();
        r = dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0);
        CHECK_INT(r, on_hint ? DXL_SLOT_CANCELLED : DXL_SLOT_CAPTURED);
        if (!on_hint) CHECK_INT(ui.in.captured_code, SDL_BUTTON_LEFT);
        dxl_hints(&ui, bar, foot, 1);
        end();
        CHECK_INT(ui.capture_id, 0);
        mouse(SDL_EVENT_MOUSE_BUTTON_UP, x, y, 1);
        begin();
        dxl_key_slot(&ui, id, (dxl_rect){ 20, 20, 160, 40 }, "W", 0);
        dxl_hints(&ui, bar, foot, 1);
        end();
        CHECK_INT(ui.capture_id, 0);
    }
}

/* Ctrl+Tab and the shoulders step through tabs, round. */
static void test_tabs(void) {
    static const char *const labels[] = { "Display", "Graphics", "Audio" };
    int cur = 0;
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = SDLK_TAB;
    e.key.mod = SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT;
    dxl_ui_event(&ui, &e);
    begin();
    CHECK_INT(dxl_tabs(&ui, "tabs", (dxl_rect){ 20, 20, 800, 40 }, labels, 3, &cur), 1);
    end();
    CHECK_INT(cur, 2);
    ui.in.shoulder_r = 1;
    begin();
    dxl_tabs(&ui, "tabs", (dxl_rect){ 20, 20, 800, 40 }, labels, 3, &cur);
    end();
    CHECK_INT(cur, 0);
}

/* The keyboard types, erases, and is done, or puts the text back. */
static void test_osk(void) {
    char buf[32] = "JC";
    dxl_osk_state st;
    dxl_osk_open(&st, buf);
    dxl_rect over = { 0, 0, ui.w, ui.h };
    dxl_ui_forget(&ui);
    dxl_ui_focus(&ui, dxl_ui_id(&ui, "osk 1 0"));   /* the q */
    begin();
    dxl_osk(&ui, &st, over, "Name", buf, sizeof buf, 0);
    end();
    ui.mode = DXL_INPUT_PAD;
    ui.in.activate = 1;
    begin();
    dxl_osk(&ui, &st, over, "Name", buf, sizeof buf, 0);
    end();
    CHECK_STR(buf, "JCq");
    ui.in.clear = 1;
    begin();
    dxl_osk(&ui, &st, over, "Name", buf, sizeof buf, 0);
    end();
    CHECK_STR(buf, "JC");
    ui.in.start = 1;
    begin();
    CHECK_INT(dxl_osk(&ui, &st, over, "Name", buf, sizeof buf, 0), 1);
    end();
    dxl_osk_open(&st, buf);
    ui.in.activate = 1;
    begin();
    dxl_osk(&ui, &st, over, "Name", buf, sizeof buf, 0);
    end();
    begin();
    CHECK_INT(dxl_osk(&ui, &st, over, "Name", buf, sizeof buf, 1), -1);
    end();
    CHECK_STR(buf, "JC");
    CHECK_INT(ui.diag_count, 0);
}

TEST_MAIN_BEGIN
    /* A video driver of none, for the clipboard SDL keeps itself. */
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_VIDEO) || !TTF_Init()) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    offscreen o;
    if (offscreen_open(&o, &ui, 1280, 720, 1) != 0) return 1;
    dxl_ui_input_begin(&ui);
    RUN(test_toggle_slider_spinner);
    RUN(test_slider_out_of_range);
    RUN(test_text_field);
    RUN(test_text_select);
    RUN(test_text_typed_on);
    RUN(test_text_mouse);
    RUN(test_text_two_fields);
    RUN(test_key_slot);
    RUN(test_key_names);
    RUN(test_tabs);
    RUN(test_osk);
    offscreen_close(&o, &ui);
    TTF_Quit();
    SDL_Quit();
TEST_MAIN_END

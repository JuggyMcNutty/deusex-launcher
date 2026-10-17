#include "test.h"
#include "ui_offscreen.h"
#include "ui/controls.h"
#include "ui/osk.h"
#include "ui/widgets.h"

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

/* A slot takes the next key as it is, Esc cancels, Delete empties. */
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
    if (!TTF_Init()) {
        fprintf(stderr, "TTF_Init: %s\n", SDL_GetError());
        return 1;
    }
    offscreen o;
    if (offscreen_open(&o, &ui, 1280, 720, 1) != 0) return 1;
    dxl_ui_input_begin(&ui);
    RUN(test_toggle_slider_spinner);
    RUN(test_text_field);
    RUN(test_key_slot);
    RUN(test_tabs);
    RUN(test_osk);
    offscreen_close(&o, &ui);
    TTF_Quit();
TEST_MAIN_END

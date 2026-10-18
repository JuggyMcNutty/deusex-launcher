#include "test.h"
#include "ui_offscreen.h"
#include "ui/widgets.h"

static dxl_ui ui;

static void key(SDL_Keycode k, SDL_Keymod mod) {
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    e.key.mod = mod;
    dxl_ui_event(&ui, &e);
}

static void mouse(Uint32 type, float x_px, float y_px) {
    SDL_Event e;
    SDL_zero(e);
    e.type = type;
    if (type == SDL_EVENT_MOUSE_MOTION) {
        e.motion.x = x_px;
        e.motion.y = y_px;
    } else {
        e.button.button = SDL_BUTTON_LEFT;
        e.button.x = x_px;
        e.button.y = y_px;
    }
    dxl_ui_event(&ui, &e);
}

static void pump(void) {
    SDL_UpdateJoysticks();
    SDL_Event e;
    while (SDL_PollEvent(&e)) dxl_ui_event(&ui, &e);
}

/* Three buttons in a column, a frame of them; their DXL_HIT_*s. */
static double clock_s = 10;
static void column(int hits[3]) {
    dxl_ui_begin(&ui, clock_s += 0.016);
    static const char *ids[3] = { "one", "two", "three" };
    for (int i = 0; i < 3; i++)
        hits[i] = dxl_ui_focusable(&ui, dxl_ui_id(&ui, ids[i]), (dxl_rect){ 20, 20 + i * 60.0f, 200, 44 }, 0);
    dxl_ui_end(&ui);
    dxl_ui_input_begin(&ui);
}

static int focused(void) {
    int hits[3];
    column(hits);
    for (int i = 0; i < 3; i++)
        if (hits[i] & DXL_HIT_FOCUSED) return i;
    return -1;
}

static void test_keyboard(void) {
    int hits[3];
    dxl_ui_input_begin(&ui);
    column(hits);
    column(hits);
    CHECK(hits[0] & DXL_HIT_FOCUSED);
    key(SDLK_DOWN, 0);
    CHECK_INT(ui.mode, DXL_INPUT_KEYBOARD);
    CHECK_INT(focused(), 1);
    key(SDLK_S, 0);
    CHECK_INT(focused(), 2);
    key(SDLK_TAB, 0);
    CHECK_INT(focused(), 0);
    key(SDLK_TAB, SDL_KMOD_SHIFT);
    CHECK_INT(focused(), 2);
    key(SDLK_RETURN, 0);
    column(hits);
    CHECK(hits[2] & DXL_HIT_ACTIVATED);
    CHECK(!(hits[0] & DXL_HIT_ACTIVATED) && !(hits[1] & DXL_HIT_ACTIVATED));
    key(SDLK_ESCAPE, 0);
    CHECK_INT(ui.in.back, 1);
    dxl_ui_input_begin(&ui);
}

/* The pointer, in dp at twice the scale; hover focuses only as it moves;
 * a click is a press and a release on the same widget. */
static void test_mouse(void) {
    int hits[3];
    mouse(SDL_EVENT_MOUSE_MOTION, 100 * ui.scale, 100 * ui.scale);
    CHECK(ui.in.px > 99.9f && ui.in.px < 100.1f);
    CHECK_INT(ui.mode, DXL_INPUT_MOUSE);
    column(hits);
    CHECK(hits[1] & DXL_HIT_HOVERED);
    CHECK(hits[1] & DXL_HIT_FOCUSED);
    /* The keys move the focus; the pointer, still, does not take it back. */
    key(SDLK_DOWN, 0);
    CHECK_INT(focused(), 2);
    CHECK_INT(focused(), 2);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, 100 * ui.scale, 40 * ui.scale);
    column(hits);
    CHECK(hits[0] & DXL_HIT_HELD);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, 100 * ui.scale, 40 * ui.scale);
    column(hits);
    CHECK(hits[0] & DXL_HIT_ACTIVATED);
    /* Pressed on one, let go off it: nothing. */
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, 100 * ui.scale, 40 * ui.scale);
    column(hits);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, 600 * ui.scale, 300 * ui.scale);
    column(hits);
    CHECK(!(hits[0] & DXL_HIT_ACTIVATED));
}

static void window_event(Uint32 type) {
    SDL_Event e;
    SDL_zero(e);
    e.type = type;
    dxl_ui_event(&ui, &e);
}

/* The pointer where it rests as it comes into the window, or as the window
 * changes under it, is no move: the focus stays where the keys put it, the
 * hints the keys', till the pointer moves. */
static void test_pointer_resting(void) {
    int hits[3];
    dxl_ui_focus(&ui, dxl_ui_id(&ui, "three"));
    column(hits);
    key(SDLK_UP, 0);
    CHECK_INT(focused(), 1);
    window_event(SDL_EVENT_WINDOW_MOUSE_ENTER);
    mouse(SDL_EVENT_MOUSE_MOTION, 100 * ui.scale, 40 * ui.scale);
    CHECK_INT(focused(), 1);
    CHECK_INT(ui.mode, DXL_INPUT_KEYBOARD);
    mouse(SDL_EVENT_MOUSE_MOTION, 101 * ui.scale, 40 * ui.scale);
    CHECK_INT(focused(), 0);
    CHECK_INT(ui.mode, DXL_INPUT_MOUSE);
    window_event(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED);
    mouse(SDL_EVENT_MOUSE_MOTION, 100 * ui.scale, 160 * ui.scale);
    column(hits);
    CHECK(hits[0] & DXL_HIT_FOCUSED);
    CHECK(!(hits[2] & DXL_HIT_FOCUSED));
    mouse(SDL_EVENT_MOUSE_MOTION, 101 * ui.scale, 160 * ui.scale);
    column(hits);
    CHECK(hits[2] & DXL_HIT_FOCUSED);
}

static SDL_JoystickID pad(Uint16 vendor, Uint16 product, const char *name) {
    SDL_VirtualJoystickDesc d;
    SDL_INIT_INTERFACE(&d);
    d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    d.vendor_id = vendor;
    d.product_id = product;
    d.naxes = SDL_GAMEPAD_AXIS_COUNT;
    d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    d.name = name;
    SDL_JoystickID id = SDL_AttachVirtualJoystick(&d);
    if (id) SDL_OpenGamepad(id);
    pump();
    return id;
}

static void press(SDL_JoystickID id, SDL_GamepadButton b) {
    SDL_Joystick *j = SDL_GetJoystickFromID(id);
    SDL_SetJoystickVirtualButton(j, b, true);
    pump();
    SDL_SetJoystickVirtualButton(j, b, false);
    pump();
}

/* Confirm and back where each pad's labels put them. */
static void test_pads(void) {
    SDL_JoystickID xbox = pad(0x045e, 0x0b12, "Xbox Series X Controller");
    SDL_JoystickID ps = pad(0x054c, 0x0ce6, "DualSense Wireless Controller");
    SDL_JoystickID sw = pad(0x057e, 0x2009, "Nintendo Switch Pro Controller");
    CHECK(xbox && ps && sw);
    dxl_ui_input_begin(&ui);

    press(xbox, SDL_GAMEPAD_BUTTON_SOUTH);
    CHECK_INT(ui.mode, DXL_INPUT_PAD);
    CHECK_INT(ui.pad_family, DXL_PADFAMILY_XBOX);
    CHECK_INT(ui.in.activate, 1);
    dxl_ui_input_begin(&ui);
    press(xbox, SDL_GAMEPAD_BUTTON_EAST);
    CHECK_INT(ui.in.back, 1);
    dxl_ui_input_begin(&ui);

    press(ps, SDL_GAMEPAD_BUTTON_SOUTH);
    CHECK_INT(ui.pad_family, DXL_PADFAMILY_PLAYSTATION);
    CHECK_INT(ui.in.activate, 1);
    dxl_ui_input_begin(&ui);

    /* A Nintendo pad's A is east: it confirms; south, its B, goes back. */
    press(sw, SDL_GAMEPAD_BUTTON_EAST);
    CHECK_INT(ui.pad_family, DXL_PADFAMILY_NINTENDO);
    CHECK_INT(ui.in.activate, 1);
    CHECK_INT(ui.in.back, 0);
    dxl_ui_input_begin(&ui);
    press(sw, SDL_GAMEPAD_BUTTON_SOUTH);
    CHECK_INT(ui.in.back, 1);
    CHECK_INT(ui.in.activate, 0);
    dxl_ui_input_begin(&ui);

    press(xbox, SDL_GAMEPAD_BUTTON_START);
    CHECK_INT(ui.in.start, 1);
    dxl_ui_input_begin(&ui);
}

/* A held direction: once, then again after 350 ms, then every 90. */
static void test_repeat(void) {
    SDL_JoystickID xbox = pad(0x045e, 0x0b12, "Xbox Series X Controller");
    SDL_Joystick *j = SDL_GetJoystickFromID(xbox);
    dxl_ui_input_begin(&ui);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_DPAD_DOWN, true);
    pump();
    CHECK_INT(ui.in.nav[DXL_DIR_DOWN], 1);
    double t = clock_s;
    int moves = 0;
    /* Frames of 10 ms for a second: the first repeat at 0.35, then 0.44,
     * 0.53, ... 0.98: eight. */
    for (int i = 1; i <= 100; i++) {
        dxl_ui_input_begin(&ui);
        dxl_ui_begin(&ui, t + i * 0.01);
        moves += ui.in.nav[DXL_DIR_DOWN];
        dxl_ui_end(&ui);
    }
    CHECK(moves >= 7 && moves <= 9);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_DPAD_DOWN, false);
    pump();
    moves = 0;
    for (int i = 101; i <= 150; i++) {
        dxl_ui_input_begin(&ui);
        dxl_ui_begin(&ui, t + i * 0.01);
        moves += ui.in.nav[DXL_DIR_DOWN];
        dxl_ui_end(&ui);
    }
    CHECK_INT(moves, 0);
    clock_s = t + 2;

    /* The stick: past half way is a move, and back under a third lets go. */
    dxl_ui_input_begin(&ui);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, 30000);
    pump();
    CHECK_INT(ui.in.nav[DXL_DIR_DOWN], 1);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, 25000);
    pump();
    CHECK_INT(ui.in.nav[DXL_DIR_DOWN], 1);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, 0);
    pump();
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTX, -30000);
    pump();
    CHECK_INT(ui.in.nav[DXL_DIR_LEFT], 1);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTX, 0);
    pump();
    dxl_ui_input_begin(&ui);
}

/* Frames of 10 ms from t, i to n: the moves down they had. */
static int downs(double t, int i, int n) {
    int moves = 0;
    for (; i <= n; i++) {
        dxl_ui_input_begin(&ui);
        dxl_ui_begin(&ui, t + i * 0.01);
        moves += ui.in.nav[DXL_DIR_DOWN];
        dxl_ui_end(&ui);
    }
    return moves;
}

/* A press after the window waited seconds for an event: one move, its
 * first repeat 350 ms after the frame that takes it -- not in that same
 * frame, as when it counted from the last frame before the wait. A
 * direction let go of stops its own repeat, not one pressed since. */
static void test_repeat_after_idle(void) {
    SDL_JoystickID xbox = pad(0x045e, 0x0b12, "Xbox Series X Controller");
    SDL_Joystick *j = SDL_GetJoystickFromID(xbox);
    dxl_ui_input_begin(&ui);
    dxl_ui_begin(&ui, clock_s += 0.016);
    dxl_ui_end(&ui);
    double t = clock_s + 5;
    dxl_ui_input_begin(&ui);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_DPAD_DOWN, true);
    pump();
    dxl_ui_begin(&ui, t);
    CHECK_INT(ui.in.nav[DXL_DIR_DOWN], 1);
    dxl_ui_end(&ui);
    CHECK_INT(downs(t, 1, 33), 0);
    CHECK_INT(downs(t, 34, 40), 1);

    /* Right pressed as down is held, then down let go: right repeats. */
    dxl_ui_input_begin(&ui);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true);
    pump();
    CHECK_INT(ui.in.nav[DXL_DIR_RIGHT], 1);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_DPAD_DOWN, false);
    pump();
    int rights = 0;
    for (int i = 41; i <= 100; i++) {
        dxl_ui_input_begin(&ui);
        dxl_ui_begin(&ui, t + i * 0.01);
        rights += ui.in.nav[DXL_DIR_RIGHT];
        CHECK_INT(ui.in.nav[DXL_DIR_DOWN], 0);
        dxl_ui_end(&ui);
    }
    CHECK(rights >= 2);
    SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false);
    pump();
    rights = 0;
    for (int i = 101; i <= 150; i++) {
        dxl_ui_input_begin(&ui);
        dxl_ui_begin(&ui, t + i * 0.01);
        rights += ui.in.nav[DXL_DIR_RIGHT];
        dxl_ui_end(&ui);
    }
    CHECK_INT(rights, 0);
    clock_s = t + 2;
    dxl_ui_input_begin(&ui);
}

/* A slot capturing takes a trigger pulled past half, as the game presses
 * it. With the pads another's (the mapping wizard's), a held stick stops
 * repeating: its letting go is not seen. */
static void test_trigger_and_pads_off(void) {
    SDL_JoystickID xbox = pad(0x045e, 0x0b12, "Xbox Series X Controller");
    SDL_Joystick *j = SDL_GetJoystickFromID(xbox);
    SDL_Gamepad *g = SDL_GetGamepadFromID(xbox);
    dxl_ui_input_begin(&ui);
    dxl_ui_capture_begin(&ui, 0x51a7);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -20000);
    pump();
    CHECK(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) < 16384);
    CHECK_INT(ui.in.captured, DXL_CAPTURED_NONE);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
    pump();
    CHECK_INT(ui.in.captured, DXL_CAPTURED_TRIGGER);
    CHECK_INT(ui.in.captured_code, 1);
    dxl_ui_capture_end(&ui);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
    pump();
    /* Not capturing, a trigger moves nothing. */
    dxl_ui_input_begin(&ui);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 32767);
    pump();
    CHECK_INT(ui.in.captured, DXL_CAPTURED_NONE);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
    pump();

    dxl_ui_input_begin(&ui);
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, 30000);
    pump();
    CHECK_INT(ui.in.nav[DXL_DIR_DOWN], 1);
    ui.pads_off = 1;
    SDL_SetJoystickVirtualAxis(j, SDL_GAMEPAD_AXIS_LEFTY, 0);
    pump();
    double t = clock_s;
    CHECK_INT(downs(t, 1, 100), 0);
    ui.pads_off = 0;
    CHECK_INT(downs(t, 101, 150), 0);
    clock_s = t + 2;
    dxl_ui_input_begin(&ui);
}

/* A list longer than its rect: down past the last row in view scrolls to
 * the next; off its end the focus leaves it. */
static void test_list(void) {
    uint32_t below = dxl_ui_id(&ui, "below");
    int last = -1, focus_below = 0;
    float offset = 0;
    for (int frame = 0; frame < 40; frame++) {
        if (frame > 1) key(SDLK_DOWN, 0);
        dxl_ui_begin(&ui, clock_s += 0.016);
        dxl_list l;
        dxl_list_begin(&ui, &l, "rows", (dxl_rect){ 20, 20, 300, 200 }, 30, 40, 0);
        for (int i = 0; i < 30; i++)
            if (dxl_list_row(&ui, &l, i, "row", NULL, ui.theme.text, 0) & DXL_HIT_FOCUSED) last = i;
        offset = l.offset;
        dxl_list_end(&ui, &l);
        focus_below = dxl_ui_focusable(&ui, below, (dxl_rect){ 20, 240, 300, 40 }, 0) & DXL_HIT_FOCUSED;
        dxl_ui_end(&ui);
        dxl_ui_input_begin(&ui);
        if (frame == 20) {
            CHECK_INT(last, 19);
            CHECK(offset > 0);
        }
    }
    CHECK_INT(last, 29);
    key(SDLK_DOWN, 0);
    dxl_ui_begin(&ui, clock_s += 0.016);
    dxl_list l;
    dxl_list_begin(&ui, &l, "rows", (dxl_rect){ 20, 20, 300, 200 }, 30, 40, 0);
    for (int i = 0; i < 30; i++) dxl_list_row(&ui, &l, i, "row", NULL, ui.theme.text, 0);
    dxl_list_end(&ui, &l);
    focus_below = dxl_ui_focusable(&ui, below, (dxl_rect){ 20, 240, 300, 40 }, 0) & DXL_HIT_FOCUSED;
    dxl_ui_end(&ui);
    dxl_ui_input_begin(&ui);
    CHECK(focus_below);
    CHECK_INT(ui.diag_count, 0);
}

/* A frame of a list of 30 rows of 40 in 200 dp: its offset. */
static float bar_frame(void) {
    dxl_ui_begin(&ui, clock_s += 0.016);
    dxl_list l;
    dxl_list_begin(&ui, &l, "bar rows", (dxl_rect){ 20, 20, 300, 200 }, 30, 40, 0);
    for (int i = 0; i < 30; i++) dxl_list_row(&ui, &l, i, "row", NULL, ui.theme.text, 0);
    float offset = l.offset;
    dxl_list_end(&ui, &l);
    dxl_ui_end(&ui);
    dxl_ui_input_begin(&ui);
    return offset;
}

/* The scroll bar: its thumb dragged to the bottom scrolls to the end; the
 * track clicked above the thumb, a page back. The mouse's back button is
 * Esc's. */
static void test_list_bar(void) {
    float s = ui.scale;
    bar_frame();
    mouse(SDL_EVENT_MOUSE_MOTION, 315 * s, 30 * s);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, 315 * s, 30 * s);
    CHECK(bar_frame() < 1);
    mouse(SDL_EVENT_MOUSE_MOTION, 315 * s, 215 * s);
    CHECK(bar_frame() > 999);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, 315 * s, 215 * s);
    bar_frame();
    mouse(SDL_EVENT_MOUSE_MOTION, 315 * s, 40 * s);
    mouse(SDL_EVENT_MOUSE_BUTTON_DOWN, 315 * s, 40 * s);
    float paged = bar_frame();
    CHECK(paged > 799 && paged < 801);
    mouse(SDL_EVENT_MOUSE_BUTTON_UP, 315 * s, 40 * s);
    bar_frame();
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.button = SDL_BUTTON_X1;
    dxl_ui_event(&ui, &e);
    CHECK_INT(ui.in.back, 1);
    dxl_ui_input_begin(&ui);
}

TEST_MAIN_BEGIN
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    if (!SDL_Init(SDL_INIT_GAMEPAD) || !TTF_Init()) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    offscreen o;
    if (offscreen_open(&o, &ui, 1280, 720, 2) != 0) return 1;
    RUN(test_keyboard);
    RUN(test_mouse);
    RUN(test_pointer_resting);
    RUN(test_pads);
    RUN(test_repeat);
    RUN(test_repeat_after_idle);
    RUN(test_trigger_and_pads_off);
    RUN(test_list);
    RUN(test_list_bar);
    offscreen_close(&o, &ui);
    TTF_Quit();
    SDL_Quit();
TEST_MAIN_END

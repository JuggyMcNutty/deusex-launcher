#include "test.h"
#include "core/keys.h"

static void test_names(void) {
    CHECK_STR(dxl_key_from_scancode(4), "A");
    CHECK_STR(dxl_key_from_scancode(26), "W");
    CHECK_STR(dxl_key_from_scancode(39), "0");
    /* As the original names them: a shift by the name of both sides. */
    CHECK_STR(dxl_key_from_scancode(225), "Shift");
    CHECK_STR(dxl_key_from_scancode(226), "Alt");
    CHECK_STR(dxl_key_from_scancode(57), "CapsLock");
    CHECK_STR(dxl_key_from_scancode(51), "Semicolon");
    CHECK_STR(dxl_key_from_scancode(85), "GreyStar");
    CHECK_STR(dxl_key_from_scancode(88), "Enter");
    CHECK_STR(dxl_key_from_scancode(70), "PrintScrn");
    CHECK_STR(dxl_key_from_scancode(53), "Tilde");
    CHECK_STR(dxl_key_from_scancode(98), "NumPad0");
    CHECK(dxl_key_from_scancode(100) == NULL);   /* no US place: the ISO key is a press's (test_press) */
    CHECK_STR(dxl_key_from_mouse(1), "LeftMouse");
    CHECK_STR(dxl_key_from_mouse(3), "RightMouse");
    CHECK(dxl_key_from_mouse(4) == NULL);
    CHECK_STR(dxl_key_from_wheel(1), "MouseWheelUp");
    CHECK_STR(dxl_key_from_wheel(-1), "MouseWheelDown");
    CHECK_INT(dxl_key_seen("W"), 1);
    CHECK_INT(dxl_key_seen("shift"), 1);
    CHECK_INT(dxl_key_seen("LShift"), 1);   /* the side's own, given as well */
    CHECK_INT(dxl_key_seen("Comma"), 1);
    CHECK_INT(dxl_key_seen("Print"), 0);
    CHECK_INT(dxl_key_seen("Cancel"), 0);
    CHECK_INT(dxl_key_seen("Joy3"), 1);
    CHECK_STR(dxl_key_label("LShift"), "Left Shift");
    CHECK_STR(dxl_key_label("GreyStar"), "Keypad *");
    CHECK_STR(dxl_key_label("W"), "W");
}

static void test_bindings(void) {
    const char *src = "[Engine.Input]\nAliases[0]=(Command=\"Button bFire | Fire\",Alias=Fire)\nW=MoveForward\n"
                      "Up=MoveForward\nS=MoveBackward\nLeftMouse=ParseLeftClick|Fire\nJoy1=Jump\nSpace=Jump\n"
                      "Comma=StrafeLeft\n";
    dxl_ini *user = dxl_ini_parse(src, strlen(src));
    const char *keys[4];
    CHECK_INT(dxl_keys_bound(user, "MoveForward", keys, 4), 2);
    CHECK_STR(keys[0], "W");
    CHECK_STR(keys[1], "Up");
    /* Compared as the engine compares: any case, any spacing. */
    CHECK_INT(dxl_keys_bound(user, "  parseleftclick|fire ", keys, 4), 1);
    CHECK_INT(dxl_keys_bound(user, "Jump", keys, 4), 2);
    dxl_key_bind(user, "W", "");
    CHECK_INT(dxl_keys_bound(user, "MoveForward", keys, 4), 1);
    CHECK_STR(dxl_key_binding(user, "W"), "");
    dxl_key_bind(user, "E", "MoveForward");
    CHECK_INT(dxl_keys_bound(user, "MoveForward", keys, 4), 2);

    /* Back to the defaults: the keyboard's and the mouse's, not the pad's. */
    const char *def = "[Engine.Input]\nW=MoveForward\nS=MoveBackward\nLeftMouse=ParseLeftClick|Fire\nSpace=Jump\n"
                      "Joy1=Duck\n";
    dxl_ini *defuser = dxl_ini_parse(def, strlen(def));
    dxl_keys_reset(user, defuser);
    CHECK_STR(dxl_key_binding(user, "W"), "MoveForward");
    CHECK_STR(dxl_key_binding(user, "E"), "");
    CHECK_STR(dxl_key_binding(user, "Up"), "");
    CHECK_STR(dxl_key_binding(user, "Comma"), "");
    CHECK_STR(dxl_key_binding(user, "Joy1"), "Jump");
    CHECK(strstr(dxl_ini_get(user, "Engine.Input", "Aliases[0]"), "bFire") != NULL);
    dxl_ini_free(defuser);
    dxl_ini_free(user);
}

/* A key bound to two actions is each one's; taken off one, the other's. */
static void test_parts(void) {
    CHECK_INT(dxl_key_binds("Jump|Duck", "Duck"), 1);
    CHECK_INT(dxl_key_binds("Jump | duck", "Duck"), 1);
    CHECK_INT(dxl_key_binds("Jump|Duck", "Jum"), 0);
    CHECK_INT(dxl_key_binds("ParseLeftClick | Fire", "ParseLeftClick|Fire"), 1);
    CHECK_INT(dxl_key_binds("ParseLeftClick", "ParseLeftClick|Fire"), 0);
    CHECK_INT(dxl_key_binds("", "Jump"), 0);
    const char *src = "[Engine.Input]\nSpace=Jump|Duck\nC=Duck\nX=Duck | ShowScores\n";
    dxl_ini *user = dxl_ini_parse(src, strlen(src));
    const char *keys[4];
    CHECK_INT(dxl_keys_bound(user, "Duck", keys, 4), 3);
    CHECK_INT(dxl_keys_bound(user, "Jump", keys, 4), 1);
    /* Taken off by the ini's own name for the key, as dxl_keys_bound gives it. */
    dxl_keys_bound(user, "Duck", keys, 4);
    dxl_key_unbind(user, keys[0], "Duck");
    CHECK_STR(dxl_key_binding(user, "Space"), "Jump");
    dxl_key_unbind(user, "X", "duck");
    CHECK_STR(dxl_key_binding(user, "X"), "ShowScores");
    dxl_key_unbind(user, "C", "Duck");
    CHECK_STR(dxl_key_binding(user, "C"), "");
    dxl_key_unbind(user, "Space", "Fire");
    CHECK_STR(dxl_key_binding(user, "Space"), "Jump");
    dxl_ini_free(user);
}

/* What a few keys type on a layout of each family, as SDL's keymap gives
 * them. */
static unsigned qwerty(int sc, void *d) { (void)d; return sc >= 4 && sc <= 29 ? (unsigned)('a' + sc - 4) : 0; }
static unsigned azerty(int sc, void *d) {
    return sc == 20 ? 'a' : sc == 26 ? 'z' : sc == 4 ? 'q' : sc == 29 ? 'w' : qwerty(sc, d);
}
static unsigned qwertz(int sc, void *d) { return sc == 28 ? 'z' : sc == 29 ? 'y' : qwerty(sc, d); }

/* A key named as the engine's window names it (bugs.md LCORE.6). */
static void test_press(void) {
    CHECK_INT(dxl_key_family_of(qwerty, NULL), DXL_KEYS_QWERTY);
    CHECK_INT(dxl_key_family_of(azerty, NULL), DXL_KEYS_AZERTY);
    CHECK_INT(dxl_key_family_of(qwertz, NULL), DXL_KEYS_QWERTZ);
    /* AZERTY: the key marked Z (the US W's place) Z, the one marked W W. */
    CHECK_STR(dxl_key_from_press(26, 1, DXL_KEYS_AZERTY), "Z");
    CHECK_STR(dxl_key_from_press(29, 1, DXL_KEYS_AZERTY), "W");
    CHECK_STR(dxl_key_from_press(54, 1, DXL_KEYS_AZERTY), "Period");      /* its ; key, Windows' VK_OEM_PERIOD */
    CHECK_STR(dxl_key_from_press(56, 1, DXL_KEYS_AZERTY), "UnknownDF");
    CHECK_STR(dxl_key_from_press(30, 1, DXL_KEYS_AZERTY), "1");
    /* QWERTZ's Y and Z crossed. */
    CHECK_STR(dxl_key_from_press(28, 1, DXL_KEYS_QWERTZ), "Z");
    CHECK_STR(dxl_key_from_press(29, 1, DXL_KEYS_QWERTZ), "Y");
    /* The keypad by Num Lock; its operators either way. */
    CHECK_STR(dxl_key_from_press(96, 1, DXL_KEYS_QWERTY), "NumPad8");
    CHECK_STR(dxl_key_from_press(96, 0, DXL_KEYS_QWERTY), "Up");
    CHECK_STR(dxl_key_from_press(93, 0, DXL_KEYS_QWERTY), "Unknown0C");
    CHECK_STR(dxl_key_from_press(99, 0, DXL_KEYS_QWERTY), "Delete");
    CHECK_STR(dxl_key_from_press(84, 0, DXL_KEYS_QWERTY), "GreySlash");
    /* The ISO key, VK_OEM_102; the ISO #; ABNT's own. */
    CHECK_STR(dxl_key_from_press(100, 1, DXL_KEYS_QWERTY), "JoyZ");
    CHECK_STR(dxl_key_from_press(50, 1, DXL_KEYS_QWERTY), "Backslash");
    CHECK_STR(dxl_key_from_press(100, 1, DXL_KEYS_ABNT), "SingleQuote");
    /* The others by US place whatever the layout. */
    CHECK_STR(dxl_key_from_press(58, 1, DXL_KEYS_AZERTY), "F1");
    CHECK_STR(dxl_key_from_press(225, 0, DXL_KEYS_DVORAK), "Shift");
    CHECK(dxl_key_from_press(0, 1, DXL_KEYS_QWERTY) == NULL);
    /* Windows' codes. */
    CHECK_STR(dxl_key_from_vk(0x57), "W");
    CHECK_STR(dxl_key_from_vk(0xE2), "JoyZ");
    CHECK(dxl_key_from_vk(0) == NULL && dxl_key_from_vk(0xFF) == NULL);
    CHECK_INT(dxl_key_seen("Unknown0C"), 1);
    CHECK_INT(dxl_key_seen("UnknownDF"), 1);
    CHECK_INT(dxl_key_seen("JoyZ"), 1);
}

TEST_MAIN_BEGIN
    RUN(test_names);
    RUN(test_press);
    RUN(test_bindings);
    RUN(test_parts);
TEST_MAIN_END

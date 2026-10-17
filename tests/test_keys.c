#include "test.h"
#include "core/keys.h"

static void test_names(void) {
    CHECK_STR(dxl_key_from_scancode(4), "A");
    CHECK_STR(dxl_key_from_scancode(26), "W");
    CHECK_STR(dxl_key_from_scancode(39), "0");
    CHECK_STR(dxl_key_from_scancode(225), "LShift");
    CHECK_STR(dxl_key_from_scancode(53), "Tilde");
    CHECK_STR(dxl_key_from_scancode(98), "NumPad0");
    /* What the engine's SDL2 window passes on as no key. */
    CHECK(dxl_key_from_scancode(51) == NULL);    /* semicolon */
    CHECK(dxl_key_from_scancode(226) == NULL);   /* left alt */
    CHECK(dxl_key_from_scancode(57) == NULL);    /* caps lock */
    CHECK_STR(dxl_key_from_mouse(1), "LeftMouse");
    CHECK_STR(dxl_key_from_mouse(3), "RightMouse");
    CHECK(dxl_key_from_mouse(4) == NULL);
    CHECK_STR(dxl_key_from_wheel(1), "MouseWheelUp");
    CHECK_STR(dxl_key_from_wheel(-1), "MouseWheelDown");
    CHECK_INT(dxl_key_seen("W"), 1);
    CHECK_INT(dxl_key_seen("shift"), 0);
    CHECK_INT(dxl_key_seen("Comma"), 0);
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

TEST_MAIN_BEGIN
    RUN(test_names);
    RUN(test_bindings);
    RUN(test_parts);
TEST_MAIN_END

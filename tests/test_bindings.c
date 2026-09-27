#include "test.h"
#include "core/bindings.h"
#include "core/ini.h"

static dxl_ini *user_ini(void) {
    char *p = fixture("DefUser.ini");
    dxl_ini *ini = dxl_ini_load(p, NULL);
    free(p);
    return ini;
}

/* An untouched install carries exactly the shipped Joy lines, and must be
 * recognised as such -- that is what triggers the first-contact switch. The
 * fixture carries them too; test_gamefiles checks the real DefUser.ini. */
static void test_shipped_bindings_are_classic(void) {
    dxl_ini *ini = user_ini();
    CHECK(ini != NULL);
    const dxl_pad_preset *p = dxl_bindings_detect(ini);
    CHECK(p != NULL);
    if (p) CHECK_STR(p->id, "classic");
    dxl_ini_free(ini);
}

static void test_apply_then_detect(void) {
    for (size_t i = 0; i < dxl_pad_preset_count(); i++) {
        dxl_ini *ini = user_ini();
        const dxl_pad_preset *want = dxl_pad_preset_at(i);
        dxl_bindings_apply(ini, want);
        const dxl_pad_preset *got = dxl_bindings_detect(ini);
        CHECK(got == want);
        dxl_ini_free(ini);
    }
}

/* Applying clears Joy keys the preset leaves unbound, and nothing else in
 * the file moves. */
static void test_apply_touches_only_joy_keys(void) {
    dxl_ini *ini = user_ini();
    const char *mouse_before = dxl_ini_get(ini, DXL_INPUT_SECTION, "MouseX");
    char *mouse = strdup(mouse_before ? mouse_before : "");

    dxl_bindings_apply(ini, dxl_pad_preset_default());
    CHECK_STR(dxl_ini_get(ini, DXL_INPUT_SECTION, "Joy1"), "Jump");
    CHECK_STR(dxl_ini_get(ini, DXL_INPUT_SECTION, "Joy13"), "");
    CHECK_STR(dxl_ini_get(ini, DXL_INPUT_SECTION, "MouseX"), mouse);
    CHECK_STR(dxl_ini_get(ini, DXL_INPUT_SECTION, "Escape"), "ShowMainMenu");

    /* Back to classic: the modern-only bindings are gone again. */
    dxl_bindings_apply(ini, dxl_pad_preset_at(2));
    CHECK_STR(dxl_ini_get(ini, DXL_INPUT_SECTION, "Joy1"), "Fire");
    CHECK_STR(dxl_ini_get(ini, DXL_INPUT_SECTION, "Joy12"), "");
    CHECK_STR(dxl_ini_get(ini, DXL_INPUT_SECTION, "JoyPovUp"), "");
    free(mouse);
    dxl_ini_free(ini);
}

/* Reapplying the preset already in force leaves the file clean. */
static void test_reapply_is_a_no_op(void) {
    dxl_ini *ini = dxl_ini_parse("", 0);
    dxl_bindings_apply(ini, dxl_pad_preset_default());
    char *once = dxl_ini_render(ini, NULL);
    dxl_ini_free(ini);

    ini = dxl_ini_parse(once, strlen(once));
    dxl_bindings_apply(ini, dxl_pad_preset_default());
    CHECK_INT(dxl_ini_dirty(ini), 0);
    dxl_ini_free(ini);
    free(once);
}

/* The engine matches bindings case-insensitively and ignores spacing, and
 * DefUser.ini itself has "Axis aBaseY  Speed=" with two spaces. */
static void test_detect_ignores_case_and_spacing(void) {
    dxl_ini *ini = user_ini();
    dxl_bindings_apply(ini, dxl_pad_preset_default());
    dxl_ini_set(ini, DXL_INPUT_SECTION, "JoyY", "axis  abasey   speed=3.75");
    dxl_ini_set(ini, DXL_INPUT_SECTION, "Joy12", "ParseLeftClick | Fire");
    const dxl_pad_preset *p = dxl_bindings_detect(ini);
    CHECK(p == dxl_pad_preset_default());

    /* A real difference is a custom layout. */
    dxl_ini_set(ini, DXL_INPUT_SECTION, "Joy1", "Duck");
    CHECK(dxl_bindings_detect(ini) == NULL);
    dxl_ini_free(ini);
}

static void test_descriptions(void) {
    char out[64];
    dxl_binding_describe("ParseLeftClick|Fire", out, sizeof out);
    CHECK_STR(out, "Fire weapon / use object in hand");
    dxl_binding_describe("ParseLeftClick | fire", out, sizeof out);
    CHECK_STR(out, "Fire weapon / use object in hand");
    dxl_binding_describe("Fire", out, sizeof out);
    CHECK_STR(out, "Fire weapon");
    dxl_binding_describe("ActivateAugmentation 0", out, sizeof out);
    CHECK_STR(out, "Augmentation F3");
    dxl_binding_describe("Axis aBaseY Speed=3.75", out, sizeof out);
    CHECK_STR(out, "Move forward / back");
    dxl_binding_describe("axis astrafe speed=2", out, sizeof out);
    CHECK_STR(out, "Strafe");
    dxl_binding_describe("", out, sizeof out);
    CHECK_STR(out, "(nothing)");
    dxl_binding_describe("SomethingNew", out, sizeof out);
    CHECK_STR(out, "SomethingNew");
    CHECK_STR(dxl_joy_key_label("Joy12"), "R2");
    CHECK_STR(dxl_joy_key_label("joypovleft"), "D-pad left");
}


/* A player who got the previous Modern layout is recognised and moved to its
 * replacement, rather than being shown a "custom" layout they never made. */
static void test_retired_layout_is_recognised(void) {
    dxl_ini *ini = user_ini();
    dxl_bindings_apply(ini, dxl_pad_preset_default());
    dxl_ini_set(ini, DXL_INPUT_SECTION, "Joy7", "ShowAugmentationsWindow");
    CHECK(dxl_bindings_detect(ini) == NULL);
    const char *why = NULL;
    CHECK(dxl_bindings_detect_retired(ini, &why) == dxl_pad_preset_default());
    CHECK(why != NULL && strstr(why, "SELECT") != NULL);
    /* A genuinely custom layout is not mistaken for a retired one. */
    dxl_ini_set(ini, DXL_INPUT_SECTION, "Joy1", "QuickSave");
    CHECK(dxl_bindings_detect_retired(ini, NULL) == NULL);
    dxl_ini_free(ini);
}

/* Every preset binding is either in the catalogue or a stick axis: a preset
 * that binds a command the picker cannot show would be un-editable. */
static void test_presets_use_catalogue_commands(void) {
    for (size_t p = 0; p < dxl_pad_preset_count(); p++) {
        const dxl_pad_preset *pr = dxl_pad_preset_at(p);
        if (strcmp(pr->id, "classic") == 0) continue;   /* the shipped file, verbatim */
        for (size_t b = 0; b < dxl_pad_button_count(); b++) {
            const char *cmd = dxl_pad_preset_binding(pr, dxl_pad_button_at(b));
            if (dxl_pad_action_find(cmd) < 0)
                fprintf(stderr, "  %s %s -> %s not in catalogue\n", pr->id,
                        dxl_pad_button_at(b), cmd);
            CHECK(dxl_pad_action_find(cmd) >= 0);
        }
    }
}

/* Remapping one button leaves the rest alone and makes the layout custom;
 * putting it back makes it the preset again. */
static void test_remap_one_button(void) {
    dxl_ini *ini = user_ini();
    const dxl_pad_preset *m = dxl_pad_preset_default();
    dxl_bindings_apply(ini, m);
    dxl_bindings_set(ini, "Joy7", "QuickSave");
    CHECK_STR(dxl_ini_get(ini, DXL_INPUT_SECTION, "Joy7"), "QuickSave");
    CHECK_STR(dxl_ini_get(ini, DXL_INPUT_SECTION, "Joy8"), "ShowMainMenu");
    CHECK(dxl_bindings_detect(ini) == NULL);
    dxl_bindings_set(ini, "Joy7", dxl_pad_preset_binding(m, "Joy7"));
    CHECK(dxl_bindings_detect(ini) == m);
    dxl_ini_free(ini);
}

/* This device has no L3/R3: they are not offered for remapping. */
static void test_no_stick_clicks_offered(void) {
    for (size_t b = 0; b < dxl_pad_button_count(); b++) {
        CHECK(strcmp(dxl_pad_button_at(b), "Joy9") != 0);
        CHECK(strcmp(dxl_pad_button_at(b), "Joy10") != 0);
    }
    CHECK(dxl_pad_action_find("") == 0);
    CHECK(dxl_pad_action_find("NoSuchThing") == -1);
}

TEST_MAIN_BEGIN
    RUN(test_shipped_bindings_are_classic);
    RUN(test_apply_then_detect);
    RUN(test_apply_touches_only_joy_keys);
    RUN(test_reapply_is_a_no_op);
    RUN(test_detect_ignores_case_and_spacing);
    RUN(test_descriptions);
    RUN(test_retired_layout_is_recognised);
    RUN(test_presets_use_catalogue_commands);
    RUN(test_remap_one_button);
    RUN(test_no_stick_clicks_offered);
TEST_MAIN_END

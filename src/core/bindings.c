#include "core/bindings.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

const char *const dxl_joy_keys[DXL_JOY_KEY_COUNT] = {
    "Joy1", "Joy2", "Joy3", "Joy4", "Joy5", "Joy6", "Joy7", "Joy8",
    "Joy9", "Joy10", "Joy11", "Joy12", "Joy13", "Joy14", "Joy15", "Joy16",
    "JoyX", "JoyY", "JoyZ", "JoyR", "JoyU", "JoyV",
    "JoyPovUp", "JoyPovDown", "JoyPovLeft", "JoyPovRight",
};

/* Joy numbering follows the XInput order UE1 ports conventionally use
 * (A=Joy1 ... R3=Joy10), with the triggers as Joy11/Joy12 when pressed past
 * half travel. The fork's SDL2 backend produces exactly these. */
static const struct { const char *key, *label; } physical[] = {
    { "Joy1", "A" },            { "Joy2", "B" },
    { "Joy3", "X" },            { "Joy4", "Y" },
    { "Joy5", "L1" },           { "Joy6", "R1" },
    { "Joy7", "SELECT" },       { "Joy8", "START" },
    { "Joy9", "L3" },           { "Joy10", "R3" },
    { "Joy11", "L2" },          { "Joy12", "R2" },
    { "JoyX", "Left stick left/right" },  { "JoyY", "Left stick up/down" },
    { "JoyU", "Right stick left/right" }, { "JoyV", "Right stick up/down" },
    { "JoyZ", "L2 (analog)" },  { "JoyR", "R2 (analog)" },
    { "JoyPovUp", "D-pad up" }, { "JoyPovDown", "D-pad down" },
    { "JoyPovLeft", "D-pad left" }, { "JoyPovRight", "D-pad right" },
};

const char *dxl_joy_key_label(const char *key) {
    for (size_t i = 0; i < sizeof physical / sizeof *physical; i++)
        if (dxl_stricmp(physical[i].key, key) == 0) return physical[i].label;
    return key;
}

/* ---- presets --------------------------------------------------------- */

/* The Smart Pro has digital L2/R2 and no stick clicks (no L3/R3), so the
 * layout uses A/B/X/Y, L1/R1, L2/R2, SELECT, START and the d-pad. Every
 * command is one the game's own key menu offers (MenuScreenCustomizeKeys in
 * DeusEx.u). Walk/run is left out: a half-pushed stick already walks. */
static const dxl_binding modern[] = {
    { "Joy1",  "Jump" },
    { "Joy2",  "Duck" },
    { "Joy3",  "ReloadWeapon" },
    { "Joy4",  "ShowInventoryWindow" },
    { "Joy5",  "PrevBeltItem" },
    { "Joy6",  "NextBeltItem" },
    { "Joy7",  "ShowMainMenu" },
    { "Joy8",  "ShowMainMenu" },
    { "Joy11", "ParseRightClick" },
    { "Joy12", "ParseLeftClick|Fire" },
    { "JoyX",  "Axis aStrafe Speed=3.75" },
    { "JoyY",  "Axis aBaseY Speed=3.75" },
    { "JoyU",  "Axis aBaseX Speed=3.75" },
    { "JoyV",  "Axis aLookUp Speed=2.25" },
    { "JoyPovUp",    "ToggleScope" },
    { "JoyPovDown",  "SwitchAmmo" },
    { "JoyPovLeft",  "LeanLeft" },
    { "JoyPovRight", "LeanRight" },
};

/* Modern with the sticks swapped: look on the left, move on the right. */
static const dxl_binding southpaw[] = {
    { "Joy1",  "Jump" },
    { "Joy2",  "Duck" },
    { "Joy3",  "ReloadWeapon" },
    { "Joy4",  "ShowInventoryWindow" },
    { "Joy5",  "PrevBeltItem" },
    { "Joy6",  "NextBeltItem" },
    { "Joy7",  "ShowMainMenu" },
    { "Joy8",  "ShowMainMenu" },
    { "Joy11", "ParseRightClick" },
    { "Joy12", "ParseLeftClick|Fire" },
    { "JoyX",  "Axis aBaseX Speed=3.75" },
    { "JoyY",  "Axis aLookUp Speed=2.25" },
    { "JoyU",  "Axis aStrafe Speed=3.75" },
    { "JoyV",  "Axis aBaseY Speed=3.75" },
    { "JoyPovUp",    "ToggleScope" },
    { "JoyPovDown",  "SwitchAmmo" },
    { "JoyPovLeft",  "LeanLeft" },
    { "JoyPovRight", "LeanRight" },
};

/* Retired layouts: what earlier launchers applied, so players who have one
 * are moved to its replacement rather than stranded on "custom".
 *
 * v2 (2026-09-22): SELECT opened the augmentations screen, which Y's
 * inventory already reaches as a tab; SELECT became the pause menu. */
static const dxl_binding modern_v2[] = {
    { "Joy1", "Jump" }, { "Joy2", "Duck" }, { "Joy3", "ReloadWeapon" },
    { "Joy4", "ShowInventoryWindow" }, { "Joy5", "PrevBeltItem" }, { "Joy6", "NextBeltItem" },
    { "Joy7", "ShowAugmentationsWindow" }, { "Joy8", "ShowMainMenu" },
    { "Joy11", "ParseRightClick" }, { "Joy12", "ParseLeftClick|Fire" },
    { "JoyX", "Axis aStrafe Speed=3.75" }, { "JoyY", "Axis aBaseY Speed=3.75" },
    { "JoyU", "Axis aBaseX Speed=3.75" }, { "JoyV", "Axis aLookUp Speed=2.25" },
    { "JoyPovUp", "ToggleScope" }, { "JoyPovDown", "SwitchAmmo" },
    { "JoyPovLeft", "LeanLeft" }, { "JoyPovRight", "LeanRight" },
};
static const dxl_binding southpaw_v2[] = {
    { "Joy1", "Jump" }, { "Joy2", "Duck" }, { "Joy3", "ReloadWeapon" },
    { "Joy4", "ShowInventoryWindow" }, { "Joy5", "PrevBeltItem" }, { "Joy6", "NextBeltItem" },
    { "Joy7", "ShowAugmentationsWindow" }, { "Joy8", "ShowMainMenu" },
    { "Joy11", "ParseRightClick" }, { "Joy12", "ParseLeftClick|Fire" },
    { "JoyX", "Axis aBaseX Speed=3.75" }, { "JoyY", "Axis aLookUp Speed=2.25" },
    { "JoyU", "Axis aStrafe Speed=3.75" }, { "JoyV", "Axis aBaseY Speed=3.75" },
    { "JoyPovUp", "ToggleScope" }, { "JoyPovDown", "SwitchAmmo" },
    { "JoyPovLeft", "LeanLeft" }, { "JoyPovRight", "LeanRight" },
};

/* Exactly what DefUser.ini ships. Kept so an untouched install is recognised
 * rather than reported as "Custom", and so it can be restored. */
static const dxl_binding classic[] = {
    { "Joy1", "Fire" },
    { "Joy2", "Jump" },
    { "Joy4", "Duck" },
    { "JoyX", "Axis astrafe speed=2" },
    { "JoyY", "Axis aBaseY speed=2" },
    { "JoyU", "Axis aturn speed=5.9" },
    { "JoyV", "Axis aLookUp speed=-3" },
};

#define N(a) (sizeof a / sizeof *a)
static const dxl_pad_preset presets[] = {
    { "modern", "Modern",
      "Left stick moves, right stick looks. R2 fires, L2 uses and picks up. "
      "A jumps, B crouches, X reloads, Y opens the inventory. SELECT or START "
      "pauses (save, load). L1/R1 cycle the belt. D-pad: up scope, down ammo, "
      "left/right lean.",
      modern, N(modern) },
    { "southpaw", "Modern, sticks swapped",
      "As Modern, but the left stick looks and the right stick moves.",
      southpaw, N(southpaw) },
    { "classic", "Original Deus Ex",
      "The joystick bindings Deus Ex shipped with: fire, jump and crouch on "
      "three buttons, nothing else. Movement is slower than on the keyboard.",
      classic, N(classic) },
};

size_t dxl_pad_preset_count(void) { return N(presets); }
const dxl_pad_preset *dxl_pad_preset_at(size_t i) {
    return i < N(presets) ? &presets[i] : NULL;
}
const dxl_pad_preset *dxl_pad_preset_default(void) { return &presets[0]; }

static const char *preset_value(const dxl_pad_preset *p, const char *key) {
    for (size_t i = 0; i < p->count; i++)
        if (dxl_stricmp(p->bindings[i].key, key) == 0) return p->bindings[i].command;
    return "";
}

void dxl_bindings_apply(dxl_ini *ini, const dxl_pad_preset *p) {
    for (size_t k = 0; k < DXL_JOY_KEY_COUNT; k++) {
        const char *want = preset_value(p, dxl_joy_keys[k]);
        const char *have = dxl_ini_get(ini, DXL_INPUT_SECTION, dxl_joy_keys[k]);
        /* Leave identical lines alone so an unchanged layout does not dirty
         * the file (and does not rewrite its line endings). */
        if (have && strcmp(have, want) == 0) continue;
        dxl_ini_set(ini, DXL_INPUT_SECTION, dxl_joy_keys[k], want);
    }
}

/* The canonical spelling of a binding, as the engine reads it: lower case,
 * whitespace runs collapsed to one space, none around "|", none at the ends.
 * "Axis aBaseY  Speed=+300.0" and "axis abasey speed=+300.0" are the same
 * binding, and DefUser.ini really does contain the double space. */
static void canonical(const char *s, char *out, size_t n) {
    size_t o = 0;
    int pending_space = 0;
    for (s = s ? s : ""; *s && o + 2 < n; s++) {
        if (*s == ' ' || *s == '\t') { pending_space = (o > 0); continue; }
        if (*s == '|') pending_space = 0;
        else if (pending_space && out[o - 1] != '|') out[o++] = ' ';
        pending_space = 0;
        out[o++] = (char)tolower((unsigned char)*s);
    }
    out[o] = '\0';
}

static int same_binding(const char *a, const char *b) {
    char ca[256], cb[256];
    canonical(a, ca, sizeof ca);
    canonical(b, cb, sizeof cb);
    return strcmp(ca, cb) == 0;
}

const dxl_pad_preset *dxl_bindings_detect(const dxl_ini *ini) {
    for (size_t i = 0; i < N(presets); i++) {
        int match = 1;
        for (size_t k = 0; k < DXL_JOY_KEY_COUNT && match; k++) {
            const char *have = dxl_ini_get(ini, DXL_INPUT_SECTION, dxl_joy_keys[k]);
            match = same_binding(have, preset_value(&presets[i], dxl_joy_keys[k]));
        }
        if (match) return &presets[i];
    }
    return NULL;
}

static const char *const v2_change =
    "SELECT now opens the pause menu (save, load). The augmentations screen is a "
    "tab of the inventory (Y).";

static const struct {
    dxl_pad_preset old;
    size_t replaced_by;          /* index into presets[] */
    const char *what_changed;    /* shown to the player once */
} retired[] = {
    { { "modern-v2", "Modern (2026-09-22)", "", modern_v2, N(modern_v2) }, 0, v2_change },
    { { "southpaw-v2", "Modern, sticks swapped (2026-09-22)", "", southpaw_v2, N(southpaw_v2) }, 1, v2_change },
};

const dxl_pad_preset *dxl_bindings_detect_retired(const dxl_ini *ini, const char **what_changed) {
    for (size_t i = 0; i < N(retired); i++) {
        int match = 1;
        for (size_t k = 0; k < DXL_JOY_KEY_COUNT && match; k++) {
            const char *have = dxl_ini_get(ini, DXL_INPUT_SECTION, dxl_joy_keys[k]);
            match = same_binding(have, preset_value(&retired[i].old, dxl_joy_keys[k]));
        }
        if (match) {
            if (what_changed) *what_changed = retired[i].what_changed;
            return &presets[retired[i].replaced_by];
        }
    }
    return NULL;
}

const char *dxl_pad_preset_binding(const dxl_pad_preset *p, const char *joy_key) {
    return preset_value(p, joy_key);
}

void dxl_bindings_set(dxl_ini *ini, const char *joy_key, const char *command) {
    const char *have = dxl_ini_get(ini, DXL_INPUT_SECTION, joy_key);
    if (have && strcmp(have, command ? command : "") == 0) return;
    dxl_ini_set(ini, DXL_INPUT_SECTION, joy_key, command ? command : "");
}

/* ---- remapping --------------------------------------------------------- */

/* The Smart Pro's buttons (ports/trimui-smartpro/README.md, "Pad controls"): no L3/R3, so Joy9
 * and Joy10 are never produced and are not offered. */
static const char *const remappable[] = {
    "Joy1", "Joy2", "Joy3", "Joy4", "Joy5", "Joy6", "Joy11", "Joy12",
    "Joy7", "Joy8", "JoyPovUp", "JoyPovDown", "JoyPovLeft", "JoyPovRight",
};

size_t dxl_pad_button_count(void) { return N(remappable); }
const char *dxl_pad_button_at(size_t i) { return i < N(remappable) ? remappable[i] : NULL; }

/* The labels are the game's own (DeusEx.int, [MenuScreenCustomizeKeys]
 * FunctionText) where it has one; the commands are the ones that menu binds
 * (its MenuValues in DeusEx.u), in the same order within each group. */
static const dxl_pad_action actions[] = {
    { "",                         "Nothing",                              "" },

    { "ShowMainMenu",             "Pause menu (save, load, options)",     "Menus" },
    { "ShowInventoryWindow",      "Inventory screen",                     "Menus" },
    { "ShowHealthWindow",         "Health screen",                        "Menus" },
    { "ShowAugmentationsWindow",  "Augmentations screen",                 "Menus" },
    { "ShowSkillsWindow",         "Skills screen",                        "Menus" },
    { "ShowGoalsWindow",          "Goals/Notes screen",                   "Menus" },
    { "ShowConversationsWindow",  "Conversations screen",                 "Menus" },
    { "ShowImagesWindow",         "Images screen",                        "Menus" },
    { "ShowLogsWindow",           "Logs screen",                          "Menus" },
    { "QuickSave",                "Quick Save",                           "Menus" },
    { "QuickLoad",                "Quick Load",                           "Menus" },

    { "ParseLeftClick|Fire",      "Fire weapon / use object in hand",     "Combat" },
    { "ParseRightClick",          "Use object in world",                  "Combat" },
    { "ReloadWeapon",             "Reload weapon",                        "Combat" },
    { "SwitchAmmo",               "Change ammo",                          "Combat" },
    { "ToggleScope",              "Toggle scope",                         "Combat" },
    { "ToggleLaser",              "Toggle laser sight",                   "Combat" },
    { "DropItem",                 "Drop / throw item",                    "Combat" },
    { "PutInHand",                "Put away item",                        "Combat" },

    { "Jump",                     "Jump",                                 "Movement" },
    { "Duck",                     "Crouch",                               "Movement" },
    { "LeanLeft",                 "Lean left",                            "Movement" },
    { "LeanRight",                "Lean right",                           "Movement" },
    { "Walking",                  "Walk (hold)",                          "Movement" },
    { "ToggleWalk",               "Toggle walk/run",                      "Movement" },
    { "MoveForward",              "Move forward",                         "Movement" },
    { "MoveBackward",             "Move backward",                        "Movement" },
    { "StrafeLeft",               "Strafe left",                          "Movement" },
    { "StrafeRight",              "Strafe right",                         "Movement" },
    { "TurnLeft",                 "Turn left",                            "Movement" },
    { "TurnRight",                "Turn right",                           "Movement" },
    { "LookUp",                   "Look up",                              "Movement" },
    { "LookDown",                 "Look down",                            "Movement" },
    { "CenterView",               "Center view",                          "Movement" },

    { "NextBeltItem",             "Next belt item",                       "Belt" },
    { "PrevBeltItem",             "Previous belt item",                   "Belt" },
    { "ActivateBelt 1",           "Belt slot 1",                          "Belt" },
    { "ActivateBelt 2",           "Belt slot 2",                          "Belt" },
    { "ActivateBelt 3",           "Belt slot 3",                          "Belt" },
    { "ActivateBelt 4",           "Belt slot 4",                          "Belt" },
    { "ActivateBelt 5",           "Belt slot 5",                          "Belt" },
    { "ActivateBelt 6",           "Belt slot 6",                          "Belt" },
    { "ActivateBelt 7",           "Belt slot 7",                          "Belt" },
    { "ActivateBelt 8",           "Belt slot 8",                          "Belt" },
    { "ActivateBelt 9",           "Belt slot 9",                          "Belt" },
    { "ActivateBelt 0",           "Belt slot 0",                          "Belt" },

    { "ActivateAllAugs",          "Activate all augmentations",           "Augmentations" },
    { "DeactivateAllAugs",        "Deactivate all augmentations",         "Augmentations" },
    { "ActivateAugmentation 0",   "Augmentation F3",                      "Augmentations" },
    { "ActivateAugmentation 1",   "Augmentation F4",                      "Augmentations" },
    { "ActivateAugmentation 2",   "Augmentation F5",                      "Augmentations" },
    { "ActivateAugmentation 3",   "Augmentation F6",                      "Augmentations" },
    { "ActivateAugmentation 4",   "Augmentation F7",                      "Augmentations" },
    { "ActivateAugmentation 5",   "Augmentation F8",                      "Augmentations" },
    { "ActivateAugmentation 6",   "Augmentation F9",                      "Augmentations" },
    { "ActivateAugmentation 7",   "Augmentation F10",                     "Augmentations" },
    { "ActivateAugmentation 8",   "Augmentation F11",                     "Augmentations" },
    { "ActivateAugmentation 9",   "Augmentation F12",                     "Augmentations" },

    { "ToggleCrosshair",          "Toggle crosshairs",                    "Display" },
    { "ToggleHitDisplay",         "Toggle hit display",                   "Display" },
    { "ToggleCompass",            "Toggle compass",                       "Display" },
    { "ToggleAugDisplay",         "Toggle augmentation display",          "Display" },
    { "ToggleObjectBelt",         "Toggle object belt",                   "Display" },
    { "ToggleAmmoDisplay",        "Toggle ammo display",                  "Display" },
    { "Shot",                     "Take screenshot",                      "Display" },
};

size_t dxl_pad_action_count(void) { return N(actions); }
const dxl_pad_action *dxl_pad_action_at(size_t i) { return i < N(actions) ? &actions[i] : NULL; }

int dxl_pad_action_find(const char *command) {
    for (size_t i = 0; i < N(actions); i++)
        if (same_binding(command, actions[i].command)) return (int)i;
    return -1;
}

/* ---- human names ----------------------------------------------------- */

/* Names for what the catalogue does not hold: the stick axes (which carry a
 * speed, so they match on the axis alone) and the shipped layout's plain
 * "Fire". */
static const struct { const char *command, *name; } extra_names[] = {
    { "Fire",         "Fire weapon" },
    { "Axis aStrafe", "Strafe" },
    { "Axis aBaseY",  "Move forward / back" },
    { "Axis aBaseX",  "Turn" },
    { "Axis aTurn",   "Turn" },
    { "Axis aLookUp", "Look up / down" },
};

void dxl_binding_describe(const char *command, char *out, size_t n) {
    if (!command || !*command) { snprintf(out, n, "(nothing)"); return; }
    int at = dxl_pad_action_find(command);
    if (at >= 0) { snprintf(out, n, "%s", actions[at].label); return; }
    for (size_t i = 0; i < N(extra_names); i++) {
        size_t len = strlen(extra_names[i].command);
        int axis = strncmp(extra_names[i].command, "Axis ", 5) == 0;
        if (axis ? (dxl_strnicmp(command, extra_names[i].command, len) == 0 &&
                    (command[len] == ' ' || command[len] == '\0'))
                 : same_binding(command, extra_names[i].command)) {
            snprintf(out, n, "%s", extra_names[i].name);
            return;
        }
    }
    snprintf(out, n, "%s", command);
}

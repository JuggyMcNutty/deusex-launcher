/* Controller layouts: the Joy* lines of User.ini [Engine.Input].
 *
 * The engine already resolves Joy1..Joy16, JoyX/Y/Z/R/U/V and JoyPov* through
 * the same binding table as the keyboard (Engine::InputEvent -> InputCommand),
 * so a layout is nothing more than those 26 lines. The fork's gamepad support
 * (engine patch 0003) is what makes the pad produce them.
 *
 * Presets are data. Applying one rewrites every Joy key -- including the ones
 * it leaves empty, so nothing from a previous layout lingers -- and touches
 * no other line of User.ini.
 *
 * Axis speeds assume the fork's convention: a stick at full deflection sends
 * 100, the engine multiplies axis input by 16, and InputCommand multiplies by
 * Speed. Keyboard run speed is MoveForward's 300 x 20 = 6000, so
 * Speed=3.75 gives the same top speed on the stick.
 */
#ifndef DXL_BINDINGS_H
#define DXL_BINDINGS_H

#include "core/ini.h"

#define DXL_JOY_KEY_COUNT 26

/* The key names, in the engine's keynames[] spelling. */
extern const char *const dxl_joy_keys[DXL_JOY_KEY_COUNT];

typedef struct {
    const char *key;       /* "Joy1", "JoyX", ... */
    const char *command;   /* the binding, e.g. "Jump" or "Axis aBaseY Speed=3.75" */
} dxl_binding;

typedef struct {
    const char *id;            /* stable, for logs and --dry-run */
    const char *label;
    const char *description;
    const dxl_binding *bindings;   /* keys not listed are cleared */
    size_t count;
} dxl_pad_preset;

size_t dxl_pad_preset_count(void);
const dxl_pad_preset *dxl_pad_preset_at(size_t i);
/* The default layout for a fresh install. */
const dxl_pad_preset *dxl_pad_preset_default(void);

#define DXL_INPUT_SECTION "Engine.Input"

/* Rewrites every Joy key in [Engine.Input] to the preset. */
void dxl_bindings_apply(dxl_ini *user_ini, const dxl_pad_preset *p);

/* The preset the ini currently matches, or NULL for a custom layout.
 * Comparison ignores case and runs of whitespace, as the engine's own
 * parsing does. */
const dxl_pad_preset *dxl_bindings_detect(const dxl_ini *user_ini);

/* A layout that shipped in an earlier launcher and was since revised: if the
 * ini matches one, the preset that replaced it, else NULL. Lets a revised
 * preset reach players who already had the old one, instead of leaving them
 * on a "custom" layout they never made. */
const dxl_pad_preset *dxl_bindings_detect_retired(const dxl_ini *user_ini,
                                                  const char **what_changed);

/* What the physical control is called on the handheld, for a Joy key:
 * "A", "L1", "Left stick (up/down)". */
const char *dxl_joy_key_label(const char *key);

/* ---- per-button remapping ------------------------------------------- */

/* The buttons a player can remap on this device, in display order: the face
 * buttons, shoulders, triggers, SELECT/START and the d-pad. The sticks are
 * chosen by preset (moves/looks, or swapped), not per axis. */
size_t      dxl_pad_button_count(void);
const char *dxl_pad_button_at(size_t i);   /* a Joy key name */

/* Everything a button can be bound to: the game's own key-binding menu
 * (MenuScreenCustomizeKeys, with its wording), plus the pause menu, the belt
 * slots and the augmentation hotkeys, which that menu files elsewhere. */
typedef struct {
    const char *command;   /* the User.ini binding; "" for nothing */
    const char *label;
    const char *group;     /* for grouping the picker */
} dxl_pad_action;

size_t dxl_pad_action_count(void);
const dxl_pad_action *dxl_pad_action_at(size_t i);
/* Index of the action a binding performs, or -1 when it is not one of them
 * (hand-written bindings). Compared the way the engine reads bindings. */
int dxl_pad_action_find(const char *command);

/* Binds one Joy key. */
void dxl_bindings_set(dxl_ini *user_ini, const char *joy_key, const char *command);
/* What a preset binds a key to ("" for nothing). */
const char *dxl_pad_preset_binding(const dxl_pad_preset *p, const char *joy_key);

/* A readable name for a binding: "Fire", "Use / pick up", "Move forward /
 * back". Falls back to the command itself. Writes into out. */
void dxl_binding_describe(const char *command, char *out, size_t n);

#endif

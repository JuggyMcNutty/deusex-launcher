/* Keys and their bindings: User.ini [Engine.Input], key=command.
 *
 * A key is named as the engine names it (its keynames[], the original's
 * EInputKey without IK_). Which physical key gives which name is the
 * engine's window's to say: the launcher's capture names a key as
 * SurrealWidgets' SDL2 window does (its ScancodeToInputKey and
 * GetMouseButtonKey), the window the launcher starts the engine with,
 * which names keys as the original does: a shift, control or alt by the
 * name of both sides (Shift, Ctrl, Alt: a capture's), then by its side's
 * own (LShift). A name it never gives (Print, Cancel) reaches nothing:
 * dxl_key_seen says which.
 *
 * Scancodes are USB HID usage numbers, SDL's own; core takes no SDL.
 */
#ifndef DXL_KEYS_H
#define DXL_KEYS_H

#include "core/ini.h"

/* The engine's name for the key SDL calls scancode, or NULL for one its
 * window passes on as no key. */
const char *dxl_key_from_scancode(int scancode);
/* SDL's mouse buttons (1 left, 2 middle, 3 right): the same. */
const char *dxl_key_from_mouse(int button);
/* A wheel notch, up positive. */
const char *dxl_key_from_wheel(int up);
/* Whether the engine's window gives the key of that name at all. */
int dxl_key_seen(const char *name);
/* The name as the keyboard page shows it: "Left Shift", "Keypad 0", "Left
 * mouse button". The name itself when it has no other. */
const char *dxl_key_label(const char *name);

/* Whether a binding runs command: one of its |-separated commands is it
 * ("Jump|Duck" runs Duck), or, for a command of several, the binding is,
 * part for part; compared as the engine reads bindings, any case, any
 * spacing. */
int  dxl_key_binds(const char *binding, const char *command);
/* The keys whose bindings run command (dxl_key_binds), in User.ini's
 * order: at most max into keys, the count returned. The names are the
 * ini's, valid until it changes. */
int  dxl_keys_bound(const dxl_ini *user, const char *command, const char **keys, int max);
/* The key's binding, "" for none. */
const char *dxl_key_binding(const dxl_ini *user, const char *key);
/* key=command; command "" unbinds it. */
void dxl_key_bind(dxl_ini *user, const char *key, const char *command);
/* command taken off key: the binding's part that runs it, or the binding
 * when that is all it is; the key's other commands kept. */
void dxl_key_unbind(dxl_ini *user, const char *key, const char *command);
/* Every [Engine.Input] key as DefUser.ini has it. */
void dxl_keys_reset(dxl_ini *user, const dxl_ini *defuser);

#endif

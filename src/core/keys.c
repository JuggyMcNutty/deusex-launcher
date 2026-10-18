#define _GNU_SOURCE
#include "core/keys.h"
#include "core/common.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define INPUT "Engine.Input"

/* SurrealWidgets' SDL3 window, ScancodeToInputKey (SDL2's alike): the
 * scancodes it passes on, and the name each is in the engine. */
static const struct {
    int scancode;
    const char *name;
} scancodes[] = {
    { 42, "Backspace" }, { 43, "Tab" }, { 156, "OEMClear" }, { 40, "Enter" }, { 88, "Enter" },
    { 225, "Shift" }, { 229, "Shift" }, { 224, "Ctrl" }, { 228, "Ctrl" }, { 226, "Alt" }, { 230, "Alt" },
    { 57, "CapsLock" }, { 227, "Unknown5B" }, { 231, "Unknown5C" }, { 101, "Unknown5D" }, { 118, "Unknown5D" },
    { 72, "Pause" }, { 41, "Escape" }, { 44, "Space" }, { 77, "End" }, { 78, "PageDown" },
    { 74, "Home" }, { 75, "PageUp" }, { 80, "Left" }, { 82, "Up" }, { 79, "Right" },
    { 81, "Down" }, { 119, "Select" }, { 70, "PrintScrn" }, { 116, "Execute" }, { 73, "Insert" },
    { 76, "Delete" }, { 117, "Help" },
    { 39, "0" }, { 30, "1" }, { 31, "2" }, { 32, "3" }, { 33, "4" },
    { 34, "5" }, { 35, "6" }, { 36, "7" }, { 37, "8" }, { 38, "9" },
    { 4, "A" }, { 5, "B" }, { 6, "C" }, { 7, "D" }, { 8, "E" }, { 9, "F" }, { 10, "G" },
    { 11, "H" }, { 12, "I" }, { 13, "J" }, { 14, "K" }, { 15, "L" }, { 16, "M" }, { 17, "N" },
    { 18, "O" }, { 19, "P" }, { 20, "Q" }, { 21, "R" }, { 22, "S" }, { 23, "T" }, { 24, "U" },
    { 25, "V" }, { 26, "W" }, { 27, "X" }, { 28, "Y" }, { 29, "Z" },
    { 98, "NumPad0" }, { 89, "NumPad1" }, { 90, "NumPad2" }, { 91, "NumPad3" }, { 92, "NumPad4" },
    { 93, "NumPad5" }, { 94, "NumPad6" }, { 95, "NumPad7" }, { 96, "NumPad8" }, { 97, "NumPad9" },
    { 85, "GreyStar" }, { 87, "GreyPlus" }, { 159, "Separator" }, { 86, "GreyMinus" }, { 99, "NumPadPeriod" },
    { 84, "GreySlash" },
    { 58, "F1" }, { 59, "F2" }, { 60, "F3" }, { 61, "F4" }, { 62, "F5" }, { 63, "F6" },
    { 64, "F7" }, { 65, "F8" }, { 66, "F9" }, { 67, "F10" }, { 68, "F11" }, { 69, "F12" },
    { 104, "F13" }, { 105, "F14" }, { 106, "F15" }, { 107, "F16" }, { 108, "F17" }, { 109, "F18" },
    { 110, "F19" }, { 111, "F20" }, { 112, "F21" }, { 113, "F22" }, { 114, "F23" }, { 115, "F24" },
    { 83, "NumLock" }, { 71, "ScrollLock" },
    { 51, "Semicolon" }, { 46, "Equals" }, { 54, "Comma" }, { 45, "Minus" }, { 55, "Period" }, { 56, "Slash" },
    { 53, "Tilde" }, { 47, "LeftBracket" }, { 49, "Backslash" }, { 48, "RightBracket" }, { 52, "SingleQuote" },
};

/* ScancodeToSideKey's: the side's own name of a shift, control or alt,
 * which the window gives after the one of both. */
static const char *const sides[] = { "LShift", "RShift", "LControl", "RControl", "UnknownA4", "UnknownA5" };

/* GetMouseButtonKey's three, and the wheel's two. */
static const char *const mouse[] = { NULL, "LeftMouse", "MiddleMouse", "RightMouse" };

const char *dxl_key_from_scancode(int scancode) {
    for (size_t i = 0; i < sizeof scancodes / sizeof *scancodes; i++)
        if (scancodes[i].scancode == scancode) return scancodes[i].name;
    return NULL;
}

const char *dxl_key_from_mouse(int button) {
    return button >= 1 && button <= 3 ? mouse[button] : NULL;
}

const char *dxl_key_from_wheel(int up) { return up > 0 ? "MouseWheelUp" : "MouseWheelDown"; }

int dxl_key_seen(const char *name) {
    if (!name || !*name) return 0;
    for (size_t i = 0; i < sizeof scancodes / sizeof *scancodes; i++)
        if (!dxl_stricmp(scancodes[i].name, name)) return 1;
    for (size_t i = 0; i < sizeof sides / sizeof *sides; i++)
        if (!dxl_stricmp(sides[i], name)) return 1;
    for (int b = 1; b <= 3; b++)
        if (!dxl_stricmp(mouse[b], name)) return 1;
    if (!dxl_stricmp(name, "MouseWheelUp") || !dxl_stricmp(name, "MouseWheelDown")) return 1;
    /* The pad's, the mouse's axes: the engine's own, not the keyboard's. */
    return !dxl_strnicmp(name, "Joy", 3) || !dxl_strnicmp(name, "Mouse", 5);
}

static const struct {
    const char *name, *label;
} labels[] = {
    { "LeftMouse", "Left mouse button" }, { "RightMouse", "Right mouse button" },
    { "MiddleMouse", "Middle mouse button" }, { "MouseWheelUp", "Wheel up" }, { "MouseWheelDown", "Wheel down" },
    { "CapsLock", "Caps Lock" }, { "Escape", "Esc" }, { "PageUp", "Page Up" }, { "PageDown", "Page Down" },
    { "Left", "Left arrow" }, { "Up", "Up arrow" }, { "Right", "Right arrow" }, { "Down", "Down arrow" },
    { "PrintScrn", "Print Screen" }, { "NumPad0", "Keypad 0" }, { "NumPad1", "Keypad 1" },
    { "NumPad2", "Keypad 2" }, { "NumPad3", "Keypad 3" }, { "NumPad4", "Keypad 4" }, { "NumPad5", "Keypad 5" },
    { "NumPad6", "Keypad 6" }, { "NumPad7", "Keypad 7" }, { "NumPad8", "Keypad 8" }, { "NumPad9", "Keypad 9" },
    { "GreyStar", "Keypad *" }, { "GreyPlus", "Keypad +" }, { "GreyMinus", "Keypad -" },
    { "GreySlash", "Keypad /" }, { "NumPadPeriod", "Keypad ." }, { "NumLock", "Num Lock" },
    { "ScrollLock", "Scroll Lock" }, { "LShift", "Left Shift" }, { "RShift", "Right Shift" },
    { "LControl", "Left Ctrl" }, { "RControl", "Right Ctrl" }, { "UnknownA4", "Left Alt" },
    { "UnknownA5", "Right Alt" }, { "Unknown5B", "Left Super" }, { "Unknown5C", "Right Super" },
    { "Unknown5D", "Menu key" }, { "Semicolon", ";" }, { "Equals", "=" },
    { "Comma", "," }, { "Minus", "-" }, { "Period", "." }, { "Slash", "/" }, { "Tilde", "~" },
    { "LeftBracket", "[" }, { "Backslash", "\\" }, { "RightBracket", "]" }, { "SingleQuote", "'" },
    { "OEMClear", "Clear" },
};

const char *dxl_key_label(const char *name) {
    for (size_t i = 0; i < sizeof labels / sizeof *labels; i++)
        if (!dxl_stricmp(labels[i].name, name)) return labels[i].label;
    return name;
}

/* A binding as the engine compares them: lower case, each run of spaces
 * one, none at the ends. */
static void plain(const char *in, char *out, size_t size) {
    size_t n = 0;
    int space = 0;
    while (*in == ' ') in++;
    for (; *in && n + 1 < size; in++) {
        if (*in == ' ') {
            space = 1;
            continue;
        }
        if (space && n + 2 < size) out[n++] = ' ';
        space = 0;
        out[n++] = (char)tolower((unsigned char)*in);
    }
    out[n] = '\0';
}

/* The binding's commands, split at its |s, each as plain() makes it. */
static int parts_of(const char *binding, char parts[][128], int max) {
    char all[512];
    plain(binding, all, sizeof all);
    int n = 0;
    for (char *p = all; n < max;) {
        char *bar = strchr(p, '|');
        if (bar) *bar = '\0';
        char *e = p + strlen(p);
        while (*p == ' ') p++;
        while (e > p && e[-1] == ' ') *--e = '\0';
        snprintf(parts[n++], sizeof parts[0], "%.127s", p);
        if (!bar) break;
        p = bar + 1;
    }
    return n;
}

int dxl_key_binds(const char *binding, const char *command) {
    if (!binding || !*binding || !command || !*command) return 0;
    char have[16][128], want[16][128];
    int n = parts_of(binding, have, 16), m = parts_of(command, want, 16);
    if (m > 1) {
        /* A command of several: the binding is it, part for part. */
        if (n != m) return 0;
        for (int i = 0; i < n; i++)
            if (strcmp(have[i], want[i]) != 0) return 0;
        return 1;
    }
    for (int i = 0; i < n; i++)
        if (!strcmp(have[i], want[0])) return 1;
    return 0;
}

/* A line of [Engine.Input] a key's, not an alias's. */
static int is_key(const char *name) { return dxl_strnicmp(name, "Aliases", 7) != 0; }

int dxl_keys_bound(const dxl_ini *user, const char *command, const char **keys, int max) {
    const char *k[512], *v[512];
    size_t n = dxl_ini_pairs(user, INPUT, k, v, 512);
    int found = 0;
    for (size_t i = 0; i < n && i < 512; i++) {
        if (!is_key(k[i]) || !dxl_key_binds(v[i], command)) continue;
        if (found < max) keys[found] = k[i];
        found++;
    }
    return found;
}

const char *dxl_key_binding(const dxl_ini *user, const char *key) {
    const char *v = user ? dxl_ini_get(user, INPUT, key) : NULL;
    return v ? v : "";
}

void dxl_key_bind(dxl_ini *user, const char *key, const char *command) {
    dxl_ini_set(user, INPUT, key, command ? command : "");
}

void dxl_key_unbind(dxl_ini *user, const char *key, const char *command) {
    char binding[512];
    snprintf(binding, sizeof binding, "%s", dxl_key_binding(user, key));
    if (!dxl_key_binds(binding, command)) return;
    if (strchr(command, '|')) {
        dxl_key_bind(user, key, "");
        return;
    }
    /* Its part alone gone, the others kept as they were written. */
    char want[512], rest[512] = "";
    plain(command, want, sizeof want);
    char *save = NULL;
    for (char *part = strtok_r(binding, "|", &save); part; part = strtok_r(NULL, "|", &save)) {
        char mine[512];
        plain(part, mine, sizeof mine);
        if (!strcmp(mine, want)) continue;
        while (*part == ' ') part++;
        char *e = part + strlen(part);
        while (e > part && e[-1] == ' ') *--e = '\0';
        size_t len = strlen(rest);
        snprintf(rest + len, sizeof rest - len, "%s%s", len ? "|" : "", part);
    }
    dxl_key_bind(user, key, rest);
}

void dxl_keys_reset(dxl_ini *user, const dxl_ini *defuser) {
    const char *k[512], *v[512];
    /* The keyboard's and the mouse's: a pad's bindings are its layout's. */
    size_t n = dxl_ini_pairs(user, INPUT, k, NULL, 512);
    char names[512][32];
    size_t m = 0;
    for (size_t i = 0; i < n && i < 512; i++)
        if (is_key(k[i]) && dxl_strnicmp(k[i], "Joy", 3) != 0) snprintf(names[m++], sizeof names[0], "%s", k[i]);
    for (size_t i = 0; i < m; i++) {
        const char *d = defuser ? dxl_ini_get(defuser, INPUT, names[i]) : NULL;
        dxl_ini_set(user, INPUT, names[i], d ? d : "");
    }
    n = defuser ? dxl_ini_pairs(defuser, INPUT, k, v, 512) : 0;
    for (size_t i = 0; i < n && i < 512; i++)
        if (is_key(k[i]) && dxl_strnicmp(k[i], "Joy", 3) != 0) dxl_ini_set(user, INPUT, k[i], v[i]);
}

#define _GNU_SOURCE
#include "core/keys.h"
#include "core/common.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define INPUT "Engine.Input"

/* DXWidgets' SDL3 window, ScancodeToInputKey (SDL2's alike): the
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

/* KeyEventToInputKey's keypad with Num Lock off: the keys they move by,
 * Windows' codes for them -- 5 VK_CLEAR's Unknown0C. */
static const struct {
    int scancode;
    const char *name;
} numlock_off[] = {
    { 98, "Insert" },   /* KP_0 */
    { 89, "End" },   /* KP_1 */
    { 90, "Down" },   /* KP_2 */
    { 91, "PageDown" },   /* KP_3 */
    { 92, "Left" },   /* KP_4 */
    { 93, "Unknown0C" },   /* KP_5 */
    { 94, "Right" },   /* KP_6 */
    { 95, "Home" },   /* KP_7 */
    { 96, "Up" },   /* KP_8 */
    { 97, "PageUp" },   /* KP_9 */
    { 99, "Delete" },   /* KP_PERIOD */
};

/* LayoutKeyName's places of the keys a US keyboard lacks: the ISO key by
 * the left Shift VK_OEM_102 (ABNT's /? and JIS's ro too), the ISO # and
 * JIS's yen VK_OEM_5. */
static const struct {
    int scancode;
    const char *name;
} iso_places[] = {
    { 100, "JoyZ" },   /* NONUSBACKSLASH */
    { 135, "JoyZ" },   /* INTERNATIONAL1 */
    { 137, "Backslash" },   /* INTERNATIONAL3 */
    { 50, "Backslash" },   /* NONUSHASH */
};

/* LayoutKeyName's tables: the typing keys a layout's family names
 * otherwise than by their US place, as Wine's X11 driver does
 * (main_key_vkey_*), the French, Dvorak and Swedish ones Windows' own. */
static const struct {
    dxl_key_family family;
    int scancode;
    const char *name;
} layouts[] = {
    { DXL_KEYS_QWERTZ, 28, "Z" },   /* Y */
    { DXL_KEYS_QWERTZ, 29, "Y" },   /* Z */
    { DXL_KEYS_AZERTY, 53, "SingleQuote" },   /* GRAVE */
    { DXL_KEYS_AZERTY, 45, "LeftBracket" },   /* MINUS */
    { DXL_KEYS_AZERTY, 20, "A" },   /* Q */
    { DXL_KEYS_AZERTY, 26, "Z" },   /* W */
    { DXL_KEYS_AZERTY, 47, "RightBracket" },   /* LEFTBRACKET */
    { DXL_KEYS_AZERTY, 48, "Semicolon" },   /* RIGHTBRACKET */
    { DXL_KEYS_AZERTY, 4, "Q" },   /* A */
    { DXL_KEYS_AZERTY, 51, "M" },   /* SEMICOLON */
    { DXL_KEYS_AZERTY, 52, "Tilde" },   /* APOSTROPHE */
    { DXL_KEYS_AZERTY, 29, "W" },   /* Z */
    { DXL_KEYS_AZERTY, 16, "Comma" },   /* M */
    { DXL_KEYS_AZERTY, 54, "Period" },   /* COMMA */
    { DXL_KEYS_AZERTY, 55, "Slash" },   /* PERIOD */
    { DXL_KEYS_AZERTY, 56, "UnknownDF" },   /* SLASH */
    { DXL_KEYS_SWEDISH, 53, "Backslash" },   /* GRAVE */
    { DXL_KEYS_SWEDISH, 45, "Equals" },   /* MINUS */
    { DXL_KEYS_SWEDISH, 46, "LeftBracket" },   /* EQUALS */
    { DXL_KEYS_SWEDISH, 47, "RightBracket" },   /* LEFTBRACKET */
    { DXL_KEYS_SWEDISH, 48, "Semicolon" },   /* RIGHTBRACKET */
    { DXL_KEYS_SWEDISH, 51, "Tilde" },   /* SEMICOLON */
    { DXL_KEYS_SWEDISH, 49, "Slash" },   /* BACKSLASH */
    { DXL_KEYS_SWEDISH, 50, "Slash" },   /* NONUSHASH */
    { DXL_KEYS_SWEDISH, 56, "Minus" },   /* SLASH */
    { DXL_KEYS_ABNT, 52, "UnknownDF" },   /* APOSTROPHE */
    { DXL_KEYS_ABNT, 100, "SingleQuote" },   /* NONUSBACKSLASH */
    { DXL_KEYS_JP106, 53, "JoyPovRight" },   /* GRAVE */
    { DXL_KEYS_JP106, 46, "SingleQuote" },   /* EQUALS */
    { DXL_KEYS_JP106, 47, "Tilde" },   /* LEFTBRACKET */
    { DXL_KEYS_JP106, 48, "LeftBracket" },   /* RIGHTBRACKET */
    { DXL_KEYS_JP106, 51, "Equals" },   /* SEMICOLON */
    { DXL_KEYS_JP106, 52, "Semicolon" },   /* APOSTROPHE */
    { DXL_KEYS_JP106, 49, "RightBracket" },   /* BACKSLASH */
    { DXL_KEYS_JP106, 50, "RightBracket" },   /* NONUSHASH */
    { DXL_KEYS_DVORAK, 45, "LeftBracket" },   /* MINUS */
    { DXL_KEYS_DVORAK, 46, "RightBracket" },   /* EQUALS */
    { DXL_KEYS_DVORAK, 20, "SingleQuote" },   /* Q */
    { DXL_KEYS_DVORAK, 26, "Comma" },   /* W */
    { DXL_KEYS_DVORAK, 8, "Period" },   /* E */
    { DXL_KEYS_DVORAK, 21, "P" },   /* R */
    { DXL_KEYS_DVORAK, 23, "Y" },   /* T */
    { DXL_KEYS_DVORAK, 28, "F" },   /* Y */
    { DXL_KEYS_DVORAK, 24, "G" },   /* U */
    { DXL_KEYS_DVORAK, 12, "C" },   /* I */
    { DXL_KEYS_DVORAK, 18, "R" },   /* O */
    { DXL_KEYS_DVORAK, 19, "L" },   /* P */
    { DXL_KEYS_DVORAK, 47, "Slash" },   /* LEFTBRACKET */
    { DXL_KEYS_DVORAK, 48, "Equals" },   /* RIGHTBRACKET */
    { DXL_KEYS_DVORAK, 22, "O" },   /* S */
    { DXL_KEYS_DVORAK, 7, "E" },   /* D */
    { DXL_KEYS_DVORAK, 9, "U" },   /* F */
    { DXL_KEYS_DVORAK, 10, "I" },   /* G */
    { DXL_KEYS_DVORAK, 11, "D" },   /* H */
    { DXL_KEYS_DVORAK, 13, "H" },   /* J */
    { DXL_KEYS_DVORAK, 14, "T" },   /* K */
    { DXL_KEYS_DVORAK, 15, "N" },   /* L */
    { DXL_KEYS_DVORAK, 51, "S" },   /* SEMICOLON */
    { DXL_KEYS_DVORAK, 52, "Minus" },   /* APOSTROPHE */
    { DXL_KEYS_DVORAK, 29, "Semicolon" },   /* Z */
    { DXL_KEYS_DVORAK, 27, "Q" },   /* X */
    { DXL_KEYS_DVORAK, 6, "J" },   /* C */
    { DXL_KEYS_DVORAK, 25, "K" },   /* V */
    { DXL_KEYS_DVORAK, 5, "X" },   /* B */
    { DXL_KEYS_DVORAK, 17, "B" },   /* N */
    { DXL_KEYS_DVORAK, 54, "W" },   /* COMMA */
    { DXL_KEYS_DVORAK, 55, "V" },   /* PERIOD */
    { DXL_KEYS_DVORAK, 56, "Z" },   /* SLASH */
};

/* The engine's keynames[], the original's EInputKey without IK_, by
 * Windows' virtual-key code. */
static const char *const keynames[256] = {
    /*00*/ "None", "LeftMouse", "RightMouse", "Cancel",
    /*04*/ "MiddleMouse", "Unknown05", "Unknown06", "Unknown07",
    /*08*/ "Backspace", "Tab", "Unknown0A", "Unknown0B",
    /*0C*/ "Unknown0C", "Enter", "Unknown0E", "Unknown0F",
    /*10*/ "Shift", "Ctrl", "Alt", "Pause",
    /*14*/ "CapsLock", "Unknown15", "Unknown16", "Unknown17",
    /*18*/ "Unknown18", "Unknown19", "Unknown1A", "Escape",
    /*1C*/ "Unknown1C", "Unknown1D", "Unknown1E", "Unknown1F",
    /*20*/ "Space", "PageUp", "PageDown", "End",
    /*24*/ "Home", "Left", "Up", "Right",
    /*28*/ "Down", "Select", "Print", "Execute",
    /*2C*/ "PrintScrn", "Insert", "Delete", "Help",
    /*30*/ "0", "1", "2", "3",
    /*34*/ "4", "5", "6", "7",
    /*38*/ "8", "9", "Unknown3A", "Unknown3B",
    /*3C*/ "Unknown3C", "Unknown3D", "Unknown3E", "Unknown3F",
    /*40*/ "Unknown40", "A", "B", "C",
    /*44*/ "D", "E", "F", "G",
    /*48*/ "H", "I", "J", "K",
    /*4C*/ "L", "M", "N", "O",
    /*50*/ "P", "Q", "R", "S",
    /*54*/ "T", "U", "V", "W",
    /*58*/ "X", "Y", "Z", "Unknown5B",
    /*5C*/ "Unknown5C", "Unknown5D", "Unknown5E", "Unknown5F",
    /*60*/ "NumPad0", "NumPad1", "NumPad2", "NumPad3",
    /*64*/ "NumPad4", "NumPad5", "NumPad6", "NumPad7",
    /*68*/ "NumPad8", "NumPad9", "GreyStar", "GreyPlus",
    /*6C*/ "Separator", "GreyMinus", "NumPadPeriod", "GreySlash",
    /*70*/ "F1", "F2", "F3", "F4",
    /*74*/ "F5", "F6", "F7", "F8",
    /*78*/ "F9", "F10", "F11", "F12",
    /*7C*/ "F13", "F14", "F15", "F16",
    /*80*/ "F17", "F18", "F19", "F20",
    /*84*/ "F21", "F22", "F23", "F24",
    /*88*/ "Unknown88", "Unknown89", "Unknown8A", "Unknown8B",
    /*8C*/ "Unknown8C", "Unknown8D", "Unknown8E", "Unknown8F",
    /*90*/ "NumLock", "ScrollLock", "Unknown92", "Unknown93",
    /*94*/ "Unknown94", "Unknown95", "Unknown96", "Unknown97",
    /*98*/ "Unknown98", "Unknown99", "Unknown9A", "Unknown9B",
    /*9C*/ "Unknown9C", "Unknown9D", "Unknown9E", "Unknown9F",
    /*A0*/ "LShift", "RShift", "LControl", "RControl",
    /*A4*/ "UnknownA4", "UnknownA5", "UnknownA6", "UnknownA7",
    /*A8*/ "UnknownA8", "UnknownA9", "UnknownAA", "UnknownAB",
    /*AC*/ "UnknownAC", "UnknownAD", "UnknownAE", "UnknownAF",
    /*B0*/ "UnknownB0", "UnknownB1", "UnknownB2", "UnknownB3",
    /*B4*/ "UnknownB4", "UnknownB5", "UnknownB6", "UnknownB7",
    /*B8*/ "UnknownB8", "UnknownB9", "Semicolon", "Equals",
    /*BC*/ "Comma", "Minus", "Period", "Slash",
    /*C0*/ "Tilde", "UnknownC1", "UnknownC2", "UnknownC3",
    /*C4*/ "UnknownC4", "UnknownC5", "UnknownC6", "UnknownC7",
    /*C8*/ "Joy1", "Joy2", "Joy3", "Joy4",
    /*CC*/ "Joy5", "Joy6", "Joy7", "Joy8",
    /*D0*/ "Joy9", "Joy10", "Joy11", "Joy12",
    /*D4*/ "Joy13", "Joy14", "Joy15", "Joy16",
    /*D8*/ "UnknownD8", "UnknownD9", "UnknownDA", "LeftBracket",
    /*DC*/ "Backslash", "RightBracket", "SingleQuote", "UnknownDF",
    /*E0*/ "JoyX", "JoyY", "JoyZ", "JoyR",
    /*E4*/ "MouseX", "MouseY", "MouseZ", "MouseW",
    /*E8*/ "JoyU", "JoyV", "UnknownEA", "UnknownEB",
    /*EC*/ "MouseWheelUp", "MouseWheelDown", "Unknown10E", "Unknown10F",
    /*F0*/ "JoyPovUp", "JoyPovDown", "JoyPovLeft", "JoyPovRight",
    /*F4*/ "UnknownF4", "UnknownF5", "Attn", "CrSel",
    /*F8*/ "ExSel", "ErEof", "Play", "Zoom",
    /*FC*/ "NoName", "PA1", "OEMClear", ""
};

/* GetMouseButtonKey's three, and the wheel's two. */
static const char *const mouse[] = { NULL, "LeftMouse", "MiddleMouse", "RightMouse" };

const char *dxl_key_from_scancode(int scancode) {
    for (size_t i = 0; i < sizeof scancodes / sizeof *scancodes; i++)
        if (scancodes[i].scancode == scancode) return scancodes[i].name;
    return NULL;
}

dxl_key_family dxl_key_family_of(unsigned (*typed)(int scancode, void *data), void *data) {
    if (typed(20, data) == 'a' && typed(26, data) == 'z') return DXL_KEYS_AZERTY;        /* Q, W */
    if (typed(28, data) == 'z' && typed(29, data) == 'y') return DXL_KEYS_QWERTZ;        /* Y, Z */
    if (typed(22, data) == 'o' && typed(7, data) == 'e') return DXL_KEYS_DVORAK;         /* S, D */
    if (typed(47, data) == 0xe5 && typed(51, data) == 0xf6 && typed(52, data) == 0xe4)   /* [ ; ' */
        return DXL_KEYS_SWEDISH;
    if (typed(51, data) == 0xe7 && typed(48, data) == '[') return DXL_KEYS_ABNT;         /* ; ] */
    if (typed(47, data) == '@' && typed(52, data) == ':') return DXL_KEYS_JP106;         /* [ ' */
    return DXL_KEYS_QWERTY;
}

int dxl_key_typing(int sc) {
    return (sc >= 4 && sc <= 39) || (sc >= 45 && sc <= 56) || sc == 100 || sc == 135 || sc == 137;
}

const char *dxl_key_from_press(int scancode, int numlock, dxl_key_family family) {
    if (!numlock)
        for (size_t i = 0; i < sizeof numlock_off / sizeof *numlock_off; i++)
            if (numlock_off[i].scancode == scancode) return numlock_off[i].name;
    if (!dxl_key_typing(scancode)) return dxl_key_from_scancode(scancode);
    for (size_t i = 0; i < sizeof layouts / sizeof *layouts; i++)
        if (layouts[i].family == family && layouts[i].scancode == scancode) return layouts[i].name;
    for (size_t i = 0; i < sizeof iso_places / sizeof *iso_places; i++)
        if (iso_places[i].scancode == scancode) return iso_places[i].name;
    return dxl_key_from_scancode(scancode);
}

const char *dxl_key_from_vk(int vk) {
    return vk > 0 && vk < 0xff && keynames[vk][0] ? keynames[vk] : NULL;
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
    /* The keypad's 5 with Num Lock off; AZERTY's and ABNT's VK_OEM_8;
     * Windows' ABNT keys' own codes. */
    if (!dxl_stricmp(name, "Unknown0C") || !dxl_stricmp(name, "UnknownDF") || !dxl_stricmp(name, "UnknownC1") ||
        !dxl_stricmp(name, "UnknownC2"))
        return 1;
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
    { "OEMClear", "Clear" }, { "Unknown0C", "Keypad Clear" },
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

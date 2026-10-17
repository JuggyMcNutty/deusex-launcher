/* The launcher's colours, made from a Deus Ex menu theme: the 13 colours of
 * one of DeusEx.u's ColorThemeMenu_* classes, MenuColor_Background to
 * MenuColor_Cursor, which the game tints its menus with. Until the player's
 * own theme is read from the install (MenuThemeName), the Default's. No
 * SDL here.
 */
#ifndef DXL_UI_THEME_H
#define DXL_UI_THEME_H

#include <stdint.h>

typedef struct { uint8_t r, g, b, a; } dxl_rgba;

/* ColorThemeMenu's colorNames, in their order. */
enum {
    DXL_MENU_BACKGROUND,
    DXL_MENU_TITLE_BACKGROUND,
    DXL_MENU_TITLE_TEXT,
    DXL_MENU_BUTTON_FACE,
    DXL_MENU_BUTTON_TEXT_NORMAL,
    DXL_MENU_BUTTON_TEXT_FOCUS,
    DXL_MENU_BUTTON_TEXT_DISABLED,
    DXL_MENU_HELP_TEXT,
    DXL_MENU_LIST_TEXT,
    DXL_MENU_LIST_TEXT_HIGHLIGHT,
    DXL_MENU_LIST_HIGHLIGHT,
    DXL_MENU_LIST_FOCUS,
    DXL_MENU_CURSOR,
    DXL_MENU_COLORS
};

typedef struct {
    char     name[64];                  /* the theme's themeName */
    dxl_rgba menu[DXL_MENU_COLORS];     /* the game's colours */

    /* What the launcher draws with, made from them. */
    dxl_rgba backdrop_top, backdrop_bottom;   /* behind everything */
    dxl_rgba grid;                            /* the backdrop's faint lines */
    dxl_rgba panel_top, panel_bottom;         /* a steel panel's body */
    dxl_rgba panel_light, panel_shadow;       /* its lit and shadowed edges */
    dxl_rgba face_top, face_bottom;           /* a button's body */
    dxl_rgba face_focus_top, face_focus_bottom;
    dxl_rgba title_bar, title_text;
    dxl_rgba text, text_focus, text_disabled, help;
    dxl_rgba accent;                          /* the title's colour, brighter */
    dxl_rgba bracket;                         /* the focus brackets */
    dxl_rgba row_highlight, row_focus;
    dxl_rgba good, warn, bad;
} dxl_theme;

/* ColorThemeMenu_Default's colours, as DeusEx.u has them. */
void dxl_theme_default(dxl_theme *t);
/* A theme from a menu theme's 13 colours. */
void dxl_theme_from_menu(dxl_theme *t, const char *name, const dxl_rgba menu[DXL_MENU_COLORS]);

/* c with its alpha scaled by a (0..1). */
dxl_rgba dxl_rgba_fade(dxl_rgba c, float a);
/* Between a and b, t from 0 (a) to 1 (b). */
dxl_rgba dxl_rgba_mix(dxl_rgba a, dxl_rgba b, float t);

#endif

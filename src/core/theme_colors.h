/* The game's menu themes, from the install's DeusEx.u: each
 * ColorThemeMenu_* class, by its themeName ("Default", "Blue & Gold"), its
 * thirteen Colors -- MenuColor_Background to MenuColor_Cursor, in
 * ColorThemeMenu's colorNames' order -- its defaults over ColorThemeMenu's,
 * as the game takes them; a colour neither gives is black. User.ini's
 * MenuThemeName names the player's. No SDL here.
 */
#ifndef DXL_THEME_COLORS_H
#define DXL_THEME_COLORS_H

#include "core/common.h"

#include <stdint.h>

#define DXL_THEME_COLOR_COUNT 13
#define DXL_MENU_THEMES_MAX   48

typedef struct {
    char    name[64];                          /* themeName */
    char    class_name[64];                    /* ColorThemeMenu_<...> */
    uint8_t colors[DXL_THEME_COLOR_COUNT][4];  /* R, G, B, A */
} dxl_menu_theme;

/* Every menu theme of the package at path, at most max into out, in its
 * order; returns how many, or -1 and why for no package or none in it. */
int dxl_menu_themes_read(const char *path, dxl_menu_theme *out, int max, dxl_err *err);

/* The theme of that themeName (any case), or NULL. */
const dxl_menu_theme *dxl_menu_theme_find(const dxl_menu_theme *themes, int count, const char *name);

#endif

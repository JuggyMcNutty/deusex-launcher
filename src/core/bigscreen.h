/* The big screen: the launcher's window full screen, its type larger, for
 * a television or a handheld (screens/window.c draws it).
 *
 * launcher.ini's [Launcher] BigScreen: auto, the default -- the big screen
 * when --big-screen asks for it, or when the launcher runs where a pad is
 * the way in: Steam's Big Picture (SteamTenfoot or SteamGamepadUI set to
 * 1), a Steam Deck (SteamDeck=1), gamescope (GAMESCOPE_WAYLAND_DISPLAY);
 * on, always; off, never. BigScreenOffered: the big screen offered once,
 * on the first press of a pad's in a window that is not it.
 */
#ifndef DXL_BIGSCREEN_H
#define DXL_BIGSCREEN_H

typedef enum { DXL_BIG_AUTO, DXL_BIG_ON, DXL_BIG_OFF } dxl_big_setting;

/* launcher.ini's value: "on", "off", anything else auto. */
dxl_big_setting dxl_big_setting_of(const char *value);

/* What of the environment asks for the big screen -- "Steam's Big
 * Picture", "a Steam Deck", "gamescope" -- or NULL for nothing. */
const char *dxl_big_screen_env(void);

/* Whether the window is the big screen, by the setting, --big-screen
 * (asked) and the environment; why: what decided it, for the log. */
int dxl_big_screen_wanted(dxl_big_setting setting, int asked, const char **why);

#endif

#include "core/bigscreen.h"
#include "core/common.h"

#include <stdlib.h>

dxl_big_setting dxl_big_setting_of(const char *value) {
    if (value && !dxl_stricmp(value, "on")) return DXL_BIG_ON;
    if (value && !dxl_stricmp(value, "off")) return DXL_BIG_OFF;
    return DXL_BIG_AUTO;
}

/* Set to 1, as Steam sets them; SteamDeck=0 is a player's "no". */
static int one(const char *name) {
    const char *v = getenv(name);
    return v && v[0] == '1' && !v[1];
}

const char *dxl_big_screen_env(void) {
    if (one("SteamTenfoot") || one("SteamGamepadUI")) return "Steam's Big Picture";
    if (one("SteamDeck")) return "a Steam Deck";
    const char *g = getenv("GAMESCOPE_WAYLAND_DISPLAY");
    if (g && *g) return "gamescope";
    return NULL;
}

int dxl_big_screen_wanted(dxl_big_setting setting, int asked, const char **why) {
    const char *env = dxl_big_screen_env();
    const char *w = NULL;
    int on = 0;
    if (asked) {
        on = 1;
        w = "--big-screen";
    } else if (setting == DXL_BIG_ON) {
        on = 1;
        w = "launcher.ini's BigScreen";
    } else if (setting == DXL_BIG_OFF) {
        w = "launcher.ini's BigScreen";
    } else if (env) {
        on = 1;
        w = env;
    }
    if (why) *why = w;
    return on;
}

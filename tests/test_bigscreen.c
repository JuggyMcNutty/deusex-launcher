#include "test.h"
#include "core/bigscreen.h"
#include "platform/os.h"

#include <stdlib.h>

static void clear_env(void) {
    dxl_env_set("SteamTenfoot", NULL);
    dxl_env_set("SteamGamepadUI", NULL);
    dxl_env_set("SteamDeck", NULL);
    dxl_env_set("GAMESCOPE_WAYLAND_DISPLAY", NULL);
}

static void test_setting(void) {
    CHECK_INT(dxl_big_setting_of("on"), DXL_BIG_ON);
    CHECK_INT(dxl_big_setting_of("Off"), DXL_BIG_OFF);
    CHECK_INT(dxl_big_setting_of("auto"), DXL_BIG_AUTO);
    CHECK_INT(dxl_big_setting_of(""), DXL_BIG_AUTO);
    CHECK_INT(dxl_big_setting_of(NULL), DXL_BIG_AUTO);
    CHECK_INT(dxl_big_setting_of("1"), DXL_BIG_AUTO);
}

/* Steam's big-screen ways, a Steam Deck, gamescope; a player's SteamDeck=0
 * is a no. */
static void test_environment(void) {
    clear_env();
    CHECK(dxl_big_screen_env() == NULL);
    dxl_env_set("SteamDeck", "0");
    CHECK(dxl_big_screen_env() == NULL);
    dxl_env_set("SteamDeck", "1");
    CHECK_STR(dxl_big_screen_env(), "a Steam Deck");
    dxl_env_set("SteamGamepadUI", "1");
    CHECK_STR(dxl_big_screen_env(), "Steam's Big Picture");
    clear_env();
    dxl_env_set("SteamTenfoot", "1");
    CHECK_STR(dxl_big_screen_env(), "Steam's Big Picture");
    clear_env();
    dxl_env_set("GAMESCOPE_WAYLAND_DISPLAY", "gamescope-0");
    CHECK_STR(dxl_big_screen_env(), "gamescope");
    dxl_env_set("GAMESCOPE_WAYLAND_DISPLAY", "");
    CHECK(dxl_big_screen_env() == NULL);
    clear_env();
}

/* --big-screen over all; then the setting; automatic, the environment. */
static void test_wanted(void) {
    clear_env();
    const char *why = NULL;
    CHECK_INT(dxl_big_screen_wanted(DXL_BIG_AUTO, 0, &why), 0);
    CHECK(why == NULL);
    CHECK_INT(dxl_big_screen_wanted(DXL_BIG_AUTO, 1, &why), 1);
    CHECK_STR(why, "--big-screen");
    CHECK_INT(dxl_big_screen_wanted(DXL_BIG_OFF, 1, &why), 1);
    CHECK_INT(dxl_big_screen_wanted(DXL_BIG_ON, 0, &why), 1);
    CHECK_STR(why, "launcher.ini's BigScreen");
    dxl_env_set("SteamDeck", "1");
    CHECK_INT(dxl_big_screen_wanted(DXL_BIG_AUTO, 0, &why), 1);
    CHECK_STR(why, "a Steam Deck");
    CHECK_INT(dxl_big_screen_wanted(DXL_BIG_OFF, 0, &why), 0);
    CHECK_INT(dxl_big_screen_wanted(DXL_BIG_ON, 0, NULL), 1);
    clear_env();
}

TEST_MAIN_BEGIN
    RUN(test_setting);
    RUN(test_environment);
    RUN(test_wanted);
TEST_MAIN_END

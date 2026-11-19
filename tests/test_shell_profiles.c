/* The Profiles page's rows past the most it lists (screens/profiles.c):
 * the first by id, and the one in use always among them (bugs.md
 * LCORE.35). */
#include "scratch.h"
#include "screens/shell.h"

static dxl_shell s;

static void test_past_the_most(void) {
    char *home = dxl_xstrdup(scratch_dir("shell-profiles-home"));
    dxl_env_set("DXL_HOME", home);
    /* One more than the page lists, made out of their order by id. */
    char rel[96];
    for (int i = DXL_PROFILES_MAX; i >= 0; i--) {
        snprintf(rel, sizeof rel, "config/dxlauncher/profiles/p%02d/profile.ini", i);
        scratch_write(home, rel, "[Profile]\nName=A\n");
        snprintf(rel, sizeof rel, "config/dxlauncher/profiles/p%02d/DeusEx.ini", i);
        scratch_write(home, rel, "");
    }
    static dxl_run r;
    memset(&r, 0, sizeof r);
    dxl_err e;
    CHECK_INT(dxl_session_open(&r.session, NULL, &e), 0);
    char last[8];
    snprintf(last, sizeof last, "p%02d", DXL_PROFILES_MAX);
    CHECK_INT(dxl_session_use_profile(&r.session, last, &e), 0);
    dxl_shell_init(&s, "test");
    s.run = &r;

    dxl_page_profiles_enter(&s);
    CHECK_INT(s.profile_count, DXL_PROFILES_MAX);
    CHECK_STR(s.profiles[0].id, "p00");
    CHECK_STR(s.profiles[DXL_PROFILES_MAX - 2].id, "p30");
    CHECK_STR(s.profiles[DXL_PROFILES_MAX - 1].id, last);
    CHECK_INT(s.profiles[DXL_PROFILES_MAX - 1].active, 1);
    /* The one in use among the first: no row given up for it. */
    CHECK_INT(dxl_session_use_profile(&r.session, "p05", &e), 0);
    dxl_page_profiles_enter(&s);
    CHECK_STR(s.profiles[DXL_PROFILES_MAX - 1].id, "p31");
    CHECK_INT(s.profiles[5].active, 1);

    dxl_session_close(&r.session);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    free(home);
}

TEST_MAIN_BEGIN
    RUN(test_past_the_most);
TEST_MAIN_END

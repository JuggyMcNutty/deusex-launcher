/* Text the screens cut to a length, a name past it, cut at a whole UTF-8
 * character (core/common.h, dxl_utf8_fit; bugs.md LUI.12). */
#include "scratch.h"
#include "screens/shell.h"

static dxl_shell s;

static void test_fit(void) {
    CHECK_INT(dxl_utf8_fit("abc", 2), 2);
    CHECK_INT(dxl_utf8_fit("a\xc3\xa9", 2), 1);
    CHECK_INT(dxl_utf8_fit("a\xc3\xa9", 3), 3);
    CHECK_INT(dxl_utf8_fit("\xe2\x82\xac\xe2\x82\xac", 5), 3);
    CHECK_INT(dxl_utf8_fit("ab\xe9", 3), 3);   /* a byte of no character, alone */
    CHECK_INT(dxl_utf8_fit("ab", 10), 2);
    CHECK_INT(dxl_utf8_fit(NULL, 10), 0);
}

/* A profile's name past the 95 bytes the Profiles page keeps of it, a
 * two-byte character across the cut: the character left out whole. */
static void test_profile_name(void) {
    char *home = dxl_xstrdup(scratch_dir("shell-cuts-home"));
    dxl_env_set("DXL_HOME", home);
    char ini[256];
    snprintf(ini, sizeof ini, "[Profile]\nName=%094d\xc3\xa9t\xc3\xa9\n", 0);
    scratch_write(home, "config/dxlauncher/profiles/long/profile.ini", ini);
    scratch_write(home, "config/dxlauncher/profiles/long/DeusEx.ini", "");
    static dxl_run r;
    memset(&r, 0, sizeof r);
    dxl_err e;
    CHECK_INT(dxl_session_open(&r.session, NULL, &e), 0);
    dxl_shell_init(&s, "test");
    s.run = &r;
    dxl_page_profiles_enter(&s);
    CHECK_INT(s.profile_count, 1);
    CHECK_INT((int)strlen(s.profiles[0].name), 94);
    dxl_session_close(&r.session);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    free(home);
}

TEST_MAIN_BEGIN
    RUN(test_fit);
    RUN(test_profile_name);
TEST_MAIN_END

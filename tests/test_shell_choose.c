/* An install chosen for the profile (screens/shell.h, dxl_shell_choose):
 * a repoint whose save fails leaves the profile on the install it had, in
 * memory as on disk, so no later save writes the other (bugs.md LUI.8). */
#include "scratch.h"
#include "screens/shell.h"

static dxl_shell s;

/* The repoint with launcher.ini a folder, so its save fails; had, the
 * profile's install before it, or NULL. */
static void repoint_not_kept(const char *tag, const char *had) {
    const char *home = scratch_dir(tag);
    dxl_env_set("DXL_HOME", home);
    static dxl_run r;
    memset(&r, 0, sizeof r);
    dxl_err e;
    CHECK_INT(dxl_session_open(&r.session, NULL, &e), 0);
    if (had) dxl_profile_set(&r.session.profile, "Install", had);
    CHECK_INT(dxl_path_mkdirs(r.session.ini_path), 0);
    dxl_shell_init(&s, "test");
    s.run = &r;
    s.repoint = 1;
    dxl_install in;
    memset(&in, 0, sizeof in);
    in.root = "/elsewhere/Deus Ex";
    in.ok = 1;
    dxl_shell_choose(&s, &in);
    CHECK_INT(s.dialog, DXL_DIALOG_MESSAGE);
    CHECK_STR(s.message_title, "The profile was not changed");
    const char *now = dxl_profile_get(&r.session.profile, "Install");
    if (had) CHECK_STR(now ? now : "(none)", had);
    else CHECK(now == NULL);
    dxl_session_close(&r.session);
    dxl_env_set("DXL_HOME", NULL);
    CHECK_INT(dxl_fs_remove_tree(home), 0);
}

static void test_repoint_not_kept(void) {
    repoint_not_kept("repoint-had", "1112fm-0badf00d");
    repoint_not_kept("repoint-none", NULL);
}

TEST_MAIN_BEGIN
    RUN(test_repoint_not_kept);
TEST_MAIN_END

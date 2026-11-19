/* A profile's import done in the window's worker, handed to the page
 * (screens/shell.h, dxl_shell_copied): made, the worker's profile the
 * session's and made as Make makes it at once; cancelled or failed,
 * nothing of it, the session's profile as it was (bugs.md LUI.9). */
#include "scratch.h"
#include "screens/shell.h"

static dxl_shell s;

static void test_copied(void) {
    char *g = dxl_xstrdup(scratch_dir("shell-make-game"));
    scratch_write(g, "System/DeusEx.ini", "[Core.System]\r\nSavePath=..\\Save\r\nPaths=..\\System\\*.u\r\n");
    scratch_write(g, "System/User.ini", "[Engine.Input]\r\nF1=X\r\n");
    scratch_write(g, "Save/Save0001/SaveInfo.dxs", "slot-1");
    char *home = dxl_xstrdup(scratch_dir("shell-make-home"));
    dxl_env_set("DXL_HOME", home);
    static dxl_run r;
    memset(&r, 0, sizeof r);
    dxl_err e;
    CHECK_INT(dxl_session_open(&r.session, NULL, &e), 0);
    dxl_shell_init(&s, "test");
    s.run = &r;
    s.chosen.root = g;
    s.chosen.system_dir = scratch_path(g, "System");
    snprintf(s.begin_name, sizeof s.begin_name, "Mine");

    /* Cancelled, then failed: the page told, nothing made. */
    dxl_profile p;
    dxl_import done;
    memset(&done, 0, sizeof done);
    CHECK_INT(dxl_profile_open(&p, &r.session.xdg, r.session.profile.id, NULL), 0);
    s.copying = 1;
    dxl_shell_copied(&s, &p, NULL, &done, "the copy was cancelled", 1);
    CHECK_INT(s.copying, 0);
    CHECK_INT(s.toast_count, 1);
    CHECK_INT(s.dialog, DXL_DIALOG_NONE);
    CHECK_INT(dxl_path_exists(r.session.profile.dir), 0);
    CHECK_INT(dxl_profile_open(&p, &r.session.xdg, r.session.profile.id, NULL), 0);
    dxl_shell_copied(&s, &p, NULL, &done, "cannot open x: No space left on device", 0);
    CHECK_INT(s.dialog, DXL_DIALOG_MESSAGE);
    CHECK_STR(s.message_title, "The profile was not made");
    s.dialog = DXL_DIALOG_NONE;

    /* Made by the worker: the session's from then on. */
    CHECK_INT(dxl_profile_open(&p, &r.session.xdg, r.session.profile.id, NULL), 0);
    dxl_made made;
    CHECK_INT(dxl_profile_import_watched(&p, s.chosen.system_dir, NULL, NULL, NULL, &made, &done, &e), 0);
    CHECK_INT(dxl_profile_exists(&r.session.profile), 0);
    dxl_shell_copied(&s, &p, &made, &done, NULL, 0);
    CHECK_INT(s.dialog, DXL_DIALOG_NONE);
    CHECK_INT(dxl_profile_exists(&r.session.profile), 1);
    CHECK_STR(dxl_profile_get(&r.session.profile, "Name"), "Mine");
    CHECK_INT(dxl_profile_count_saves(&r.session.profile), 1);
    CHECK_INT(s.result, 1);

    free(s.chosen.system_dir);
    s.chosen.root = s.chosen.system_dir = NULL;
    dxl_session_close(&r.session);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(g);
    scratch_remove(home);
    free(g);
    free(home);
}

TEST_MAIN_BEGIN
    RUN(test_copied);
TEST_MAIN_END

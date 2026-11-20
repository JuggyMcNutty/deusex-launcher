/* The Profiles page's rows past the most it lists (screens/profiles.c):
 * the first by id, and the one in use always among them (bugs.md
 * LCORE.35); a delete asked again where no trash can be made (bugs.md
 * LAPP.26). */
#include "scratch.h"
#include "screens/shell.h"

#ifndef _WIN32
#include "platform/trash.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

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

#ifndef _WIN32
/* A profile deleted with its state folder where no trash can be made (the
 * tests' file system top, home/disk, not writable): the page asks again,
 * naming the folder and why, nothing gone; that question's yes deletes it
 * for good, the rest to the trash, the list without it (bugs.md
 * LAPP.26). */
static void test_delete_for_good(void) {
    if (geteuid() == 0) {
        printf("test_delete_for_good: root writes any folder: skipped\n");
        return;
    }
    char *home = dxl_xstrdup(scratch_dir("shell-profiles-good"));
    char *disk = scratch_path(home, "disk"), *disk_state = scratch_path(home, "disk/state");
    char *state = scratch_path(home, "state");
    CHECK_INT(dxl_path_mkdirs(disk_state), 0);
    CHECK_INT(symlink(disk_state, state), 0);
    dxl_env_set("DXL_HOME", home);
    const char *ids[2] = { "first", "second" };
    char rel[96];
    for (int i = 0; i < 2; i++) {
        snprintf(rel, sizeof rel, "config/dxlauncher/profiles/%s/profile.ini", ids[i]);
        scratch_write(home, rel, i ? "[Profile]\nName=Second\n" : "[Profile]\nName=First\n");
        snprintf(rel, sizeof rel, "config/dxlauncher/profiles/%s/DeusEx.ini", ids[i]);
        scratch_write(home, rel, "");
    }
    static dxl_run r;
    memset(&r, 0, sizeof r);
    dxl_err e;
    CHECK_INT(dxl_session_open(&r.session, NULL, &e), 0);
    CHECK_INT(dxl_session_use_profile(&r.session, "first", &e), 0);
    dxl_profile p;
    CHECK_INT(dxl_profile_open(&p, &r.session.xdg, "second", &e), 0);
    char *dirs[4];
    dxl_profile_folders(&p, dirs);
    dxl_profile_free(&p);
    for (int i = 0; i < 4; i++) {
        CHECK_INT(dxl_path_mkdirs(dirs[i]), 0);
        scratch_write(dirs[i], "x", "x");
    }
    dxl_shell_init(&s, "test");
    s.run = &r;
    dxl_page_profiles_enter(&s);
    CHECK_INT(s.profile_count, 2);
    CHECK_STR(s.profiles[1].id, "second");
    dxl_trash_test_top = disk;
    CHECK_INT(chmod(disk, 0555), 0);

    /* Delete...'s yes: asked again, nothing gone. */
    dxl_page_profiles_delete(&s, 1, 0);
    CHECK_INT(s.dialog, DXL_DIALOG_CONFIRM);
    CHECK_INT((long)s.asked_for, (long)dxl_ui_id(&s.ui, "profiles delete for good"));
    CHECK_STR(s.ask_title, "Delete Second for good?");
    CHECK(strstr(s.message, "/disk/state/") != NULL && strstr(s.message, strerror(EACCES)) != NULL);
    CHECK_STR(s.ask_yes, "Delete for good");
    for (int i = 0; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[i]), 1);
    CHECK_INT(s.profile_count, 2);
    /* Its no: nothing more done. */
    dxl_shell_close_dialog(&s);
    for (int i = 0; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[i]), 1);
    /* A yes for another profile than the one asked about: nothing. */
    dxl_page_profiles_delete(&s, 0, 1);
    CHECK_INT(s.dialog, DXL_DIALOG_NONE);
    for (int i = 0; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[i]), 1);
    /* Its yes where what has no trash is no longer what it named (as if a
     * disk went read-only meanwhile): nothing deleted, asked again. */
    snprintf(s.profile_doomed_why, sizeof s.profile_doomed_why, "another folder");
    dxl_page_profiles_delete(&s, 1, 1);
    CHECK_INT(s.dialog, DXL_DIALOG_CONFIRM);
    CHECK(strstr(s.message, "/disk/state/") != NULL);
    for (int i = 0; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[i]), 1);
    dxl_shell_close_dialog(&s);
    /* Its yes: that folder for good, not what a link in it names, the
     * rest to the trash. */
    scratch_write(home, "outside/kept", "kept");
    char *outside = scratch_path(home, "outside"), *link = dxl_path_join(dirs[2], "link");
    CHECK_INT(symlink(outside, link), 0);
    dxl_page_profiles_delete(&s, 1, 1);
    CHECK_INT(s.dialog, DXL_DIALOG_NONE);
    for (int i = 0; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[i]), 0);
    CHECK(scratch_exists(home, "outside/kept"));
    CHECK(scratch_exists(home, "data/Trash/files/second/x"));
    CHECK(scratch_exists(home, "data/Trash/files/second.2/x"));
    CHECK(!scratch_exists(home, "data/Trash/files/second.3"));
    CHECK_INT(s.profile_count, 1);
    CHECK_STR(s.profiles[0].id, "first");
    free(link);
    free(outside);

    /* Asked, then a trash made possible there before the yes: the folder
     * to that trash, not deleted for good. */
    scratch_write(home, "config/dxlauncher/profiles/second/profile.ini", "[Profile]\nName=Second\n");
    scratch_write(home, "config/dxlauncher/profiles/second/DeusEx.ini", "");
    for (int i = 0; i < 4; i++) {
        CHECK_INT(dxl_path_mkdirs(dirs[i]), 0);
        scratch_write(dirs[i], "x", "x");
    }
    dxl_page_profiles_enter(&s);
    CHECK_INT(s.profile_count, 2);
    dxl_page_profiles_delete(&s, 1, 0);
    CHECK_INT(s.dialog, DXL_DIALOG_CONFIRM);
    dxl_shell_close_dialog(&s);
    CHECK_INT(chmod(disk, 0755), 0);
    dxl_page_profiles_delete(&s, 1, 1);
    CHECK_INT(s.dialog, DXL_DIALOG_NONE);
    for (int i = 0; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[i]), 0);
    char own[600];
    snprintf(own, sizeof own, "%s/.Trash-%lu/files/second", disk, (unsigned long)getuid());
    CHECK(scratch_exists(own, "x"));
    CHECK_INT(s.profile_count, 1);

    dxl_trash_test_top = NULL;
    for (int i = 0; i < 4; i++) free(dirs[i]);
    dxl_session_close(&r.session);
    dxl_env_set("DXL_HOME", NULL);
    free(state);
    free(disk_state);
    free(disk);
    scratch_remove(home);
    free(home);
}
#endif

TEST_MAIN_BEGIN
    RUN(test_past_the_most);
#ifndef _WIN32
    RUN(test_delete_for_good);
#endif
TEST_MAIN_END

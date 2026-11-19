#include "test.h"
#include "scratch.h"
#include "platform/delete.h"
#include "app/session.h"

#include <sys/stat.h>
#include <unistd.h>

/* A folder with everything in it gone, a link in it removed and not
 * followed; a file; one not there said so. */
static void test_delete(void) {
    const char *dir = scratch_dir("delete");
    scratch_write(dir, "outside/kept", "kept");
    scratch_write(dir, "profile/Save/Save0001/SaveInfo.dxs", "info");
    scratch_write(dir, "profile/profile.ini", "[Profile]\n");
    char *outside = scratch_path(dir, "outside"), *link = scratch_path(dir, "profile/link");
    CHECK_INT(symlink(outside, link), 0);
    char *profile = scratch_path(dir, "profile");
    dxl_err e;
    CHECK_INT(dxl_delete(profile, &e), 0);
    CHECK(!scratch_exists(dir, "profile"));
    CHECK(scratch_exists(dir, "outside/kept"));

    char *kept = scratch_path(dir, "outside/kept");
    CHECK_INT(dxl_delete(kept, &e), 0);
    CHECK(!scratch_exists(dir, "outside/kept"));
    CHECK_INT(dxl_delete(kept, &e), -1);
    CHECK(strstr(dxl_err_msg(&e), "cannot delete") != NULL);
    free(kept);
    free(profile);
    free(link);
    free(outside);
    scratch_remove(dir);
}

/* A profile whose data folder cannot all be deleted (a folder in it
 * read-only): its settings, profile.ini among them, are kept, so the
 * profile is still there, as the message says (bugs.md LAPP.27). Skipped
 * as root, whom no mode stops. */
static void test_delete_profile_part(void) {
    if (geteuid() == 0) {
        printf("test_delete_profile_part: as root: skipped\n");
        return;
    }
    char *home = dxl_xstrdup(scratch_dir("delete-part"));
    dxl_env_set("DXL_HOME", home);
    dxl_session s;
    dxl_err e;
    CHECK_INT(dxl_session_open(&s, NULL, &e), 0);
    dxl_profile p;
    CHECK_INT(dxl_profile_open(&p, &s.xdg, "second", &e), 0);
    char *dirs[4];
    dxl_profile_folders(&p, dirs);
    dxl_profile_free(&p);
    for (int i = 0; i < 4; i++) {
        CHECK_INT(dxl_path_mkdirs(dirs[i]), 0);
        scratch_write(dirs[i], "x", "x");
    }
    scratch_write(dirs[1], "Save/Save0001/SaveInfo.dxs", "info");
    char *locked = scratch_path(dirs[1], "Save/Save0001");
    CHECK_INT(chmod(locked, 0500), 0);
    CHECK_INT(dxl_session_delete_profile(&s, "second", &e), -1);
    CHECK(scratch_exists(dirs[0], "x"));
    CHECK(scratch_exists(dirs[1], "Save/Save0001/SaveInfo.dxs"));
    CHECK_INT(dxl_path_exists(dirs[3]), 0);
    CHECK_INT(chmod(locked, 0700), 0);
    free(locked);
    for (int i = 0; i < 4; i++) free(dirs[i]);
    dxl_session_close(&s);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    free(home);
}

TEST_MAIN_BEGIN
    RUN(test_delete);
    RUN(test_delete_profile_part);
TEST_MAIN_END

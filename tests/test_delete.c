#include "test.h"
#include "scratch.h"
#include "platform/delete.h"

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

TEST_MAIN_BEGIN
    RUN(test_delete);
TEST_MAIN_END

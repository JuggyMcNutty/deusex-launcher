/* What the launcher deletes of the player's, into Windows' Recycle Bin
 * (platform/win32/trash.c): gone from where it was, never put back by the
 * launcher; several at once, all of them or none. Under Wine the Recycle
 * Bin is the desktop's trash: port.sh gives the tests a scratch one. */
#include "test.h"
#include "scratch.h"
#include "platform/os.h"
#include "platform/trash.h"

static void test_trash(void) {
    char *dir = dxl_xstrdup(scratch_dir("trash"));
    scratch_write(dir, "stuff/Save0001/SaveInfo.dxs", "info");
    scratch_write(dir, "stuff/a file", "a");
    char *save = scratch_path(dir, "stuff/Save0001"), *file = scratch_path(dir, "stuff/a file");
    CHECK_INT(dxl_trash_can(save, NULL), 1);
    char name[256] = "unchanged";
    CHECK_INT(dxl_trash(save, NULL, name, sizeof name, NULL), 0);
    CHECK_STR(name, "");   /* the Recycle Bin says nothing back */
    CHECK(!scratch_exists(dir, "stuff/Save0001"));
    CHECK_INT(dxl_trash(file, NULL, NULL, 0, NULL), 0);
    CHECK(!scratch_exists(dir, "stuff/a file"));
    /* Put back from the Recycle Bin, not by the launcher. */
    dxl_err e;
    CHECK_INT(dxl_trash_undo(NULL, "Save0001", save, &e), -1);
    /* What is not there stays not there. */
    CHECK_INT(dxl_trash(save, NULL, NULL, 0, &e), -1);
    CHECK_INT(dxl_trash_can("C:/nonexistent/dxl", NULL), 0);
    free(save);
    free(file);
    scratch_remove(dir);
    free(dir);
}

/* Several at once: each moved aside and back first, and when one cannot
 * be, those moved put back and nothing gone; then all of them, each by its
 * own name, as the Recycle Bin restores it. */
static void test_trash_all(void) {
    char *dir = dxl_xstrdup(scratch_dir("trash-all"));
    scratch_write(dir, "a/x", "a");
    scratch_write(dir, "b/y", "b");
    char *a = scratch_path(dir, "a"), *b = scratch_path(dir, "b");
    char *paths[] = { a, b };
    /* b's way aside taken by a folder already there. */
    char aside[64];
    snprintf(aside, sizeof aside, "b.deleting-%ld/z", dxl_os_pid());
    scratch_write(dir, aside, "z");
    dxl_err e;
    CHECK_INT(dxl_trash_all(paths, 2, NULL, &e), -1);
    CHECK(strstr(dxl_err_msg(&e), "nothing was deleted") != NULL);
    CHECK(scratch_exists(dir, "a/x"));
    CHECK(scratch_exists(dir, "b/y"));
    char a_aside[64];
    snprintf(a_aside, sizeof a_aside, "a.deleting-%ld", dxl_os_pid());
    CHECK(!scratch_exists(dir, a_aside));

    *strrchr(aside, '/') = '\0';
    char *taken = scratch_path(dir, aside);
    scratch_remove(taken);
    CHECK_INT(dxl_trash_all(paths, 2, NULL, &e), 0);
    CHECK(!scratch_exists(dir, "a"));
    CHECK(!scratch_exists(dir, "b"));
    CHECK(!scratch_exists(dir, a_aside));
    free(taken);
    free(a);
    free(b);
    scratch_remove(dir);
    free(dir);
}

TEST_MAIN_BEGIN
    RUN(test_trash);
    RUN(test_trash_all);
TEST_MAIN_END

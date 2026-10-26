#include "test.h"
#include "scratch.h"
#include "platform/trash.h"
#include "platform/fs.h"

#include <dirent.h>

/* The .trashinfo of name in the trash, its text. */
static char *info_of(const char *trash, const char *name) {
    char rel[300];
    snprintf(rel, sizeof rel, "Trash/info/%s.trashinfo", name);
    char *p = scratch_path(trash, rel);
    size_t len = 0;
    char *text = slurp(p, &len);
    free(p);
    return text;
}

/* A folder and a file to the trash: each in files/ by its name, a second
 * of a name taken given another, each's .trashinfo where it was. */
static void test_trash(void) {
    const char *dir = scratch_dir("trash");
    scratch_write(dir, "home/data/x", "");
    scratch_write(dir, "stuff/Save0001/SaveInfo.dxs", "info");
    scratch_write(dir, "stuff/a file", "a");
    scratch_write(dir, "again/Save0001/SaveInfo.dxs", "again");
    char *data = scratch_path(dir, "home/data");
    char *save = scratch_path(dir, "stuff/Save0001"), *file = scratch_path(dir, "stuff/a file");
    char *again = scratch_path(dir, "again/Save0001");
    CHECK_INT(dxl_trash_can(save, data), 1);
    char name[256];
    CHECK_INT(dxl_trash(save, data, name, sizeof name, NULL), 0);
    CHECK_STR(name, "Save0001");
    CHECK(!scratch_exists(dir, "stuff/Save0001"));
    CHECK(scratch_exists(dir, "home/data/Trash/files/Save0001/SaveInfo.dxs"));
    char *info = info_of(data, "Save0001");
    CHECK(info && strstr(info, "[Trash Info]\nPath=") == info);
    CHECK(info && strstr(info, "/stuff/Save0001\n") != NULL);
    CHECK(info && strstr(info, "\nDeletionDate=") != NULL);
    free(info);
    CHECK_INT(dxl_trash(again, data, name, sizeof name, NULL), 0);
    CHECK_STR(name, "Save0001.2");
    CHECK(scratch_exists(dir, "home/data/Trash/files/Save0001.2/SaveInfo.dxs"));
    info = info_of(data, "Save0001.2");
    CHECK(info && strstr(info, "/again/Save0001\n") != NULL);
    free(info);
    /* Put back: where it was, its .trashinfo gone; then again. */
    CHECK_INT(dxl_trash_undo(data, "Save0001.2", again, NULL), 0);
    CHECK(scratch_exists(dir, "again/Save0001/SaveInfo.dxs"));
    CHECK(!scratch_exists(dir, "home/data/Trash/info/Save0001.2.trashinfo"));
    CHECK_INT(dxl_trash_undo(data, "Save0001.2", again, NULL), -1);
    /* Its name escaped as a URL's. */
    CHECK_INT(dxl_trash(file, data, NULL, 0, NULL), 0);
    info = info_of(data, "a file");
    CHECK(info && strstr(info, "/stuff/a%20file\n") != NULL);
    free(info);
    /* A link is trashed, not its target. */
    scratch_write(dir, "stuff/target", "t");
    char *link = scratch_path(dir, "stuff/a link");
    CHECK_INT(dxl_fs_link_dir("target", link), 0);
    CHECK_INT(dxl_trash(link, data, name, sizeof name, NULL), 0);
    CHECK_STR(name, "a link");
    CHECK(scratch_exists(dir, "stuff/target"));
    char *moved = scratch_path(data, "Trash/files/a link");
    CHECK_INT(dxl_fs_is_link(moved), 1);
    info = info_of(data, "a link");
    CHECK(info && strstr(info, "/stuff/a%20link\n") != NULL);
    free(info);
    free(moved);
    free(link);
    /* What is not there stays not there. */
    dxl_err e;
    CHECK_INT(dxl_trash(save, data, NULL, 0, &e), -1);
    CHECK_INT(dxl_trash_can("/nonexistent", data), 0);
    free(data);
    free(save);
    free(file);
    free(again);
    scratch_remove(dir);
}

TEST_MAIN_BEGIN
    RUN(test_trash);
TEST_MAIN_END

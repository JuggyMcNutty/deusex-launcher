#include "test.h"
#include "scratch.h"
#include "platform/trash.h"
#include "platform/fs.h"
#include "app/session.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

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

/* A profile deleted with its cache folder on another file system than the
 * trash, a tmpfs (/dev/shm): its other three folders to the trash, the
 * cache folder gone (bugs.md LAPP.26). Skipped with no such tmpfs. */
static void test_delete_cache_elsewhere(void) {
    char *home = dxl_xstrdup(scratch_dir("trash-xdev"));
    char shm[256];
    snprintf(shm, sizeof shm, "/dev/shm/dxl-trash-xdev-%ld", dxl_os_pid());
    struct stat a, b;
    if (stat("/dev/shm", &a) != 0 || stat(home, &b) != 0 || a.st_dev == b.st_dev || dxl_fs_mkdir(shm) != 0) {
        printf("test_delete_cache_elsewhere: no tmpfs apart from %s: skipped\n", home);
        scratch_remove(home);
        free(home);
        return;
    }
    char *cache = scratch_path(home, "cache");
    CHECK_INT(symlink(shm, cache), 0);
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
    CHECK_INT(dxl_session_delete_profile(&s, "second", &e), 0);
    for (int i = 0; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[i]), 0);
    CHECK(scratch_exists(home, "data/Trash/files/second/x"));
    CHECK(scratch_exists(home, "data/Trash/files/second.2/x"));
    CHECK(scratch_exists(home, "data/Trash/files/second.3/x"));
    CHECK(!scratch_exists(home, "data/Trash/files/second.4"));
    for (int i = 0; i < 4; i++) free(dirs[i]);
    dxl_session_close(&s);
    dxl_env_set("DXL_HOME", NULL);
    CHECK_INT(dxl_fs_remove_tree(shm), 0);
    free(cache);
    scratch_remove(home);
    free(home);
}

/* Long paths are neither cut nor misplaced (bugs.md LAPP.29): a trash in a
 * data home over 1,024 bytes long takes a file whole; a path whose escaped
 * Path= would pass 4,096 bytes (long names of letters escaped six bytes
 * each) is refused, the file left where it is. */
static void test_long_paths(void) {
    const char *dir = scratch_dir("trash-long");
    char rel[1400] = "home";
    while (strlen(rel) < 1100) strcat(rel, "/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    char marker[1500];
    snprintf(marker, sizeof marker, "%s/x", rel);
    scratch_write(dir, marker, "");
    char *data = scratch_path(dir, rel);
    scratch_write(dir, "stuff/file", "f");
    char *file = scratch_path(dir, "stuff/file");
    dxl_err e;
    CHECK_INT(dxl_trash(file, data, NULL, 0, &e), 0);
    char *moved = dxl_path_join(data, "Trash/files/file");
    CHECK_INT(dxl_path_exists(moved), 1);
    CHECK(!scratch_exists(dir, "stuff/file"));
    free(moved);

    char deep[2000] = "stuff";
    for (int i = 0; i < 8; i++) {
        strcat(deep, "/");
        for (int k = 0; k < 100; k++) strcat(deep, "\xc3\xa9");
    }
    char deep_file[2100];
    snprintf(deep_file, sizeof deep_file, "%s/file", deep);
    scratch_write(dir, deep_file, "f");
    char *long_one = scratch_path(dir, deep_file);
    CHECK_INT(dxl_path_exists(long_one), 1);
    CHECK_INT(dxl_trash(long_one, data, NULL, 0, &e), -1);
    CHECK(strstr(dxl_err_msg(&e), "too long") != NULL);
    CHECK_INT(dxl_path_exists(long_one), 1);
    free(long_one);
    free(file);
    free(data);
    scratch_remove(dir);
}

TEST_MAIN_BEGIN
    RUN(test_trash);
    RUN(test_long_paths);
    RUN(test_delete_cache_elsewhere);
TEST_MAIN_END

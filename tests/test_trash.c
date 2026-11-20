#include "test.h"
#include "scratch.h"
#include "platform/trash.h"
#include "platform/fs.h"
#include "app/session.h"

#include <dirent.h>
#include <errno.h>
#include <string.h>
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
    CHECK_INT(dxl_session_delete_profile(&s, "second", 0, NULL, 0, &e), 0);
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

/* A file of a trash, by its path, its text. */
static char *text_of(const char *path) {
    size_t len = 0;
    return slurp(path, &len);
}

/* <dir>/<.Trash-uid> into out. */
static void own_trash_name(const char *dir, char *out, size_t size) {
    snprintf(out, size, "%s/.Trash-%lu", dir, (unsigned long)getuid());
}

/* A trash a test made, taken away where it holds nothing but what the test
 * put there and took out: another's entries, made meanwhile, kept. */
static void drop_trash(const char *trash) {
    char sub[300];
    snprintf(sub, sizeof sub, "%s/files", trash);
    rmdir(sub);
    snprintf(sub, sizeof sub, "%s/info", trash);
    rmdir(sub);
    rmdir(trash);
}

/* A folder on another file system than the home trash, a tmpfs (/dev/shm):
 * to that file system's own trash, /dev/shm/.Trash-<uid>, made 0700 at
 * need, its .trashinfo's Path relative to the top, /dev/shm; the home
 * trash untouched (bugs.md LAPP.26). Skipped with no such tmpfs, or one
 * with an administrator's .Trash, whose rules test_top_trashes holds. */
static void test_other_fs(void) {
    char *home = dxl_xstrdup(scratch_dir("trash-other"));
    char *data = scratch_path(home, "data");
    CHECK_INT(dxl_path_mkdirs(data), 0);
    char shm[256], base[128], trash[128];
    snprintf(base, sizeof base, "dxl-trash-other-%ld", dxl_os_pid());
    snprintf(shm, sizeof shm, "/dev/shm/%s", base);
    own_trash_name("/dev/shm", trash, sizeof trash);
    struct stat a, b, t;
    if (stat("/dev/shm", &a) != 0 || stat(home, &b) != 0 || a.st_dev == b.st_dev || lstat("/dev/shm/.Trash", &t) == 0 ||
        dxl_fs_mkdir(shm) != 0) {
        printf("test_other_fs: no tmpfs apart from %s: skipped\n", home);
        free(data);
        scratch_remove(home);
        free(home);
        return;
    }
    int made = lstat(trash, &t) != 0;
    scratch_write(shm, "x", "x");
    char *paths[1] = { shm };
    dxl_err e;
    char why[600];
    CHECK_INT(dxl_trash_or_delete(paths, 1, data, 0, why, sizeof why, &e), 0);
    CHECK_STR(why, "");
    CHECK_INT(dxl_path_exists(shm), 0);
    char files[300], info[300];
    snprintf(files, sizeof files, "%s/files/%s", trash, base);
    snprintf(info, sizeof info, "%s/info/%s.trashinfo", trash, base);
    CHECK(scratch_exists(files, "x"));
    char *text = text_of(info), want[200];
    snprintf(want, sizeof want, "[Trash Info]\nPath=%s\nDeletionDate=", base);
    CHECK(text && strstr(text, want) == text);
    /* DeletionDate as the home trash's: YYYY-MM-DDThh:mm:ss. */
    CHECK(text && strlen(text) == strlen(want) + 20 && text[strlen(want) + 10] == 'T');
    free(text);
    CHECK(lstat(trash, &t) == 0 && S_ISDIR(t.st_mode) && (t.st_mode & 0777) == 0700);
    CHECK(!scratch_exists(data, "Trash/files/x"));
    CHECK(!scratch_exists(data, "Trash/info/x"));
    dxl_fs_remove_tree(files);
    unlink(info);
    if (made) drop_trash(trash);
    free(data);
    scratch_remove(home);
    free(home);
}

/* A profile with its state folder on a tmpfs (/dev/shm), its settings and
 * data folders in the home trash's: deleted, the two to the home trash,
 * the state folder to /dev/shm's own (bugs.md LAPP.26). Skipped as
 * test_other_fs is. */
static void test_profile_other_fs(void) {
    char *home = dxl_xstrdup(scratch_dir("trash-prof"));
    char shm[256], trash[128];
    snprintf(shm, sizeof shm, "/dev/shm/dxl-trash-prof-%ld", dxl_os_pid());
    own_trash_name("/dev/shm", trash, sizeof trash);
    struct stat a, b, t;
    if (stat("/dev/shm", &a) != 0 || stat(home, &b) != 0 || a.st_dev == b.st_dev || lstat("/dev/shm/.Trash", &t) == 0 ||
        dxl_fs_mkdir(shm) != 0) {
        printf("test_profile_other_fs: no tmpfs apart from %s: skipped\n", home);
        scratch_remove(home);
        free(home);
        return;
    }
    int made = lstat(trash, &t) != 0;
    char *state = scratch_path(home, "state");
    CHECK_INT(symlink(shm, state), 0);
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
    CHECK_INT(dxl_session_delete_profile(&s, "second", 0, NULL, 0, &e), 0);
    for (int i = 0; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[i]), 0);
    CHECK(scratch_exists(home, "data/Trash/files/second/x"));
    CHECK(scratch_exists(home, "data/Trash/files/second.2/x"));
    CHECK(!scratch_exists(home, "data/Trash/files/second.3"));
    char files[300], info[300];
    snprintf(files, sizeof files, "%s/files/second", trash);
    snprintf(info, sizeof info, "%s/info/second.trashinfo", trash);
    if (!made) {
        /* The player's own trash there: ours, whichever name it took. */
        printf("test_profile_other_fs: %s was there: its entry not checked by name\n", trash);
    } else {
        CHECK(scratch_exists(files, "x"));
        char *text = text_of(info), want[200];
        snprintf(want, sizeof want, "[Trash Info]\nPath=dxl-trash-prof-%ld/dxlauncher/", dxl_os_pid());
        CHECK(text && strstr(text, want) == text);
        free(text);
        dxl_fs_remove_tree(files);
        unlink(info);
        drop_trash(trash);
    }
    for (int i = 0; i < 4; i++) free(dirs[i]);
    dxl_session_close(&s);
    dxl_env_set("DXL_HOME", NULL);
    dxl_fs_remove_tree(shm);
    free(state);
    scratch_remove(home);
    free(home);
}

/* One thing to the trash of the tests' file system top, under dxl_trash_test_top. */
static int trash_one(const char *dir, const char *rel, const char *data) {
    char *path = scratch_path(dir, rel);
    CHECK_INT(dxl_path_mkdirs(path), 0);
    scratch_write(path, "x", "x");
    char *paths[1] = { path };
    dxl_err e;
    int rc = dxl_trash_or_delete(paths, 1, data, 0, NULL, 0, &e);
    if (rc != 0) printf("trash_one %s: %s\n", rel, dxl_err_msg(&e));
    CHECK_INT(dxl_path_exists(path), 0);
    free(path);
    return rc;
}

/* A file system's own trashes, the spec's two, in a folder the tests take
 * for its top: $topdir/.Trash-<uid> where there is no .Trash, where it is
 * a folder with no sticky bit, and where it is a link to a sticky one;
 * $topdir/.Trash/<uid> where it is a folder with its sticky bit set. Each
 * entry's Path relative to the top. */
static void test_top_trashes(void) {
    const char *dir = scratch_dir("trash-top");
    char *data = scratch_path(dir, "home/data"), *disk = scratch_path(dir, "disk");
    CHECK_INT(dxl_path_mkdirs(data), 0);
    CHECK_INT(dxl_path_mkdirs(disk), 0);
    dxl_trash_test_top = disk;
    char own[600], admin[600], uid[32];
    own_trash_name(disk, own, sizeof own);
    snprintf(admin, sizeof admin, "%s/.Trash", disk);
    snprintf(uid, sizeof uid, "%lu", (unsigned long)getuid());

    CHECK_INT(trash_one(disk, "games/a", data), 0);
    CHECK(scratch_exists(own, "files/a/x"));
    char *info = scratch_path(own, "info/a.trashinfo"), *text = text_of(info);
    CHECK(text && strstr(text, "[Trash Info]\nPath=games/a\nDeletionDate=") == text);
    free(text);
    free(info);

    CHECK_INT(dxl_fs_mkdir(admin), 0);
    CHECK_INT(chmod(admin, 0777), 0);
    CHECK_INT(trash_one(disk, "b", data), 0);
    CHECK(scratch_exists(own, "files/b/x"));
    CHECK(!scratch_exists(admin, uid));

    char real[700];
    snprintf(real, sizeof real, "%s/admin", disk);
    CHECK_INT(rename(admin, real), 0);
    CHECK_INT(chmod(real, 01777), 0);
    CHECK_INT(symlink("admin", admin), 0);
    CHECK_INT(trash_one(disk, "c", data), 0);
    CHECK(scratch_exists(own, "files/c/x"));
    CHECK(!scratch_exists(real, uid));

    CHECK_INT(unlink(admin), 0);
    CHECK_INT(rename(real, admin), 0);
    CHECK_INT(trash_one(disk, "d", data), 0);
    char rel[64];
    snprintf(rel, sizeof rel, "%s/files/d/x", uid);
    CHECK(scratch_exists(admin, rel));
    snprintf(rel, sizeof rel, "%s/info/d.trashinfo", uid);
    info = scratch_path(admin, rel);
    text = text_of(info);
    CHECK(text && strstr(text, "[Trash Info]\nPath=d\nDeletionDate=") == text);
    free(text);
    free(info);
    CHECK(!scratch_exists(data, "Trash/files/d"));

    dxl_trash_test_top = NULL;
    free(data);
    free(disk);
    scratch_remove(dir);
}

/* A profile's folders on two file systems, its state folder on the tests'
 * own (home/state a link into home/disk): where no trash can be made there
 * (its top not writable), found before anything moves -- nothing moved, no
 * .trashinfo left, the folder and why said -- then the second answer's
 * yes deleting that folder for good, the other two to the home trash;
 * where one can, each to its own file system's trash. */
static void test_profile_two_fs(void) {
    if (geteuid() == 0) {
        printf("test_profile_two_fs: root writes any folder: skipped\n");
        return;
    }
    char *home = dxl_xstrdup(scratch_dir("trash-two"));
    char *disk = scratch_path(home, "disk"), *disk_state = scratch_path(home, "disk/state");
    char *state = scratch_path(home, "state");
    CHECK_INT(dxl_path_mkdirs(disk_state), 0);
    CHECK_INT(symlink(disk_state, state), 0);
    dxl_env_set("DXL_HOME", home);
    dxl_session s;
    dxl_err e;
    CHECK_INT(dxl_session_open(&s, NULL, &e), 0);
    char *dirs[2][4];
    const char *ids[2] = { "third", "second" };
    for (int k = 0; k < 2; k++) {
        dxl_profile p;
        CHECK_INT(dxl_profile_open(&p, &s.xdg, ids[k], &e), 0);
        dxl_profile_folders(&p, dirs[k]);
        dxl_profile_free(&p);
        for (int i = 0; i < 4; i++) {
            CHECK_INT(dxl_path_mkdirs(dirs[k][i]), 0);
            scratch_write(dirs[k][i], "x", "x");
        }
    }
    dxl_trash_test_top = disk;
    char own[600];
    own_trash_name(disk, own, sizeof own);

    CHECK_INT(chmod(disk, 0555), 0);
    /* dxl_trash_all, which never deletes for good: refused, said why. */
    CHECK_INT(dxl_trash_all(&dirs[0][2], 1, s.xdg.data_home, &e), -1);
    CHECK(strstr(dxl_err_msg(&e), "cannot go to a trash") != NULL);
    CHECK_INT(dxl_path_exists(dirs[0][2]), 1);
    char why[600];
    CHECK_INT(dxl_session_delete_profile(&s, "third", 0, why, sizeof why, &e), 2);
    for (int i = 0; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[0][i]), 1);
    CHECK(strstr(why, "/disk/state/") != NULL);
    CHECK(strstr(why, ".Trash-") != NULL && strstr(why, strerror(EACCES)) != NULL);
    CHECK(strchr(why, '\n') == NULL);
    CHECK(strstr(dxl_err_msg(&e), "cannot go to a trash") != NULL);
    CHECK(!scratch_exists(home, "data/Trash/files/third"));
    CHECK(!scratch_exists(home, "data/Trash/info/third.trashinfo"));
    CHECK_INT(dxl_path_exists(own), 0);
    /* The second question's yes. */
    CHECK_INT(dxl_session_delete_profile(&s, "third", 1, why, sizeof why, &e), 0);
    for (int i = 0; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[0][i]), 0);
    CHECK(scratch_exists(home, "data/Trash/files/third/x"));
    CHECK(scratch_exists(home, "data/Trash/files/third.2/x"));
    CHECK(!scratch_exists(home, "data/Trash/files/third.3"));
    CHECK_INT(dxl_path_exists(own), 0);

    /* A trash there: each to its own. */
    CHECK_INT(chmod(disk, 0755), 0);
    CHECK_INT(dxl_session_delete_profile(&s, "second", 0, why, sizeof why, &e), 0);
    for (int i = 0; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[1][i]), 0);
    CHECK(scratch_exists(home, "data/Trash/files/second/x"));
    CHECK(scratch_exists(home, "data/Trash/files/second.2/x"));
    CHECK(!scratch_exists(home, "data/Trash/files/second.3"));
    CHECK(scratch_exists(own, "files/second/x"));
    char *info = scratch_path(own, "info/second.trashinfo"), *text = text_of(info);
    CHECK(text && strstr(text, "[Trash Info]\nPath=state/dxlauncher/") == text);
    free(text);
    free(info);

    dxl_trash_test_top = NULL;
    for (int k = 0; k < 2; k++)
        for (int i = 0; i < 4; i++) free(dirs[k][i]);
    dxl_session_close(&s);
    dxl_env_set("DXL_HOME", NULL);
    free(state);
    free(disk_state);
    free(disk);
    scratch_remove(home);
    free(home);
}

/* A mount of its own on the home trash's device (a bind mount, a
 * container's view of the home), which no test may make: the tests' own
 * word for a path's mount. What is under it goes to that mount's top, a
 * profile's folder and a save alike, not to the home trash, which no
 * rename reaches from there; told no mounts, the device alone decides and
 * the home trash takes it. */
static const char *other_mount_root;
static uint64_t other_mount(const char *path) {
    size_t n = strlen(other_mount_root);
    return !strncmp(path, other_mount_root, n) && (path[n] == '/' || !path[n]) ? 7 : 3;
}

static void test_other_mount(void) {
    const char *dir = scratch_dir("trash-mount");
    char *data = scratch_path(dir, "home/data"), *made = scratch_path(dir, "disk");
    CHECK_INT(dxl_path_mkdirs(data), 0);
    CHECK_INT(dxl_path_mkdirs(made), 0);
    char *disk = realpath(made, NULL);
    CHECK(disk != NULL);
    char own[600];
    own_trash_name(disk, own, sizeof own);

    CHECK_INT(trash_one(disk, "before", data), 0);
    CHECK(scratch_exists(data, "Trash/files/before/x"));
    CHECK(!scratch_exists(own, "files/before"));

    other_mount_root = disk;
    dxl_trash_test_mount = other_mount;
    CHECK_INT(trash_one(disk, "games/a", data), 0);
    CHECK(scratch_exists(own, "files/a/x"));
    CHECK(!scratch_exists(data, "Trash/files/a"));
    char *info = scratch_path(own, "info/a.trashinfo"), *text = text_of(info);
    CHECK(text && strstr(text, "[Trash Info]\nPath=games/a\nDeletionDate=") == text);
    free(text);
    free(info);

    char *save = scratch_path(disk, "saves/s");
    CHECK_INT(dxl_path_mkdirs(save), 0);
    dxl_err e;
    CHECK_INT(dxl_trash(save, data, NULL, 0, &e), 0);
    CHECK(scratch_exists(own, "files/s"));
    CHECK(!scratch_exists(data, "Trash/files/s"));
    free(save);

    dxl_trash_test_mount = NULL;
    free(disk);
    free(made);
    free(data);
    scratch_remove(dir);
}

/* A trash on a folder's own device that its move is refused to as one
 * across file systems (another mount of it where mounts are not told, a
 * layered file system's folder), which no test can make: the tests' own
 * move. Each such folder is named as one no trash takes and nothing has
 * gone, what moved put back; on the yes each is deleted for good and the
 * rest is in the trash; a move that fails otherwise deletes nothing. */
static const char *refused_root;
static int refused_errno;
static int refused_move(const char *from, const char *to) {
    size_t n = strlen(refused_root);
    if (!strncmp(from, refused_root, n) && from[n] == '/') {
        errno = refused_errno;
        return -1;
    }
    return rename(from, to);
}

static void test_move_refused(void) {
    const char *dir = scratch_dir("trash-refused");
    char *data = scratch_path(dir, "home/data");
    CHECK_INT(dxl_path_mkdirs(data), 0);
    char *paths[3] = { scratch_path(dir, "keep/a"), scratch_path(dir, "away/b"), scratch_path(dir, "away/c") };
    for (int i = 0; i < 3; i++) {
        CHECK_INT(dxl_path_mkdirs(paths[i]), 0);
        scratch_write(paths[i], "x", "x");
    }
    char *made = scratch_path(dir, "away");
    char *away = realpath(made, NULL);
    CHECK(away != NULL);
    refused_root = away;
    refused_errno = EXDEV;
    dxl_trash_test_move = refused_move;

    dxl_err e;
    char why[1600];
    CHECK_INT(dxl_trash_or_delete(paths, 3, data, 0, why, sizeof why, &e), DXL_TRASH_NONE);
    CHECK(strstr(why, "/away/b: ") && strstr(why, "/away/c: ") && !strstr(why, "/keep/a"));
    CHECK(scratch_exists(dir, "keep/a/x") && scratch_exists(dir, "away/b/x") && scratch_exists(dir, "away/c/x"));
    CHECK(!scratch_exists(data, "Trash/files/a") && !scratch_exists(data, "Trash/info/a.trashinfo"));
    CHECK(!scratch_exists(data, "Trash/info/b.trashinfo") && !scratch_exists(data, "Trash/info/c.trashinfo"));

    refused_errno = EACCES;
    CHECK_INT(dxl_trash_or_delete(paths, 3, data, 1, why, sizeof why, &e), -1);
    CHECK(scratch_exists(dir, "keep/a/x") && scratch_exists(dir, "away/b/x") && scratch_exists(dir, "away/c/x"));
    CHECK(!scratch_exists(data, "Trash/files/a") && !scratch_exists(data, "Trash/info/a.trashinfo"));
    CHECK(!scratch_exists(data, "Trash/info/b.trashinfo"));

    refused_errno = EXDEV;
    CHECK_INT(dxl_trash_or_delete(paths, 3, data, 1, why, sizeof why, &e), 0);
    CHECK(scratch_exists(data, "Trash/files/a/x") && scratch_exists(data, "Trash/info/a.trashinfo"));
    CHECK(!scratch_exists(dir, "keep/a") && !scratch_exists(dir, "away/b") && !scratch_exists(dir, "away/c"));
    CHECK(!scratch_exists(data, "Trash/files/b") && !scratch_exists(data, "Trash/files/c"));
    CHECK(!scratch_exists(data, "Trash/info/b.trashinfo") && !scratch_exists(data, "Trash/info/c.trashinfo"));

    dxl_trash_test_move = NULL;
    free(away);
    free(made);
    for (int i = 0; i < 3; i++) free(paths[i]);
    free(data);
    scratch_remove(dir);
}

TEST_MAIN_BEGIN
    RUN(test_trash);
    RUN(test_long_paths);
    RUN(test_delete_cache_elsewhere);
    RUN(test_other_fs);
    RUN(test_profile_other_fs);
    RUN(test_top_trashes);
    RUN(test_profile_two_fs);
    RUN(test_other_mount);
    RUN(test_move_refused);
TEST_MAIN_END

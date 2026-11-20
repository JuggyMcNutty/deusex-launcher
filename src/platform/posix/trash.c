#define _GNU_SOURCE
#include "platform/trash.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/fs.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

const char *dxl_trash_test_top;
uint64_t (*dxl_trash_test_mount)(const char *path);

/* A path's mount, where the kernel tells it (Linux 5.8's statx): a folder
 * on another mount of the trash's own file system -- a bind mount, a
 * container's view of the home -- has its device and is yet no rename
 * away from it. 0 where it is not told, and the device alone decides
 * then. */
static uint64_t mount_of(const char *path, int follow) {
    if (dxl_trash_test_mount) return dxl_trash_test_mount(path);
#ifdef __linux__
    enum { MNT_ID = 0x1000, MNT_ID_AT = 144 };   /* STATX_MNT_ID and stx_mnt_id's place, by the kernel's layout */
    struct statx sx;
    memset(&sx, 0, sizeof sx);
    if (sizeof sx < MNT_ID_AT + sizeof(uint64_t)) return 0;
    if (statx(AT_FDCWD, path, follow ? 0 : AT_SYMLINK_NOFOLLOW, MNT_ID, &sx) != 0 || !(sx.stx_mask & MNT_ID)) return 0;
    uint64_t id;
    memcpy(&id, (const char *)&sx + MNT_ID_AT, sizeof id);
    return id;
#else
    (void)follow;
    return 0;
#endif
}

static int same_mount(uint64_t a, uint64_t b) { return !a || !b || a == b; }

int (*dxl_trash_test_move)(const char *from, const char *to);

static int move_to(const char *from, const char *to) {
    return dxl_trash_test_move ? dxl_trash_test_move(from, to) : rename(from, to);
}

/* The trash's own folder, made with its two. Caller frees. */
static char *trash_dir(const char *data_home) {
    char *dir = dxl_path_join(data_home, "Trash");
    char *files = dxl_path_join(dir, "files"), *info = dxl_path_join(dir, "info");
    dxl_path_mkdirs(files);
    dxl_path_mkdirs(info);
    chmod(dir, 0700);
    free(files);
    free(info);
    return dir;
}

int dxl_trash_can(const char *path, const char *data_home) {
    struct stat a, b;
    if (lstat(path, &a) != 0) return 0;
    char *dir = trash_dir(data_home);
    int same = stat(dir, &b) == 0 && a.st_dev == b.st_dev && same_mount(mount_of(path, 0), mount_of(dir, 1));
    free(dir);
    return same;
}

/* path, its bytes outside the unreserved ones and '/' escaped as a URL's.
 * 0, or -1 for one too long for out, which a cut Path= would put back
 * elsewhere (bugs.md LAPP.29). */
static int url_escape(const char *path, char *out, size_t size) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)path;
    for (; *p && n + 4 < size; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || strchr("/-._~", *p))
            out[n++] = (char)*p;
        else
            n += (size_t)snprintf(out + n, size - n, "%%%02X", *p);
    }
    out[n] = '\0';
    return *p ? -1 : 0;
}

/* The path, absolute: its folder resolved, its last name as it is, so that
 * a link is itself, not its target (dxl_trash_or_delete's check lstats it too).
 * NULL, with errno, for one that is not there. Caller frees. */
static char *absolute(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return NULL;
    if (!S_ISLNK(st.st_mode)) return realpath(path, NULL);
    char *dir = dxl_path_dirname(path), *real = realpath(dir, NULL);
    char *abs = real ? dxl_path_join(real, dxl_path_basename(path)) : NULL;
    free(dir);
    free(real);
    return abs;
}

/* A trash a thing goes to: its folder, and the folder its entries' Path is
 * relative to -- the top of a file system's own trash -- or NULL for the
 * home trash's, whose Path is absolute. */
typedef struct {
    char *dir;
    char *top;
} place;

static void place_free(place *pl) {
    free(pl->dir);
    free(pl->top);
    pl->dir = pl->top = NULL;
}

/* The folder of a file system's trash there, made 0700 if it is not, and
 * its files and info: each a folder of the user's own, no link, on dev.
 * 0, or -1 with errno. */
static int own_trash(const char *dir, dev_t dev) {
    char *files = dxl_path_join(dir, "files"), *info = dxl_path_join(dir, "info");
    const char *each[3] = { dir, files, info };
    int rc = 0;
    for (int i = 0; i < 3 && rc == 0; i++) {
        struct stat st;
        if (mkdir(each[i], 0700) != 0 && errno != EEXIST) rc = -1;
        else if (lstat(each[i], &st) != 0) rc = -1;
        else if (!S_ISDIR(st.st_mode) || st.st_uid != getuid()) {
            errno = S_ISLNK(st.st_mode) ? ELOOP : S_ISDIR(st.st_mode) ? EPERM : ENOTDIR;
            rc = -1;
        } else if (st.st_dev != dev) {
            errno = EXDEV;
            rc = -1;
        }
    }
    free(files);
    free(info);
    return rc;
}

/* The top of abs's file system, its mount point: the last of its folders,
 * going up, on dev and, where mounts are told, on abs's mount, mnt. Each
 * folder is stat'ed in turn; the mount table is not read, whose escaped
 * paths a folder's own name can make ambiguous. abs is realpath's, so its
 * '/'s alone part its names. Caller frees. */
static char *mount_top(const char *abs, dev_t dev, uint64_t mnt) {
    char *top = dxl_xstrdup(abs);
    for (;;) {
        char *slash = strrchr(top, '/');
        if (!slash) break;
        char *up = slash == top ? dxl_xstrdup("/") : dxl_xstrndup(top, (size_t)(slash - top));
        struct stat st;
        if (!strcmp(up, top) || stat(up, &st) != 0 || st.st_dev != dev || !same_mount(mnt, mount_of(up, 1))) {
            free(up);
            break;
        }
        free(top);
        top = up;
    }
    return top;
}

/* The trash abs goes to, as freedesktop.org's Trash spec has it: the home
 * trash where abs is on its file system, else its own file system's --
 * $topdir/.Trash/$uid where an administrator made $topdir/.Trash, a folder
 * with its sticky bit set, no link; else $topdir/.Trash-$uid, made at
 * need. 0, or -1 and why none takes it (bugs.md LAPP.26). */
static int find_place(const char *abs, const char *data_home, place *pl, char *why, size_t why_size) {
    pl->dir = pl->top = NULL;
    struct stat st;
    if (lstat(abs, &st) != 0) {
        snprintf(why, why_size, "%s", strerror(errno));
        return -1;
    }
    dev_t dev = st.st_dev;
    uint64_t mnt = mount_of(abs, 0);
    size_t tn = dxl_trash_test_top ? strlen(dxl_trash_test_top) : 0;
    int test = tn && !strncmp(abs, dxl_trash_test_top, tn) && abs[tn] == '/';
    if (!test) {
        char *home = trash_dir(data_home);
        struct stat hs;
        if (stat(home, &hs) == 0 && hs.st_dev == dev && same_mount(mnt, mount_of(home, 1))) {
            pl->dir = home;
            return 0;
        }
        free(home);
    }
    char *top = test ? dxl_xstrdup(dxl_trash_test_top) : mount_top(abs, dev, mnt);
    if (!strcmp(top, abs)) {
        snprintf(why, why_size, "it is the top of a file system of its own, which no trash in it can take");
        free(top);
        return -1;
    }
    char uid[32];
    snprintf(uid, sizeof uid, "%lu", (unsigned long)getuid());
    char *admin = dxl_path_join(top, ".Trash");
    if (lstat(admin, &st) == 0) {
        if (S_ISDIR(st.st_mode) && (st.st_mode & S_ISVTX) && st.st_dev == dev) {
            char *dir = dxl_path_join(admin, uid);
            if (own_trash(dir, dev) == 0) {
                free(admin);
                pl->dir = dir;
                pl->top = top;
                return 0;
            }
            dxl_log("warning: %s cannot be a trash: %s", dir, strerror(errno));
            free(dir);
        } else {
            /* The spec's checks failed: not used, and said for whoever
             * keeps the machine. */
            dxl_log("warning: %s is no trash: not a folder with its sticky bit set, or a link", admin);
        }
    }
    free(admin);
    char name[48];
    snprintf(name, sizeof name, ".Trash-%s", uid);
    char *dir = dxl_path_join(top, name);
    if (own_trash(dir, dev) != 0) {
        int e = errno;
        snprintf(why, why_size, "no trash can be made on its file system, %s/%s: %s", strcmp(top, "/") ? top : "",
                 name, e == EPERM ? "not the user's own" : strerror(e));
        free(dir);
        free(top);
        return -1;
    }
    pl->dir = dir;
    pl->top = top;
    return 0;
}

/* One thing's way to its trash: the name it takes there, its .trashinfo
 * and the place it goes to. */
typedef struct {
    char *abs;
    place pl;
    char  name[512], info[4096], to[4096];
    int   reserved, moved;
} entry;

/* abs's name in pl reserved by its .trashinfo, made alone and written
 * whole: a name taken gets ".2", ".3"...; a path cut short would name
 * another place, outside the trash's files (bugs.md LAPP.29). 0, or -1 and
 * why, nothing left behind. */
static int reserve(entry *en, dxl_err *err) {
    const char *rel = en->abs;
    if (en->pl.top) {
        /* Relative to the top of its file system, as the spec has it for
         * the trashes there. */
        size_t n = strlen(en->pl.top);
        rel = en->abs + (strcmp(en->pl.top, "/") ? n : 0);
        while (*rel == '/') rel++;
    }
    char escaped[4096], when[32];
    if (url_escape(rel, escaped, sizeof escaped) != 0) {
        dxl_err_set(err, "a path too long for the trash: %s", en->abs);
        return -1;
    }
    const char *base = dxl_path_basename(en->abs);
    int fd = -1;
    for (int n = 1; n < 1000 && fd < 0; n++) {
        if (n == 1) snprintf(en->name, sizeof en->name, "%s", base);
        else snprintf(en->name, sizeof en->name, "%s.%d", base, n);
        if (snprintf(en->info, sizeof en->info, "%s/info/%s.trashinfo", en->pl.dir, en->name) >= (int)sizeof en->info ||
            snprintf(en->to, sizeof en->to, "%s/files/%s", en->pl.dir, en->name) >= (int)sizeof en->to) {
            errno = ENAMETOOLONG;
            break;
        }
        struct stat st;
        if (lstat(en->to, &st) == 0) continue;
        fd = open(en->info, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0 && errno != EEXIST) break;
    }
    if (fd < 0) {
        dxl_err_set(err, "no name in %s/info: %s", en->pl.dir, strerror(errno));
        return -1;
    }
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%S", &tm);
    char text[4200];
    int len = snprintf(text, sizeof text, "[Trash Info]\nPath=%s\nDeletionDate=%s\n", escaped, when);
    /* Written and closed whole: a full disk is found here, before anything
     * moves. */
    errno = 0;
    int wrote = len > 0 && write(fd, text, (size_t)len) == len;
    if (close(fd) != 0) wrote = 0;
    if (!wrote) {
        dxl_err_set(err, "no room for %s: %s", en->info, strerror(errno ? errno : ENOSPC));
        unlink(en->info);
        return -1;
    }
    en->reserved = 1;
    return 0;
}

/* path removed for good, its links and not what they name, and nothing on
 * a file system other than dev, a mount inside it left where it is (its
 * folder then not removed). 0, or -1 with errno for what is left. */
static int remove_on(const char *path, dev_t dev) {
    struct stat st;
    if (lstat(path, &st) != 0) return errno == ENOENT ? 0 : -1;
    if (st.st_dev != dev) {
        errno = EXDEV;
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) return unlink(path);
    int rc = 0, e = 0;
    DIR *d = opendir(path);
    if (!d) return -1;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        char *p = dxl_path_join(path, de->d_name);
        if (remove_on(p, dev) != 0 && !e) e = errno;
        free(p);
    }
    closedir(d);
    if (rmdir(path) != 0) rc = -1;
    if (rc && e) errno = e;
    return rc;
}

int dxl_trash(const char *path, const char *data_home, char *trashed, size_t trashed_size, dxl_err *err) {
    entry en;
    memset(&en, 0, sizeof en);
    en.abs = absolute(path);
    if (!en.abs) {
        dxl_err_set(err, "%s: %s", path, strerror(errno));
        return -1;
    }
    /* To the trash of its own file system, as a profile's folders go
     * (bugs.md LAPP.26); where none takes it, said. */
    char because[600];
    if (find_place(en.abs, data_home, &en.pl, because, sizeof because) != 0) {
        dxl_err_set(err, "%s cannot go to a trash: %s", en.abs, because);
        free(en.abs);
        return -1;
    }
    int rc = reserve(&en, err);
    if (rc == 0 && move_to(en.abs, en.to) != 0) {
        dxl_err_set(err, "cannot move %s to the trash: %s", en.abs, strerror(errno));
        unlink(en.info);
        rc = -1;
    }
    if (rc == 0 && trashed) snprintf(trashed, trashed_size, "%s", en.name);
    place_free(&en.pl);
    free(en.abs);
    return rc;
}

int dxl_trash_undo(const char *data_home, const char *name, const char *path, dxl_err *err) {
    char *dir = dxl_path_join(data_home, "Trash");
    char from[4096], info[4096];
    int cut = snprintf(from, sizeof from, "%s/files/%s", dir, name) >= (int)sizeof from ||
              snprintf(info, sizeof info, "%s/info/%s.trashinfo", dir, name) >= (int)sizeof info;
    free(dir);
    if (cut) {
        dxl_err_set(err, "cannot put %s back: its path in the trash is too long", name);
        return -1;
    }
    if (dxl_path_exists(path) || rename(from, path) != 0) {
        dxl_err_set(err, "cannot put %s back at %s: %s", from, path, dxl_path_exists(path) ? "it is there" : strerror(errno));
        return -1;
    }
    unlink(info);
    return 0;
}

int dxl_trash_all(char *const paths[], int n, const char *data_home, dxl_err *err) {
    int rc = dxl_trash_or_delete(paths, n, data_home, 0, NULL, 0, err);
    return rc == DXL_TRASH_NONE ? -1 : rc;
}

int dxl_trash_or_delete(char *const paths[], int n, const char *data_home, int for_good, char *why,
                        size_t why_size, dxl_err *err) {
    entry *en = dxl_xmalloc(sizeof *en * (size_t)(n > 0 ? n : 1));
    memset(en, 0, sizeof *en * (size_t)(n > 0 ? n : 1));
    if (why && why_size) why[0] = '\0';
    int rc = 0, none = 0;
    size_t told = 0;
    /* Where each goes, every trash made: what no trash takes, found. */
    for (int i = 0; i < n && rc == 0; i++) {
        struct stat st;
        if (lstat(paths[i], &st) != 0) continue;
        en[i].abs = absolute(paths[i]);
        if (!en[i].abs) {
            dxl_err_set(err, "%s: %s: nothing was deleted", paths[i], strerror(errno));
            rc = -1;
            break;
        }
        char because[600];
        if (find_place(en[i].abs, data_home, &en[i].pl, because, sizeof because) != 0) {
            none++;
            if (why && told < why_size)
                told += (size_t)snprintf(why + told, why_size - told, "%s%s: %s", told ? "\n" : "", en[i].abs, because);
            if (!for_good) dxl_err_set(err, "%s cannot go to a trash: %s", en[i].abs, because);
        }
    }
    /* Each name taken, each .trashinfo written, before anything moves. */
    for (int i = 0; i < n && rc == 0; i++)
        if (en[i].pl.dir && reserve(&en[i], err) != 0) rc = -1;
    /* Then each moved; one that fails puts back those before it. A move
     * the system refuses as one across file systems, to a trash on the
     * folder's own device -- another mount of it where mounts are not
     * told, a folder of a layered file system -- is no failure: no trash
     * takes that folder, as those found above, and the rest are still
     * tried, that every such folder is known when the player is asked. */
    int failed = -1;
    for (int i = 0; i < n && rc == 0; i++) {
        if (!en[i].reserved) continue;
        if (move_to(en[i].abs, en[i].to) == 0) {
            en[i].moved = 1;
        } else if (errno == EXDEV) {
            char because[160];
            snprintf(because, sizeof because, "the move to its file system's trash is refused: %s", strerror(EXDEV));
            unlink(en[i].info);
            en[i].reserved = 0;
            place_free(&en[i].pl);
            none++;
            if (why && told < why_size)
                told += (size_t)snprintf(why + told, why_size - told, "%s%s: %s", told ? "\n" : "", en[i].abs, because);
            if (!for_good) dxl_err_set(err, "%s cannot go to a trash: %s", en[i].abs, because);
        } else {
            dxl_err_set(err, "cannot move %s to the trash: %s", en[i].abs, strerror(errno));
            failed = i;
            rc = -1;
        }
    }
    /* One no trash takes, and no yes to its going for good: nothing is
     * done, what moved put back. */
    if (rc == 0 && none && !for_good) {
        failed = n;
        rc = DXL_TRASH_NONE;
    }
    if (failed >= 0) {
        /* What is where, said: each put back, or where it stays. */
        char stays[DXL_ERR_LEN] = "";
        for (int i = failed - 1; i >= 0; i--) {
            if (!en[i].moved) continue;
            if (rename(en[i].to, en[i].abs) == 0) {
                unlink(en[i].info);
                en[i].moved = 0;
            } else {
                dxl_log("warning: %s stays in the trash as %s", en[i].abs, en[i].to);
                if (!stays[0]) snprintf(stays, sizeof stays, "%s", en[i].to);
            }
        }
        dxl_err e;
        snprintf(e.msg, sizeof e.msg, "%s", err ? err->msg : "");
        if (stays[0]) {
            dxl_err_set(err, "%s; %s stays in the trash; the rest is where it was", e.msg, stays);
            rc = -1;
        } else if (rc == -1) {
            dxl_err_set(err, "%s: nothing was deleted", e.msg);
        }
    }
    for (int i = 0; i < n; i++)
        if (en[i].reserved && !en[i].moved) unlink(en[i].info);
    /* Last, what no trash takes, for good: what cannot be put back goes
     * once the rest is in its trash. */
    for (int i = 0; i < n && rc == 0 && for_good; i++) {
        if (!en[i].abs || en[i].pl.dir) continue;
        struct stat st;
        if (lstat(en[i].abs, &st) == 0 && remove_on(en[i].abs, st.st_dev) != 0) {
            dxl_log("warning: %s is left: %s", en[i].abs, strerror(errno));
            dxl_err_set(err, "%s could not be deleted whole: %s", en[i].abs, strerror(errno));
            rc = 1;
        } else {
            dxl_log("%s: deleted for good, no trash taking it", en[i].abs);
        }
    }
    for (int i = 0; i < n; i++) {
        free(en[i].abs);
        place_free(&en[i].pl);
    }
    free(en);
    return rc;
}

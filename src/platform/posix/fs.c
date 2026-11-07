#define _GNU_SOURCE
#include "platform/fs.h"
#include "core/paths.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

size_t dxl_fs_root_len(const char *path) { return path && path[0] == '/' ? 1 : 0; }

int dxl_fs_is_absolute(const char *path) { return dxl_fs_root_len(path) > 0; }

const char *dxl_fs_home(void) {
    const char *h = getenv("HOME");
    if (h && h[0] == '/') return h;
    struct passwd *pw = getpwuid(getuid());
    return pw && pw->pw_dir && pw->pw_dir[0] == '/' ? pw->pw_dir : NULL;
}

char *dxl_fs_path(const char *outside) { return dxl_xstrdup(outside ? outside : ""); }

char *dxl_fs_stored(const char *dir, const char *leaf) {
    char *p = dxl_path_join(dir, leaf);
    struct stat st;
    if (stat(p, &st) == 0) return p;
    free(p);
    return NULL;
}

char *dxl_fs_self(void) {
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n > 0) { buf[n] = '\0'; return dxl_xstrdup(buf); }
    return NULL;
}

char *dxl_fs_real(const char *path) { return path ? realpath(path, NULL) : NULL; }

char *dxl_fs_cwd(void) {
    char buf[PATH_MAX];
    return getcwd(buf, sizeof buf) ? dxl_xstrdup(buf) : NULL;
}

int dxl_fs_mkdir(const char *path) { return mkdir(path, 0755); }

int dxl_fs_replace(const char *from, const char *to) { return rename(from, to); }

int dxl_fs_is_link(const char *path) {
    struct stat st;
    return path && lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
}

int dxl_fs_link_dir(const char *target, const char *link) { return symlink(target, link); }

int dxl_fs_is_program(const char *path) { return path && access(path, X_OK) == 0; }

/* "e": not inherited by a program the launcher starts. */
FILE *dxl_fs_open_log(const char *path) { return fopen(path, "abe"); }

/* The file's mode as it was, or 0644: changed only where it differs, and a
 * filesystem with no permissions to keep (exFAT, FAT) refusing the change
 * is no failure. ENOTSUP is EOPNOTSUPP on Linux. */
static int keep_mode(int fd, mode_t mode) {
    struct stat st;
    if (fstat(fd, &st) == 0 && (st.st_mode & 07777) == mode) return 0;
    if (fchmod(fd, mode) == 0) return 0;
    return errno == EPERM || errno == EOPNOTSUPP ? 0 : -1;
}

/* Where a link leads, followed link by link as the system follows one, a
 * relative target from its link's folder: the end, there or not yet; NULL
 * for a loop or a link that cannot be read. Caller frees. */
static char *link_end(const char *path) {
    char *at = dxl_xstrdup(path);
    for (int hops = 0; hops < 40; hops++) {
        struct stat st;
        char to[PATH_MAX];
        if (lstat(at, &st) != 0 || !S_ISLNK(st.st_mode)) return at;
        ssize_t n = readlink(at, to, sizeof to - 1);
        if (n <= 0) break;
        to[n] = '\0';
        const char *slash = strrchr(at, '/');
        size_t keep = to[0] == '/' || !slash ? 0 : (size_t)(slash - at) + 1;
        char *next = dxl_xmalloc(keep + (size_t)n + 1);
        memcpy(next, at, keep);
        memcpy(next + keep, to, (size_t)n + 1);
        free(at);
        at = next;
    }
    free(at);
    return NULL;
}

int dxl_fs_write_atomic(const char *path, const void *data, size_t len, dxl_err *err) {
    /* A linked file (a dotfiles repository's) is written where it points:
     * the rename would put a file in the link's place, its target stale.
     * One that points nowhere yet is made where it points, as the engine's
     * write through the link makes it; one that cannot be followed, a loop,
     * is refused, as the engine's write fails on it. */
    struct stat ls;
    char *target = NULL;
    if (lstat(path, &ls) == 0 && S_ISLNK(ls.st_mode)) {
        if (!(target = link_end(path))) {
            dxl_err_set(err, "%s: its link cannot be followed", path);
            return -1;
        }
        path = target;
    }
    /* A read-only file is refused, not replaced: the original's write in
     * place fails on one, as Windows' replace does. One whose every write
     * bit is off is read-only to root too, as Windows' read-only attribute
     * is to an administrator, where access() passes every file for root,
     * the Smart Pro's user (bugs.md LCORE.10). */
    struct stat ro;
    if (stat(path, &ro) == 0 && (access(path, W_OK) != 0 || !(ro.st_mode & 0222))) {
        dxl_err_set(err, "%s is read-only", path);
        free(target);
        return -1;
    }
    char *dir = dxl_path_dirname(path);
    size_t n = strlen(dir) + strlen(dxl_path_basename(path)) + 16;
    char *tmp = dxl_xmalloc(n);
    snprintf(tmp, n, "%s/.%s.XXXXXX", dir, dxl_path_basename(path));

    int rc = -1;
    int fd = mkostemp(tmp, O_CLOEXEC);
    if (fd < 0) {
        dxl_err_set(err, "cannot write in %s: %s", dir, strerror(errno));
        goto out;
    }
    const char *p = data;
    size_t left = len;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) break;
        p += w;
        left -= (size_t)w;
    }
    struct stat old;
    mode_t mode = stat(path, &old) == 0 ? (old.st_mode & 07777) : 0644;
    int ok = left == 0 && keep_mode(fd, mode) == 0 && fsync(fd) == 0;
    ok = close(fd) == 0 && ok;
    if (!ok) {
        dxl_err_set(err, "cannot write %s: %s", tmp, strerror(errno));
        unlink(tmp);
        goto out;
    }
    if (rename(tmp, path) != 0) {
        dxl_err_set(err, "cannot replace %s: %s", path, strerror(errno));
        unlink(tmp);
        goto out;
    }
    /* The rename itself is in the folder: synced too, or a power cut can
     * lose it. */
    int dfd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dfd >= 0) {
        fsync(dfd);
        close(dfd);
    }
    rc = 0;
out:
    free(tmp);
    free(dir);
    free(target);
    return rc;
}

char **dxl_fs_roots(void) {
    char **roots = dxl_xmalloc(2 * sizeof *roots);
    roots[0] = dxl_xstrdup("/");
    roots[1] = NULL;
    return roots;
}

void dxl_fs_roots_free(char **roots) {
    for (size_t i = 0; roots && roots[i]; i++) free(roots[i]);
    free(roots);
}

char *dxl_fs_temp_dir(void) {
    const char *t = getenv("TMPDIR");
    return dxl_xstrdup(t && t[0] == '/' ? t : "/tmp");
}

int dxl_fs_set_mtime(const char *path, long when) {
    struct timeval tv[2] = { { when, 0 }, { when, 0 } };
    return utimes(path, tv);
}

int dxl_fs_remove_tree(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return errno == ENOENT ? 0 : -1;
    if (!S_ISDIR(st.st_mode)) return unlink(path);
    DIR *d = opendir(path);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            size_t n = strlen(path) + strlen(e->d_name) + 2;
            char *p = dxl_xmalloc(n);
            snprintf(p, n, "%s/%s", path, e->d_name);
            dxl_fs_remove_tree(p);
            free(p);
        }
        closedir(d);
    }
    return rmdir(path);
}

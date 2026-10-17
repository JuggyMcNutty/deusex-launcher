#define _GNU_SOURCE
#include "platform/trash.h"
#include "core/paths.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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
    int same = stat(dir, &b) == 0 && a.st_dev == b.st_dev;
    free(dir);
    return same;
}

/* path, its bytes outside the unreserved ones and '/' escaped as a URL's. */
static void url_escape(const char *path, char *out, size_t size) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)path; *p && n + 4 < size; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || strchr("/-._~", *p))
            out[n++] = (char)*p;
        else
            n += (size_t)snprintf(out + n, size - n, "%%%02X", *p);
    }
    out[n] = '\0';
}

int dxl_trash(const char *path, const char *data_home, char *trashed, size_t trashed_size, dxl_err *err) {
    char *abs = realpath(path, NULL);
    if (!abs) {
        dxl_err_set(err, "%s: %s", path, strerror(errno));
        return -1;
    }
    if (!dxl_trash_can(abs, data_home)) {
        dxl_err_set(err, "%s is not on the trash's filesystem", abs);
        free(abs);
        return -1;
    }
    char *dir = trash_dir(data_home);
    const char *base = dxl_path_basename(abs);
    char name[512], info[1024], to[1024];
    int fd = -1;
    /* The name reserved by its .trashinfo, made alone. */
    for (int n = 1; n < 1000 && fd < 0; n++) {
        if (n == 1) snprintf(name, sizeof name, "%s", base);
        else snprintf(name, sizeof name, "%s.%d", base, n);
        snprintf(info, sizeof info, "%s/info/%s.trashinfo", dir, name);
        snprintf(to, sizeof to, "%s/files/%s", dir, name);
        if (dxl_path_exists(to)) continue;
        fd = open(info, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0 && errno != EEXIST) break;
    }
    if (fd < 0) {
        dxl_err_set(err, "no name in %s/info: %s", dir, strerror(errno));
        free(dir);
        free(abs);
        return -1;
    }
    char escaped[4096], when[32];
    url_escape(abs, escaped, sizeof escaped);
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%S", &tm);
    char text[4200];
    int len = snprintf(text, sizeof text, "[Trash Info]\nPath=%s\nDeletionDate=%s\n", escaped, when);
    int wrote = len > 0 && write(fd, text, (size_t)len) == len;
    close(fd);
    if (!wrote || rename(abs, to) != 0) {
        dxl_err_set(err, "cannot move %s to the trash: %s", abs, strerror(errno));
        unlink(info);
        free(dir);
        free(abs);
        return -1;
    }
    if (trashed) snprintf(trashed, trashed_size, "%s", name);
    free(dir);
    free(abs);
    return 0;
}

int dxl_trash_undo(const char *data_home, const char *name, const char *path, dxl_err *err) {
    char *dir = dxl_path_join(data_home, "Trash");
    char from[1024], info[1024];
    snprintf(from, sizeof from, "%s/files/%s", dir, name);
    snprintf(info, sizeof info, "%s/info/%s.trashinfo", dir, name);
    free(dir);
    if (dxl_path_exists(path) || rename(from, path) != 0) {
        dxl_err_set(err, "cannot put %s back at %s: %s", from, path, dxl_path_exists(path) ? "it is there" : strerror(errno));
        return -1;
    }
    unlink(info);
    return 0;
}

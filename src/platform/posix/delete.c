#define _GNU_SOURCE
#include "platform/delete.h"

#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* nftw's walk keeps no state of its own for its callback: the first
 * failure, kept here. */
static int failed_errno;
static char failed_path[512];

static int remove_one(const char *path, const struct stat *st, int flag, struct FTW *ftw) {
    (void)st;
    (void)flag;
    (void)ftw;
    if (remove(path) != 0 && !failed_errno) {
        failed_errno = errno;
        snprintf(failed_path, sizeof failed_path, "%s", path);
    }
    return 0;
}

int dxl_delete(const char *path, dxl_err *err) {
    struct stat st;
    if (lstat(path, &st) != 0) {
        dxl_err_set(err, "cannot delete %s: %s", path, strerror(errno));
        return -1;
    }
    failed_errno = 0;
    /* Depth first, links as links: a folder's contents before it. */
    if (nftw(path, remove_one, 16, FTW_DEPTH | FTW_PHYS) != 0 && !failed_errno) {
        failed_errno = errno;
        snprintf(failed_path, sizeof failed_path, "%s", path);
    }
    if (failed_errno) {
        dxl_err_set(err, "cannot delete %s: %s", failed_path, strerror(failed_errno));
        return -1;
    }
    return 0;
}

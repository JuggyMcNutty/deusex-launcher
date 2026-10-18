#define _GNU_SOURCE
#include "platform/desktop.h"
#include "platform/process.h"
#include "core/common.h"
#include "core/paths.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const int dxl_desktop_icon_sizes[] = { 32, 48, 64, 128, 256 };
const int dxl_desktop_icon_count = (int)(sizeof dxl_desktop_icon_sizes / sizeof *dxl_desktop_icon_sizes);

char *dxl_desktop_entry_path(const char *data_home) {
    char leaf[128];
    snprintf(leaf, sizeof leaf, "applications/%s.desktop", DXL_APP_ID);
    return dxl_path_join(data_home, leaf);
}

char *dxl_desktop_icon_path(const char *data_home, int px) {
    char leaf[160];
    snprintf(leaf, sizeof leaf, "icons/hicolor/%dx%d/apps/%s.png", px, px, DXL_APP_ID);
    return dxl_path_join(data_home, leaf);
}

char *dxl_desktop_exec(const char *program) {
    static const char reserved[] = " \t\n\"'\\><~|&;$*?#()`";
    dxl_buf b;
    dxl_buf_init(&b);
    int quote = strpbrk(program, reserved) != NULL;
    if (quote) dxl_buf_add(&b, "\"", 1);
    for (const char *p = program; *p; p++) {
        /* Inside the quotes, ", `, $ and \ take a backslash; then the
         * string type's own escape doubles every backslash; % is a field
         * code's, doubled. */
        if (quote && strchr("\"`$\\", *p)) dxl_buf_add(&b, "\\\\", 2);
        if (*p == '\\') dxl_buf_add(&b, "\\\\", 2);
        else if (*p == '%') dxl_buf_add(&b, "%%", 2);
        else dxl_buf_add(&b, p, 1);
    }
    if (quote) dxl_buf_add(&b, "\"", 1);
    return b.data ? b.data : dxl_xstrdup("");
}

/* The applications folder's MimeType cache made again, where the desktop
 * has the tool for it: xdg-open and GIO find a scheme's handler there. */
static void refresh_cache(const char *data_home) {
    char *apps = dxl_path_join(data_home, "applications");
    char *argv[] = { "update-desktop-database", "-q", apps, NULL };
    char out[256], errs[256];
    int status = 0;
    dxl_err e;
    dxl_run_capture(argv, 5000, out, sizeof out, errs, sizeof errs, &status, &e);
    free(apps);
}

int dxl_desktop_install(const char *data_home, const char *template_path, const char *program, dxl_err *err) {
    FILE *f = fopen(template_path, "rb");
    if (!f) {
        dxl_err_set(err, "cannot read %s: %s", template_path, strerror(errno));
        return -1;
    }
    dxl_buf in;
    dxl_buf_init(&in);
    char chunk[1024];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) dxl_buf_add(&in, chunk, n);
    fclose(f);
    char *exec = dxl_desktop_exec(program);
    dxl_buf out;
    dxl_buf_init(&out);
    const char *p = in.data ? in.data : "";
    for (const char *at; (at = strstr(p, "@EXEC@"));) {
        dxl_buf_add(&out, p, (size_t)(at - p));
        dxl_buf_puts(&out, exec);
        p = at + 6;
    }
    dxl_buf_puts(&out, p);
    free(exec);
    dxl_buf_free(&in);

    char *path = dxl_desktop_entry_path(data_home);
    char *dir = dxl_path_dirname(path);
    int rc = -1;
    if (dxl_path_mkdirs(dir) != 0) dxl_err_set(err, "cannot create %s", dir);
    else rc = dxl_path_write_atomic(path, out.data ? out.data : "", out.len, err);
    if (rc == 0) refresh_cache(data_home);
    free(dir);
    free(path);
    dxl_buf_free(&out);
    return rc;
}

int dxl_desktop_remove(const char *data_home, dxl_err *err) {
    int rc = 0;
    char *path = dxl_desktop_entry_path(data_home);
    if (unlink(path) != 0 && errno != ENOENT) {
        dxl_err_set(err, "cannot remove %s: %s", path, strerror(errno));
        rc = -1;
    }
    free(path);
    for (int i = 0; i < dxl_desktop_icon_count; i++) {
        char *icon = dxl_desktop_icon_path(data_home, dxl_desktop_icon_sizes[i]);
        unlink(icon);
        free(icon);
    }
    refresh_cache(data_home);
    return rc;
}

int dxl_desktop_installed(const char *data_home) {
    char *path = dxl_desktop_entry_path(data_home);
    int there = dxl_path_exists(path);
    free(path);
    return there;
}

#include "app/gamehook.h"
#include "core/cpumode.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The last line of text, without its end: what the hook says it did. */
static void last_line(const char *text, char *out, size_t size) {
    size_t n = strlen(text);
    while (n && (text[n - 1] == '\n' || text[n - 1] == '\r')) n--;
    size_t start = n;
    while (start && text[start - 1] != '\n') start--;
    snprintf(out, size, "%.*s", (int)(n - start), text + start);
}

void dxl_gamehook(const dxl_run *r, const char *log_path, const char *when) {
    char *hook = dxl_path_join(r->session.app_dir, DXL_GAMEHOOK);
    if (!dxl_path_exists(hook)) {
        free(hook);
        return;
    }
    const char *mode = !strcmp(when, "before") ? dxl_cpu_mode(r->session.ini) : NULL;
    char *argv[] = { "/bin/sh", hook, (char *)when, (char *)mode, NULL };
    char out[2048], errs[1024], said[256];
    int status = -1;
    dxl_err e;
    int rc = dxl_run_capture(argv, DXL_GAMEHOOK_MS, out, sizeof out, errs, sizeof errs, &status, &e);
    FILE *f = log_path ? fopen(log_path, "a") : NULL;
    if (f) {
        fprintf(f, "%s%s", out, errs);
        if (rc != 0) fprintf(f, "%s %s: %s\n", DXL_GAMEHOOK, when, status == 128 + 9 ? "out of time" : dxl_err_msg(&e));
        fclose(f);
    }
    last_line(out, said, sizeof said);
    if (rc == 0)
        dxl_log("%s %s%s%s: exit %d%s%s", DXL_GAMEHOOK, when, mode ? " " : "", mode ? mode : "", status,
                said[0] ? ", " : "", said);
    else
        dxl_log("%s %s: %s; the game goes on", DXL_GAMEHOOK, when,
                status == 128 + 9 ? "out of time" : dxl_err_msg(&e));
    free(hook);
}

#include "core/log.h"
#include "platform/fs.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *log_file;
static int   log_echo = 1;
static int   log_to_file = 1;

void dxl_log_set_to_file(int on) { log_to_file = on; }

/* Never inherited by a program the launcher starts. */
static void open_log(const char *path) {
    if (log_file) { fclose(log_file); log_file = NULL; }
    if (!log_to_file || !path) return;
    log_file = dxl_fs_open_log(path);
}

void dxl_log_open(const char *path) { open_log(path); }
/* Emptied first, then written in append mode like the other: whatever a
 * child adds to the same file meanwhile is not written over. */
void dxl_log_open_new(const char *path) {
    if (log_to_file && path) {
        FILE *f = fopen(path, "wb");
        if (f) fclose(f);
    }
    open_log(path);
}

void dxl_log_close(void) {
    if (log_file) { fclose(log_file); log_file = NULL; }
}

void dxl_log_set_echo(int on) { log_echo = on; }

void dxl_log(const char *fmt, ...) {
    char stamp[32];
    time_t now = time(NULL);
    struct tm tm;
    if (localtime_r(&now, &tm))
        strftime(stamp, sizeof stamp, "%H:%M:%S", &tm);
    else
        strcpy(stamp, "--:--:--");

    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    /* Others' text among it -- a server's, a master's refusal -- acts on no
     * terminal the log is shown in. */
    dxl_text_no_controls(msg, msg, sizeof msg);

    if (log_file) {
        fprintf(log_file, "[%s] %s\n", stamp, msg);
        fflush(log_file);
    }
    if (log_echo) fprintf(stderr, "[%s] %s\n", stamp, msg);
}

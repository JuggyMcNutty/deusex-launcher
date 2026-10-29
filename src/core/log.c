#include "core/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

static FILE *log_file;

/* "e": not inherited by a program the launcher starts. */
static void open_mode(const char *path, const char *mode) {
    if (log_file) { fclose(log_file); log_file = NULL; }
    if (!path) return;
    log_file = fopen(path, mode);
}

void dxl_log_open(const char *path) { open_mode(path, "abe"); }
/* Emptied first by the launcher that holds it, then written in append mode
 * like the other: whatever a child adds to the same file meanwhile is not
 * written over. Held: an exclusive lock on it, kept while it is open, as
 * the original's first writer holds its log, a launch started at the same
 * moment then adding to it, never emptying it. */
void dxl_log_open_new(const char *path) {
    open_mode(path, "abe");
    if (log_file && flock(fileno(log_file), LOCK_EX | LOCK_NB) == 0 && ftruncate(fileno(log_file), 0) != 0) {}
}

void dxl_log_close(void) {
    if (log_file) { fclose(log_file); log_file = NULL; }
}

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

    if (log_file) {
        fprintf(log_file, "[%s] %s\n", stamp, msg);
        fflush(log_file);
    }
    fprintf(stderr, "[%s] %s\n", stamp, msg);
}

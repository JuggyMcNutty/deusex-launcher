#include "core/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *log_file;
static int   log_echo = 1;
static int   log_to_file = 1;

void dxl_log_set_to_file(int on) { log_to_file = on; }

void dxl_log_open(const char *path) {
    if (log_file) { fclose(log_file); log_file = NULL; }
    if (!log_to_file || !path) return;
    log_file = fopen(path, "ab");
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

    if (log_file) {
        fprintf(log_file, "[%s] %s\n", stamp, msg);
        fflush(log_file);
    }
    if (log_echo) fprintf(stderr, "[%s] %s\n", stamp, msg);
}

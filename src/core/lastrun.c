#include "core/lastrun.h"
#include "core/ini.h"
#include "core/paths.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SECTION "LastRun"
#define ERROR_PREFIX "DXEngine error: "

int dxl_lastrun_load(dxl_lastrun *r, const char *state_dir) {
    memset(r, 0, sizeof *r);
    char *path = dxl_path_join(state_dir, "LastRun.ini");
    dxl_ini *ini = dxl_ini_load(path, NULL);
    free(path);
    if (!ini) return -1;
    r->known = 1;
    const char *ended = dxl_ini_get(ini, SECTION, "Ended");
    r->ended = ended ? strtol(ended, NULL, 10) : 0;
    r->status = dxl_ini_get_int(ini, SECTION, "Status", 0);
    r->signal = dxl_ini_get_int(ini, SECTION, "Signal", 0);
    r->ready = dxl_ini_get_bool(ini, SECTION, "Ready", 0);
    r->clean = dxl_ini_get_bool(ini, SECTION, "Clean", 0);
    const char *e = dxl_ini_get(ini, SECTION, "Error");
    snprintf(r->error, sizeof r->error, "%s", e ? e : "");
    const char *start = dxl_ini_get(ini, SECTION, "Start");
    snprintf(r->start, sizeof r->start, "%s", start ? start : "");
    dxl_ini_free(ini);
    return 0;
}

int dxl_lastrun_save(const dxl_lastrun *r, const char *state_dir, dxl_err *err) {
    if (dxl_path_mkdirs(state_dir) != 0) {
        dxl_err_set(err, "cannot create %s", state_dir);
        return -1;
    }
    dxl_ini *ini = dxl_ini_new();
    dxl_ini_set_eol(ini, "\n");
    char n[32];
    snprintf(n, sizeof n, "%ld", r->ended);
    dxl_ini_set(ini, SECTION, "Ended", n);
    dxl_ini_set_int(ini, SECTION, "Status", r->status);
    dxl_ini_set_int(ini, SECTION, "Signal", r->signal);
    dxl_ini_set_bool(ini, SECTION, "Ready", r->ready);
    dxl_ini_set_bool(ini, SECTION, "Clean", r->clean);
    /* One line: an error with a line break in it would end the value. */
    char error[sizeof r->error];
    snprintf(error, sizeof error, "%s", r->error);
    for (char *c = error; *c; c++) if (*c == '\r' || *c == '\n') *c = ' ';
    dxl_ini_set(ini, SECTION, "Error", error);
    if (r->start[0]) dxl_ini_set(ini, SECTION, "Start", r->start);
    char *path = dxl_path_join(state_dir, "LastRun.ini");
    int rc = dxl_ini_save(ini, path, err);
    free(path);
    dxl_ini_free(ini);
    return rc;
}

char *dxl_lastrun_rotate(const char *logs_dir) {
    char *last = dxl_path_join(logs_dir, DXL_LASTRUN_LOG);
    char *previous = dxl_path_join(logs_dir, DXL_PREVIOUSRUN_LOG);
    dxl_path_mkdirs(logs_dir);
    if (dxl_path_exists(last)) rename(last, previous);
    free(previous);
    return last;
}

int dxl_lastrun_error_line(const char *log_path, char *out, size_t size) {
    FILE *f = log_path ? fopen(log_path, "rb") : NULL;
    if (!f) return 0;
    char line[2048];
    int found = 0;
    /* The last one: an engine that failed twice says why the second time. */
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, ERROR_PREFIX, strlen(ERROR_PREFIX)) != 0) continue;
        char *msg = line + strlen(ERROR_PREFIX);
        msg[strcspn(msg, "\r\n")] = '\0';
        snprintf(out, size, "%s", msg);
        found = 1;
    }
    fclose(f);
    return found;
}

char *dxl_lastrun_tail(const char *log_path, int max_lines) {
    FILE *f = log_path ? fopen(log_path, "rb") : NULL;
    if (!f || max_lines <= 0) {
        if (f) fclose(f);
        return NULL;
    }
    /* Read the end of the file, enough for that many long lines. */
    long want = (long)max_lines * 240;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    long from = size > want ? size - want : 0;
    fseek(f, from, SEEK_SET);
    char *buf = dxl_xmalloc((size_t)(size - from) + 1);
    size_t got = fread(buf, 1, (size_t)(size - from), f);
    fclose(f);
    buf[got] = '\0';
    if (got == 0) {
        free(buf);
        return NULL;
    }
    /* Drop a final newline, then count back. */
    size_t end = got;
    while (end > 0 && (buf[end - 1] == '\n' || buf[end - 1] == '\r')) end--;
    buf[end] = '\0';
    size_t start = end;
    int lines = 0;
    while (start > 0) {
        if (buf[start - 1] == '\n' && ++lines == max_lines) break;
        start--;
    }
    /* A cut first line, when the read began mid-file, is dropped. */
    if (start == 0 && from > 0) {
        char *nl = strchr(buf, '\n');
        start = nl ? (size_t)(nl - buf) + 1 : 0;
    }
    char *tail = dxl_xstrdup(buf + start);
    free(buf);
    return tail;
}

const char *dxl_signal_name(int sig, char *buf, size_t size) {
    static const struct { int sig; const char *name; } names[] = {
        { SIGHUP, "SIGHUP" }, { SIGINT, "SIGINT" }, { SIGQUIT, "SIGQUIT" }, { SIGILL, "SIGILL" },
        { SIGTRAP, "SIGTRAP" }, { SIGABRT, "SIGABRT" }, { SIGBUS, "SIGBUS" }, { SIGFPE, "SIGFPE" },
        { SIGKILL, "SIGKILL" }, { SIGSEGV, "SIGSEGV" }, { SIGPIPE, "SIGPIPE" }, { SIGALRM, "SIGALRM" },
        { SIGTERM, "SIGTERM" },
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++)
        if (names[i].sig == sig) return names[i].name;
    snprintf(buf, size, "signal %d", sig);
    return buf;
}

void dxl_lastrun_describe(const dxl_lastrun *r, char *out, size_t size) {
    char sig[32];
    if (r->signal)
        snprintf(out, size, "killed by %s%s", dxl_signal_name(r->signal, sig, sizeof sig),
                 r->ready ? "" : ", before its first map was in");
    else if (r->error[0])
        snprintf(out, size, "exit %d: %s", r->status, r->error);
    else if (r->status == 0 && !r->ready)
        snprintf(out, size, "exit 0 before its first map was in: no game found, or none started");
    else
        snprintf(out, size, "exit %d", r->status);
}

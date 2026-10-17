/* What a profile's last run left: how it ended, and its log.
 *
 *   <state>/LastRun.ini         [LastRun] Ended, Status, Signal, Ready, Clean,
 *                               Error: the run's end, written as it ends
 *   <state>/logs/last-run.log   the engine's output, all of it
 *   <state>/logs/previous-run.log   the run's before it
 *
 * Running.ini, the crash sentinel beside them (core/sentinel.h), says that a
 * run did not end cleanly; this says how it ended, for the Recovery screen:
 * the exit code or the signal, the engine's own error line, the log's last
 * lines, and whether the engine was ever up.
 */
#ifndef DXL_LASTRUN_H
#define DXL_LASTRUN_H

#include "core/common.h"

#define DXL_LASTRUN_LOG     "last-run.log"
#define DXL_PREVIOUSRUN_LOG "previous-run.log"

typedef struct {
    int  known;        /* a record was there */
    long ended;        /* seconds since the epoch */
    int  status;       /* the exit code, or 128 + the signal */
    int  signal;       /* 0 when it exited */
    int  ready;        /* the engine said its first map was in */
    int  clean;        /* exit 0 with ready seen */
    char error[512];   /* the engine's "SurrealEngine error: ..." line, or "" */
} dxl_lastrun;

int  dxl_lastrun_load(dxl_lastrun *r, const char *state_dir);
int  dxl_lastrun_save(const dxl_lastrun *r, const char *state_dir, dxl_err *err);

/* Before a run: last-run.log becomes previous-run.log. Returns the new
 * log's path; caller frees. */
char *dxl_lastrun_rotate(const char *logs_dir);

/* The engine's error line in a log, without its "SurrealEngine error: ":
 * 1 when there is one. */
int   dxl_lastrun_error_line(const char *log_path, char *out, size_t size);
/* The last lines of a log, at most max_lines; caller frees. NULL when
 * there is none. */
char *dxl_lastrun_tail(const char *log_path, int max_lines);

/* "SIGSEGV" for 11, ... ; "signal <n>" for one without a name here. */
const char *dxl_signal_name(int sig, char *buf, size_t size);

/* One line saying how a run ended: "exit 0", "exit 1: <the error>",
 * "killed by SIGSEGV". */
void dxl_lastrun_describe(const dxl_lastrun *r, char *out, size_t size);

#endif

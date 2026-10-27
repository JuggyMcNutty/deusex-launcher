/* Logging.
 *
 * The original opens a WLog window on every launch and writes DeusEx.log.
 * Here the window is gone (it is a Win32 widget), but the file stays: it is
 * the only way to see what happened once the launcher has handed over to
 * the game. It is <Package>.log, written afresh each run, as the original
 * does, the game's own output appended to it (dxl_log_open_new).
 */
#ifndef DXL_LOG_H
#define DXL_LOG_H

#include "core/common.h"

/* path may be NULL for stderr only. Appends, keeping the file across runs. */
void dxl_log_open(const char *path);
/* The same, starting the file afresh -- as the original's own log is, each
 * run. */
void dxl_log_open_new(const char *path);
void dxl_log_close(void);
/* To the file and to stderr. */
void dxl_log(const char *fmt, ...);

#endif

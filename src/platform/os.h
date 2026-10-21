/* Processes where POSIX and Windows differ: this one's environment, its
 * id, a pause; and how a program the launcher started ended when it did
 * not exit by itself. platform/posix/os.c is POSIX's.
 */
#ifndef DXL_OS_H
#define DXL_OS_H

#include <stddef.h>

/* Sets name to value in this process's environment; a NULL value takes it
 * out. 0, or -1. */
int  dxl_env_set(const char *name, const char *value);

/* This process's id. */
long dxl_os_pid(void);

/* Sleeps ms milliseconds. */
void dxl_os_sleep_ms(int ms);

/* What ended a program that did not exit by itself (dxl_game's signal,
 * LastRun.ini's Signal) -- a signal here: its name, "SIGSEGV", or "signal
 * <n>" for one without a name here. */
const char *dxl_end_name(int end, char *buf, size_t size);
/* Whether that end was the program's own crash, not a stop. */
int dxl_end_crashed(int end);

#endif

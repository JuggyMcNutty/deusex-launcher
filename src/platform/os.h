/* Processes where POSIX and Windows differ: this one's environment, its
 * id, a pause; and how a program the launcher started ended when it did
 * not exit by itself. platform/posix/os.c is POSIX's, platform/win32/os.c
 * Windows'.
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

/* Milliseconds by a clock no one sets, from some start of its own: what
 * time a wait took. */
long long dxl_os_ms(void);

/* What ended a program that did not exit by itself (dxl_game's signal,
 * LastRun.ini's Signal) -- on POSIX a signal: its name, "SIGSEGV", or
 * "signal <n>" for one without a name; on Windows the code it ended with,
 * an exception's or the launcher's kill: "an access violation
 * (0xC0000005)", or "exception 0x<code>" for one without a name. */
const char *dxl_end_name(int end, char *buf, size_t size);
/* Whether that end was the program's own crash, not a stop. */
int dxl_end_crashed(int end);

#endif

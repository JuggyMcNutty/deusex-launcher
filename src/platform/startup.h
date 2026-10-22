/* A start of either program, where POSIX and Windows differ: what ends it
 * from outside. platform/posix/startup.c is POSIX's: SIGINT, SIGTERM and
 * SIGHUP; a write to a closed pipe is no signal. platform/win32/startup.c
 * Windows': the console's Ctrl+C, Ctrl+Break and its closing.
 */
#ifndef DXL_STARTUP_H
#define DXL_STARTUP_H

/* From here on, an end asked from outside sets *stop and calls wake (NULL:
 * nothing to wake); a wait it breaks ends at once. */
void dxl_startup(volatile int *stop, void (*wake)(void));

#endif

/* A start of either program, where POSIX and Windows differ: what ends it
 * from outside. platform/posix/startup.c is POSIX's: SIGINT, SIGTERM and
 * SIGHUP; a write to a closed pipe is no signal; SIGCHLD as the system has
 * it, whatever the parent left, so a child's end is the launcher's to see.
 */
#ifndef DXL_STARTUP_H
#define DXL_STARTUP_H

#include <stdatomic.h>

/* From here on, an end asked from outside sets *stop and calls wake (NULL:
 * nothing to wake); a wait it breaks ends at once. An atomic, which a
 * signal's handler may set, and Windows' console handler on a thread of its
 * own. */
void dxl_startup(atomic_int *stop, void (*wake)(void));

/* The program's work is done (the run returned): what waits for the end to
 * be clean -- Windows' closing console, which ends the process as its
 * handler returns -- may let go. Nothing on POSIX. */
void dxl_startup_ended(void);

#endif

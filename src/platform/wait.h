/* Waiting where POSIX and Windows differ: on wakes -- set by another
 * thread, or by a signal's handler (Windows: the console's) -- and on a
 * second start's line to the single instance (platform/instance.h).
 * platform/posix/wait.c is POSIX's: pipes and poll; platform/win32/wait.c
 * Windows': events.
 */
#ifndef DXL_WAIT_H
#define DXL_WAIT_H

#include "platform/instance.h"

typedef struct dxl_wake dxl_wake;

/* NULL when the platform has none to give. */
dxl_wake *dxl_wake_new(void);
void      dxl_wake_free(dxl_wake *w);
/* Sets it until it is cleared; safe in a signal's handler. */
void      dxl_wake_set(dxl_wake *w);
void      dxl_wake_clear(dxl_wake *w);

/* A line waiting on the instance's channel, among dxl_wait's answers. */
#define DXL_WAIT_LINE 0x100

/* Waits up to ms (-1: for as long as it takes) for one of the n wakes to
 * be set, or for a line on inst's channel (inst NULL: none): bit i of the
 * answer for wakes[i], DXL_WAIT_LINE for the line; 0 when nothing came, a
 * signal having broken the wait among those; -1 when it cannot wait. */
int dxl_wait(dxl_wake *const *wakes, int n, dxl_instance *inst, int ms);

#endif

/* The launcher's window, SDL3's, on the desktop's display.
 *
 * It opens when run.c first asks for a screen -- none at all for a start
 * that plays at once -- and closes before the game starts: the window, its
 * renderer and SDL's gamepads given up, so the game has the pads to itself;
 * the next screen opens it again. While it is up it costs nothing idle: it
 * waits on its events, drawing only while something moves, VSync-paced. A
 * second start's command line (platform/instance.h) and a signal wake it,
 * by a thread that waits on both (platform/wait.h).
 */
#ifndef DXL_WINDOW_H
#define DXL_WINDOW_H

#include "app/run.h"

typedef struct dxl_window dxl_window;

/* stop is set by a signal: the screen up ends as a quit. */
dxl_window *dxl_window_new(const char *version, atomic_int *stop);
void        dxl_window_free(dxl_window *w);
/* run.c's dxl_run_ui, the window's. */
void        dxl_window_ui(dxl_window *w, dxl_run_ui *ui);
/* From a signal handler: wakes the window, if it is up. */
void        dxl_window_wake(void);

#endif

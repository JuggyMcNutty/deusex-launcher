/* deusex-launcher: DXLauncher, the desktop's launcher for Deus Ex.
 *
 * One start is app/run.h's; its screens are the launcher's window
 * (screens/window.h), which opens only when one is due (docs/LAUNCHER.md).
 */
#define _GNU_SOURCE
#include "app/run.h"
#include "platform/startup.h"
#include "screens/window.h"

#include <stdio.h>
#include <string.h>

#ifndef DXL_VERSION
#define DXL_VERSION "unversioned"
#endif

static volatile int stop;

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("deusex-launcher %s\n", DXL_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("usage: deusex-launcher [--game=<dir>] [--profile=<id>] [--play] "
                   "[the original's command line...]\n"
                   "dxl-cli --help says more: the same start, with no window.\n");
            return 0;
        }
    }
    dxl_startup(&stop, dxl_window_wake);

    dxl_window *w = dxl_window_new(DXL_VERSION, &stop);
    dxl_run_ui ui;
    dxl_window_ui(w, &ui);
    int rc = dxl_run_main(argc, argv, &ui, &stop);
    dxl_startup_ended();
    dxl_window_free(w);
    return rc;
}

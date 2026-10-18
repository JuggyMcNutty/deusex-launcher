/* deusex-launcher: DXLauncher, the desktop's launcher for Deus Ex.
 *
 * One start is app/run.h's; its screens are the launcher's window
 * (screens/window.h), which opens only when one is due (docs/LAUNCHER.md).
 */
#define _GNU_SOURCE
#include "app/run.h"
#include "screens/window.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>

#ifndef DXL_VERSION
#define DXL_VERSION "unversioned"
#endif

static volatile int stop;

static void on_signal(int sig) {
    (void)sig;
    stop = 1;
    dxl_window_wake();
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("deusex-launcher %s\n", DXL_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("usage: deusex-launcher [--game=<dir>] [--profile=<id>] [--play] [--big-screen] "
                   "[the original's command line...]\n"
                   "       deusex-launcher --install-desktop | --remove-desktop\n"
                   "dxl-cli --help says more: the same start, with no window.\n");
            return 0;
        }
        if (strcmp(argv[i], "--install-desktop") == 0) return dxl_window_desktop_entry(1);
        if (strcmp(argv[i], "--remove-desktop") == 0) return dxl_window_desktop_entry(0);
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;   /* no SA_RESTART: the wait ends at once */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    dxl_window *w = dxl_window_new(DXL_VERSION, &stop);
    dxl_run_ui ui;
    dxl_window_ui(w, &ui);
    int rc = dxl_run_main(argc, argv, &ui, &stop);
    dxl_window_free(w);
    return rc;
}

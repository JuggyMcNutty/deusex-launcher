/* deusex-launcher: DXLauncher, the desktop's launcher for Deus Ex.
 *
 * One start is app/run.h's. Its screens -- Home, First run, Recovery,
 * Graphics -- are not built yet: with none, Home is skipped, and any other
 * screen that is due is a message on stderr and the exit code
 * DXL_RUN_EXIT_SCREEN (docs/LAUNCHER.md).
 */
#define _GNU_SOURCE
#include "app/run.h"

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
}

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
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;   /* no SA_RESTART: the wait ends at once */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    return dxl_run_main(argc, argv, NULL, &stop);
}

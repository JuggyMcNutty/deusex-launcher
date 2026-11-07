#define _GNU_SOURCE
#include "platform/startup.h"

#include <signal.h>
#include <string.h>

static atomic_int *stop_flag;
static void (*wake_up)(void);

static void on_signal(int sig) {
    (void)sig;
    if (stop_flag) *stop_flag = 1;
    if (wake_up) wake_up();
}

void dxl_startup(atomic_int *stop, void (*wake)(void)) {
    stop_flag = stop;
    wake_up = wake;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;   /* no SA_RESTART: the wait ends at once */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    /* A parent that ignores SIGCHLD passes that on across exec, and the
     * system then reaps the game itself: its end would never be seen. */
    signal(SIGCHLD, SIG_DFL);
}

void dxl_startup_ended(void) {}

void dxl_startup_detach(void) {}

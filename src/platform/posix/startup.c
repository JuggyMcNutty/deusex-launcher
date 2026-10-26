#define _GNU_SOURCE
#include "platform/startup.h"

#include <signal.h>
#include <string.h>

static volatile int *stop_flag;
static void (*wake_up)(void);

static void on_signal(int sig) {
    (void)sig;
    if (stop_flag) *stop_flag = 1;
    if (wake_up) wake_up();
}

void dxl_startup(volatile int *stop, void (*wake)(void)) {
    stop_flag = stop;
    wake_up = wake;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;   /* no SA_RESTART: the wait ends at once */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
}

void dxl_startup_ended(void) {}

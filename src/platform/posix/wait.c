#define _GNU_SOURCE
#include "platform/wait.h"
#include "platform/posix/internal.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>

/* A pipe: set writes a byte, which a signal's handler may do; clear reads
 * them all. */
struct dxl_wake {
    int fd[2];
};

dxl_wake *dxl_wake_new(void) {
    dxl_wake *w = dxl_xmalloc(sizeof *w);
    if (pipe2(w->fd, O_CLOEXEC | O_NONBLOCK) != 0) {
        free(w);
        return NULL;
    }
    return w;
}

void dxl_wake_free(dxl_wake *w) {
    if (!w) return;
    close(w->fd[0]);
    close(w->fd[1]);
    free(w);
}

void dxl_wake_set(dxl_wake *w) {
    if (!w) return;
    char c = 's';
    ssize_t n = write(w->fd[1], &c, 1);
    (void)n;
}

void dxl_wake_clear(dxl_wake *w) {
    if (!w) return;
    char buf[16];
    while (read(w->fd[0], buf, sizeof buf) > 0) {}
}

int dxl_wait(dxl_wake *const *wakes, int n, dxl_instance *inst, int ms) {
    struct pollfd p[9];
    int k = 0;
    for (int i = 0; i < n && k < 8; i++) {
        p[k].fd = wakes[i] ? wakes[i]->fd[0] : -1;
        p[k].events = POLLIN;
        p[k++].revents = 0;
    }
    int line = dxl_instance_fd(inst);
    if (line >= 0) {
        p[k].fd = line;
        p[k].events = POLLIN;
        p[k++].revents = 0;
    }
    int r = poll(p, (nfds_t)k, ms);
    if (r < 0) return errno == EINTR ? 0 : -1;
    if (r == 0) return 0;
    int got = 0;
    for (int i = 0; i < n && i < 8; i++)
        if (p[i].revents & (POLLIN | POLLHUP | POLLERR)) got |= 1 << i;
    if (line >= 0 && (p[k - 1].revents & POLLIN)) got |= DXL_WAIT_LINE;
    return got;
}

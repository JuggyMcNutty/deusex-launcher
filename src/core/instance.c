#define _GNU_SOURCE
#include "core/instance.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct dxl_instance {
    int   lock_fd;
    int   sock_fd;
    char *lock_path;
};

/* FNV-1a over the install path, so two installs on the same card get
 * different names. Collisions would merely make two installs share an
 * instance; the hash only needs to be stable, not cryptographic. */
static unsigned long id_hash(const char *id) {
    unsigned long h = 2166136261UL;
    for (const unsigned char *p = (const unsigned char *)(id ? id : ""); *p; p++) {
        h ^= *p;
        h *= 16777619UL;
    }
    return h;
}

/* Abstract socket address: sun_path[0] is NUL, so the name lives in the
 * abstract namespace and vanishes with the process. */
static socklen_t abstract_addr(struct sockaddr_un *sa, const char *id) {
    memset(sa, 0, sizeof *sa);
    sa->sun_family = AF_UNIX;
    char name[64];
    int n = snprintf(name, sizeof name, "deusex-launcher.%08lx", id_hash(id));
    sa->sun_path[0] = '\0';
    memcpy(sa->sun_path + 1, name, (size_t)n);
    return (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + n);
}

static char *lock_path_for(const char *id) {
    const char *dir = getenv("XDG_RUNTIME_DIR");
    if (!dir || !*dir) dir = "/tmp";
    size_t n = strlen(dir) + 48;
    char *p = dxl_xmalloc(n);
    snprintf(p, n, "%s/deusex-launcher.%08lx.lock", dir, id_hash(id));
    return p;
}

dxl_instance *dxl_instance_acquire(const char *id) {
    char *path = lock_path_for(id);
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) { free(path); return NULL; }

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        free(path);
        return NULL;                      /* someone else holds it */
    }

    if (ftruncate(fd, 0) == 0) {
        char pid[32];
        int n = snprintf(pid, sizeof pid, "%ld\n", (long)getpid());
        ssize_t ignored = write(fd, pid, (size_t)n);
        (void)ignored;
    }

    dxl_instance *inst = dxl_xmalloc(sizeof *inst);
    inst->lock_fd = fd;
    inst->lock_path = path;
    inst->sock_fd = -1;

    /* The handoff channel. Failing to open it is not fatal -- we are still the
     * only instance, we just cannot receive URLs. */
    int s = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (s >= 0) {
        struct sockaddr_un sa;
        socklen_t len = abstract_addr(&sa, id);
        if (bind(s, (struct sockaddr *)&sa, len) == 0) inst->sock_fd = s;
        else close(s);
    }
    return inst;
}

void dxl_instance_release(dxl_instance *inst) {
    if (!inst) return;
    if (inst->sock_fd >= 0) close(inst->sock_fd);
    if (inst->lock_fd >= 0) {
        flock(inst->lock_fd, LOCK_UN);
        close(inst->lock_fd);
    }
    /* Leaving the (empty) lock file behind is harmless: the lock is on the
     * descriptor, not the name, so a stale file never blocks anyone. */
    free(inst->lock_path);
    free(inst);
}

int dxl_instance_other_running(const char *id) {
    char *path = lock_path_for(id);
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    free(path);
    if (fd < 0) return 0;

    int busy = (flock(fd, LOCK_EX | LOCK_NB) != 0);
    if (!busy) flock(fd, LOCK_UN);
    close(fd);
    return busy;
}

int dxl_instance_forward(const char *id, const char *message, int timeout_ms,
                         dxl_err *err) {
    int s = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (s < 0) { dxl_err_set(err, "socket: %s", strerror(errno)); return -1; }

    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);

    struct sockaddr_un sa;
    socklen_t len = abstract_addr(&sa, id);
    size_t mlen = strlen(message ? message : "");
    if (mlen > DXL_HANDOFF_MAX) mlen = DXL_HANDOFF_MAX;

    ssize_t sent = sendto(s, message ? message : "", mlen, 0,
                          (struct sockaddr *)&sa, len);
    close(s);
    if (sent < 0) {
        dxl_err_set(err, "handoff failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

int dxl_instance_poll(dxl_instance *inst, char *buf, size_t size) {
    if (!inst || inst->sock_fd < 0 || size == 0) return 0;
    ssize_t n = recv(inst->sock_fd, buf, size - 1, MSG_DONTWAIT);
    if (n < 0) return 0;
    buf[n] = '\0';
    return 1;
}

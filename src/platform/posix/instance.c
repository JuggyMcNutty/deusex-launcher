/* The single instance on POSIX (platform/instance.h). The lock is flock on
 * a pidfile in the runtime folder ($XDG_RUNTIME_DIR, else one of the user's
 * own under /tmp), held on the descriptor, not the name: the file stays
 * after the process, holding the last pid, and a stale one never reads as
 * live -- nothing is written to the install. The channel is a unix socket
 * file in that folder, whose mode (0700) keeps other users' lines out, as
 * an abstract name would not; removed at release, a stale one at the next
 * bind.
 */
#define _GNU_SOURCE
#include "platform/instance.h"
#include "platform/posix/internal.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

struct dxl_instance {
    int   lock_fd;
    int   sock_fd;
    char *lock_path;
    char *sock_path;   /* the channel's file, removed at release */
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

/* The folder of the lock and the channel: $XDG_RUNTIME_DIR, the user's own
 * (0700 by the system), else /tmp/deusex-launcher-<uid>, made 0700 and
 * checked to be the user's own folder, no link, for a channel's permissions
 * are its folder's. NULL where neither is to be had. Caller frees. */
static char *runtime_dir(void) {
    const char *dir = getenv("XDG_RUNTIME_DIR");
    if (dir && *dir) return dxl_xstrdup(dir);
    char *own = dxl_xmalloc(64);
    snprintf(own, 64, "/tmp/deusex-launcher-%ld", (long)getuid());
    struct stat st;
    if ((mkdir(own, 0700) != 0 && errno != EEXIST) || lstat(own, &st) != 0 || !S_ISDIR(st.st_mode) ||
        st.st_uid != getuid() || (st.st_mode & 077) != 0) {
        free(own);
        return NULL;
    }
    return own;
}

/* The channel's address, a socket file in the runtime folder: only its
 * owner can send a line, where an abstract name took anyone's. 0 where
 * there is no folder, or the name would not fit. */
static socklen_t socket_addr(struct sockaddr_un *sa, const char *id) {
    memset(sa, 0, sizeof *sa);
    sa->sun_family = AF_UNIX;
    char *dir = runtime_dir();
    if (!dir) return 0;
    int n = snprintf(sa->sun_path, sizeof sa->sun_path, "%s/deusex-launcher.%08lx.sock", dir, id_hash(id));
    free(dir);
    if (n < 0 || (size_t)n >= sizeof sa->sun_path) return 0;
    return (socklen_t)(offsetof(struct sockaddr_un, sun_path) + (size_t)n + 1);
}

static char *lock_path_for(const char *id) {
    char *dir = runtime_dir();
    size_t n = (dir ? strlen(dir) : 4) + 48;
    char *p = dxl_xmalloc(n);
    snprintf(p, n, "%s/deusex-launcher.%08lx.lock", dir ? dir : "/tmp", id_hash(id));
    free(dir);
    return p;
}

/* Whether a launcher listens on the channel at sa. */
static int listened(const struct sockaddr_un *sa, socklen_t len) {
    int s = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (s < 0) return 0;
    int on = connect(s, (const struct sockaddr *)sa, len) == 0;
    close(s);
    return on;
}

/* The handoff channel, into inst, unless a launcher listens on it already
 * (a client beside a server, which holds the lock). Failing to open it is
 * not fatal -- the instance just cannot receive URLs. */
static void open_channel(dxl_instance *inst, const char *id) {
    int s = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (s < 0) return;
    struct sockaddr_un sa;
    socklen_t len = socket_addr(&sa, id);
    /* A file an earlier launcher left, no one listening on it, would refuse
     * the bind. */
    if (len && !listened(&sa, len)) unlink(sa.sun_path);
    if (len && bind(s, (struct sockaddr *)&sa, len) == 0) {
        inst->sock_fd = s;
        inst->sock_path = dxl_xstrdup(sa.sun_path);
    } else {
        close(s);
    }
}

static dxl_instance *acquire(const char *id, int listen) {
    char *path = lock_path_for(id);
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);   /* a planted link is refused */
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
    inst->sock_path = NULL;
    if (listen) open_channel(inst, id);
    return inst;
}

dxl_instance *dxl_instance_acquire(const char *id) { return acquire(id, 1); }
dxl_instance *dxl_instance_acquire_lock(const char *id) { return acquire(id, 0); }

dxl_instance *dxl_instance_listen(const char *id) {
    dxl_instance *inst = dxl_xmalloc(sizeof *inst);
    inst->lock_fd = -1;
    inst->lock_path = NULL;
    inst->sock_fd = -1;
    inst->sock_path = NULL;
    open_channel(inst, id);
    if (inst->sock_fd < 0) {
        free(inst);
        return NULL;
    }
    return inst;
}

int dxl_instance_fd(const dxl_instance *inst) { return inst ? inst->sock_fd : -1; }

int dxl_instance_listens(const dxl_instance *inst) { return dxl_instance_fd(inst) >= 0; }

int dxl_instance_locked(const dxl_instance *inst) { return inst && inst->lock_fd >= 0; }

void dxl_instance_release(dxl_instance *inst) {
    if (!inst) return;
    if (inst->sock_fd >= 0) {
        close(inst->sock_fd);
        unlink(inst->sock_path);
    }
    free(inst->sock_path);
    if (inst->lock_fd >= 0) {
        flock(inst->lock_fd, LOCK_UN);
        close(inst->lock_fd);
    }
    /* Leaving the lock file behind (it holds the last pid) is harmless: the
     * lock is on the descriptor, not the name, so a stale file never blocks
     * anyone. */
    free(inst->lock_path);
    free(inst);
}

int dxl_instance_other_running(const char *id) {
    char *path = lock_path_for(id);
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);   /* a planted link is refused */
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
    socklen_t len = socket_addr(&sa, id);
    if (!len) {
        close(s);
        dxl_err_set(err, "handoff failed: no runtime folder of this user's");
        return -1;
    }
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

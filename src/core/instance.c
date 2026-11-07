#define _GNU_SOURCE
#include "core/instance.h"

#include <dirent.h>
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
    int   lock_fd;     /* -1: a channel alone */
    int   sock_fd;
    char *lock_path;
    char *sock_path;   /* the channel's file, removed at release */
};

/* A client's channel: deusex-launcher-<pid>.<n>.sock in the runtime folder,
 * the n-th this process opened. */
#define CHANNEL_PREFIX "deusex-launcher-"
#define CHANNEL_SUFFIX ".sock"

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

/* A channel's address, a socket file in the runtime folder dir: only its
 * owner can send a line, where an abstract name took anyone's (the
 * original's WM_COPYDATA stays within the player's desktop). 0 where the
 * name would not fit. */
static socklen_t socket_addr(struct sockaddr_un *sa, const char *dir, const char *name) {
    memset(sa, 0, sizeof *sa);
    sa->sun_family = AF_UNIX;
    int n = snprintf(sa->sun_path, sizeof sa->sun_path, "%s/%s", dir, name);
    if (n < 0 || (size_t)n >= sizeof sa->sun_path) return 0;
    return (socklen_t)(offsetof(struct sockaddr_un, sun_path) + (size_t)n + 1);
}

/* This process's next channel, into inst. Failing to open it is not fatal:
 * the instance just cannot receive URLs. */
static void open_channel(dxl_instance *inst) {
    static int opened;
    char *dir = runtime_dir();
    if (!dir) return;
    char name[64];
    snprintf(name, sizeof name, CHANNEL_PREFIX "%ld.%d" CHANNEL_SUFFIX, (long)getpid(), ++opened);
    struct sockaddr_un sa;
    socklen_t len = socket_addr(&sa, dir, name);
    free(dir);
    int s = len ? socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0) : -1;
    if (s < 0) return;
    /* A file an earlier process of this pid left would refuse the bind. */
    unlink(sa.sun_path);
    if (bind(s, (struct sockaddr *)&sa, len) == 0) {
        inst->sock_fd = s;
        inst->sock_path = dxl_xstrdup(sa.sun_path);
    } else {
        close(s);
    }
}

static char *lock_path(void) {
    char *dir = runtime_dir();
    size_t n = (dir ? strlen(dir) : 4) + 32;
    char *p = dxl_xmalloc(n);
    snprintf(p, n, "%s/deusex-launcher.lock", dir ? dir : "/tmp");
    free(dir);
    return p;
}

static dxl_instance *acquire(int listen) {
    char *path = lock_path();
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
    if (listen) open_channel(inst);
    return inst;
}

dxl_instance *dxl_instance_acquire(void) { return acquire(1); }
dxl_instance *dxl_instance_acquire_lock(void) { return acquire(0); }

dxl_instance *dxl_instance_listen(void) {
    dxl_instance *inst = dxl_xmalloc(sizeof *inst);
    inst->lock_fd = -1;
    inst->lock_path = NULL;
    inst->sock_fd = -1;
    inst->sock_path = NULL;
    open_channel(inst);
    if (inst->sock_fd < 0) {
        free(inst);
        return NULL;
    }
    return inst;
}

int dxl_instance_fd(const dxl_instance *inst) { return inst ? inst->sock_fd : -1; }

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

int dxl_instance_other_running(void) {
    char *path = lock_path();
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);   /* a planted link is refused */
    free(path);
    if (fd < 0) return 0;

    int busy = (flock(fd, LOCK_EX | LOCK_NB) != 0);
    if (!busy) flock(fd, LOCK_UN);
    close(fd);
    return busy;
}

int dxl_instance_forward(const char *message, int timeout_ms,
                         dxl_err *err) {
    char *dir = runtime_dir();
    if (!dir) {
        dxl_err_set(err, "handoff failed: no runtime folder of this user's");
        return -1;
    }
    int s = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (s < 0) {
        dxl_err_set(err, "socket: %s", strerror(errno));
        free(dir);
        return -1;
    }

    struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);

    size_t mlen = strlen(message ? message : "");
    if (mlen > DXL_HANDOFF_MAX) mlen = DXL_HANDOFF_MAX;

    /* Every client's channel in turn, until one takes the line: a file a
     * process that died left takes none. */
    int sent = 0, why = ENOENT;
    size_t pn = strlen(CHANNEL_PREFIX), sn = strlen(CHANNEL_SUFFIX);
    DIR *d = opendir(dir);
    struct dirent *de;
    while (!sent && d && (de = readdir(d)) != NULL) {
        size_t n = strlen(de->d_name);
        if (n <= pn + sn || strncmp(de->d_name, CHANNEL_PREFIX, pn) != 0 ||
            strcmp(de->d_name + n - sn, CHANNEL_SUFFIX) != 0)
            continue;
        struct sockaddr_un sa;
        socklen_t len = socket_addr(&sa, dir, de->d_name);
        if (!len) continue;
        if (sendto(s, message ? message : "", mlen, 0, (struct sockaddr *)&sa, len) >= 0) sent = 1;
        else why = errno;
    }
    if (d) closedir(d);
    close(s);
    free(dir);
    if (!sent) {
        dxl_err_set(err, "handoff failed: %s", strerror(why));
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

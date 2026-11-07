/* The single instance on POSIX (platform/instance.h). Every launcher marks
 * itself running, for its whole run, with a lock it shares with the others,
 * as the original's every instance keeps its DeusExIsRunning mutex: a read
 * lock on the first byte of a file in the runtime folder ($XDG_RUNTIME_DIR,
 * else one of the user's own under /tmp), an open file description's
 * (F_OFD_SETLK), so each instance's is its own, a process's two among them.
 * Another launcher is live where a write lock would meet a read lock of
 * another description's (F_OFD_GETLK). The lock is on the description, not
 * the name, and goes with it however the process ends: the file stays, and a
 * stale one never reads as live -- nothing is written to the install. Each
 * client, as the original marks every client's log window IsBrowser, has a
 * channel of its own: a unix socket file in that folder, whose mode (0700)
 * keeps other users' lines out, as an abstract name would not; removed at
 * release. A second start sends to the newest that takes its line.
 */
#define _GNU_SOURCE
#include "platform/instance.h"
#include "platform/posix/internal.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

struct dxl_instance {
    int   lock_fd;     /* its mark of a launcher running; -1 for none */
    int   sock_fd;
    char *sock_path;   /* the channel's file, removed at release */
};

/* A client's channel: deusex-launcher.<hash>.<pid>.<n>.sock in the runtime
 * folder, the n-th this process opened; a home's are the files of its
 * prefix (channel_prefix). */
#define CHANNEL_SUFFIX ".sock"

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

/* The start of the names of id's channels, into out. */
static void channel_prefix(const char *id, char *out, size_t size) {
    snprintf(out, size, "deusex-launcher.%08lx.", id_hash(id));
}

/* The folder of the lock and the channels: $XDG_RUNTIME_DIR, the user's own
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

/* A channel's address, the socket file name in the runtime folder dir:
 * only its owner can send a line, where an abstract name took anyone's. 0
 * where the name would not fit. */
static socklen_t socket_addr(struct sockaddr_un *sa, const char *dir, const char *name) {
    memset(sa, 0, sizeof *sa);
    sa->sun_family = AF_UNIX;
    int n = snprintf(sa->sun_path, sizeof sa->sun_path, "%s/%s", dir, name);
    if (n < 0 || (size_t)n >= sizeof sa->sun_path) return 0;
    return (socklen_t)(offsetof(struct sockaddr_un, sun_path) + (size_t)n + 1);
}

/* The file the launchers mark themselves running on, opened: a planted link
 * is refused. -1 when it cannot be. */
static int open_lock(const char *id) {
    char *dir = runtime_dir();
    size_t n = (dir ? strlen(dir) : 4) + 48;
    char *path = dxl_xmalloc(n);
    snprintf(path, n, "%s/deusex-launcher.%08lx.lock", dir ? dir : "/tmp", id_hash(id));
    free(dir);
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    free(path);
    return fd;
}

/* The first byte's lock, of type, asked (F_OFD_SETLK) or tested
 * (F_OFD_GETLK) through fd's description; l_pid 0, as a description's lock
 * asks. -1 when the call fails. */
static int byte_lock(int fd, int cmd, struct flock *l, short type) {
    memset(l, 0, sizeof *l);
    l->l_type = type;
    l->l_whence = SEEK_SET;
    l->l_start = 0;
    l->l_len = 1;
    return fcntl(fd, cmd, l);
}

/* Whether a launcher other than fd's description's marks itself running:
 * a write lock there would meet its read lock. */
static int marked_by_another(int fd) {
    struct flock l;
    return byte_lock(fd, F_OFD_GETLK, &l, F_WRLCK) == 0 && l.l_type != F_UNLCK;
}

/* Whether a launcher listens on the channel at sa. */
static int listened(const struct sockaddr_un *sa, socklen_t len) {
    int s = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (s < 0) return 0;
    int on = connect(s, (const struct sockaddr *)sa, len) == 0;
    close(s);
    return on;
}

/* This client's channel, a socket file of its own, into inst: the next name
 * of this process's, so a client started beside another listens too, as
 * the original's every client's log window is IsBrowser. A file of the name
 * no one listens on, an earlier process's of this pid, makes way; one a
 * launcher listens on (the same pid in another pid namespace) is passed for
 * the next name. Failing to open it is not fatal -- the instance just
 * cannot receive URLs. */
static void open_channel(dxl_instance *inst, const char *id) {
    static int opened;
    char *dir = runtime_dir();
    if (!dir) return;
    char prefix[48];
    channel_prefix(id, prefix, sizeof prefix);
    int s = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    for (int tries = 0; s >= 0 && tries < 8; tries++) {
        char name[96];
        snprintf(name, sizeof name, "%s%ld.%d" CHANNEL_SUFFIX, prefix, (long)getpid(), ++opened);
        struct sockaddr_un sa;
        socklen_t len = socket_addr(&sa, dir, name);
        if (!len) break;
        int why = bind(s, (struct sockaddr *)&sa, len) == 0 ? 0 : errno;
        if (why == EADDRINUSE && !listened(&sa, len)) {
            unlink(sa.sun_path);
            why = bind(s, (struct sockaddr *)&sa, len) == 0 ? 0 : errno;
        }
        if (!why) {
            inst->sock_fd = s;
            inst->sock_path = dxl_xstrdup(sa.sun_path);
            s = -1;
        } else if (why != EADDRINUSE) {
            break;
        }
    }
    if (s >= 0) close(s);
    free(dir);
}

static dxl_instance *acquire(const char *id, int listen) {
    dxl_instance *inst = dxl_xmalloc(sizeof *inst);
    inst->sock_fd = -1;
    inst->sock_path = NULL;
    /* The mark: a read lock, which no other launcher's refuses. */
    struct flock l;
    inst->lock_fd = open_lock(id);
    if (inst->lock_fd >= 0 && byte_lock(inst->lock_fd, F_OFD_SETLK, &l, F_RDLCK) != 0) {
        close(inst->lock_fd);
        inst->lock_fd = -1;
    }
    if (listen) open_channel(inst, id);
    if (inst->lock_fd < 0 && inst->sock_fd < 0) {
        free(inst);
        return NULL;
    }
    return inst;
}

dxl_instance *dxl_instance_acquire(const char *id) { return acquire(id, 1); }
dxl_instance *dxl_instance_acquire_lock(const char *id) { return acquire(id, 0); }

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
    /* The mark goes with its description, as it does however the process
     * ends. The file stays behind, harmless: the lock is on the
     * description, not the name, so a stale file never reads as live. */
    if (inst->lock_fd >= 0) close(inst->lock_fd);
    free(inst);
}

int dxl_instance_other_running(const dxl_instance *inst, const char *id) {
    if (dxl_instance_locked(inst)) return marked_by_another(inst->lock_fd);
    int fd = open_lock(id);
    if (fd < 0) return 0;
    int other = marked_by_another(fd);
    close(fd);
    return other;
}

/* A client's channel found in the runtime folder: its file's name, and when
 * it was made, its bind. */
typedef struct {
    char           *name;
    struct timespec made;
} channel;

/* The newest first; of two made at the same moment, a process's later by
 * its name, the longer or the greater. */
static int newest_first(const void *a, const void *b) {
    const channel *x = a, *y = b;
    if (x->made.tv_sec != y->made.tv_sec) return x->made.tv_sec < y->made.tv_sec ? 1 : -1;
    if (x->made.tv_nsec != y->made.tv_nsec) return x->made.tv_nsec < y->made.tv_nsec ? 1 : -1;
    size_t xn = strlen(x->name), yn = strlen(y->name);
    if (xn != yn) return xn < yn ? 1 : -1;
    return strcmp(y->name, x->name);
}

int dxl_instance_forward(const char *id, const char *message, int timeout_ms,
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

    /* The home's channels, the newest first: the original's FindWindowEx
     * walks the windows in their stacking order, a window made later put on
     * top of those before it, so the log window it finds first is the
     * newest client's. */
    char prefix[48];
    channel_prefix(id, prefix, sizeof prefix);
    channel *found = NULL;
    size_t count = 0, cap = 0;
    size_t pn = strlen(prefix), sn = strlen(CHANNEL_SUFFIX);
    DIR *d = opendir(dir);
    struct dirent *de;
    while (d && (de = readdir(d)) != NULL) {
        size_t n = strlen(de->d_name);
        if (n <= pn + sn || strncmp(de->d_name, prefix, pn) != 0 || strcmp(de->d_name + n - sn, CHANNEL_SUFFIX) != 0)
            continue;
        struct sockaddr_un sa;
        struct stat st;
        if (!socket_addr(&sa, dir, de->d_name) || lstat(sa.sun_path, &st) != 0 || !S_ISSOCK(st.st_mode)) continue;
        if (count == cap) {
            cap = cap ? 2 * cap : 8;
            found = dxl_xrealloc(found, cap * sizeof *found);
        }
        found[count].name = dxl_xstrdup(de->d_name);
        found[count].made = st.st_mtim;
        count++;
    }
    if (d) closedir(d);
    if (count) qsort(found, count, sizeof *found, newest_first);

    /* Each in turn until one takes the line: a file a launcher that died
     * left takes none. */
    int sent = 0, why = ENOENT;
    for (size_t i = 0; i < count; i++) {
        struct sockaddr_un sa;
        socklen_t len = socket_addr(&sa, dir, found[i].name);
        if (!sent && len) {
            if (sendto(s, message ? message : "", mlen, 0, (struct sockaddr *)&sa, len) >= 0) sent = 1;
            else why = errno;
        }
        free(found[i].name);
    }
    free(found);
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

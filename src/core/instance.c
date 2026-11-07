#define _GNU_SOURCE
#include "core/instance.h"

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
    int   lock_fd;     /* its mark of a game running; -1 for none */
    int   sock_fd;
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

/* The file the instances mark themselves running on, opened: a planted link
 * is refused. -1 when it cannot be. */
static int open_lock(void) {
    char *dir = runtime_dir();
    size_t n = (dir ? strlen(dir) : 4) + 32;
    char *path = dxl_xmalloc(n);
    snprintf(path, n, "%s/deusex-launcher.lock", dir ? dir : "/tmp");
    free(dir);
    int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    free(path);
    return fd;
}

/* The first byte's lock, of type, asked (F_OFD_SETLK) or tested
 * (F_OFD_GETLK) through fd's open file description, each instance's its
 * own; l_pid 0, as a description's lock asks. -1 when the call fails. */
static int byte_lock(int fd, int cmd, struct flock *l, short type) {
    memset(l, 0, sizeof *l);
    l->l_type = type;
    l->l_whence = SEEK_SET;
    l->l_start = 0;
    l->l_len = 1;
    return fcntl(fd, cmd, l);
}

/* The mark, made or found made, as the original's CreateMutex makes its
 * DeusExIsRunning or opens it, ERROR_ALREADY_EXISTS saying another had it:
 * a write lock, had only where no other instance holds its read lock, then
 * kept as a read lock, the change one step; else a read lock beside the
 * others', which a write lock held for that step delays for no longer. 0
 * when held, *other set. */
static int take_mark(int fd, int *other) {
    struct flock l;
    if (byte_lock(fd, F_OFD_SETLK, &l, F_WRLCK) == 0) {
        *other = 0;
        return byte_lock(fd, F_OFD_SETLK, &l, F_RDLCK);
    }
    if (errno != EAGAIN && errno != EACCES) return -1;
    *other = 1;
    for (int tries = 0; tries < 100; tries++) {
        if (byte_lock(fd, F_OFD_SETLK, &l, F_RDLCK) == 0) return 0;
        if (errno != EAGAIN && errno != EACCES) return -1;
        struct timespec pause = { 0, 10 * 1000 * 1000 };
        nanosleep(&pause, NULL);
    }
    return -1;
}

dxl_instance *dxl_instance_acquire(int listen, int *other) {
    int was = 0;
    dxl_instance *inst = dxl_xmalloc(sizeof *inst);
    inst->sock_fd = -1;
    inst->sock_path = NULL;
    inst->lock_fd = open_lock();
    if (inst->lock_fd >= 0 && take_mark(inst->lock_fd, &was) != 0) {
        close(inst->lock_fd);
        inst->lock_fd = -1;
        was = 0;
    }
    if (other) *other = was;
    if (listen) open_channel(inst);
    if (inst->lock_fd < 0 && inst->sock_fd < 0) {
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
    /* The mark goes with its description, as the original's mutex with its
     * last handle, however the process ends. The file stays behind,
     * harmless: the lock is on the description, not the name, so a stale
     * file never reads as live. */
    if (inst->lock_fd >= 0) close(inst->lock_fd);
    free(inst);
}

int dxl_instance_other_running(void) {
    int fd = open_lock();
    if (fd < 0) return 0;
    struct flock l;
    int busy = byte_lock(fd, F_OFD_GETLK, &l, F_WRLCK) == 0 && l.l_type != F_UNLCK;
    close(fd);
    return busy;
}

/* A client's channel found in the runtime folder: its file's name, and when
 * it was made, its bind. */
typedef struct {
    char           *name;
    struct timespec made;
} channel;

/* The newest first; one made at the same moment by its name. */
static int newest_first(const void *a, const void *b) {
    const channel *x = a, *y = b;
    if (x->made.tv_sec != y->made.tv_sec) return x->made.tv_sec < y->made.tv_sec ? 1 : -1;
    if (x->made.tv_nsec != y->made.tv_nsec) return x->made.tv_nsec < y->made.tv_nsec ? 1 : -1;
    return strcmp(y->name, x->name);
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

    /* Every client's channel, the newest first: the original's FindWindowEx
     * walks the windows in their stacking order, a window made later put on
     * top of those before it, so the log window it finds first is the
     * newest client's. */
    channel *found = NULL;
    size_t count = 0, cap = 0;
    size_t pn = strlen(CHANNEL_PREFIX), sn = strlen(CHANNEL_SUFFIX);
    DIR *d = opendir(dir);
    struct dirent *de;
    while (d && (de = readdir(d)) != NULL) {
        size_t n = strlen(de->d_name);
        if (n <= pn + sn || strncmp(de->d_name, CHANNEL_PREFIX, pn) != 0 ||
            strcmp(de->d_name + n - sn, CHANNEL_SUFFIX) != 0)
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

    /* Each in turn until one takes the line: a file a process that died
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

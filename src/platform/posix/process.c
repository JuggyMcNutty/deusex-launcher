#define _GNU_SOURCE
#include "platform/process.h"
#include "platform/posix/internal.h"


#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct dxl_game_os {
    pid_t pid;
    int   channel;       /* the launcher's end of the line, or -1 */
};

/* In a child, after fork: every descriptor from 3 up closes as it execs,
 * but keep. Not all the launcher's are marked so -- the window's fonts,
 * which SDL_ttf opens without it -- and the game or a helper is given none
 * of them. glibc 2.28 has no close_range, nor a 4.9 kernel the call: each
 * one marked, up to the open files' limit. */
static void cloexec_all(int keep) {
    struct rlimit rl;
    int max = 1024;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY)
        max = rl.rlim_cur < 65536 ? (int)rl.rlim_cur : 65536;
    for (int fd = 3; fd < max; fd++)
        if (fd != keep) fcntl(fd, F_SETFD, FD_CLOEXEC);
}

/* The program's path: a name with a '/' as it is, a bare one found on
 * PATH as a shell finds it, so that the child execs a path and searches
 * nothing after the fork. NULL for a bare name that is nowhere. Caller
 * frees. */
static char *on_path(const char *name) {
    if (strchr(name, '/')) return dxl_xstrdup(name);
    const char *path = getenv("PATH");
    if (!path || !*path) path = "/usr/local/bin:/usr/bin:/bin";
    for (const char *p = path;; p++) {
        const char *end = strchr(p, ':');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        size_t size = n + strlen(name) + 3;
        char *full = dxl_xmalloc(size);
        snprintf(full, size, "%.*s/%s", n ? (int)n : 1, n ? p : ".", name);
        if (access(full, X_OK) == 0) return full;
        free(full);
        if (!end) return NULL;
        p = end;
    }
}

/* The game's environment, made before the fork (after it the child calls
 * nothing but what is safe there: setenv takes a lock another thread may
 * hold at the fork, for good). This process's, less the launcher's line
 * and the names env sets or takes out, then env's settings, then the
 * line's descriptor (fd_entry's text). The array is the caller's to free;
 * its strings are environ's, env's and fd_entry. */
static char **child_env(char *const env[], int keep_fd, char *fd_entry, size_t fd_entry_size) {
    extern char **environ;
    size_t ne = 0, nv = 0;
    while (environ[ne]) ne++;
    while (env && env[nv]) nv++;
    char **out = dxl_xmalloc((ne + nv + 2) * sizeof *out);
    size_t k = 0;
    for (size_t i = 0; i < ne; i++) {
        const char *eq = strchr(environ[i], '=');
        size_t n = eq ? (size_t)(eq - environ[i]) : strlen(environ[i]);
        int dropped = strncmp(environ[i], DXL_LAUNCHER_FD_ENV, n) == 0 && DXL_LAUNCHER_FD_ENV[n] == '\0';
        for (size_t j = 0; !dropped && j < nv; j++) {
            const char *veq = strchr(env[j], '=');
            size_t vn = veq ? (size_t)(veq - env[j]) : strlen(env[j]);
            dropped = vn == n && strncmp(env[j], environ[i], n) == 0;
        }
        if (!dropped) out[k++] = environ[i];
    }
    for (size_t j = 0; j < nv; j++)
        if (strchr(env[j], '=')) out[k++] = env[j];
    if (keep_fd >= 0) {
        snprintf(fd_entry, fd_entry_size, "%s=%d", DXL_LAUNCHER_FD_ENV, keep_fd);
        out[k++] = fd_entry;
    }
    out[k] = NULL;
    return out;
}

/* In the child, after fork: exec, or report why not through report_fd. */
static void exec_child(char *const argv[], char *const envp[], const char *workdir, int report_fd) {
    if (workdir && *workdir && chdir(workdir) != 0) {
        int e = errno;
        ssize_t w = write(report_fd, &e, sizeof e);
        (void)w;
        _exit(127);
    }
    execve(argv[0], argv, envp);
    int e = errno;
    ssize_t w = write(report_fd, &e, sizeof e);
    (void)w;
    _exit(127);
}

/* fork, exec, and 0 once exec has happened; -1 with the reason if it did not. */
static int spawn(pid_t *pid, char *const argv[], char *const env[], const char *workdir,
                 int keep_fd, const char *log_path, dxl_err *err) {
    const char *exe = argv[0];
    int report[2];
    if (pipe2(report, O_CLOEXEC) != 0) {
        dxl_err_set(err, "pipe: %s", strerror(errno));
        return -1;
    }
    int log_fd = -1;
    if (log_path) {
        log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
        if (log_fd < 0) {
            dxl_err_set(err, "cannot open %s: %s", log_path, strerror(errno));
            close(report[0]); close(report[1]);
            return -1;
        }
    }
    char fd_entry[64];
    char **envp = child_env(env, keep_fd, fd_entry, sizeof fd_entry);
    *pid = fork();
    if (*pid < 0) {
        dxl_err_set(err, "fork: %s", strerror(errno));
        close(report[0]); close(report[1]);
        if (log_fd >= 0) close(log_fd);
        free(envp);
        return -1;
    }
    if (*pid == 0) {
        signal(SIGPIPE, SIG_DFL);
        if (log_fd >= 0) { dup2(log_fd, STDOUT_FILENO); dup2(log_fd, STDERR_FILENO); }
        cloexec_all(keep_fd);
        if (keep_fd >= 0) fcntl(keep_fd, F_SETFD, 0);   /* the one descriptor the game keeps */
        exec_child(argv, envp, workdir, report[1]);
    }
    free(envp);
    close(report[1]);
    if (log_fd >= 0) close(log_fd);
    int e = 0;
    ssize_t got;
    do got = read(report[0], &e, sizeof e); while (got < 0 && errno == EINTR);
    close(report[0]);
    if (got == (ssize_t)sizeof e) {
        waitpid(*pid, NULL, 0);
        dxl_err_set(err, "cannot run %s: %s", exe, strerror(e));
        return -1;
    }
    return 0;
}

int dxl_game_start_argv(dxl_game *g, char *const argv[], char *const env[], const char *workdir,
                        const char *log_path, dxl_err *err) {
    memset(g, 0, sizeof *g);
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) {
        dxl_err_set(err, "socketpair: %s", strerror(errno));
        return -1;
    }
    pid_t pid;
    if (spawn(&pid, argv, env, workdir, sv[1], log_path, err) != 0) {
        close(sv[0]); close(sv[1]);
        return -1;
    }
    close(sv[1]);
    fcntl(sv[0], F_SETFL, fcntl(sv[0], F_GETFL) | O_NONBLOCK);
    g->os = dxl_xmalloc(sizeof *g->os);
    g->os->pid = pid;
    g->os->channel = sv[0];
    g->pid = (long)pid;
    return 0;
}

void dxl_game_close(dxl_game *g) {
    free(g->pending);
    g->pending = NULL;
    g->pending_len = g->pending_cap = 0;
    if (!g->os) return;
    if (g->os->channel >= 0) close(g->os->channel);
    free(g->os);
    g->os = NULL;
}


static int reap(dxl_game *g) {
    if (g->ended) return 1;
    int st;
    pid_t r = waitpid(g->os->pid, &st, WNOHANG);
    if (r != g->os->pid) return 0;
    g->ended = 1;
    g->status = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
    g->signal = WIFSIGNALED(st) ? WTERMSIG(st) : 0;
    return 1;
}

/* A line is any length; one past this with no end is taken as it is. */
#define LINE_MOST (1 << 20)

/* A whole line from what has come in, malloc'd into *line; 1 if there was
 * one. */
static int take_line(dxl_game *g, char **line) {
    char *nl = g->pending_len ? memchr(g->pending, '\n', g->pending_len) : NULL;
    size_t used;
    if (nl) {
        used = (size_t)(nl - g->pending) + 1;
    } else if (g->pending_len >= LINE_MOST) {
        used = g->pending_len;
        nl = g->pending + used;
    } else {
        return 0;
    }
    size_t n = (size_t)(nl - g->pending);
    if (n && g->pending[n - 1] == '\r') n--;
    char *out = dxl_xmalloc(n + 1);
    memcpy(out, g->pending, n);
    out[n] = '\0';
    memmove(g->pending, g->pending + used, g->pending_len - used);
    g->pending_len -= used;
    *line = out;
    return 1;
}

int dxl_game_wait(dxl_game *g, dxl_instance *inst, int ms, char **line) {
    *line = NULL;
    if (take_line(g, line)) return DXL_GAME_LINE;
    int extra_fd = dxl_instance_fd(inst);
    struct pollfd p[2];
    int n = 0, chan = -1, extra = -1;
    if (g->os->channel >= 0) { p[n].fd = g->os->channel; p[n].events = POLLIN; chan = n++; }
    if (extra_fd >= 0)       { p[n].fd = extra_fd;       p[n].events = POLLIN; extra = n++; }
    int r = poll(p, (nfds_t)n, ms);
    int seen = 0;
    if (r > 0 && chan >= 0 && (p[chan].revents & (POLLIN | POLLHUP | POLLERR))) {
        if (g->pending_len == g->pending_cap) {
            g->pending_cap = g->pending_cap ? g->pending_cap * 2 : 1024;
            g->pending = dxl_xrealloc(g->pending, g->pending_cap);
        }
        ssize_t got = read(g->os->channel, g->pending + g->pending_len,
                           g->pending_cap - g->pending_len);
        if (got > 0) g->pending_len += (size_t)got;
        else if (got == 0 || (errno != EAGAIN && errno != EINTR)) {
            close(g->os->channel);      /* the engine's end is gone: it ended, or dropped it */
            g->os->channel = -1;
        }
        if (take_line(g, line)) seen |= DXL_GAME_LINE;
    }
    if (r > 0 && extra >= 0 && (p[extra].revents & POLLIN)) seen |= DXL_GAME_EXTRA;
    /* The end is told once nothing else is waiting to be read. */
    if (!seen && reap(g)) seen |= DXL_GAME_ENDED;
    return seen;
}

void dxl_game_send(dxl_game *g, const char *line) {
    if (!g->os || g->os->channel < 0 || !line) return;
    size_t n = strlen(line);
    char *buf = dxl_xmalloc(n + 2);
    memcpy(buf, line, n);
    buf[n] = '\n';
    ssize_t w = send(g->os->channel, buf, n + 1, MSG_NOSIGNAL | MSG_DONTWAIT);
    (void)w;
    free(buf);
}

void dxl_game_stop(dxl_game *g) {
    if (!g->ended && g->os) kill(g->os->pid, SIGTERM);
}

void dxl_game_kill(dxl_game *g) {
    if (!g->ended && g->os) kill(g->os->pid, SIGKILL);
}

int dxl_game_clean(const dxl_game *g) { return g->ended && g->status == 0; }


int dxl_open_url(const char *url) {
    /* Two forks, so the opener is nobody's child to wait for. */
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        if (fork() == 0) {
            cloexec_all(-1);
            execlp("xdg-open", "xdg-open", url, (char *)NULL);
            _exit(127);
        }
        _exit(0);
    }
    waitpid(pid, NULL, 0);
    return 0;
}

int dxl_open_path(const char *path) {
    /* A file URL, its bytes outside the unreserved ones escaped. */
    char url[4096];
    size_t n = snprintf(url, sizeof url, "file://");
    for (const unsigned char *p = (const unsigned char *)path; *p && n + 4 < sizeof url; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
            strchr("/-._~", *p))
            url[n++] = (char)*p;
        else
            n += snprintf(url + n, sizeof url - n, "%%%02X", *p);
    }
    url[n] = '\0';
    return dxl_open_url(url);
}

/* What a pipe has, onto buf's end (cut at size, NUL-terminated); 0 at the
 * pipe's end. */
static int take_pipe(int fd, char *buf, size_t size, size_t *got) {
    char chunk[512];
    ssize_t n = read(fd, chunk, sizeof chunk);
    if (n <= 0) return 0;
    for (ssize_t i = 0; i < n && *got + 1 < size; i++) buf[(*got)++] = chunk[i];
    if (size) buf[*got] = '\0';
    return 1;
}

/* The time now, in ms, by the clock no one sets. */
static long long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

int dxl_run_capture(char *const argv[], int timeout_ms, char *out, size_t size, char *errs, size_t errs_size,
                    int *status, dxl_err *err) {
    if (size) out[0] = '\0';
    if (errs && errs_size) errs[0] = '\0';
    char *exe = on_path(argv[0]);
    if (!exe) {
        dxl_err_set(err, "cannot run %s: not found", argv[0]);
        if (status) *status = 127;
        return -1;
    }
    int pipefd[2], errfd[2] = { -1, -1 };
    if (pipe2(pipefd, O_CLOEXEC) != 0) {
        dxl_err_set(err, "pipe: %s", strerror(errno));
        free(exe);
        return -1;
    }
    if (errs && pipe2(errfd, O_CLOEXEC) != 0) {
        dxl_err_set(err, "pipe: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        free(exe);
        return -1;
    }
    /* Its environment this one's but the launcher's line, made before the
     * fork: after it the child calls nothing but what is safe there. */
    extern char **environ;
    size_t ne = 0;
    while (environ[ne]) ne++;
    char **envp = dxl_xmalloc((ne + 1) * sizeof *envp);
    size_t k = 0, fd_len = strlen(DXL_LAUNCHER_FD_ENV);
    for (size_t i = 0; i < ne; i++)
        if (strncmp(environ[i], DXL_LAUNCHER_FD_ENV, fd_len) != 0 || environ[i][fd_len] != '=') envp[k++] = environ[i];
    envp[k] = NULL;
    pid_t pid = fork();
    if (pid < 0) {
        dxl_err_set(err, "fork: %s", strerror(errno));
        close(pipefd[0]); close(pipefd[1]);
        if (errfd[0] >= 0) { close(errfd[0]); close(errfd[1]); }
        free(envp);
        free(exe);
        return -1;
    }
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        int null = open("/dev/null", O_RDWR | O_CLOEXEC);
        if (null >= 0) dup2(null, STDIN_FILENO);
        if (errfd[1] >= 0) dup2(errfd[1], STDERR_FILENO);
        else if (null >= 0) dup2(null, STDERR_FILENO);
        cloexec_all(-1);
        execve(exe, argv, envp);
        /* Why, as a shell says it, on the errors' line: write alone, the
         * launcher's other threads' locks being anyone's after the fork. */
        const char *why = errno == ENOENT ? ": No such file or directory\n"
                        : errno == EACCES ? ": Permission denied\n"
                        : errno == ENOEXEC ? ": Exec format error\n" : ": cannot be run\n";
        if (write(STDERR_FILENO, argv[0], strlen(argv[0])) < 0 || write(STDERR_FILENO, why, strlen(why)) < 0) {}
        _exit(127);
    }
    free(envp);
    free(exe);
    close(pipefd[1]);
    if (errfd[1] >= 0) close(errfd[1]);
    size_t got = 0, got_err = 0;
    int open_out = 1, open_err = errfd[0] >= 0, timed_out = 0;
    /* A deadline, not a count of quiet polls: a program that talks on and
     * on is stopped at it too. */
    long long deadline = now_ms() + timeout_ms;
    while (open_out || open_err) {
        long long left = deadline - now_ms();
        if (left <= 0) {
            timed_out = 1;
            break;
        }
        struct pollfd p[2] = { { open_out ? pipefd[0] : -1, POLLIN, 0 }, { open_err ? errfd[0] : -1, POLLIN, 0 } };
        int r = poll(p, 2, left > 100 ? 100 : (int)left);
        if (r > 0) {
            if (open_out && (p[0].revents & (POLLIN | POLLHUP | POLLERR))) open_out = take_pipe(pipefd[0], out, size, &got);
            if (open_err && (p[1].revents & (POLLIN | POLLHUP | POLLERR)))
                open_err = take_pipe(errfd[0], errs, errs_size, &got_err);
        }
    }
    close(pipefd[0]);
    if (errfd[0] >= 0) close(errfd[0]);
    if (timed_out) kill(pid, SIGKILL);
    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
    int code = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
    if (status) *status = code;
    if (timed_out) {
        dxl_err_set(err, "%s gave no answer in %d ms", argv[0], timeout_ms);
        return DXL_RUN_TIMED_OUT;
    }
    if (code == 127 && got == 0) {
        dxl_err_set(err, "cannot run %s", argv[0]);
        return -1;
    }
    return 0;
}

#define _GNU_SOURCE
#include "platform/process.h"


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

/* In the child, after fork: exec, or report why not through report_fd. */
static void exec_child(char *const argv[], char *const env[], const char *workdir, int report_fd) {
    for (size_t i = 0; env && env[i]; i++) {
        const char *eq = strchr(env[i], '=');
        if (!eq) {
            unsetenv(env[i]);
            continue;
        }
        char name[256];
        snprintf(name, sizeof name, "%.*s", (int)(eq - env[i]), env[i]);
        setenv(name, eq + 1, 1);
    }
    if (workdir && *workdir && chdir(workdir) != 0) {
        int e = errno;
        ssize_t w = write(report_fd, &e, sizeof e);
        (void)w;
        _exit(127);
    }
    execv(argv[0], argv);
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
    *pid = fork();
    if (*pid < 0) {
        dxl_err_set(err, "fork: %s", strerror(errno));
        close(report[0]); close(report[1]);
        if (log_fd >= 0) close(log_fd);
        return -1;
    }
    if (*pid == 0) {
        signal(SIGPIPE, SIG_DFL);
        if (log_fd >= 0) { dup2(log_fd, STDOUT_FILENO); dup2(log_fd, STDERR_FILENO); }
        cloexec_all(keep_fd);
        if (keep_fd >= 0) {
            fcntl(keep_fd, F_SETFD, 0);   /* the one descriptor the game keeps */
            char n[16];
            snprintf(n, sizeof n, "%d", keep_fd);
            setenv(DXL_LAUNCHER_FD_ENV, n, 1);
        } else {
            unsetenv(DXL_LAUNCHER_FD_ENV);
        }
        exec_child(argv, env, workdir, report[1]);
    }
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
    g->channel = -1;
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) {
        dxl_err_set(err, "socketpair: %s", strerror(errno));
        return -1;
    }
    if (spawn(&g->pid, argv, env, workdir, sv[1], log_path, err) != 0) {
        close(sv[0]); close(sv[1]);
        return -1;
    }
    close(sv[1]);
    fcntl(sv[0], F_SETFL, fcntl(sv[0], F_GETFL) | O_NONBLOCK);
    g->channel = sv[0];
    return 0;
}


static int reap(dxl_game *g) {
    if (g->ended) return 1;
    int st;
    pid_t r = waitpid(g->pid, &st, WNOHANG);
    if (r != g->pid) return 0;
    g->ended = 1;
    g->status = WIFEXITED(st) ? WEXITSTATUS(st) : 128 + (WIFSIGNALED(st) ? WTERMSIG(st) : 0);
    g->signal = WIFSIGNALED(st) ? WTERMSIG(st) : 0;
    return 1;
}

/* A whole line from what has come in, into line; 1 if there was one. */
static int take_line(dxl_game *g, char *line, size_t size) {
    char *nl = memchr(g->pending, '\n', g->pending_len);
    if (!nl) {
        if (g->pending_len < sizeof g->pending) return 0;
        nl = g->pending + g->pending_len - 1;   /* an over-long line: cut it */
    }
    size_t n = (size_t)(nl - g->pending);
    if (size) {
        size_t c = n < size - 1 ? n : size - 1;
        memcpy(line, g->pending, c);
        line[c] = '\0';
        if (c && line[c - 1] == '\r') line[c - 1] = '\0';
    }
    size_t used = n + 1 <= g->pending_len ? n + 1 : g->pending_len;
    memmove(g->pending, g->pending + used, g->pending_len - used);
    g->pending_len -= used;
    return 1;
}

int dxl_game_wait(dxl_game *g, int extra_fd, int ms, char *line, size_t size) {
    if (take_line(g, line, size)) return DXL_GAME_LINE;
    struct pollfd p[2];
    int n = 0, chan = -1, extra = -1;
    if (g->channel >= 0) { p[n].fd = g->channel; p[n].events = POLLIN; chan = n++; }
    if (extra_fd >= 0)   { p[n].fd = extra_fd;   p[n].events = POLLIN; extra = n++; }
    int r = poll(p, (nfds_t)n, ms);
    int seen = 0;
    if (r > 0 && chan >= 0 && (p[chan].revents & (POLLIN | POLLHUP | POLLERR))) {
        ssize_t got = read(g->channel, g->pending + g->pending_len,
                           sizeof g->pending - g->pending_len);
        if (got > 0) g->pending_len += (size_t)got;
        else if (got == 0 || (errno != EAGAIN && errno != EINTR)) {
            close(g->channel);          /* the engine's end is gone: it ended, or dropped it */
            g->channel = -1;
        }
        if (take_line(g, line, size)) seen |= DXL_GAME_LINE;
    }
    if (r > 0 && extra >= 0 && (p[extra].revents & POLLIN)) seen |= DXL_GAME_EXTRA;
    /* The end is told once nothing else is waiting to be read. */
    if (!seen && reap(g)) seen |= DXL_GAME_ENDED;
    return seen;
}

void dxl_game_send(dxl_game *g, const char *line) {
    if (g->channel < 0 || !line) return;
    size_t n = strlen(line);
    char *buf = dxl_xmalloc(n + 2);
    memcpy(buf, line, n);
    buf[n] = '\n';
    ssize_t w = send(g->channel, buf, n + 1, MSG_NOSIGNAL | MSG_DONTWAIT);
    (void)w;
    free(buf);
}

void dxl_game_signal(dxl_game *g, int sig) {
    if (!g->ended && g->pid > 0) kill(g->pid, sig);
}

int dxl_game_clean(const dxl_game *g) { return g->ended && g->status == 0; }


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
    int pipefd[2], errfd[2] = { -1, -1 };
    if (pipe2(pipefd, O_CLOEXEC) != 0) {
        dxl_err_set(err, "pipe: %s", strerror(errno));
        return -1;
    }
    if (errs && pipe2(errfd, O_CLOEXEC) != 0) {
        dxl_err_set(err, "pipe: %s", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
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
        return -1;
    }
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        int null = open("/dev/null", O_RDWR | O_CLOEXEC);
        if (null >= 0) dup2(null, STDIN_FILENO);
        if (errfd[1] >= 0) dup2(errfd[1], STDERR_FILENO);
        else if (null >= 0) dup2(null, STDERR_FILENO);
        cloexec_all(-1);
        execve(argv[0], argv, envp);
        /* Why, as a shell says it, on the errors' line: write alone, the
         * launcher's other threads' locks being anyone's after the fork. */
        const char *why = errno == ENOENT ? ": No such file or directory\n"
                        : errno == EACCES ? ": Permission denied\n"
                        : errno == ENOEXEC ? ": Exec format error\n" : ": cannot be run\n";
        if (write(STDERR_FILENO, argv[0], strlen(argv[0])) < 0 || write(STDERR_FILENO, why, strlen(why)) < 0) {}
        _exit(127);
    }
    free(envp);
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
        return -1;
    }
    if (code == 127 && got == 0) {
        dxl_err_set(err, "cannot run %s", argv[0]);
        return -1;
    }
    return 0;
}

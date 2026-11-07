#define _GNU_SOURCE
#include "platform/process.h"

#include "core/argv.h"
#include "core/log.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

/* The game's environment, made before the fork (after it the child calls
 * nothing but what is safe there: setenv takes a lock another thread may
 * hold at the fork, for good -- the wizard's renderer has threads up at
 * the detection run). This process's, less the launcher's line, then the
 * line's descriptor (fd_entry's text). The array is the caller's to free;
 * its strings are environ's and fd_entry. */
static char **child_env(int keep_fd, char *fd_entry, size_t fd_entry_size) {
    extern char **environ;
    size_t ne = 0;
    while (environ[ne]) ne++;
    char **out = dxl_xmalloc((ne + 2) * sizeof *out);
    size_t k = 0, fd_len = strlen(DXL_LAUNCHER_FD_ENV);
    for (size_t i = 0; i < ne; i++)
        if (strncmp(environ[i], DXL_LAUNCHER_FD_ENV, fd_len) != 0 || environ[i][fd_len] != '=') out[k++] = environ[i];
    if (keep_fd >= 0) {
        snprintf(fd_entry, fd_entry_size, "%s=%d", DXL_LAUNCHER_FD_ENV, keep_fd);
        out[k++] = fd_entry;
    }
    out[k] = NULL;
    return out;
}

/* In the child, after fork: exec, or report why not through report_fd. */
static void exec_child(const char *exe, char *const argv[], char *const envp[], const char *workdir, int report_fd) {
    if (workdir && *workdir && chdir(workdir) != 0) {
        int e = errno;
        ssize_t w = write(report_fd, &e, sizeof e);
        (void)w;
        _exit(127);
    }
    execve(exe, argv, envp);
    int e = errno;
    ssize_t w = write(report_fd, &e, sizeof e);
    (void)w;
    _exit(127);
}

/* fork, exec, and 0 once exec has happened; -1 with the reason if it did not. */
static int spawn(pid_t *pid, const char *exe, const char *flags, const char *workdir,
                 int keep_fd, const char *log_path, dxl_err *err) {
    int report[2];
    if (pipe2(report, O_CLOEXEC) != 0) {
        dxl_err_set(err, "pipe: %s", strerror(errno));
        return -1;
    }
    int log_fd = -1;
    if (log_path) {
        log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
        /* A log that cannot be opened stops nothing: the game runs without
         * one, as the original's does when its log device cannot make the
         * file (FOutputDeviceFile: the device dead, the game on). */
        if (log_fd < 0) dxl_log("warning: cannot open %s: %s: the game runs without its log", log_path, strerror(errno));
    }
    char **argv = dxl_argv_line(exe, flags);
    char fd_entry[64];
    char **envp = child_env(keep_fd, fd_entry, sizeof fd_entry);
    *pid = fork();
    if (*pid < 0) {
        dxl_err_set(err, "fork: %s", strerror(errno));
        close(report[0]); close(report[1]);
        if (log_fd >= 0) close(log_fd);
        dxl_argv_free(argv);
        free(envp);
        return -1;
    }
    if (*pid == 0) {
        signal(SIGPIPE, SIG_DFL);
        if (log_fd >= 0) { dup2(log_fd, STDOUT_FILENO); dup2(log_fd, STDERR_FILENO); }
        if (keep_fd >= 0) fcntl(keep_fd, F_SETFD, 0);   /* the one descriptor the game keeps */
        exec_child(exe, argv, envp, workdir, report[1]);
    }
    dxl_argv_free(argv);
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

int dxl_game_start(dxl_game *g, const char *exe, const char *flags, const char *workdir,
                   const char *log_path, dxl_err *err) {
    memset(g, 0, sizeof *g);
    g->channel = -1;
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) != 0) {
        dxl_err_set(err, "socketpair: %s", strerror(errno));
        return -1;
    }
    if (spawn(&g->pid, exe, flags, workdir, sv[1], log_path, err) != 0) {
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

int dxl_game_wait(dxl_game *g, int extra_fd, int ms, char **line) {
    *line = NULL;
    if (take_line(g, line)) return DXL_GAME_LINE;
    struct pollfd p[2];
    int n = 0, chan = -1, extra = -1;
    if (g->channel >= 0) { p[n].fd = g->channel; p[n].events = POLLIN; chan = n++; }
    if (extra_fd >= 0)   { p[n].fd = extra_fd;   p[n].events = POLLIN; extra = n++; }
    int r = poll(p, (nfds_t)n, ms);
    int seen = 0;
    if (r > 0 && chan >= 0 && (p[chan].revents & (POLLIN | POLLHUP | POLLERR))) {
        if (g->pending_len == g->pending_cap) {
            g->pending_cap = g->pending_cap ? g->pending_cap * 2 : 1024;
            g->pending = dxl_xrealloc(g->pending, g->pending_cap);
        }
        ssize_t got = read(g->channel, g->pending + g->pending_len,
                           g->pending_cap - g->pending_len);
        if (got > 0) g->pending_len += (size_t)got;
        else if (got == 0 || (errno != EAGAIN && errno != EINTR)) {
            close(g->channel);          /* the engine's end is gone: it ended, or dropped it */
            g->channel = -1;
        }
        if (take_line(g, line)) seen |= DXL_GAME_LINE;
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

void dxl_game_close(dxl_game *g) {
    free(g->pending);
    g->pending = NULL;
    g->pending_len = g->pending_cap = 0;
    if (g->channel >= 0) close(g->channel);
    g->channel = -1;
}

int dxl_child_start(const char *exe, const char *flags, const char *workdir, pid_t *pid,
                    dxl_err *err) {
    return spawn(pid, exe, flags, workdir, -1, NULL, err);
}

int dxl_child_reap(pid_t pid) {
    int st;
    pid_t r = waitpid(pid, &st, WNOHANG);
    return r == pid || (r < 0 && errno == ECHILD);
}

int dxl_open_url(const char *url) {
    /* Two forks, so the opener is nobody's child to wait for. */
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        if (fork() == 0) {
            signal(SIGPIPE, SIG_DFL);   /* the launcher ignores it; the browser must not */
            execlp("xdg-open", "xdg-open", url, (char *)NULL);
            _exit(127);
        }
        _exit(0);
    }
    waitpid(pid, NULL, 0);
    return 0;
}

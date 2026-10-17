/* A stand-in for the engine, for the tests: started with the engine's own
 * command line, it does what DXL_STUB says, and talks on DXL_LAUNCHER_FD as
 * the engine does (hello as it starts, ready once "its map is in").
 *
 *   --version   (any mode) answers as VibeEngine does; DXL_STUB_FEATURES
 *               the features (default "userdir")
 *   args        prints its working folder, then its words one a line
 *   env         prints the value of the variable DXL_STUB_ENV names
 *   ready       hello, ready, then prints each line the launcher sends,
 *               exiting 0 at "quit" or after DXL_STUB_MS (default 5000)
 *   crash       hello, ready, then SIGSEGV
 *   fail        hello, then "SurrealEngine error: <DXL_STUB_LINE>", exit 1
 *   relaunch    hello, ready, "relaunch <DXL_STUB_LINE>", exit 0
 *   stubborn    hello, ready, SIGTERM ignored, sleeps until killed
 *   nogame      "No UE1 game found in the given folder.", exit 0, no ready
 */
#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int line_fd = -1;

static void say(const char *line) {
    if (line_fd < 0) return;
    char buf[1100];
    int n = snprintf(buf, sizeof buf, "%s\n", line);
    if (send(line_fd, buf, (size_t)n, MSG_NOSIGNAL) < 0) perror("stub: send");
}

static const char *env_or(const char *name, const char *fallback) {
    const char *v = getenv(name);
    return v && *v ? v : fallback;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            printf("VibeEngine stub000 (2026-10-16)\nfeatures: %s\n", env_or("DXL_STUB_FEATURES", "userdir"));
            return 0;
        }
    }
    const char *fd = getenv("DXL_LAUNCHER_FD");
    if (fd) line_fd = atoi(fd);
    const char *mode = env_or("DXL_STUB", "ready");
    setvbuf(stdout, NULL, _IONBF, 0);

    if (strcmp(mode, "args") == 0) {
        char cwd[1024];
        printf("cwd %s\n", getcwd(cwd, sizeof cwd) ? cwd : "?");
        for (int i = 0; i < argc; i++) printf("[%s]\n", argv[i]);
        return 0;
    }
    if (strcmp(mode, "env") == 0) {
        const char *v = getenv(env_or("DXL_STUB_ENV", "HOME"));
        printf("%s\n", v ? v : "(unset)");
        return 0;
    }
    if (strcmp(mode, "nogame") == 0) {
        say("hello");
        printf("No UE1 game found in the given folder.\n");
        return 0;
    }
    say("hello");
    if (strcmp(mode, "fail") == 0) {
        printf("SurrealEngine error: %s\n", env_or("DXL_STUB_LINE", "Could not open the map"));
        return 1;
    }
    say("ready");
    if (strcmp(mode, "crash") == 0) {
        printf("stub: crashing\n");
        raise(SIGSEGV);
        return 2;
    }
    if (strcmp(mode, "relaunch") == 0) {
        char line[1100];
        snprintf(line, sizeof line, "relaunch %s", env_or("DXL_STUB_LINE", "-changevideo"));
        say(line);
        return 0;
    }
    if (strcmp(mode, "stubborn") == 0) {
        signal(SIGTERM, SIG_IGN);
        for (;;) pause();
    }

    /* ready: the launcher's lines, until quit or the time is up. */
    int ms = atoi(env_or("DXL_STUB_MS", "5000"));
    char pending[2048];
    size_t have = 0;
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long gone = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_nsec - start.tv_nsec) / 1000000;
        if (gone >= ms || line_fd < 0) break;
        struct pollfd p = { line_fd, POLLIN, 0 };
        if (poll(&p, 1, 50) <= 0) continue;
        ssize_t n = recv(line_fd, pending + have, sizeof pending - have - 1, 0);
        if (n <= 0) break;
        have += (size_t)n;
        pending[have] = '\0';
        char *nl;
        while ((nl = strchr(pending, '\n')) != NULL) {
            *nl = '\0';
            printf("line: %s\n", pending);
            if (strcmp(pending, "quit") == 0) return 0;
            memmove(pending, nl + 1, have - (size_t)(nl + 1 - pending) + 1);
            have = strlen(pending);
        }
    }
    return 0;
}

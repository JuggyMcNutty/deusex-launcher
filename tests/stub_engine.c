/* A stand-in for the engine, for the tests: started with the engine's own
 * command line, it does what DXL_STUB says, and talks on DXL_LAUNCHER_FD as
 * the engine does (hello as it starts, ready once "its map is in").
 *
 *   --version   (any mode) answers as DXEngine does; DXL_STUB_FEATURES
 *               the features (default "userdir"); with DXL_STUB_VERSION_ERR
 *               set, that line on stderr and exit 127, as a program the
 *               loader could not start; with DXL_STUB_VERSION_EXIT, that
 *               exit code alone, as Windows' loader ends one (0xC0000135)
 *   args        prints its working folder, then its words one a line, as
 *               the engine splits its command line
 *               (any other mode prints its words too with DXL_STUB_ARGS set)
 *   env         prints the value of the variable DXL_STUB_ENV names
 *   ready       hello, ready, then prints each line the launcher sends,
 *               exiting 0 at "quit", or at "Quit", the launcher's stop the
 *               engine takes, or after DXL_STUB_MS (default 5000)
 *   rewrite     ready's, having written to its --userini as the engine does
 *               at a clean end ([DXL.Stub] Wrote=1), and added an x to the
 *               file DXL_STUB_COUNT names
 *   crash       hello, ready, then SIGSEGV (Windows: an access violation)
 *   fail        hello, then "DXEngine error: <DXL_STUB_LINE>", exit 1
 *   relaunch    hello, ready, "relaunch <DXL_STUB_LINE>", exit 0
 *   stubborn    hello, ready, deaf to the launcher's stop (SIGTERM; on
 *               Windows Quit and WM_CLOSE), sleeps until killed
 *   handles     (Windows) whether the handle DXL_STUB_HANDLE names is the
 *               file DXL_STUB_FILE names, inherited: "handle kept" or
 *               "handle not kept"; and "line kept" when the launcher's line
 *               is its pipe
 *   nogame      "No UE1 game found in the given folder.", exit 0, no ready
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <shellapi.h>
#else
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#endif

#ifdef _WIN32
static HANDLE line = INVALID_HANDLE_VALUE;

static int line_open(void) { return line != INVALID_HANDLE_VALUE; }

static void say(const char *text) {
    if (!line_open()) return;
    char buf[1100];
    int n = snprintf(buf, sizeof buf, "%s\n", text);
    DWORD wrote = 0;
    if (!WriteFile(line, buf, (DWORD)n, &wrote, NULL)) fprintf(stderr, "stub: send: error %lu\n", GetLastError());
}

/* What has come on the line, at most size: its bytes, 0 when nothing came
 * in a while, -1 at its end. */
static long hear(char *buf, size_t size) {
    DWORD avail = 0, n = 0;
    if (!PeekNamedPipe(line, NULL, 0, NULL, &avail, NULL)) return -1;
    if (!avail) {
        Sleep(20);
        return 0;
    }
    if (!ReadFile(line, buf, avail < size ? avail : (DWORD)size, &n, NULL)) return -1;
    return (long)n;
}

static long long now_ms(void) { return (long long)GetTickCount64(); }

static char *utf8(const wchar_t *w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = malloc(n > 0 ? (size_t)n : 1);
    if (n <= 0 || !WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL)) s[0] = '\0';
    return s;
}
#else
static int line_fd = -1;

static int line_open(void) { return line_fd >= 0; }

static void say(const char *text) {
    if (line_fd < 0) return;
    char buf[1100];
    int n = snprintf(buf, sizeof buf, "%s\n", text);
    if (send(line_fd, buf, (size_t)n, MSG_NOSIGNAL) < 0) perror("stub: send");
}

static long hear(char *buf, size_t size) {
    struct pollfd p = { line_fd, POLLIN, 0 };
    if (poll(&p, 1, 50) <= 0) return 0;
    ssize_t n = recv(line_fd, buf, size, 0);
    return n <= 0 ? -1 : (long)n;
}

static long long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
#endif

static const char *env_or(const char *name, const char *fallback) {
    const char *v = getenv(name);
    return v && *v ? v : fallback;
}

/* Its working folder, '/' between names, then its words, one a line. */
static void print_words(int argc, char **argv, int with_cwd) {
#ifdef _WIN32
    if (with_cwd) {
        wchar_t cwd[1024];
        char *c = GetCurrentDirectoryW(1024, cwd) ? utf8(cwd) : NULL;
        for (char *p = c; p && *p; p++)
            if (*p == '\\') *p = '/';
        printf("cwd %s\n", c ? c : "?");
        free(c);
    }
    /* Split as the engine splits it (CommandLineToArgvW). */
    (void)argc; (void)argv;
    int n = 0;
    wchar_t **words = CommandLineToArgvW(GetCommandLineW(), &n);
    for (int i = 0; words && i < n; i++) {
        char *w = utf8(words[i]);
        printf("[%s]\n", w);
        free(w);
    }
    LocalFree(words);
#else
    if (with_cwd) {
        char cwd[1024];
        printf("cwd %s\n", getcwd(cwd, sizeof cwd) ? cwd : "?");
    }
    for (int i = 0; i < argc; i++) printf("[%s]\n", argv[i]);
#endif
}

int main(int argc, char **argv) {
#ifdef _WIN32
    /* Its output as the engine's, lines ended by \n alone. */
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            const char *err = getenv("DXL_STUB_VERSION_ERR");
            if (err && *err) {
                fprintf(stderr, "%s\n", err);
                return 127;
            }
            const char *code = getenv("DXL_STUB_VERSION_EXIT");
            if (code && *code) return (int)strtoul(code, NULL, 0);
            printf("DXEngine stub000 (2026-10-16)\nfeatures: %s\n", env_or("DXL_STUB_FEATURES", "userdir"));
            return 0;
        }
    }
    const char *fd = getenv("DXL_LAUNCHER_FD");
#ifdef _WIN32
    if (fd) line = (HANDLE)(intptr_t)strtoll(fd, NULL, 10);
#else
    if (fd) line_fd = atoi(fd);
#endif
    const char *mode = env_or("DXL_STUB", "ready");
    setvbuf(stdout, NULL, _IONBF, 0);

    if (strcmp(mode, "args") == 0) {
        print_words(argc, argv, 1);
        return 0;
    }
    if (strcmp(mode, "env") == 0) {
        const char *v = getenv(env_or("DXL_STUB_ENV", "HOME"));
        printf("%s\n", v ? v : "(unset)");
        return 0;
    }
#ifdef _WIN32
    if (strcmp(mode, "handles") == 0) {
        /* The file's handle, by its value, names the file here only if it
         * was inherited: any other is no handle, or another thing's. */
        HANDLE h = (HANDLE)(intptr_t)strtoll(env_or("DXL_STUB_HANDLE", "0"), NULL, 10);
        wchar_t path[1024];
        DWORD n = GetFinalPathNameByHandleW(h, path, 1024, FILE_NAME_NORMALIZED);
        char *got = n && n < 1024 ? utf8(path) : NULL;
        const char *want = env_or("DXL_STUB_FILE", "?");
        size_t gl = got ? strlen(got) : 0, wl = strlen(want);
        printf("handle %s\n", got && gl >= wl && _stricmp(got + gl - wl, want) == 0 ? "kept" : "not kept");
        free(got);
        if (line_open() && GetFileType(line) == FILE_TYPE_PIPE) printf("line kept\n");
        return 0;
    }
#endif
    if (strcmp(mode, "nogame") == 0) {
        say("hello");
        printf("No UE1 game found in the given folder.\n");
        return 0;
    }
    say("hello");
    /* Its words too, with DXL_STUB_ARGS set: what a run was started with. */
    if (getenv("DXL_STUB_ARGS")) print_words(argc, argv, 0);
    if (strcmp(mode, "fail") == 0) {
        printf("DXEngine error: %s\n", env_or("DXL_STUB_LINE", "Could not open the map"));
        return 1;
    }
    say("ready");
    if (strcmp(mode, "crash") == 0) {
        printf("stub: crashing\n");
#ifdef _WIN32
        RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0, NULL);
#else
        raise(SIGSEGV);
#endif
        return 2;
    }
    if (strcmp(mode, "relaunch") == 0) {
        char text[1100];
        snprintf(text, sizeof text, "relaunch %s", env_or("DXL_STUB_LINE", "-changevideo"));
        say(text);
        return 0;
    }
    if (strcmp(mode, "stubborn") == 0) {
#ifdef _WIN32
        /* Nothing read from the line, no window to close. */
        for (;;) Sleep(1000);
#else
        signal(SIGTERM, SIG_IGN);
        for (;;) pause();
#endif
    }

    if (strcmp(mode, "rewrite") == 0) {
        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "--userini=", 10) != 0) continue;
            FILE *f = fopen(argv[i] + 10, "a");
            if (f) {
                fputs("\n[DXL.Stub]\nWrote=1\n", f);
                fclose(f);
            }
        }
        const char *count = getenv("DXL_STUB_COUNT");
        FILE *c = count ? fopen(count, "a") : NULL;
        if (c) {
            fputc('x', c);
            fclose(c);
        }
    }

    /* ready: the launcher's lines, until quit or the time is up. */
    int ms = atoi(env_or("DXL_STUB_MS", "5000"));
    char pending[2048];
    size_t have = 0;
    long long start = now_ms();
    for (;;) {
        if (now_ms() - start >= ms || !line_open()) break;
        long n = hear(pending + have, sizeof pending - have - 1);
        if (n < 0) break;
        if (n == 0) continue;
        have += (size_t)n;
        pending[have] = '\0';
        char *nl;
        while ((nl = strchr(pending, '\n')) != NULL) {
            *nl = '\0';
            printf("line: %s\n", pending);
            if (strcmp(pending, "quit") == 0 || strcmp(pending, "Quit") == 0) return 0;
            memmove(pending, nl + 1, have - (size_t)(nl + 1 - pending) + 1);
            have = strlen(pending);
        }
    }
    return 0;
}

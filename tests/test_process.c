#include "test.h"
#include "scratch.h"
#include "ends.h"
#include "app/run.h"
#include "app/session.h"
#include "core/engine_args.h"
#include "platform/process.h"
#include "platform/os.h"
#include "platform/fs.h"
#include "platform/startup.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/* The stub engine (tests/stub_engine.c), built beside the tests. */
#ifndef DXL_STUB_ENGINE
#define DXL_STUB_ENGINE "stub_engine"
#endif

/* A word onto a line of them, ' ' between, cut to fit. */
static void append(char *dst, size_t size, const char *word) {
    size_t n = strlen(dst);
    if (n && n + 1 < size) dst[n++] = ' ';
    size_t m = strlen(word);
    if (m > size - n - 1) m = size - n - 1;
    memcpy(dst + n, word, m);
    dst[n + m] = '\0';
}

/* Waits for the game's end, at most ms; the lines it said, ' ' between. */
static int wait_end(dxl_game *g, int ms, char *said, size_t size) {
    if (size) said[0] = '\0';
    for (int waited = 0; waited < ms; waited += 20) {
        char *line = NULL;
        int ev = dxl_game_wait(g, NULL, 20, &line);
        if (ev & DXL_GAME_LINE) {
            append(said, size, line);
            free(line);
        }
        if (ev & DXL_GAME_ENDED) return 1;
    }
    return 0;
}

/* The words as given, a quoted one whole; the working folder. */
static void test_words_kept(void) {
    char *dir = dxl_xstrdup(scratch_dir("process-args"));
    char *log = scratch_path(dir, "run.log");
    char *argv[] = { DXL_STUB_ENGINE, "--no-launcher", "/games/Deus Ex",
                     "--cmdline=INI=\"My Mod.ini\" -nosound", NULL };
    char *env[] = { "DXL_STUB=args", NULL };
    dxl_game g;
    dxl_err e;
    CHECK_INT(dxl_game_start_argv(&g, argv, env, dir, log, &e), 0);
    char said[256];
    CHECK_INT(wait_end(&g, 5000, said, sizeof said), 1);
    CHECK_INT(g.status, 0);
    char *text = slurp(log, NULL);
    char want[600];
    snprintf(want, sizeof want, "cwd %s\n", dir);
    CHECK(text && strstr(text, want) != NULL);
    CHECK(text && strstr(text, "[/games/Deus Ex]\n") != NULL);
    CHECK(text && strstr(text, "[--cmdline=INI=\"My Mod.ini\" -nosound]\n") != NULL);
    free(text);
    free(log);
    dxl_game_close(&g);
    scratch_remove(dir);
    free(dir);
}

/* Variables set and taken out for the game alone. */
static void test_env(void) {
    char *dir = dxl_xstrdup(scratch_dir("process-env"));
    char *log = scratch_path(dir, "run.log");
    dxl_env_set("DXL_TEST_GONE", "present");
    char *gone[] = { "DXL_STUB=env", "DXL_STUB_ENV=DXL_TEST_GONE", "DXL_TEST_GONE", NULL };
    char *argv[] = { DXL_STUB_ENGINE, NULL };
    dxl_game g;
    CHECK_INT(dxl_game_start_argv(&g, argv, gone, NULL, log, NULL), 0);
    char said[64];
    wait_end(&g, 5000, said, sizeof said);
    dxl_game_close(&g);
    char *set[] = { "DXL_STUB=env", "DXL_STUB_ENV=DXL_TEST_SET", "DXL_TEST_SET=yes it is", NULL };
    CHECK_INT(dxl_game_start_argv(&g, argv, set, NULL, log, NULL), 0);
    wait_end(&g, 5000, said, sizeof said);
    dxl_game_close(&g);
    /* A long value whole: a pad mapping block is one. */
    char *big = dxl_xmalloc(2100);
    memcpy(big, "DXL_TEST_SET=", 13);
    memset(big + 13, 'm', 2000);
    big[2013] = '\0';
    char *large[] = { "DXL_STUB=env", "DXL_STUB_ENV=DXL_TEST_SET", big, NULL };
    CHECK_INT(dxl_game_start_argv(&g, argv, large, NULL, log, NULL), 0);
    wait_end(&g, 5000, said, sizeof said);
    dxl_game_close(&g);
    char *text = slurp(log, NULL), *want = dxl_xmalloc(2100);
    snprintf(want, 2100, "(unset)\nyes it is\n%s\n", big + 13);
    CHECK_STR(text, want);
    free(want);
    free(big);
    free(text);
    /* This process's own are as they were. */
    CHECK_STR(getenv("DXL_TEST_GONE"), "present");
    CHECK(getenv("DXL_TEST_SET") == NULL);
    dxl_env_set("DXL_TEST_GONE", NULL);
    free(log);
    scratch_remove(dir);
    free(dir);
}

/* The line both ways: hello and ready from the game, the launcher's lines
 * to it. */
static void test_line(void) {
    char *dir = dxl_xstrdup(scratch_dir("process-line"));
    char *log = scratch_path(dir, "run.log");
    char *argv[] = { DXL_STUB_ENGINE, NULL };
    char *env[] = { "DXL_STUB=ready", "DXL_STUB_MS=5000", NULL };
    dxl_game g;
    CHECK_INT(dxl_game_start_argv(&g, argv, env, NULL, log, NULL), 0);
    int hello = 0, ready = 0;
    for (int i = 0; i < 250 && !ready; i++) {
        char *line = NULL;
        if (dxl_game_wait(&g, NULL, 20, &line) & DXL_GAME_LINE) {
            if (strcmp(line, "hello") == 0) hello = 1;
            if (strcmp(line, "ready") == 0) ready = 1;
            free(line);
        }
    }
    CHECK_INT(hello, 1);
    CHECK_INT(ready, 1);
    dxl_game_send(&g, "TakeFocus");
    dxl_game_send(&g, "Open 01_NYC_UNATCOIsland");
    dxl_game_send(&g, "Quit");
    char said[64];
    CHECK_INT(wait_end(&g, 5000, said, sizeof said), 1);
    CHECK_INT(dxl_game_clean(&g), 1);
    char *text = slurp(log, NULL);
    CHECK(text && strstr(text, "line: TakeFocus\nline: Open 01_NYC_UNATCOIsland\nline: Quit\n") != NULL);
    free(text);
    free(log);
    dxl_game_close(&g);
    scratch_remove(dir);
    free(dir);
}

/* A line of any length arrives whole: the engine's RELAUNCH line can be
 * long (the old reader cut one at 1023 bytes, the rest a line of its own). */
static void test_long_line(void) {
    char *dir = dxl_xstrdup(scratch_dir("process-long"));
    char *log = scratch_path(dir, "run.log");
    char *argv[] = { DXL_STUB_ENGINE, NULL };
    char *want = dxl_xmalloc(3000 + 16);
    memset(want, 'a', 3000);
    memcpy(want + 3000, " -server", 9);
    char *entry = dxl_xmalloc(3100);
    snprintf(entry, 3100, "DXL_STUB_LINE=%s", want);
    char *env[] = { "DXL_STUB=relaunch", entry, NULL };
    dxl_game g;
    CHECK_INT(dxl_game_start_argv(&g, argv, env, NULL, log, NULL), 0);
    char *relaunch = NULL;
    for (int i = 0; i < 250 && !relaunch; i++) {
        char *line = NULL;
        if (dxl_game_wait(&g, NULL, 20, &line) & DXL_GAME_LINE) {
            if (strncmp(line, "relaunch ", 9) == 0) relaunch = dxl_xstrdup(line + 9);
            free(line);
        }
    }
    CHECK(relaunch != NULL);
    CHECK_INT(relaunch ? (int)strlen(relaunch) : 0, 3008);
    CHECK(relaunch && strcmp(relaunch, want) == 0);
    char said[64];
    wait_end(&g, 5000, said, sizeof said);
    free(relaunch);
    free(entry);
    free(want);
    free(log);
    dxl_game_close(&g);
    scratch_remove(dir);
    free(dir);
}

static void test_crash(void) {
    char *argv[] = { DXL_STUB_ENGINE, NULL };
    char *env[] = { "DXL_STUB=crash", NULL };
    char *dir = dxl_xstrdup(scratch_dir("process-crash"));
    char *log = scratch_path(dir, "run.log");
    dxl_game g;
    CHECK_INT(dxl_game_start_argv(&g, argv, env, NULL, log, NULL), 0);
    char said[64];
    CHECK_INT(wait_end(&g, 5000, said, sizeof said), 1);
    CHECK_INT(g.status, END_CRASH_STATUS);
    CHECK_INT(dxl_game_clean(&g), 0);
    CHECK_STR(said, "hello ready");
    free(log);
    dxl_game_close(&g);
    scratch_remove(dir);
    free(dir);
}

#ifndef _WIN32
/* Started by a parent that ignores SIGCHLD, which an exec passes on: the
 * start takes it back, so the game's end is seen, not waited for ever. */
static void test_child_ignored(void) {
    signal(SIGCHLD, SIG_IGN);
    static atomic_int stop;
    dxl_startup(&stop, NULL);
    char *argv[] = { DXL_STUB_ENGINE, NULL };
    char *env[] = { "DXL_STUB=crash", NULL };
    char *dir = dxl_xstrdup(scratch_dir("process-sigchld"));
    char *log = scratch_path(dir, "run.log");
    dxl_game g;
    CHECK_INT(dxl_game_start_argv(&g, argv, env, NULL, log, NULL), 0);
    char said[64];
    CHECK_INT(wait_end(&g, 5000, said, sizeof said), 1);
    CHECK_INT(g.status, END_CRASH_STATUS);
    dxl_game_close(&g);
    free(log);
    scratch_remove(dir);
    free(dir);
}
#endif

#ifdef _WIN32
/* The console's code page as the launcher found it, after it ends: its
 * UTF-8 is its own while it runs. dxl-cli started on this console, as from
 * a cmd.exe; one is made where there is none. */
static void test_console_code_page(void) {
    if (!GetConsoleOutputCP() && !AllocConsole()) {
        printf("  no console to be had: its code page not checked\n");
        return;
    }
    UINT was = GetConsoleOutputCP();
    CHECK(SetConsoleOutputCP(437));
    wchar_t line[1024];
    swprintf(line, sizeof line / sizeof *line, L"\"%hs\" --version", DXL_CLI);
    STARTUPINFOW si;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    CHECK(CreateProcessW(NULL, line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi));
    if (pi.hProcess) {
        CHECK_INT((int)WaitForSingleObject(pi.hProcess, 10000), (int)WAIT_OBJECT_0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    CHECK_INT((int)GetConsoleOutputCP(), 437);
    SetConsoleOutputCP(was);
}
#endif

static void test_cannot_run(void) {
    char *argv[] = { "/nonexistent/DXEngine", NULL };
    dxl_game g;
    dxl_err e;
    CHECK_INT(dxl_game_start_argv(&g, argv, NULL, NULL, NULL, &e), -1);
    CHECK(strstr(dxl_err_msg(&e), "cannot run /nonexistent/DXEngine") != NULL);
}

/* --version's answer captured; a program that never ends is killed when
 * its time is up. */
static void test_capture(void) {
    char out[512];
    int status = -1;
    char *version[] = { DXL_STUB_ENGINE, "--version", NULL };
    CHECK_INT(dxl_run_capture(version, 3000, out, sizeof out, NULL, 0, &status, NULL), 0);
    CHECK_INT(status, 0);
    dxl_engine_version v;
    CHECK_INT(dxl_engine_version_parse(out, &v), 0);
    CHECK_INT(dxl_engine_has(&v, "userdir"), 1);

    dxl_env_set("DXL_STUB", "stubborn");
    char *hang[] = { DXL_STUB_ENGINE, NULL };
    dxl_err e;
    CHECK_INT(dxl_run_capture(hang, 300, out, sizeof out, NULL, 0, &status, &e), DXL_RUN_TIMED_OUT);
    CHECK_INT(status, END_KILL_STATUS);
    dxl_env_set("DXL_STUB", NULL);

    char errs[512];
#ifdef _WIN32
    /* A program Windows' loader stopped: its end, no output. */
    dxl_env_set("DXL_STUB_VERSION_EXIT", "0xC0000135");
    CHECK_INT(dxl_run_capture(version, 3000, out, sizeof out, errs, sizeof errs, &status, &e), 0);
    CHECK_INT(status, (int)0xC0000135u);
    CHECK_STR(out, "");
    dxl_env_set("DXL_STUB_VERSION_EXIT", NULL);
#else
    /* Its errors, when asked for: a loader's that stopped it, which did
     * not run. */
    dxl_env_set("DXL_STUB_VERSION_ERR", "stub: version `GLIBC_2.99' not found");
    CHECK_INT(dxl_run_capture(version, 3000, out, sizeof out, errs, sizeof errs, &status, &e), -1);
    CHECK_INT(status, 127);
    CHECK_STR(out, "");
    CHECK(strstr(errs, "GLIBC_2.99") != NULL);
    dxl_env_set("DXL_STUB_VERSION_ERR", NULL);
#endif

    char *none[] = { "/nonexistent/engine", "--version", NULL };
    CHECK_INT(dxl_run_capture(none, 1000, out, sizeof out, errs, sizeof errs, &status, &e), -1);
    CHECK(strstr(errs, "/nonexistent/engine") != NULL);
#ifndef _WIN32
    /* A bare name, found on PATH as a shell finds it; one that is nowhere
     * is said so. */
    char *bare[] = { "sh", "-c", "echo found", NULL };
    CHECK_INT(dxl_run_capture(bare, 3000, out, sizeof out, errs, sizeof errs, &status, &e), 0);
    CHECK_STR(out, "found\n");
    char *nowhere[] = { "dxl-no-such-program", NULL };
    CHECK_INT(dxl_run_capture(nowhere, 1000, out, sizeof out, errs, sizeof errs, &status, &e), -1);
    CHECK(strstr(dxl_err_msg(&e), "cannot run dxl-no-such-program: not found") != NULL);
#endif
}

/* A capture given up once its stop is set: the program killed then, not
 * at its time limit -- the window's probe as the window goes. */
static void test_capture_stopped(void) {
    atomic_int stop;
    atomic_init(&stop, 1);
    dxl_env_set("DXL_STUB", "stubborn");
    char *hang[] = { DXL_STUB_ENGINE, NULL };
    char out[64];
    int status = -1;
    dxl_err e;
    long long t0 = dxl_os_ms();
    CHECK_INT(dxl_run_capture_until(hang, 10000, &stop, out, sizeof out, NULL, 0, &status, &e), DXL_RUN_STOPPED);
    CHECK(dxl_os_ms() - t0 < 3000);
    CHECK_INT(status, END_KILL_STATUS);
    dxl_env_set("DXL_STUB", NULL);
}

#ifndef _WIN32
/* Whether a /proc/<pid>/status text has SIGPIPE among the signals ignored;
 * -1 for no such text. */
static int ignores_sigpipe(const char *status) {
    const char *ign = status ? strstr(status, "SigIgn:") : NULL;
    if (!ign) return -1;
    return (int)((strtoull(ign + 7, NULL, 16) >> (SIGPIPE - 1)) & 1);
}

/* A helper run for its output starts with SIGPIPE's default, as the game
 * does: the launcher ignores it (as this test does), and an exec keeps an
 * ignored signal ignored. */
static void test_capture_sigpipe(void) {
    char out[4096];
    int status = -1;
    dxl_err e;
    char *cat[] = { "cat", "/proc/self/status", NULL };
    CHECK_INT(dxl_run_capture(cat, 3000, out, sizeof out, NULL, 0, &status, &e), 0);
    CHECK_INT(ignores_sigpipe(out), 0);
}

/* The opener too, and what it opens: xdg-open, here a stand-in on PATH that
 * keeps its own status. */
static void test_opener_sigpipe(void) {
    char *dir = dxl_xstrdup(scratch_dir("process-opener"));
    char *said = scratch_path(dir, "said");
    char script[1200];
    snprintf(script, sizeof script, "#!/bin/sh\ncat /proc/$$/status > '%s.part' && mv '%s.part' '%s'\n", said, said,
             said);
    scratch_write(dir, "xdg-open", script);
    char *opener = scratch_path(dir, "xdg-open");
    chmod(opener, 0755);
    const char *was = getenv("PATH");
    char *path_was = dxl_xstrdup(was ? was : "/usr/bin:/bin");
    size_t n = strlen(dir) + strlen(path_was) + 2;
    char *path = dxl_xmalloc(n);
    snprintf(path, n, "%s:%s", dir, path_was);
    dxl_env_set("PATH", path);
    CHECK_INT(dxl_open_url("https://example.invalid/"), 0);
    char *text = NULL;
    for (int i = 0; i < 250 && !text; i++) {
        text = slurp(said, NULL);
        if (!text) dxl_os_sleep_ms(20);
    }
    CHECK_INT(ignores_sigpipe(text), 0);
    dxl_env_set("PATH", path_was);
    free(text);
    free(path);
    free(path_was);
    free(opener);
    free(said);
    scratch_remove(dir);
    free(dir);
}
#endif

#ifdef _WIN32
/* A handle the launcher holds inheritable -- a library's file, as one may
 * open it -- reaches neither the game nor a helper; the game's own line
 * does: the stub finds the file by the handle's value only if it was
 * handed on. */
static void test_handles_kept(void) {
    char *dir = dxl_xstrdup(scratch_dir("process-handles"));
    scratch_write(dir, "held.txt", "held");
    char *path = scratch_path(dir, "held.txt");
    wchar_t w[1024];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w, 1024);
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE held = CreateFileW(w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa,
                              OPEN_EXISTING, 0, NULL);
    CHECK(held != INVALID_HANDLE_VALUE);
    char value[32];
    snprintf(value, sizeof value, "%llu", (unsigned long long)(uintptr_t)held);
    dxl_env_set("DXL_STUB", "handles");
    dxl_env_set("DXL_STUB_HANDLE", value);
    dxl_env_set("DXL_STUB_FILE", "held.txt");
    char *log = scratch_path(dir, "run.log");
    char *argv[] = { DXL_STUB_ENGINE, NULL };
    dxl_game g;
    dxl_err e;
    CHECK_INT(dxl_game_start_argv(&g, argv, NULL, dir, log, &e), 0);
    char said[64];
    CHECK_INT(wait_end(&g, 5000, said, sizeof said), 1);
    char *text = slurp(log, NULL);
    CHECK(text && strstr(text, "handle not kept\n") != NULL);
    CHECK(text && strstr(text, "line kept\n") != NULL);
    free(text);
    dxl_game_close(&g);
    char out[256];
    int status = -1;
    CHECK_INT(dxl_run_capture(argv, 3000, out, sizeof out, NULL, 0, &status, &e), 0);
    CHECK(strstr(out, "handle not kept\n") != NULL);
    CHECK(strstr(out, "line kept") == NULL);
    dxl_env_set("DXL_STUB", NULL);
    dxl_env_set("DXL_STUB_HANDLE", NULL);
    dxl_env_set("DXL_STUB_FILE", NULL);
    CloseHandle(held);
    free(log);
    free(path);
    scratch_remove(dir);
    free(dir);
}
#else
/* Whether a listing of /proc/<pid>/fd, one a line, has fd. */
static int lists_fd(const char *text, int fd) {
    char want[32];
    snprintf(want, sizeof want, "\n%d\n", fd);
    return text && strstr(text, want) != NULL;
}

/* A descriptor the launcher holds without close-on-exec -- the window's
 * fonts, as SDL_ttf opens them -- reaches neither the game nor a helper;
 * the game's own line does. */
static void test_descriptors_kept(void) {
    char *dir = dxl_xstrdup(scratch_dir("process-fds"));
    char *log = scratch_path(dir, "run.log");
    int held = open("/dev/null", O_RDONLY);
    CHECK(held > 2);
    char *argv[] = { "/bin/sh", "-c", "echo; ls /proc/$$/fd; echo \"line $DXL_LAUNCHER_FD\"", NULL };
    dxl_game g;
    dxl_err e;
    CHECK_INT(dxl_game_start_argv(&g, argv, NULL, dir, log, &e), 0);
    char said[256];
    CHECK_INT(wait_end(&g, 5000, said, sizeof said), 1);
    char *text = slurp(log, NULL);
    CHECK(!lists_fd(text, held));
    const char *line = text ? strstr(text, "line ") : NULL;
    CHECK(line && lists_fd(text, atoi(line + 5)));
    free(text);
    char out[1024];
    int status = -1;
    char *ls[] = { "/bin/sh", "-c", "echo; ls /proc/$$/fd", NULL };
    CHECK_INT(dxl_run_capture(ls, 3000, out, sizeof out, NULL, 0, &status, &e), 0);
    CHECK(lists_fd(out, 1));
    CHECK(!lists_fd(out, held));
    close(held);
    free(log);
    dxl_game_close(&g);
    scratch_remove(dir);
    free(dir);
}
#endif

/* A file a test makes a program to run: its mode, where files have modes. */
static void make_program(const char *path) {
#ifdef _WIN32
    (void)path;
#else
    chmod(path, 0755);
#endif
}

/* The engine asked as the launcher asks it, and why it is refused said:
 * not there, one the loader cannot start (a glibc newer than this one's, a
 * library this system lacks), one without the userdir feature. */
static void test_engine_check(void) {
    dxl_engine_version v;
    char brief[160], why[700];
    CHECK_INT(dxl_engine_check("/nonexistent/DXEngine", &v, brief, sizeof brief, why, sizeof why), -1);
    CHECK_STR(brief, "not there");
    CHECK_INT(dxl_engine_check(DXL_STUB_ENGINE, &v, brief, sizeof brief, why, sizeof why), 0);
    CHECK(strstr(v.version, "stub000") != NULL);

#ifdef _WIN32
    /* Windows' loader says nothing: the end it gave the program, named. */
    dxl_env_set("DXL_STUB_VERSION_EXIT", "0xC0000135");
    CHECK_INT(dxl_engine_check(DXL_STUB_ENGINE, &v, brief, sizeof brief, why, sizeof why), -1);
    CHECK_STR(brief, "does not start");
    CHECK(strstr(why, "a DLL it needs not found (0xC0000135)") != NULL);
    dxl_env_set("DXL_STUB_VERSION_EXIT", NULL);
#else
    dxl_env_set("DXL_STUB_VERSION_ERR", "x: /lib64/libm.so.6: version `GLIBC_2.99' not found (required by x)");
    CHECK_INT(dxl_engine_check(DXL_STUB_ENGINE, &v, brief, sizeof brief, why, sizeof why), -1);
    CHECK(strncmp(brief, "needs glibc 2.99, this system has ", 34) == 0);
    CHECK(strstr(why, "built on a newer system") != NULL);
    dxl_env_set("DXL_STUB_VERSION_ERR",
           "x: error while loading shared libraries: libSDL2-2.0.so.0: cannot open shared object file: No such file");
    CHECK_INT(dxl_engine_check(DXL_STUB_ENGINE, &v, brief, sizeof brief, why, sizeof why), -1);
    CHECK_STR(brief, "needs libSDL2-2.0.so.0");
    dxl_env_set("DXL_STUB_VERSION_ERR", NULL);
#endif

    dxl_env_set("DXL_STUB_FEATURES", "other");
    CHECK_INT(dxl_engine_check(DXL_STUB_ENGINE, &v, brief, sizeof brief, why, sizeof why), -1);
    CHECK(strstr(brief, "without --userdir") != NULL);
    dxl_env_set("DXL_STUB_FEATURES", NULL);

    /* In a folder: DXEngine is found; a folder with only an old name has no
     * engine. */
    const char *dir = scratch_dir("engine-in");
    CHECK(dxl_engine_in(dir) == NULL);
    const char *old[] = { "SurrealEngine" DXL_EXE_SUFFIX, "VibeEngine" DXL_EXE_SUFFIX };
    for (size_t i = 0; i < sizeof old / sizeof *old; i++) {
        scratch_write(dir, old[i], "#!/bin/sh\n");
        char *o = scratch_path(dir, old[i]);
        make_program(o);
        free(o);
    }
    CHECK(dxl_engine_in(dir) == NULL);
    scratch_write(dir, "DXEngine" DXL_EXE_SUFFIX, "#!/bin/sh\n");
    char *p = scratch_path(dir, "DXEngine" DXL_EXE_SUFFIX);
    make_program(p);
    char *found = dxl_engine_in(dir);
    CHECK(found && strcmp(found, p) == 0);
    free(found);
    free(p);
    scratch_remove(dir);
}

/* The engine chosen is launcher.ini's, the launcher's own its absence;
 * DXL_ENGINE's over both. */
static void test_engine_chosen(void) {
    const char *home = scratch_dir("engine-home");
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_session s;
    dxl_err e;
    CHECK_INT(dxl_session_open(&s, NULL, &e), 0);
    char *own = dxl_session_own_engine(&s);
    CHECK_STR(s.engine, own);
    dxl_session_set_engine(&s, "/x/DXEngine");
    CHECK_STR(s.engine, "/x/DXEngine");
    CHECK_INT(dxl_session_save(&s, &e), 0);
    dxl_session_close(&s);
    CHECK_INT(dxl_session_open(&s, NULL, &e), 0);
    CHECK_STR(s.engine, "/x/DXEngine");
    dxl_session_set_engine(&s, NULL);
    CHECK_STR(s.engine, own);
    CHECK(dxl_ini_get(s.ini, "Launcher", "Engine") == NULL);
    dxl_session_set_engine(&s, "/x/DXEngine");
    dxl_session_save(&s, &e);
    dxl_session_close(&s);
    dxl_env_set("DXL_ENGINE", "/y/DXEngine");
    CHECK_INT(dxl_session_open(&s, NULL, &e), 0);
    CHECK_STR(s.engine, "/y/DXEngine");
    CHECK_INT(s.engine_by_env, 1);
    dxl_session_set_engine(&s, NULL);
    CHECK_STR(s.engine, "/y/DXEngine");
    dxl_session_close(&s);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_env_set("DXL_HOME", NULL);
    free(own);
    CHECK_INT(dxl_fs_remove_tree(home), 0);
}

TEST_MAIN_BEGIN
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    RUN(test_words_kept);
    RUN(test_env);
    RUN(test_line);
    RUN(test_long_line);
    RUN(test_crash);
    RUN(test_cannot_run);
    RUN(test_capture);
    RUN(test_capture_stopped);
#ifndef _WIN32
    RUN(test_capture_sigpipe);
    RUN(test_opener_sigpipe);
#endif
#ifdef _WIN32
    RUN(test_handles_kept);
#else
    RUN(test_descriptors_kept);
#endif
    RUN(test_engine_check);
    RUN(test_engine_chosen);
#ifndef _WIN32
    RUN(test_child_ignored);
#else
    RUN(test_console_code_page);
#endif
TEST_MAIN_END

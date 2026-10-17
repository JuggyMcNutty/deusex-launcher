#include "test.h"
#include "scratch.h"
#include "core/engine_args.h"
#include "platform/process.h"

#include <signal.h>

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
        char line[1024];
        int ev = dxl_game_wait(g, -1, 20, line, sizeof line);
        if (ev & DXL_GAME_LINE) append(said, size, line);
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
    scratch_remove(dir);
    free(dir);
}

/* Variables set and taken out for the game alone. */
static void test_env(void) {
    char *dir = dxl_xstrdup(scratch_dir("process-env"));
    char *log = scratch_path(dir, "run.log");
    setenv("DXL_TEST_GONE", "present", 1);
    char *gone[] = { "DXL_STUB=env", "DXL_STUB_ENV=DXL_TEST_GONE", "DXL_TEST_GONE", NULL };
    char *argv[] = { DXL_STUB_ENGINE, NULL };
    dxl_game g;
    CHECK_INT(dxl_game_start_argv(&g, argv, gone, NULL, log, NULL), 0);
    char said[64];
    wait_end(&g, 5000, said, sizeof said);
    char *set[] = { "DXL_STUB=env", "DXL_STUB_ENV=DXL_TEST_SET", "DXL_TEST_SET=yes it is", NULL };
    CHECK_INT(dxl_game_start_argv(&g, argv, set, NULL, log, NULL), 0);
    wait_end(&g, 5000, said, sizeof said);
    char *text = slurp(log, NULL);
    CHECK_STR(text, "(unset)\nyes it is\n");
    free(text);
    /* This process's own are as they were. */
    CHECK_STR(getenv("DXL_TEST_GONE"), "present");
    CHECK(getenv("DXL_TEST_SET") == NULL);
    unsetenv("DXL_TEST_GONE");
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
    char line[256];
    int hello = 0, ready = 0;
    for (int i = 0; i < 250 && !ready; i++) {
        if (dxl_game_wait(&g, -1, 20, line, sizeof line) & DXL_GAME_LINE) {
            if (strcmp(line, "hello") == 0) hello = 1;
            if (strcmp(line, "ready") == 0) ready = 1;
        }
    }
    CHECK_INT(hello, 1);
    CHECK_INT(ready, 1);
    dxl_game_send(&g, "TakeFocus");
    dxl_game_send(&g, "Open 01_NYC_UNATCOIsland");
    dxl_game_send(&g, "quit");
    char said[64];
    CHECK_INT(wait_end(&g, 5000, said, sizeof said), 1);
    CHECK_INT(dxl_game_clean(&g), 1);
    char *text = slurp(log, NULL);
    CHECK(text && strstr(text, "line: TakeFocus\nline: Open 01_NYC_UNATCOIsland\nline: quit\n") != NULL);
    free(text);
    free(log);
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
    CHECK_INT(g.status, 128 + SIGSEGV);
    CHECK_INT(dxl_game_clean(&g), 0);
    CHECK_STR(said, "hello ready");
    free(log);
    scratch_remove(dir);
    free(dir);
}

static void test_cannot_run(void) {
    char *argv[] = { "/nonexistent/SurrealEngine", NULL };
    dxl_game g;
    dxl_err e;
    CHECK_INT(dxl_game_start_argv(&g, argv, NULL, NULL, NULL, &e), -1);
    CHECK(strstr(dxl_err_msg(&e), "cannot run /nonexistent/SurrealEngine") != NULL);
}

/* --version's answer captured; a program that never ends is killed when
 * its time is up. */
static void test_capture(void) {
    char out[512];
    int status = -1;
    char *version[] = { DXL_STUB_ENGINE, "--version", NULL };
    CHECK_INT(dxl_run_capture(version, 3000, out, sizeof out, &status, NULL), 0);
    CHECK_INT(status, 0);
    dxl_engine_version v;
    CHECK_INT(dxl_engine_version_parse(out, &v), 0);
    CHECK_INT(dxl_engine_has(&v, "userdir"), 1);

    setenv("DXL_STUB", "stubborn", 1);
    char *hang[] = { DXL_STUB_ENGINE, NULL };
    dxl_err e;
    CHECK_INT(dxl_run_capture(hang, 300, out, sizeof out, &status, &e), -1);
    CHECK_INT(status, 128 + SIGKILL);
    unsetenv("DXL_STUB");

    char *none[] = { "/nonexistent/engine", "--version", NULL };
    CHECK_INT(dxl_run_capture(none, 1000, out, sizeof out, &status, &e), -1);
}

TEST_MAIN_BEGIN
    signal(SIGPIPE, SIG_IGN);
    RUN(test_words_kept);
    RUN(test_env);
    RUN(test_line);
    RUN(test_crash);
    RUN(test_cannot_run);
    RUN(test_capture);
TEST_MAIN_END

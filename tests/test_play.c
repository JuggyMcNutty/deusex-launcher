#include "test.h"
#include "scratch.h"
#include "ends.h"
#include "app/play.h"
#include "app/run.h"
#include "platform/instance.h"
#include "platform/os.h"

#include <pthread.h>
#include <signal.h>
#include <time.h>

/* The stub engine (tests/stub_engine.c), built beside the tests. */
#ifndef DXL_STUB_ENGINE
#define DXL_STUB_ENGINE "stub_engine"
#endif

typedef struct {
    char *dir;
    char *log;
} scratch_run;

static void play_stub(scratch_run *s, const char *tag, char *const env[], dxl_instance *inst,
                      volatile int *stop, dxl_play_result *out) {
    s->dir = dxl_xstrdup(scratch_dir(tag));
    s->log = scratch_path(s->dir, "run.log");
    char *argv[] = { DXL_STUB_ENGINE, "--no-launcher", "/games/Deus Ex",
                     "--cmdline=INI=\"My Mod.ini\" -nosound", NULL };
    dxl_play_request req = {
        .argv = argv, .env = env, .workdir = s->dir, .log_path = s->log,
        .instance = inst, .stop = stop,
    };
    dxl_play(&req, out);
}

static void done(scratch_run *s, dxl_play_result *r) {
    dxl_play_result_free(r);
    scratch_remove(s->dir);
    free(s->dir);
    free(s->log);
}

/* hello, ready, exit 0: clean. */
static void test_clean(void) {
    char *env[] = { "DXL_STUB=ready", "DXL_STUB_MS=100", NULL };
    scratch_run s;
    dxl_play_result r;
    play_stub(&s, "play-clean", env, NULL, NULL, &r);
    CHECK_INT(r.started, 1);
    CHECK_INT(r.hello, 1);
    CHECK_INT(r.ready, 1);
    CHECK_INT(r.status, 0);
    CHECK_INT(r.clean, 1);
    CHECK(r.relaunch == NULL);
    done(&s, &r);
}

/* exit 1 after the engine's line that it found no game, before any ready. */
static void test_no_game(void) {
    char *env[] = { "DXL_STUB=nogame", NULL };
    scratch_run s;
    dxl_play_result r;
    play_stub(&s, "play-nogame", env, NULL, NULL, &r);
    CHECK_INT(r.status, 1);
    CHECK_INT(r.ready, 0);
    CHECK_INT(r.clean, 0);
    done(&s, &r);
}

/* exit 1 after the engine's error line. */
static void test_failed(void) {
    char *env[] = { "DXL_STUB=fail", "DXL_STUB_LINE=Could not open /g/Maps/.dx", NULL };
    scratch_run s;
    dxl_play_result r;
    play_stub(&s, "play-fail", env, NULL, NULL, &r);
    CHECK_INT(r.status, 1);
    CHECK_INT(r.clean, 0);
    char *text = slurp(s.log, NULL);
    CHECK(text && strstr(text, "DXEngine error: Could not open /g/Maps/.dx") != NULL);
    free(text);
    done(&s, &r);
}

static void test_crash(void) {
    char *env[] = { "DXL_STUB=crash", NULL };
    scratch_run s;
    dxl_play_result r;
    play_stub(&s, "play-crash", env, NULL, NULL, &r);
    CHECK_INT(r.ready, 1);
    CHECK_INT(r.signal, END_CRASH);
    CHECK_INT(r.status, END_CRASH_STATUS);
    CHECK_INT(r.clean, 0);
    done(&s, &r);
}

/* A RELAUNCH, kept for after a clean end. */
static void test_relaunch(void) {
    char *env[] = { "DXL_STUB=relaunch", "DXL_STUB_LINE=DX.dx?Name=JC -server", NULL };
    scratch_run s;
    dxl_play_result r;
    play_stub(&s, "play-relaunch", env, NULL, NULL, &r);
    CHECK_INT(r.clean, 1);
    CHECK_STR(r.relaunch, "DX.dx?Name=JC -server");
    done(&s, &r);
}

/* The quotes of a word arrive as they were given. */
static void test_quotes_kept(void) {
    char *env[] = { "DXL_STUB=args", NULL };
    scratch_run s;
    dxl_play_result r;
    play_stub(&s, "play-args", env, NULL, NULL, &r);
    char *text = slurp(s.log, NULL);
    CHECK(text && strstr(text, "[--cmdline=INI=\"My Mod.ini\" -nosound]\n") != NULL);
    CHECK(text && strstr(text, "[/games/Deus Ex]\n") != NULL);
    free(text);
    done(&s, &r);
}

/* A game that takes no notice of the stop (SIGTERM; Windows' Quit and
 * WM_CLOSE) is killed when its time is up. Stopped once it is ready, deaf
 * by then: a stop sent while it still starts would end it before it could
 * take no notice. */
static volatile int stop_now;
static void stop_on_ready(void *ctx) {
    (void)ctx;
    stop_now = 1;
}
static void test_stop_then_kill(void) {
    char *env[] = { "DXL_STUB=stubborn", NULL };
    stop_now = 0;
    scratch_run s = { dxl_xstrdup(scratch_dir("play-stubborn")), NULL };
    s.log = scratch_path(s.dir, "run.log");
    char *argv[] = { DXL_STUB_ENGINE, NULL };
    dxl_play_request req = {
        .argv = argv, .env = env, .workdir = s.dir, .log_path = s.log,
        .stop = &stop_now, .on_ready = stop_on_ready,
    };
    dxl_play_result r;
    time_t t0 = time(NULL);
    dxl_play(&req, &r);
    time_t t1 = time(NULL);
    CHECK_INT(r.stopped, 1);
    CHECK_INT(r.killed, 1);
    CHECK_INT(r.signal, END_KILL);
    CHECK_INT(r.clean, 0);
    CHECK((t1 - t0) >= DXL_PLAY_KILL_AFTER_MS / 1000 - 1 && (t1 - t0) <= DXL_PLAY_KILL_AFTER_MS / 1000 + 3);
    stop_now = 0;
    done(&s, &r);
}

/* A stop the engine takes: it quits at its next turn, exit 0, a clean end
 * (DXEngine's SIGTERM), well before the kill. */
static void test_stop_clean(void) {
    char *env[] = { "DXL_STUB=ready", "DXL_STUB_MS=20000", NULL };
    stop_now = 0;
    scratch_run s = { dxl_xstrdup(scratch_dir("play-stop")), NULL };
    s.log = scratch_path(s.dir, "run.log");
    char *argv[] = { DXL_STUB_ENGINE, NULL };
    dxl_play_request req = {
        .argv = argv, .env = env, .workdir = s.dir, .log_path = s.log,
        .stop = &stop_now, .on_ready = stop_on_ready,
    };
    dxl_play_result r;
    time_t t0 = time(NULL);
    dxl_play(&req, &r);
    CHECK_INT(r.stopped, 1);
    CHECK_INT(r.killed, 0);
    CHECK_INT(r.status, 0);
    CHECK_INT(r.clean, 1);
    CHECK(time(NULL) - t0 < DXL_PLAY_KILL_AFTER_MS / 1000);
    stop_now = 0;
    done(&s, &r);
}

#ifndef _WIN32
/* The engine's end seen though a child of its own still holds the line (a
 * helper it started): the wait wakes once a second for it, not on the
 * line's end alone. */
static void test_line_outlived(void) {
    char *env[] = { "DXL_STUB=fork", NULL };
    scratch_run s;
    dxl_play_result r;
    time_t t0 = time(NULL);
    play_stub(&s, "play-fork", env, NULL, NULL, &r);
    CHECK_INT(r.started, 1);
    CHECK_INT(r.clean, 1);
    CHECK(time(NULL) - t0 < 3);
    done(&s, &r);
}
#endif

/* A line a second launch forwards: TakeFocus and Open for its URL once the
 * engine is up; one sent before then is dropped, as the original's log
 * window takes one only once it has the engine. The second launches are a
 * thread's, as another process's would be, the launcher's loop free to read
 * each: on Windows a sender waits for the one before it to be read. */
static char forward_id[64];
static int forward_rc[2];
static pthread_t forwarder;
static void *forward_main(void *p) {
    (void)p;
    forward_rc[0] = dxl_instance_forward(forward_id, "127.0.0.1:7790 -log", 1000, NULL);
    /* A link's server, as a second start hands it on: the game's own way. */
    forward_rc[1] = dxl_instance_forward(forward_id, "deusex://10.0.0.5:7780/ --big-screen", 1000, NULL);
    return NULL;
}
static void forward_on_ready(void *ctx) {
    (void)ctx;
    pthread_create(&forwarder, NULL, forward_main, NULL);
}

static void test_forwarded(void) {
    snprintf(forward_id, sizeof forward_id, "dxl-test-play-%ld", dxl_os_pid());
    dxl_instance *inst = dxl_instance_acquire(forward_id);
    CHECK(inst != NULL);
    CHECK_INT(dxl_instance_forward(forward_id, "too-early", 1000, NULL), 0);
    char *dir = dxl_xstrdup(scratch_dir("play-forward"));
    char *log = scratch_path(dir, "run.log");
    char *argv[] = { DXL_STUB_ENGINE, NULL };
    char *env[] = { "DXL_STUB=ready", "DXL_STUB_MS=1500", NULL };
    dxl_play_request req = {
        .argv = argv, .env = env, .workdir = dir, .log_path = log, .instance = inst,
        .on_ready = forward_on_ready,
    };
    dxl_play_result r;
    forward_rc[0] = forward_rc[1] = -1;
    dxl_play(&req, &r);
    pthread_join(forwarder, NULL);
    CHECK_INT(forward_rc[0], 0);
    CHECK_INT(forward_rc[1], 0);
    CHECK_INT(r.clean, 1);
    char *text = slurp(log, NULL);
    CHECK(text && strstr(text, "line: TakeFocus\nline: Open 127.0.0.1:7790\n") != NULL);
    CHECK(text && strstr(text, "line: TakeFocus\nline: Open 10.0.0.5:7780\n") != NULL);
    CHECK(text && strstr(text, "too-early") == NULL);
    free(text);
    dxl_play_result_free(&r);
    dxl_instance_release(inst);
    scratch_remove(dir);
    free(log);
    free(dir);
}

/* A deusex:// link is the launcher's own, its server to join, not a word
 * of the original's command line; --big-screen too. */
static void test_parse_link(void) {
    char *argv[] = { "deusex-launcher", "deusex://203.0.113.9:7790/", "-nosound", "--big-screen", NULL };
    dxl_run_options opt;
    char *words = NULL;
    CHECK_INT(dxl_run_parse(4, argv, &opt, &words, NULL), 0);
    CHECK_STR(opt.join, "deusex://203.0.113.9:7790/");
    CHECK_INT(opt.big_screen, 1);
    CHECK(words && strstr(words, "deusex://") == NULL);
    CHECK(words && strstr(words, "-nosound") != NULL);
    free(words);
}

static void test_cannot_start(void) {
    char *argv[] = { "/nonexistent/DXEngine", NULL };
    dxl_play_request req = { .argv = argv };
    dxl_play_result r;
    dxl_play(&req, &r);
    CHECK_INT(r.started, 0);
    CHECK(strstr(r.error, "cannot run") != NULL);
    dxl_play_result_free(&r);
}

TEST_MAIN_BEGIN
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    RUN(test_parse_link);
    RUN(test_clean);
    RUN(test_no_game);
    RUN(test_failed);
    RUN(test_crash);
    RUN(test_relaunch);
    RUN(test_quotes_kept);
    RUN(test_stop_then_kill);
    RUN(test_stop_clean);
#ifndef _WIN32
    RUN(test_line_outlived);
#endif
    RUN(test_forwarded);
    RUN(test_cannot_start);
TEST_MAIN_END

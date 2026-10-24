#include "test.h"
#include "ends.h"
#include "scratch.h"
#include "core/lastrun.h"
#include "platform/os.h"

/* A run's end, written and read back. */
static void test_record(void) {
    const char *dir = scratch_dir("lastrun");
    dxl_lastrun r = { .ended = 1760000000, .status = END_CRASH_STATUS, .signal = END_CRASH, .ready = 1,
                      .clean = 0 };
    snprintf(r.error, sizeof r.error, "two\nlines");
    snprintf(r.fallback, sizeof r.fallback, "The Vulkan device did not start\n(x)");
    snprintf(r.start, sizeof r.start, "?loadgame=10");
    CHECK_INT(dxl_lastrun_save(&r, dir, NULL), 0);
    dxl_lastrun back;
    CHECK_INT(dxl_lastrun_load(&back, dir), 0);
    CHECK_INT(back.known, 1);
    CHECK_INT(back.ended, 1760000000);
    CHECK_INT(back.status, END_CRASH_STATUS);
    CHECK_INT(back.signal, END_CRASH);
    CHECK_INT(back.ready, 1);
    CHECK_INT(back.clean, 0);
    CHECK_STR(back.error, "two lines");
    CHECK_STR(back.fallback, "The Vulkan device did not start (x)");
    CHECK_STR(back.start, "?loadgame=10");
    char line[256];
    dxl_lastrun_describe(&back, line, sizeof line);
    CHECK_STR(line, "killed by " END_CRASH_NAME);
    scratch_remove(dir);
    CHECK_INT(dxl_lastrun_load(&back, dir), -1);
    CHECK_INT(back.known, 0);
}

static void test_describe(void) {
    char line[256];
    dxl_lastrun r = { .known = 1, .status = 1 };
    snprintf(r.error, sizeof r.error, "Could not open /g/Maps/.dx");
    dxl_lastrun_describe(&r, line, sizeof line);
    CHECK_STR(line, "exit 1: Could not open /g/Maps/.dx");
    dxl_lastrun z = { .known = 1, .status = 0, .ready = 0 };
    dxl_lastrun_describe(&z, line, sizeof line);
    CHECK_STR(line, "exit 0 before its first map was in: no game found, or none started");
    dxl_lastrun k = { .known = 1, .status = END_KILL_STATUS, .signal = END_KILL, .ready = 0 };
    dxl_lastrun_describe(&k, line, sizeof line);
    CHECK_STR(line, "killed by " END_KILL_NAME ", before its first map was in");
    char buf[64];
    CHECK_STR(dxl_end_name(END_UNNAMED, buf, sizeof buf), END_UNNAMED_NAME);
}

/* The log's last lines, and the engine's error line, the last of them. */
static void test_log(void) {
    const char *dir = scratch_dir("lastrun-log");
    scratch_write(dir, "logs/last-run.log",
                  "Launching Deus Ex 1112fm\n[] Game: Deus Ex\n"
                  "[] The Vulkan device did not start (No Vulkan devices found): the software device draws in its "
                  "place\r\n[] Software device: 16 threads, SSE2\nDXEngine error: first\n"
                  "  log: one\nDXEngine error: Could not open x\r\n  log: two\n");
    char *logs = scratch_path(dir, "logs");
    char *log = scratch_path(dir, "logs/last-run.log");
    char err[128];
    CHECK_INT(dxl_lastrun_error_line(log, err, sizeof err), 1);
    CHECK_STR(err, "Could not open x");
    char fell[256];
    CHECK_INT(dxl_lastrun_fallback_line(log, fell, sizeof fell), 1);
    CHECK_STR(fell, "The Vulkan device did not start (No Vulkan devices found): the software device draws in its place");
    char *tail = dxl_lastrun_tail(log, 2);
    CHECK_STR(tail, "DXEngine error: Could not open x\r\n  log: two");
    free(tail);
    tail = dxl_lastrun_tail(log, 100);
    CHECK(tail && strncmp(tail, "Launching", 9) == 0);
    free(tail);

    /* Rotation: the last log becomes the previous one. */
    char *fresh = dxl_lastrun_rotate(logs);
    CHECK_STR(fresh, log);
    CHECK_INT(scratch_exists(dir, "logs/last-run.log"), 0);
    CHECK_INT(scratch_exists(dir, "logs/previous-run.log"), 1);
    CHECK_INT(dxl_lastrun_error_line(log, err, sizeof err), 0);
    CHECK_INT(dxl_lastrun_fallback_line(log, fell, sizeof fell), 0);
    CHECK(dxl_lastrun_tail(log, 5) == NULL);
    free(fresh);
    free(log);
    free(logs);
    scratch_remove(dir);
}

TEST_MAIN_BEGIN
    RUN(test_record);
    RUN(test_describe);
    RUN(test_log);
TEST_MAIN_END

/* The hand-over's pipe on Windows (platform/win32/instance.c): a second
 * start's line read whenever it comes -- while the primary waits, after a
 * line before, before the primary's first wait (bugs.md LPORT.7) -- one that
 * says nothing passed over, and no wait woken with nothing to read. */
#include "test.h"
#include "platform/instance.h"
#include "platform/os.h"
#include "platform/wait.h"
#include "platform/win32/internal.h"

#include <wchar.h>

static char g_id[256];
static wchar_t g_pipe[64];
/* The start that comes and goes before the primary's first wait, when
 * there is one: its line, or NULL for one that says nothing. */
static int g_early;
static const char *g_early_line;

/* A start that connects and goes, saying nothing. */
static void say_nothing(const wchar_t *pipe) {
    HANDLE h = CreateFileW(pipe, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

static void forward(const char *line) {
    dxl_err e;
    CHECK_INT(dxl_instance_forward(g_id, line, 1000, &e), 0);
}

/* Between the pipe's making and the first wait: the moment a start can
 * connect, write and close before the primary waits for it, its
 * ConnectNamedPipe then failing ERROR_NO_DATA. */
static void made(const wchar_t *pipe) {
    swprintf(g_pipe, sizeof g_pipe / sizeof *g_pipe, L"%ls", pipe);
    if (!g_early) return;
    if (g_early_line) forward(g_early_line);
    else say_nothing(pipe);
}

/* Nothing to read: no wait woken, no line. */
static void check_quiet(dxl_instance *inst) {
    char buf[DXL_HANDOFF_MAX];
    CHECK_INT(dxl_wait(NULL, 0, inst, 0), 0);
    CHECK_INT(dxl_instance_poll(inst, buf, sizeof buf), 0);
}

/* A line, as the window takes it: a wait woken, then the line read. */
static void check_line(dxl_instance *inst, const char *want) {
    char buf[DXL_HANDOFF_MAX] = "";
    CHECK_INT(dxl_wait(NULL, 0, inst, 1000), DXL_WAIT_LINE);
    CHECK_INT(dxl_instance_poll(inst, buf, sizeof buf), 1);
    CHECK_STR(buf, want);
}

/* While the primary waits, and again after each line: two in a row. */
static void test_lines(void) {
    dxl_instance *inst = dxl_instance_acquire(g_id);
    CHECK(dxl_instance_listens(inst));
    check_quiet(inst);
    forward("127.0.0.1:7790 -log");
    check_line(inst, "127.0.0.1:7790 -log");
    check_quiet(inst);
    forward("");   /* a start with no words: the window raised */
    check_line(inst, "");
    forward("deusex://10.0.0.5:7780/ --big-screen");
    check_line(inst, "deusex://10.0.0.5:7780/ --big-screen");
    check_quiet(inst);
    dxl_instance_release(inst);
}

/* A start that connects and goes saying nothing is passed over, and the
 * next one's line comes. */
static void test_nothing_said(void) {
    dxl_instance *inst = dxl_instance_acquire(g_id);
    CHECK(dxl_instance_listens(inst));
    say_nothing(g_pipe);
    char buf[DXL_HANDOFF_MAX];
    CHECK_INT(dxl_instance_poll(inst, buf, sizeof buf), 0);
    check_quiet(inst);
    forward("after");
    check_line(inst, "after");
    check_quiet(inst);
    dxl_instance_release(inst);
}

/* A start that connected, wrote and closed before the primary's first
 * wait: its line read all the same, the pipe waiting again, no wait woken
 * for nothing, the next start's line come; and one that said nothing then
 * passed over. */
static void test_before_the_first_wait(void) {
    g_early = 1;
    g_early_line = "-nosound";
    dxl_instance *inst = dxl_instance_acquire(g_id);
    g_early = 0;
    CHECK(dxl_instance_listens(inst));
    check_line(inst, "-nosound");
    check_quiet(inst);
    forward("later");
    check_line(inst, "later");
    check_quiet(inst);
    dxl_instance_release(inst);

    g_early = 1;
    g_early_line = NULL;
    inst = dxl_instance_acquire(g_id);
    g_early = 0;
    CHECK(dxl_instance_listens(inst));
    check_quiet(inst);
    forward("then");
    check_line(inst, "then");
    dxl_instance_release(inst);
}

TEST_MAIN_BEGIN
    snprintf(g_id, sizeof g_id, "%s/dxl-instance-%ld", dxl_test_tmp(), dxl_os_pid());
    dxl_instance_test_made = made;
    RUN(test_lines);
    RUN(test_nothing_said);
    RUN(test_before_the_first_wait);
TEST_MAIN_END

#include "test.h"
#include "core/argv.h"

/* The argv a program is started with: the launcher's command line, kept as
 * one string (dx-reverse-info/cli-flags.md: appStrfind matches anywhere),
 * handed on as one argument, as it is. */

static void test_argv_line(void) {
    char **a = dxl_argv_line("/app/run-game.sh", "-log -firstrun");
    CHECK_STR(a[0], "/app/run-game.sh");
    CHECK_STR(a[1], "-log -firstrun");
    CHECK(a[2] == NULL);
    dxl_argv_free(a);
}

static void test_argv_line_empty(void) {
    char **a = dxl_argv_line("/app/x", "");
    CHECK_STR(a[0], "/app/x");
    CHECK(a[1] == NULL);
    dxl_argv_free(a);

    a = dxl_argv_line("/app/x", NULL);
    CHECK_STR(a[0], "/app/x");
    CHECK(a[1] == NULL);
    dxl_argv_free(a);
}

/* A quoted value keeps its quotes and its spaces: the engine reads
 * INI="My Mod.ini" whole, as the original does. */
static void test_argv_line_keeps_quotes(void) {
    char **a = dxl_argv_line("/app/x", "INI=\"My Mod.ini\" -map=\"Deus Ex.dx\" -log");
    CHECK_STR(a[1], "INI=\"My Mod.ini\" -map=\"Deus Ex.dx\" -log");
    CHECK(a[2] == NULL);
    dxl_argv_free(a);
}

/* The line's own spacing too: nothing is split, so nothing is joined. */
static void test_argv_line_keeps_whitespace(void) {
    char **a = dxl_argv_line("x", "  -a \t -b  ");
    CHECK_STR(a[1], "  -a \t -b  ");
    CHECK(a[2] == NULL);
    dxl_argv_free(a);
}

TEST_MAIN_BEGIN
    RUN(test_argv_line);
    RUN(test_argv_line_empty);
    RUN(test_argv_line_keeps_quotes);
    RUN(test_argv_line_keeps_whitespace);
TEST_MAIN_END

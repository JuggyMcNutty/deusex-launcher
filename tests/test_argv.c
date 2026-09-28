#include "test.h"
#include "core/argv.h"

/* argv construction for starting the game (and safe mode's relaunch). The
 * launcher's command line is kept as one string (dx-reverse-info/cli-flags.md:
 * appStrfind matches anywhere), so it is split exactly once, here. */

static void test_argv_build(void) {
    char **a = dxl_argv_build("/app/run-game.sh", "-log -firstrun");
    CHECK_STR(a[0], "/app/run-game.sh");
    CHECK_STR(a[1], "-log");
    CHECK_STR(a[2], "-firstrun");
    CHECK(a[3] == NULL);
    dxl_argv_free(a);
}

static void test_argv_build_empty_flags(void) {
    char **a = dxl_argv_build("/app/x", "");
    CHECK_STR(a[0], "/app/x");
    CHECK(a[1] == NULL);
    dxl_argv_free(a);

    a = dxl_argv_build("/app/x", NULL);
    CHECK_STR(a[0], "/app/x");
    CHECK(a[1] == NULL);
    dxl_argv_free(a);
}

/* A game directory with a space in it is normal on an SD card. */
static void test_argv_build_quoted(void) {
    char **a = dxl_argv_build("/app/x", "-map=\"Deus Ex.dx\" -log");
    CHECK_STR(a[1], "-map=Deus Ex.dx");
    CHECK_STR(a[2], "-log");
    CHECK(a[3] == NULL);
    dxl_argv_free(a);
}

static void test_argv_build_collapses_whitespace(void) {
    char **a = dxl_argv_build("x", "  -a \t -b  ");
    CHECK_STR(a[1], "-a");
    CHECK_STR(a[2], "-b");
    CHECK(a[3] == NULL);
    dxl_argv_free(a);
}

TEST_MAIN_BEGIN
    RUN(test_argv_build);
    RUN(test_argv_build_empty_flags);
    RUN(test_argv_build_quoted);
    RUN(test_argv_build_collapses_whitespace);
TEST_MAIN_END

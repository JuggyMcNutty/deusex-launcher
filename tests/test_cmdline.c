#include "test.h"
#include "core/cmdline.h"

/* The three parsers are not interchangeable. dx-reverse-info/cli-flags.md lists which
 * flag goes through which, and the differences are observable -- these tests
 * are the record of that. */

static void test_param_requires_a_dash(void) {
    CHECK_INT(dxl_cmd_param("-safe", "safe"), 1);
    CHECK_INT(dxl_cmd_param("/safe", "safe"), 1);          /* UE1 accepts both */
    CHECK_INT(dxl_cmd_param("DX.dx -safe", "safe"), 1);
    CHECK_INT(dxl_cmd_param("-safe -log", "safe"), 1);
    CHECK_INT(dxl_cmd_param("-log -safe", "safe"), 1);

    /* No leading dash: not a param. This is the difference from appStrfind. */
    CHECK_INT(dxl_cmd_param("safe", "safe"), 0);
    CHECK_INT(dxl_cmd_param("DX.dx safe", "safe"), 0);
}

static void test_param_must_end_at_whitespace(void) {
    /* "-safemode" is not "-safe". */
    CHECK_INT(dxl_cmd_param("-safemode", "safe"), 0);
    CHECK_INT(dxl_cmd_param("-safe=1", "safe"), 0);
    /* but the real one alongside it still matches */
    CHECK_INT(dxl_cmd_param("-safemode -safe", "safe"), 1);
}

static void test_param_is_case_insensitive(void) {
    CHECK_INT(dxl_cmd_param("-SAFE", "safe"), 1);
    CHECK_INT(dxl_cmd_param("-Safe", "safe"), 1);
    CHECK_INT(dxl_cmd_param("-safe", "SAFE"), 1);
    CHECK_INT(dxl_cmd_param("-FIRSTRUN", "firstrun"), 1);
}

static void test_param_empty_cmdline(void) {
    CHECK_INT(dxl_cmd_param("", "safe"), 0);
    CHECK_INT(dxl_cmd_param(NULL, "safe"), 0);
}

static void test_value_extraction(void) {
    char v[128];
    CHECK_INT(dxl_cmd_value("-testrendev=D3DDrv.D3DRenderDevice", "testrendev", v, sizeof v), 1);
    CHECK_STR(v, "D3DDrv.D3DRenderDevice");

    CHECK_INT(dxl_cmd_value("-exec=startup.txt -log", "exec", v, sizeof v), 1);
    CHECK_STR(v, "startup.txt");

    /* Case-insensitive, like Parse(). */
    CHECK_INT(dxl_cmd_value("-EXEC=Foo.txt", "exec", v, sizeof v), 1);
    CHECK_STR(v, "Foo.txt");

    /* Absent key leaves the buffer empty and reports failure. */
    CHECK_INT(dxl_cmd_value("-log", "exec", v, sizeof v), 0);
    CHECK_STR(v, "");
}

static void test_value_quoted(void) {
    char v[128];
    CHECK_INT(dxl_cmd_value("-exec=\"my file.txt\" -log", "exec", v, sizeof v), 1);
    CHECK_STR(v, "my file.txt");
}

static void test_value_truncates_safely(void) {
    char v[8];
    CHECK_INT(dxl_cmd_value("-exec=0123456789abcdef", "exec", v, sizeof v), 1);
    CHECK_INT(strlen(v), 7);            /* NUL-terminated, never overrun */
    CHECK_STR(v, "0123456");
}

/* The documented surprise: these five match anywhere, with no leading dash,
 * including inside a map name or a URL. dx-reverse-info/cli-flags.md "Substring-matched
 * tokens". A naive argv parser would behave differently, which is exactly why
 * the core keeps the command line as one string. */
static void test_strfind_matches_anywhere(void) {
    CHECK_INT(dxl_cmd_find("-safe", "readini"), 0);
    CHECK_INT(dxl_cmd_find("readini", "readini"), 1);
    CHECK_INT(dxl_cmd_find("-readini", "readini"), 1);
    /* inside a map name */
    CHECK_INT(dxl_cmd_find("MyMap_readini.dx", "readini"), 1);
    /* inside a URL */
    CHECK_INT(dxl_cmd_find("deusex://host/Map?NewWindow", "NewWindow"), 1);
    CHECK_INT(dxl_cmd_find("unreal://a.server.example/", "Server"), 1);
    CHECK_INT(dxl_cmd_find("-CHANGEVIDEO", "changevideo"), 1);
    CHECK_INT(dxl_cmd_find("-testrendev=X", "TestRenDev"), 1);
}

static void test_join(void) {
    char *argv1[] = { "deusex-launcher", "-safe", "-log" };
    char *j = dxl_cmdline_join(3, argv1);
    CHECK_STR(j, "-safe -log");
    free(j);

    char *argv2[] = { "deusex-launcher" };
    j = dxl_cmdline_join(1, argv2);
    CHECK_STR(j, "");
    free(j);
}

/* argv[0] is excluded from the joined command line, the one the ports' policy
 * reads (core/policy.c): a launcher living in a directory called
 * ".../Server/" still hands over to a running one. main's launch sequence
 * checks the bypass words on the raw line instead, the program's path
 * included, as the original does (dx-reverse-info/launch-flow.md). */
static void test_join_excludes_argv0(void) {
    char *argv[] = { "/mnt/SDCARD/Roms/Server/deusex-launcher", "-log" };
    char *j = dxl_cmdline_join(2, argv);
    CHECK_INT(dxl_cmd_find(j, "Server"), 0);
    free(j);
}

TEST_MAIN_BEGIN
    RUN(test_param_requires_a_dash);
    RUN(test_param_must_end_at_whitespace);
    RUN(test_param_is_case_insensitive);
    RUN(test_param_empty_cmdline);
    RUN(test_value_extraction);
    RUN(test_value_quoted);
    RUN(test_value_truncates_safely);
    RUN(test_strfind_matches_anywhere);
    RUN(test_join);
    RUN(test_join_excludes_argv0);
TEST_MAIN_END

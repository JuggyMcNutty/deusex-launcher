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
    CHECK_INT(dxl_cmd_param("DXMP_Silo?game=DeusEx.DeathMatchGame?-server?log=server.log", "server"), 1);
    CHECK_INT(dxl_cmd_param("DXMP_Silo?game=DeusEx.DeathMatchGame?-serverx", "server"), 0);

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
    CHECK_INT(dxl_cmd_value("-testrendev=D3DDrv.D3DRenderDevice", "testrendev", DXL_PARSE_FSTRING, v, sizeof v), 1);
    CHECK_STR(v, "D3DDrv.D3DRenderDevice");

    CHECK_INT(dxl_cmd_value("-exec=startup.txt -log", "exec", DXL_PARSE_FSTRING, v, sizeof v), 1);
    CHECK_STR(v, "startup.txt");

    /* Case-insensitive, like Parse(). */
    CHECK_INT(dxl_cmd_value("-EXEC=Foo.txt", "exec", DXL_PARSE_FSTRING, v, sizeof v), 1);
    CHECK_STR(v, "Foo.txt");

    /* Absent key leaves the buffer empty and reports failure. */
    CHECK_INT(dxl_cmd_value("-log", "exec", DXL_PARSE_FSTRING, v, sizeof v), 0);
    CHECK_STR(v, "");
}

static void test_value_quoted(void) {
    char v[128];
    CHECK_INT(dxl_cmd_value("-exec=\"my file.txt\" -log", "exec", DXL_PARSE_FSTRING, v, sizeof v), 1);
    CHECK_STR(v, "my file.txt");
}

static void test_value_truncates_safely(void) {
    char v[8];
    CHECK_INT(dxl_cmd_value("-exec=0123456789abcdef", "exec", DXL_PARSE_FSTRING, v, sizeof v), 1);
    CHECK_INT(strlen(v), 7);            /* NUL-terminated, never overrun */
    CHECK_STR(v, "0123456");
}

/* Parse's cut (Core.dll 0x10146bc0): a value not in quotes ends at a space,
 * tab, CR, LF or comma, and at no other blank; one in quotes at its closing
 * quote, or at the line's end without one, its spaces and commas kept; a
 * quote within a value not in quotes is the value's. Found, it is there,
 * empty or not. */
static void test_value_cut_as_parse(void) {
    char v[128];
    CHECK_INT(dxl_cmd_value("EXEC=startup.txt,x", "EXEC", DXL_PARSE_FSTRING, v, sizeof v), 1);
    CHECK_STR(v, "startup.txt");
    CHECK_INT(dxl_cmd_value("INI=a.ini\tx", "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, "a.ini");
    CHECK_INT(dxl_cmd_value("INI=a.ini\r\n", "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, "a.ini");
    CHECK_INT(dxl_cmd_value("INI=a\vb\f.ini", "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, "a\vb\f.ini");
    CHECK_INT(dxl_cmd_value("INI=\"My Games/a,b.ini\" -log", "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, "My Games/a,b.ini");
    CHECK_INT(dxl_cmd_value("INI=\"My Games/a.ini -log", "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, "My Games/a.ini -log");
    CHECK_INT(dxl_cmd_value("INI=a\"b c\" -log", "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, "a\"b");
    CHECK_INT(dxl_cmd_value("INI=,a.ini", "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, "");
}

/* Of a value, as much as the original's buffer holds, its len - 1
 * characters, quoted or not: INI='s 255. A character past U+FFFF counts two,
 * as UTF-16 has it, and one that does not fit goes whole. */
static void test_value_cut_at_the_buffer(void) {
    char line[1024], v[1024], a[256];
    memset(a, 'a', 255);
    a[255] = '\0';
    snprintf(line, sizeof line, "INI=%sb", a);
    CHECK_INT(dxl_cmd_value(line, "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, a);
    snprintf(line, sizeof line, "INI=\"%sb\"", a);
    CHECK_INT(dxl_cmd_value(line, "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, a);
    snprintf(line, sizeof line, "INI=%.254s\xc3\xa9x", a);        /* an e acute: one */
    CHECK_INT(dxl_cmd_value(line, "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_INT(strlen(v), 256);
    CHECK_STR(v + 254, "\xc3\xa9");
    snprintf(line, sizeof line, "INI=%.254s\xf0\x9f\x98\x80", a);  /* U+1F600: two */
    CHECK_INT(dxl_cmd_value(line, "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_INT(strlen(v), 254);
    snprintf(line, sizeof line, "INI=%.253s\xf0\x9f\x98\x80x", a);
    CHECK_INT(dxl_cmd_value(line, "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_INT(strlen(v), 257);
    CHECK_STR(v + 253, "\xf0\x9f\x98\x80");
}

/* The documented surprise: these five match anywhere not straight after a
 * letter or digit, with no leading dash, inside a map name or a URL too
 * (dx-reverse-info/cli-flags.md). A naive argv parser would behave
 * differently, which is exactly why the core keeps the command line as one
 * string. */
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

/* appStrfind's boundary: never straight after a letter or digit, so one
 * name within another is not it; the first match that is wins. */
static void test_strfind_not_after_a_letter_or_digit(void) {
    char v[64];
    CHECK_INT(dxl_cmd_find("DX.dx?Game=ReServer", "Server"), 0);
    CHECK_INT(dxl_cmd_find("Map2Server", "Server"), 0);
    CHECK_INT(dxl_cmd_find("ReServer -Server", "Server"), 1);
    CHECK_INT(dxl_cmd_value("USERINI=u.ini", "INI", DXL_PARSE_INI, v, sizeof v), 0);
    CHECK_INT(dxl_cmd_value("USERINI=u.ini INI=s.ini", "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, "s.ini");
    CHECK_INT(dxl_cmd_value("USERINI=u.ini INI=s.ini", "USERINI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, "u.ini");
    CHECK_INT(dxl_cmd_value("ABSLOG=a.log", "LOG", DXL_PARSE_FSTRING, v, sizeof v), 0);
    CHECK_INT(dxl_cmd_value("ABSLOG=a.log LOG=b.log", "LOG", DXL_PARSE_FSTRING, v, sizeof v), 1);
    CHECK_STR(v, "b.log");
    CHECK_INT(dxl_cmd_value("-log=x.log", "LOG", DXL_PARSE_FSTRING, v, sizeof v), 1);
    CHECK_STR(v, "x.log");
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

    /* A word holding a space, quoted after its NAME=, read back whole. */
    char *argv3[] = { "deusex-launcher", "INI=/home/u/My Games/x.ini", "My Map", "-log" };
    j = dxl_cmdline_join(4, argv3);
    CHECK_STR(j, "INI=\"/home/u/My Games/x.ini\" \"My Map\" -log");
    char v[128];
    CHECK_INT(dxl_cmd_value(j, "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, "/home/u/My Games/x.ini");
    CHECK_INT(dxl_cmd_param(j, "log"), 1);
    free(j);

    /* A comma is no cause to quote: Parse cuts the value there, as the
     * original's does that word; quoted for its space, a value keeps its
     * comma. */
    char *argv4[] = { "deusex-launcher", "EXEC=a.txt,b", "INI=/home/u/My Games/a,b.ini" };
    j = dxl_cmdline_join(3, argv4);
    CHECK_STR(j, "EXEC=a.txt,b INI=\"/home/u/My Games/a,b.ini\"");
    CHECK_INT(dxl_cmd_value(j, "EXEC", DXL_PARSE_FSTRING, v, sizeof v), 1);
    CHECK_STR(v, "a.txt");
    CHECK_INT(dxl_cmd_value(j, "INI", DXL_PARSE_INI, v, sizeof v), 1);
    CHECK_STR(v, "/home/u/My Games/a,b.ini");
    free(j);
}

/* argv[0] is excluded from the joined command line, the one core/policy.c
 * reads: a launcher living in a directory called
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
    RUN(test_value_cut_as_parse);
    RUN(test_value_cut_at_the_buffer);
    RUN(test_strfind_matches_anywhere);
    RUN(test_strfind_not_after_a_letter_or_digit);
    RUN(test_join);
    RUN(test_join_excludes_argv0);
TEST_MAIN_END

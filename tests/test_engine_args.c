#include "test.h"
#include "core/argv.h"
#include "core/engine_args.h"

static size_t count(char **v) {
    size_t n = 0;
    while (v[n]) n++;
    return n;
}

/* The vector in its order; a quoted word kept whole, as no shell splits
 * it again: main's launcher lost these quotes (its known defects). */
static void test_argv(void) {
    dxl_engine_command c = {
        .engine = "/app/SurrealEngine", .game = "/games/Deus Ex",
        .ini = "/p/DeusEx.ini", .user_ini = "/p/User.ini", .user_dir = "/p/engine",
        .url = "01_NYC_UNATCOIsland", .extra = "--timeline=/tmp/t.txt -v",
        .cmdline = "INI=\"My Mod.ini\" -nosound",
    };
    char **v = dxl_engine_argv(&c);
    CHECK_INT(count(v), 10);
    CHECK_STR(v[0], "/app/SurrealEngine");
    CHECK_STR(v[1], "--no-launcher");
    CHECK_STR(v[2], "/games/Deus Ex");
    CHECK_STR(v[3], "--ini=/p/DeusEx.ini");
    CHECK_STR(v[4], "--userini=/p/User.ini");
    CHECK_STR(v[5], "--userdir=/p/engine");
    CHECK_STR(v[6], "--url=01_NYC_UNATCOIsland");
    CHECK_STR(v[7], "--timeline=/tmp/t.txt");
    CHECK_STR(v[8], "-v");
    CHECK_STR(v[9], "--cmdline=INI=\"My Mod.ini\" -nosound");
    dxl_argv_free(v);
}

/* No URL, no extras, no command line: --cmdline= still goes, empty, as the
 * original's command line was. */
static void test_argv_bare(void) {
    dxl_engine_command c = {
        .engine = "e", .game = "g", .ini = "i", .user_ini = "u", .user_dir = "d",
    };
    char **v = dxl_engine_argv(&c);
    CHECK_INT(count(v), 7);
    CHECK_STR(v[6], "--cmdline=");
    char *q = dxl_argv_quoted(v);
    CHECK_STR(q, "e --no-launcher g --ini=i --userini=u --userdir=d --cmdline=");
    free(q);
    dxl_argv_free(v);
}

static void test_quoting(void) {
    char *argv[] = { "a b", "it's", "plain-word_1.txt", "", NULL };
    char *q = dxl_argv_quoted(argv);
    CHECK_STR(q, "'a b' 'it'\\''s' plain-word_1.txt ''");
    free(q);
}

static void test_version(void) {
    dxl_engine_version v;
    CHECK_INT(dxl_engine_version_parse("VibeEngine e781e36 (2026-10-16)\nfeatures: userdir loadgame-url\n", &v), 0);
    CHECK_STR(v.version, "VibeEngine e781e36 (2026-10-16)");
    CHECK_STR(v.features, "userdir loadgame-url");
    CHECK_INT(dxl_engine_has(&v, "userdir"), 1);
    CHECK_INT(dxl_engine_has(&v, "loadgame-url"), 1);
    CHECK_INT(dxl_engine_has(&v, "loadgame"), 0);
    CHECK_INT(dxl_engine_has(&v, "user"), 0);
    /* An engine from before --version prints its help instead, or nothing. */
    CHECK_INT(dxl_engine_version_parse("SurrealEngine [--url=<mapname>] ...\n", &v), -1);
    CHECK_INT(dxl_engine_has(&v, "userdir"), 0);
    CHECK_INT(dxl_engine_version_parse("", &v), -1);
    /* CR LF, and no features line. */
    CHECK_INT(dxl_engine_version_parse("VibeEngine abc1234-dirty (2026-10-16)\r\n", &v), 0);
    CHECK_STR(v.version, "VibeEngine abc1234-dirty (2026-10-16)");
    CHECK_STR(v.features, "");
}

TEST_MAIN_BEGIN
    RUN(test_argv);
    RUN(test_argv_bare);
    RUN(test_quoting);
    RUN(test_version);
TEST_MAIN_END

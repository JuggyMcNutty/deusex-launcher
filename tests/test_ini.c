#include "test.h"
#include "core/ini.h"

/* The property that matters most: a file in the game's format, loaded and
 * saved untouched, must come back byte-identical -- CRLF, blank lines,
 * repeated keys and all. Everything else in the config layer is built on the
 * assumption that we never gratuitously rewrite the engine's own file. The
 * same round trip against the real files is in test_gamefiles. */
static void test_roundtrip_fixtures(void) {
    const char *names[] = { "DeusEx.ini", "Default.ini", "DefUser.ini" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        char *path = fixture(names[i]);
        size_t orig_len = 0;
        char *orig = slurp(path, &orig_len);
        CHECK(orig != NULL);
        if (!orig) { free(path); continue; }

        dxl_ini *ini = dxl_ini_load(path, NULL);
        CHECK(ini != NULL);
        if (ini) {
            size_t out_len = 0;
            char *out = dxl_ini_render(ini, &out_len);
            CHECK_INT(out_len, orig_len);
            CHECK(out_len == orig_len && memcmp(out, orig, orig_len) == 0);
            CHECK_INT(dxl_ini_dirty(ini), 0);
            free(out);
            dxl_ini_free(ini);
        }
        free(orig);
        free(path);
    }
}

static void test_reads_values(void) {
    char *path = fixture("DeusEx.ini");
    dxl_ini *ini = dxl_ini_load(path, NULL);
    free(path);
    CHECK(ini != NULL);
    if (!ini) return;

    CHECK_INT(dxl_ini_get_int(ini, "FirstRun", "FirstRun", -1), 0);
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "CdPath"), "..\\");
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "GameEngine"), "DeusEx.DeusExGameEngine");
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "GameRenderDevice"),
              "GlideDrv.GlideRenderDevice");

    /* Section and key lookup is case-insensitive, like FConfigCacheIni. */
    CHECK_STR(dxl_ini_get(ini, "engine.engine", "cdpath"), "..\\");
    CHECK_STR(dxl_ini_get(ini, "ENGINE.ENGINE", "CDPATH"), "..\\");

    /* A key the section lacks is absent, not empty. */
    CHECK(dxl_ini_get(ini, "D3DDrv.D3DRenderDevice", "DescFlags") == NULL);
    CHECK_STR(dxl_ini_get(ini, "URL", "Host"), "");

    dxl_ini_free(ini);
}

/* [Core.System] has five Paths= lines. A key is not unique within a section. */
static void test_repeated_keys(void) {
    char *path = fixture("DeusEx.ini");
    dxl_ini *ini = dxl_ini_load(path, NULL);
    free(path);
    CHECK(ini != NULL);
    if (!ini) return;

    size_t n = dxl_ini_get_all(ini, "Core.System", "Paths", NULL, 0);
    CHECK_INT(n, 5);

    const char *vals[8];
    size_t got = dxl_ini_get_all(ini, "Core.System", "Paths", vals, 8);
    CHECK_INT(got, 5);
    /* get() yields the first, in file order. */
    CHECK_STR(dxl_ini_get(ini, "Core.System", "Paths"), vals[0]);

    dxl_ini_free(ini);
}

static void test_set_preserves_everything_else(void) {
    char *path = fixture("DeusEx.ini");
    size_t orig_len = 0;
    char *orig = slurp(path, &orig_len);
    dxl_ini *ini = dxl_ini_load(path, NULL);
    free(path);
    CHECK(ini && orig);
    if (!ini || !orig) { free(orig); return; }

    dxl_ini_set(ini, "Engine.Engine", "GameRenderDevice", "OpenGLDrv.OpenGLRenderDevice");
    CHECK_INT(dxl_ini_dirty(ini), 1);
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "GameRenderDevice"),
              "OpenGLDrv.OpenGLRenderDevice");

    size_t out_len = 0;
    char *out = dxl_ini_render(ini, &out_len);
    /* Exactly one line differs, and the file still has CRLF throughout. */
    CHECK(strstr(out, "GameRenderDevice=OpenGLDrv.OpenGLRenderDevice\r\n") != NULL);
    CHECK(strstr(out, "GameRenderDevice=GlideDrv.GlideRenderDevice") == NULL);
    CHECK(strstr(out, "CdPath=..\\\r\n") != NULL);
    CHECK(strstr(out, "\n\n") == NULL);   /* no bare-LF crept in */

    free(out);
    free(orig);
    dxl_ini_free(ini);
}

/* Setting the value a key already has must not dirty the file: the launcher
 * writes FirstRun and GameRenderDevice on every run, and rewriting an
 * unchanged config for no reason is how you lose someone's settings to a
 * half-finished save. */
static void test_noop_set_is_not_dirty(void) {
    char *path = fixture("DeusEx.ini");
    dxl_ini *ini = dxl_ini_load(path, NULL);
    free(path);
    CHECK(ini != NULL);
    if (!ini) return;
    dxl_ini_set(ini, "Engine.Engine", "GameRenderDevice", "GlideDrv.GlideRenderDevice");
    dxl_ini_set_int(ini, "FirstRun", "FirstRun", 0);
    CHECK_INT(dxl_ini_dirty(ini), 0);
    dxl_ini_free(ini);
}

static void test_new_key_and_section(void) {
    dxl_ini *ini = dxl_ini_parse("[A]\r\nx=1\r\n", 10);
    CHECK(ini != NULL);

    /* New key joins its section rather than landing at end of file. */
    dxl_ini_set(ini, "A", "y", "2");
    /* New section is appended, separated by a blank line. */
    dxl_ini_set(ini, "B", "z", "3");

    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_STR(out, "[A]\r\nx=1\r\ny=2\r\n\r\n[B]\r\nz=3\r\n");
    free(out);

    CHECK_STR(dxl_ini_get(ini, "A", "y"), "2");
    CHECK_STR(dxl_ini_get(ini, "B", "z"), "3");
    CHECK_INT(dxl_ini_has_section(ini, "b"), 1);
    CHECK_INT(dxl_ini_has_section(ini, "C"), 0);
    dxl_ini_free(ini);
}

/* The detail auto-configuration writes booleans; the engine reads back the
 * word, not the digit (dx-reverse-info/ini-keys.md: UseReverb=False). */
static void test_bool_spelling(void) {
    dxl_ini *ini = dxl_ini_new();
    dxl_ini_set_bool(ini, "Galaxy.GalaxyAudioSubsystem", "UseReverb", 0);
    dxl_ini_set_bool(ini, "Galaxy.GalaxyAudioSubsystem", "UseFilter", 1);
    CHECK_STR(dxl_ini_get(ini, "Galaxy.GalaxyAudioSubsystem", "UseReverb"), "False");
    CHECK_STR(dxl_ini_get(ini, "Galaxy.GalaxyAudioSubsystem", "UseFilter"), "True");
    CHECK_INT(dxl_ini_get_bool(ini, "Galaxy.GalaxyAudioSubsystem", "UseReverb", 1), 0);
    CHECK_INT(dxl_ini_get_bool(ini, "Galaxy.GalaxyAudioSubsystem", "Missing", 1), 1);
    dxl_ini_free(ini);
}

static void test_comments_and_oddities(void) {
    const char *src =
        "; leading comment\r\n"
        "\r\n"
        "[S]\r\n"
        "  spaced = value with spaces  \r\n"
        "empty=\r\n"
        "novalue\r\n"
        "[Unclosed\r\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));

    CHECK_STR(dxl_ini_get(ini, "S", "spaced"), "value with spaces");
    CHECK_STR(dxl_ini_get(ini, "S", "empty"), "");
    /* A line with no '=' is not a pair; it is preserved verbatim. */
    CHECK(dxl_ini_get(ini, "S", "novalue") == NULL);

    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_INT(len, strlen(src));
    CHECK(memcmp(out, src, len) == 0);
    free(out);
    dxl_ini_free(ini);
}

/* A file whose last line has no terminator must still gain a well-formed
 * section when one is appended. */
static void test_unterminated_last_line(void) {
    const char *src = "[A]\r\nx=1";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    dxl_ini_set(ini, "B", "y", "2");
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_STR(out, "[A]\r\nx=1\r\n\r\n[B]\r\ny=2\r\n");
    free(out);
    dxl_ini_free(ini);
}

static void test_lf_file_stays_lf(void) {
    const char *src = "[A]\nx=1\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    dxl_ini_set(ini, "A", "y", "2");
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_STR(out, "[A]\nx=1\ny=2\n");
    free(out);
    dxl_ini_free(ini);
}

/* A line we rewrite adopts the file's own terminator, rather than keeping a
 * stray one another tool left behind. Mixed endings in a file the engine also
 * writes are the kind of thing that goes unnoticed until something downstream
 * chokes; a key we touch should look like its neighbours. */
static void test_rewrite_normalises_line_ending(void) {
    /* Middle line has a bare LF in an otherwise CRLF file. */
    const char *src = "[A]\r\nx=1\ny=2\r\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));

    dxl_ini_set(ini, "A", "x", "9");
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_STR(out, "[A]\r\nx=9\r\ny=2\r\n");
    free(out);
    dxl_ini_free(ini);

    /* An untouched odd line is still left exactly as found. */
    ini = dxl_ini_parse(src, strlen(src));
    out = dxl_ini_render(ini, &len);
    CHECK_INT(len, strlen(src));
    CHECK(memcmp(out, src, len) == 0);
    free(out);
    dxl_ini_free(ini);
}

TEST_MAIN_BEGIN
    RUN(test_roundtrip_fixtures);
    RUN(test_reads_values);
    RUN(test_repeated_keys);
    RUN(test_set_preserves_everything_else);
    RUN(test_noop_set_is_not_dirty);
    RUN(test_new_key_and_section);
    RUN(test_bool_spelling);
    RUN(test_comments_and_oddities);
    RUN(test_unterminated_last_line);
    RUN(test_lf_file_stays_lf);
    RUN(test_rewrite_normalises_line_ending);
TEST_MAIN_END

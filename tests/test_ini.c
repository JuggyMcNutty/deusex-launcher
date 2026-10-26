#include "test.h"
#include "scratch.h"
#include "core/ini.h"
#include "platform/fs.h"

#include <dirent.h>
#include <sys/stat.h>

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
    /* get() yields the last, the value the engine reads. */
    CHECK_STR(dxl_ini_get(ini, "Core.System", "Paths"), vals[4]);

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

/* A line written with the ini's own strings, as dxl_ini_pairs gives them:
 * a key's name, a value. */
static void test_set_with_its_own_strings(void) {
    const char *src = "[S]\nKey=One\nOther=Two\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    const char *k[2], *v[2];
    CHECK_INT(dxl_ini_pairs(ini, "S", k, v, 2), 2);
    dxl_ini_set(ini, "S", k[0], v[1]);
    dxl_ini_set(ini, "S", k[1], "");
    size_t len = 0;
    char *text = dxl_ini_render(ini, &len);
    CHECK_STR(text, "[S]\nKey=Two\nOther=\n");
    free(text);
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
    dxl_ini_set(ini, "Galaxy.GalaxyAudioSubsystem", "UseCDMusic", "yes");   /* a hand's: as the engine reads it */
    CHECK_INT(dxl_ini_get_bool(ini, "Galaxy.GalaxyAudioSubsystem", "UseCDMusic", 0), 1);
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

/* A key given twice reads and is set at its last value, and a section
 * written twice is one section, its blocks read in order: the engine reads
 * them so, as the original does. */
static void test_last_given_wins(void) {
    const char *src = "[A]\nx=1\nx=2\n[B]\nq=0\n[A]\nx=3\ny=1\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    CHECK_STR(dxl_ini_get(ini, "A", "x"), "3");
    CHECK_INT(dxl_ini_get_all(ini, "A", "x", NULL, 0), 3);
    CHECK_INT(dxl_ini_pairs(ini, "A", NULL, NULL, 0), 4);
    const char *names[4];
    CHECK_INT(dxl_ini_sections(ini, "", names, 4), 2);
    dxl_ini_set(ini, "A", "x", "4");
    dxl_ini_set(ini, "A", "z", "5");
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_STR(out, "[A]\nx=1\nx=2\n[B]\nq=0\n[A]\nx=4\ny=1\nz=5\n");
    free(out);
    dxl_ini_remove(ini, "A", "x");
    CHECK(dxl_ini_get(ini, "A", "x") == NULL);
    CHECK_INT(dxl_ini_pairs(ini, "A", NULL, NULL, 0), 2);
    dxl_ini_free(ini);
}

/* A key a partial file gives, once or as a list given line by line, stands
 * in for all of the base's lines of it, in the place of the first; a key
 * above the file's first header is in no section, and goes nowhere. */
static void test_overlay(void) {
    const char *base_src = "[S]\nK=1\nK=2\n; kept\nK=3\nL=x\n[T]\nM=0\n[S]\nK=4\n";
    const char *over_src = "top=above\n[S]\nK=a\nK=b\nL=x\n[U]\nN=new\nN=newer\n[S]\nK=c\n";
    dxl_ini *base = dxl_ini_parse(base_src, strlen(base_src));
    dxl_ini *over = dxl_ini_parse(over_src, strlen(over_src));
    dxl_ini_overlay(base, over);
    size_t len = 0;
    char *out = dxl_ini_render(base, &len);
    CHECK_STR(out, "[S]\nK=a\nK=b\nK=c\n; kept\nL=x\n[T]\nM=0\n[S]\n\n[U]\nN=new\nN=newer\n");
    free(out);
    CHECK(dxl_ini_get(base, "", "top") == NULL);
    /* Put over it again: the same lines, nothing changed. */
    dxl_ini_clean(base);
    dxl_ini_overlay(base, over);
    CHECK_INT(dxl_ini_dirty(base), 0);
    dxl_ini_free(over);
    dxl_ini_free(base);
}

/* Notepad's "Unicode" ini, UTF-16 by its byte-order mark, reads as the
 * game's 8-bit text and is written so; a NUL in a line is written back. */
static void test_utf16_and_nul(void) {
    const char src[] = "\xff\xfe[\0A\0]\0\r\0\n\0x\0=\0\xe9\0\x20\x20\r\0\n\0";
    dxl_ini *ini = dxl_ini_parse(src, sizeof src - 1);
    CHECK_STR(dxl_ini_get(ini, "A", "x"), "\xe9?");
    CHECK_INT(dxl_ini_dirty(ini), 1);
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_STR(out, "[A]\r\nx=\xe9?\r\n");
    free(out);
    dxl_ini_free(ini);

    const char nul[] = "[A]\r\nx=1\r\nju\0nk\r\n";
    ini = dxl_ini_parse(nul, sizeof nul - 1);
    CHECK_STR(dxl_ini_get(ini, "A", "x"), "1");
    CHECK_INT(dxl_ini_dirty(ini), 0);
    out = dxl_ini_render(ini, &len);
    CHECK_INT(len, sizeof nul - 1);
    CHECK(memcmp(out, nul, len) == 0);
    free(out);
    dxl_ini_free(ini);
}

/* A UTF-8 byte-order mark an editor left is read past, and gone at the next
 * save: the engine's reader takes it as text and loses the section after it. */
static void test_bom_is_read_past(void) {
    const char *src = "\xef\xbb\xbf[Launcher]\nProfile=x\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    CHECK_STR(dxl_ini_get(ini, "Launcher", "Profile"), "x");
    CHECK_INT(dxl_ini_dirty(ini), 1);
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_STR(out, "[Launcher]\nProfile=x\n");
    free(out);
    dxl_ini_free(ini);
}

/* ... and a key appended to that line's own section, which landed on it. */
static void test_unterminated_last_line_key(void) {
    const char *src = "[A]\r\nx=1";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    dxl_ini_set(ini, "A", "y", "2");
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_STR(out, "[A]\r\nx=1\r\ny=2\r\n");
    CHECK_STR(dxl_ini_get(ini, "A", "x"), "1");
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

/* A save replaces the file whole: no temporary file is left beside it, and
 * the file keeps the mode it had, where files have modes (not Windows'). */
static void test_save_is_atomic_and_keeps_the_mode(void) {
    const char *dir = scratch_dir("ini-save");
    char *path = scratch_path(dir, "DeusEx.ini");
    scratch_write(dir, "DeusEx.ini", "[A]\r\nx=1\r\n");
    chmod(path, 0600);
    dxl_ini *ini = dxl_ini_load(path, NULL);
    dxl_ini_set(ini, "A", "x", "2");
    dxl_err e;
    CHECK_INT(dxl_ini_save(ini, path, &e), 0);
    dxl_ini_free(ini);
    size_t len = 0;
    char *text = slurp(path, &len);
    CHECK_STR(text, "[A]\r\nx=2\r\n");
    free(text);
    struct stat st;
    CHECK_INT(stat(path, &st), 0);
#ifndef _WIN32
    CHECK_INT(st.st_mode & 0777, 0600);
#endif
    /* Nothing but the file in the folder. */
    DIR *d = opendir(dir);
    int others = 0;
    for (struct dirent *de; d && (de = readdir(d)); )
        if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..") && strcmp(de->d_name, "DeusEx.ini")) others++;
    if (d) closedir(d);
    CHECK_INT(others, 0);
    /* A new file is 0644. */
    char *fresh = scratch_path(dir, "User.ini");
    ini = dxl_ini_parse("[B]\ny=1\n", 9);
    CHECK_INT(dxl_ini_save(ini, fresh, &e), 0);
    CHECK_INT(stat(fresh, &st), 0);
#ifndef _WIN32
    CHECK_INT(st.st_mode & 0777, 0644);
#endif
    dxl_ini_free(ini);
#ifndef _WIN32
    /* Through a link: the target replaced, the link kept. */
    char *linked = scratch_path(dir, "linked.ini");
    CHECK_INT(dxl_fs_link_dir("DeusEx.ini", linked), 0);
    ini = dxl_ini_parse("[C]\nz=1\n", 9);
    CHECK_INT(dxl_ini_save(ini, linked, &e), 0);
    dxl_ini_free(ini);
    CHECK_INT(dxl_fs_is_link(linked), 1);
    text = slurp(path, &len);
    CHECK_STR(text, "[C]\nz=1\n");
    free(text);
    free(linked);
#endif
    free(fresh);
    free(path);
    scratch_remove(dir);
}

/* A CRLF file made LF, as the desktop launcher writes its profiles' inis. */
static void test_set_eol(void) {
    const char *src = "[A]\r\nx=1\r\n\r\n[B]\r\ny=2";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    CHECK_INT(dxl_ini_dirty(ini), 0);
    dxl_ini_set_eol(ini, "\n");
    CHECK_INT(dxl_ini_dirty(ini), 1);
    dxl_ini_set(ini, "A", "z", "3");
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_STR(out, "[A]\nx=1\nz=3\n\n[B]\ny=2");
    free(out);
    dxl_ini_free(ini);
    /* Already LF: nothing to change. */
    ini = dxl_ini_parse("[A]\nx=1\n", 9);
    dxl_ini_set_eol(ini, "\n");
    CHECK_INT(dxl_ini_dirty(ini), 0);
    dxl_ini_free(ini);
}

/* The sections by a prefix: launcher.ini's [Install.<id>]s. */
static void test_sections(void) {
    const char *src = "[Launcher]\nProfile=a\n[Install.one]\nPath=/a\n[install.TWO]\nPath=/b\n"
                      "[Install.one]\nVersion=x\n[Other]\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    const char *names[4];
    CHECK_INT(dxl_ini_sections(ini, "Install.", names, 4), 2);
    CHECK_STR(names[0], "Install.one");
    CHECK_STR(names[1], "install.TWO");
    CHECK_INT(dxl_ini_sections(ini, "Install.", names, 1), 2);
    CHECK_INT(dxl_ini_sections(ini, "Nothing", NULL, 0), 0);
    dxl_ini_free(ini);
}

TEST_MAIN_BEGIN
    RUN(test_roundtrip_fixtures);
    RUN(test_sections);
    RUN(test_reads_values);
    RUN(test_repeated_keys);
    RUN(test_set_preserves_everything_else);
    RUN(test_noop_set_is_not_dirty);
    RUN(test_set_with_its_own_strings);
    RUN(test_new_key_and_section);
    RUN(test_bool_spelling);
    RUN(test_comments_and_oddities);
    RUN(test_unterminated_last_line);
    RUN(test_last_given_wins);
    RUN(test_overlay);
    RUN(test_utf16_and_nul);
    RUN(test_bom_is_read_past);
    RUN(test_unterminated_last_line_key);
    RUN(test_lf_file_stays_lf);
    RUN(test_rewrite_normalises_line_ending);
    RUN(test_save_is_atomic_and_keeps_the_mode);
    RUN(test_set_eol);
TEST_MAIN_END

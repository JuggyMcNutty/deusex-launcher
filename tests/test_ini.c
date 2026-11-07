#include "test.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include "scratch.h"
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

/* A read-only ini, a player's way of keeping settings, is not replaced:
 * the save refuses, as the original's write in place would. */
static void test_save_refuses_read_only(void) {
    if (geteuid() == 0) return;   /* root writes anything */
    const char *dir = scratch_dir("ini-ro");
    scratch_write(dir, "System/DeusEx.ini", "[A]\r\nx=1\r\n");
    char *path = scratch_path(dir, "System/DeusEx.ini");
    chmod(path, 0444);
    const char *src = "[A]\r\nx=2\r\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    dxl_err err = { 0 };
    CHECK_INT(dxl_ini_save(ini, path, &err), -1);
    size_t n = 0;
    char *kept = slurp(path, &n);
    CHECK(kept && strcmp(kept, "[A]\r\nx=1\r\n") == 0);
    free(kept);
    chmod(path, 0644);
    free(path);
    dxl_ini_free(ini);
    scratch_remove(dir);
}

/* A linked ini is written where its link leads, the link kept; one whose
 * link points nowhere yet is made there, as the engine's write makes it,
 * not called read-only (bugs.md LCORE.13). */
static void test_save_through_a_link(void) {
    const char *dir = scratch_dir("ini-link");
    scratch_write(dir, "Real/DeusEx.ini", "[A]\r\nx=1\r\n");
    char *real = scratch_path(dir, "Real/DeusEx.ini");
    char *link = scratch_path(dir, "System/DeusEx.ini");
    CHECK_INT(symlink("../Real/DeusEx.ini", link), 0);
    const char *src = "[A]\r\nx=2\r\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    dxl_err err = { 0 };
    for (int nowhere = 0; nowhere < 2; nowhere++) {
        if (nowhere) remove(real);
        CHECK_INT(dxl_ini_save(ini, link, &err), 0);
        struct stat st;
        CHECK(lstat(link, &st) == 0 && S_ISLNK(st.st_mode));
        char *got = slurp(real, NULL);
        CHECK(got && strcmp(got, "[A]\r\nx=2\r\n") == 0);
        free(got);
    }
    dxl_ini_free(ini);
    free(real);
    free(link);
    scratch_remove(dir);
}

/* The entries of a folder but . and .. */
static int entries(const char *path) {
    DIR *d = opendir(path);
    int n = 0;
    for (struct dirent *e; d && (e = readdir(d));)
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) n++;
    if (d) closedir(d);
    return n;
}

/* A save writes a name of its own, beside the ini, and keeps its mode: a
 * file at the one name every save took before is left as it is, and none
 * is left behind. */
static void test_save_keeps_the_mode(void) {
    const char *dir = scratch_dir("ini-mode");
    scratch_write(dir, "System/DeusEx.ini", "[A]\r\nx=1\r\n");
    scratch_write(dir, "System/DeusEx.ini.tmpXXX", "another launcher's");
    char *path = scratch_path(dir, "System/DeusEx.ini");
    char *other = scratch_path(dir, "System/DeusEx.ini.tmpXXX");
    chmod(path, 0600);
    const char *src = "[A]\r\nx=2\r\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    dxl_err err = { 0 };
    CHECK_INT(dxl_ini_save(ini, path, &err), 0);
    size_t n = 0;
    char *saved = slurp(path, &n);
    CHECK(saved && strcmp(saved, src) == 0);
    free(saved);
    struct stat st;
    CHECK(stat(path, &st) == 0 && (st.st_mode & 07777) == 0600);
    char *kept = slurp(other, &n);
    CHECK(kept && strcmp(kept, "another launcher's") == 0);
    free(kept);
    char *sys = scratch_path(dir, "System");
    CHECK_INT(entries(sys), 2);   /* the ini and the other's file alone */
    free(sys);
    free(other);
    free(path);
    dxl_ini_free(ini);
    scratch_remove(dir);
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
    /* get() yields the last, the value the original reads (MAIN.10). */
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
    dxl_ini_free(ini);
}

/* A line as the original's reader takes it (bugs.md MAIN.14): a header a
 * line that begins [ and ends ], else a key up to its first = and its value
 * after it, nothing trimmed, a value in quotes without them (a lone "
 * empty); ; and # begin no comment. Every line is written back as it was. */
static void test_comments_and_oddities(void) {
    const char *src =
        "; leading comment\r\n"
        "\r\n"
        "[S]\r\n"
        "  spaced = value with spaces  \r\n"
        "empty=\r\n"
        "novalue\r\n"
        ";key=x\r\n"
        "#hash=y\r\n"
        "quoted=\"a b\"\r\n"
        "lone=\"\r\n"
        "half=\"open\r\n"
        "[T] \r\n"
        "under=S\r\n"
        "[Unclosed\r\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));

    CHECK(dxl_ini_get(ini, "S", "spaced") == NULL);
    CHECK_STR(dxl_ini_get(ini, "S", "  spaced "), " value with spaces  ");
    CHECK_STR(dxl_ini_get(ini, "S", "empty"), "");
    /* A line with no '=' is not a pair; it is preserved verbatim. */
    CHECK(dxl_ini_get(ini, "S", "novalue") == NULL);
    CHECK_STR(dxl_ini_get(ini, "S", ";key"), "x");
    CHECK_STR(dxl_ini_get(ini, "S", "#hash"), "y");
    CHECK_STR(dxl_ini_get(ini, "S", "quoted"), "a b");
    CHECK_STR(dxl_ini_get(ini, "S", "lone"), "");
    CHECK_STR(dxl_ini_get(ini, "S", "half"), "\"open");
    /* "[T] " is no header: the keys after it are the section's before. */
    CHECK_STR(dxl_ini_get(ini, "S", "under"), "S");
    CHECK(dxl_ini_get(ini, "T", "under") == NULL);

    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_INT(len, strlen(src));
    CHECK(memcmp(out, src, len) == 0);
    free(out);
    /* The value as it is read, set again, changes nothing. */
    dxl_ini_set(ini, "S", "quoted", "a b");
    CHECK_INT(dxl_ini_dirty(ini), 0);
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

/* A key appended to a section that ends the file's last, unterminated line
 * gets a line of its own: it was joined onto that line ("x=1y=2"). */
static void test_append_after_unterminated_last_line(void) {
    const char *src = "[A]\r\nx=1";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    dxl_ini_set(ini, "A", "y", "2");
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_STR(out, "[A]\r\nx=1\r\ny=2\r\n");
    free(out);
    CHECK_STR(dxl_ini_get(ini, "A", "x"), "1");
    CHECK_STR(dxl_ini_get(ini, "A", "y"), "2");
    dxl_ini_free(ini);
}

/* A key given twice reads and is set at its last value, a section written
 * twice is one section, a new key joining its last block: the original's
 * reading (bugs.md MAIN.10). */
static void test_last_given_wins(void) {
    const char *src = "[FirstRun]\r\nFirstRun=0\r\n[Engine.Engine]\r\nGameRenderDevice=GlideDrv.GlideRenderDevice\r\n"
                      "[FirstRun]\r\nFirstRun=1100\r\n[Engine.Engine]\r\nGameRenderDevice=D3DDrv.D3DRenderDevice\r\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    CHECK_INT(dxl_ini_get_int(ini, "FirstRun", "FirstRun", -1), 1100);
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "GameRenderDevice"), "D3DDrv.D3DRenderDevice");
    CHECK_INT((int)dxl_ini_get_all(ini, "Engine.Engine", "GameRenderDevice", NULL, 0), 2);
    dxl_ini_set(ini, "Engine.Engine", "GameRenderDevice", "OpenGLDrv.OpenGLRenderDevice");
    dxl_ini_set(ini, "Engine.Engine", "CdPath", "..\\");
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK_STR(out, "[FirstRun]\r\nFirstRun=0\r\n[Engine.Engine]\r\nGameRenderDevice=GlideDrv.GlideRenderDevice\r\n"
                   "[FirstRun]\r\nFirstRun=1100\r\n[Engine.Engine]\r\nGameRenderDevice=OpenGLDrv.OpenGLRenderDevice\r\n"
                   "CdPath=..\\\r\n");
    free(out);
    dxl_ini_free(ini);
}

/* UTF-16 read as the original reads it, written back as it came while a
 * character needs it, a byte a character once none does; big-endian read,
 * written little-endian; an odd size Latin-1; a NUL written back (bugs.md
 * MAIN.9). */
static void test_utf16(void) {
    static const char le[] = "\xff\xfe[\0A\0]\0\r\0\n\0x\0=\0\xe9\0 \0\xa9\x03\r\0\n\0";
    dxl_ini *ini = dxl_ini_parse(le, sizeof le - 1);
    CHECK_STR(dxl_ini_get(ini, "A", "x"), "\xc3\xa9 \xce\xa9");
    CHECK_INT(dxl_ini_dirty(ini), 0);
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK(len == sizeof le - 1 && memcmp(out, le, len) == 0);
    free(out);
    dxl_ini_set(ini, "A", "x", "\xc3\xa9");
    out = dxl_ini_render(ini, &len);
    CHECK(len == 10 && memcmp(out, "[A]\r\nx=\xe9\r\n", 10) == 0);
    free(out);
    dxl_ini_free(ini);

    static const char be[] = "\xfe\xff\0[\0A\0]\0\r\0\n\0x\0=\x03\xa9\0\r\0\n";
    ini = dxl_ini_parse(be, sizeof be - 1);
    CHECK_STR(dxl_ini_get(ini, "A", "x"), "\xce\xa9");
    dxl_ini_set(ini, "A", "y", "1");
    static const char want[] = "\xff\xfe[\0A\0]\0\r\0\n\0x\0=\0\xa9\x03\r\0\n\0y\0=\0" "1\0\r\0\n\0";
    out = dxl_ini_render(ini, &len);
    CHECK(len == sizeof want - 1 && memcmp(out, want, len) == 0);
    free(out);
    dxl_ini_free(ini);

    static const char odd[] = "\xff\xfe[\0A\0]\0\r\0\n\0x\0=\0" "1\0\n";
    ini = dxl_ini_parse(odd, sizeof odd - 1);
    CHECK(dxl_ini_get(ini, "A", "x") == NULL);
    dxl_ini_free(ini);

    static const char nul[] = "[A]\r\nx=1\r\nju\0nk\r\n";
    ini = dxl_ini_parse(nul, sizeof nul - 1);
    out = dxl_ini_render(ini, &len);
    CHECK(len == sizeof nul - 1 && memcmp(out, nul, len) == 0);
    free(out);
    dxl_ini_free(ini);
}

/* 8-bit is Latin-1, UTF-8 in memory, written back as it was; a character
 * past U+00FF makes the file UTF-16, as the original's writer does. */
static void test_latin1(void) {
    const char *src = "[Engine.Engine]\r\nCdPath=D:\\Spiele\\J\xfcrgen\\\r\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "CdPath"), "D:\\Spiele\\J\xc3\xbcrgen\\");
    size_t len = 0;
    char *out = dxl_ini_render(ini, &len);
    CHECK(len == strlen(src) && memcmp(out, src, len) == 0);
    free(out);
    dxl_ini_set(ini, "Engine.Engine", "Language", "\xce\xa9");
    out = dxl_ini_render(ini, &len);
    CHECK(len > 2 && (unsigned char)out[0] == 0xff && (unsigned char)out[1] == 0xfe);
    dxl_ini *back = dxl_ini_parse(out, len);
    CHECK_STR(dxl_ini_get(back, "Engine.Engine", "CdPath"), "D:\\Spiele\\J\xc3\xbcrgen\\");
    CHECK_STR(dxl_ini_get(back, "Engine.Engine", "Language"), "\xce\xa9");
    dxl_ini_free(back);
    free(out);
    dxl_ini_free(ini);
}

TEST_MAIN_BEGIN
    RUN(test_roundtrip_fixtures);
    RUN(test_save_refuses_read_only);
    RUN(test_save_keeps_the_mode);
    RUN(test_save_through_a_link);
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
    RUN(test_append_after_unterminated_last_line);
    RUN(test_last_given_wins);
    RUN(test_utf16);
    RUN(test_latin1);
TEST_MAIN_END

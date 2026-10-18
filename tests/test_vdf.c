#include "test.h"
#include "scratch.h"
#include "core/vdf.h"

static dxl_vdf *parse(const char *text) {
    dxl_err e;
    return dxl_vdf_parse(text, strlen(text), &e);
}

/* Today's libraryfolders.vdf: a block a library, its path in it. */
static void test_libraryfolders(void) {
    dxl_vdf *v = parse("\"libraryfolders\"\n{\n"
                       "\t\"0\"\n\t{\n\t\t\"path\"\t\t\"/home/me/.local/share/Steam\"\n\t\t\"label\"\t\t\"\"\n"
                       "\t\t\"apps\"\n\t\t{\n\t\t\t\"6910\"\t\t\"749652391\"\n\t\t}\n\t}\n"
                       "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"/run/media/me/Games\"\n\t}\n}\n");
    CHECK(v != NULL);
    CHECK_INT((int)dxl_vdf_count(v), 1);
    const dxl_vdf *top = dxl_vdf_get(v, "LibraryFolders");   /* any case */
    CHECK(top != NULL);
    CHECK_INT((int)dxl_vdf_count(top), 2);
    CHECK_STR(dxl_vdf_key(dxl_vdf_at(top, 1)), "1");
    CHECK_STR(dxl_vdf_get_string(dxl_vdf_at(top, 0), "path"), "/home/me/.local/share/Steam");
    CHECK_STR(dxl_vdf_get_string(dxl_vdf_at(top, 1), "path"), "/run/media/me/Games");
    CHECK_STR(dxl_vdf_get_string(dxl_vdf_get(dxl_vdf_at(top, 0), "apps"), "6910"), "749652391");
    CHECK(dxl_vdf_value(dxl_vdf_at(top, 0)) == NULL);              /* a block has no value */
    CHECK(dxl_vdf_get_string(dxl_vdf_at(top, 0), "apps") == NULL); /* nor reads as one */
    CHECK(dxl_vdf_at(top, 2) == NULL);
    dxl_vdf_free(v);
}

/* An older Steam's: the path a numbered key's value, beside keys of
 * another kind; bare words, comments, a condition, escapes. */
static void test_older_and_odd(void) {
    dxl_vdf *v = parse("// written by Steam\n\"LibraryFolders\" {\n"
                       "  \"TimeNextStatsReport\" \"1700000000\"\n"
                       "  \"1\" \"/mnt/games/SteamLibrary\" // a comment\n"
                       "  bare word\n"
                       "  \"cond\" \"x\" [$WIN32]\n"
                       "  \"win\" \"C:\\\\Games\\\\Steam \\\"quoted\\\"\"\n"
                       "}");
    CHECK(v != NULL);
    const dxl_vdf *top = dxl_vdf_get(v, "libraryfolders");
    CHECK_INT((int)dxl_vdf_count(top), 5);
    CHECK_STR(dxl_vdf_get_string(top, "1"), "/mnt/games/SteamLibrary");
    CHECK_STR(dxl_vdf_get_string(top, "bare"), "word");
    CHECK_STR(dxl_vdf_get_string(top, "cond"), "x");
    CHECK_STR(dxl_vdf_get_string(top, "win"), "C:\\Games\\Steam \"quoted\"");
    dxl_vdf_free(v);
}

/* What does not read: NULL and why, never a loop or a crash. */
static void test_broken(void) {
    static const char *const bad[] = {
        "\"a\" {", "\"a\" { \"b\" \"c\"", "\"a\" \"open", "}", "\"a\"", "\"a\" { \"b\" }", "{ \"a\" \"b\" }",
    };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        dxl_err e;
        e.msg[0] = '\0';
        dxl_vdf *v = dxl_vdf_parse(bad[i], strlen(bad[i]), &e);
        CHECK(v == NULL);
        CHECK(e.msg[0] != '\0');
    }
    /* Nested past what any of Steam's files do. */
    char deep[400] = "";
    for (int i = 0; i < 40; i++) strcat(deep, "\"k\" { ");
    CHECK(parse(deep) == NULL);
    /* A NUL in the text is passed over. */
    const char nul[] = "\"a\" \"b\"\0\"c\" \"d\"";
    dxl_err e;
    dxl_vdf *v = dxl_vdf_parse(nul, sizeof nul - 1, &e);
    CHECK(v != NULL);
    CHECK_STR(dxl_vdf_get_string(v, "c"), "d");
    dxl_vdf_free(v);
    /* Nothing at all reads as nothing. */
    v = parse("  // only a comment\n");
    CHECK(v != NULL);
    CHECK_INT((int)dxl_vdf_count(v), 0);
    dxl_vdf_free(v);
}

/* A manifest from a file; a file not there. */
static void test_load(void) {
    char *dir = dxl_xstrdup(scratch_dir("vdf"));
    scratch_write(dir, "appmanifest_6910.acf",
                  "\"AppState\"\n{\n\t\"appid\"\t\t\"6910\"\n\t\"name\"\t\t\"Deus Ex: Game of the Year Edition\"\n"
                  "\t\"installdir\"\t\t\"Deus Ex\"\n}\n");
    char *path = scratch_path(dir, "appmanifest_6910.acf");
    dxl_err e;
    dxl_vdf *v = dxl_vdf_load(path, &e);
    CHECK(v != NULL);
    CHECK_STR(dxl_vdf_get_string(dxl_vdf_get(v, "AppState"), "installdir"), "Deus Ex");
    dxl_vdf_free(v);
    char *none = scratch_path(dir, "not-there.vdf");
    CHECK(dxl_vdf_load(none, &e) == NULL);
    CHECK(strstr(e.msg, "not-there.vdf") != NULL);
    free(none);
    free(path);
    scratch_remove(dir);
    free(dir);
}

TEST_MAIN_BEGIN
    RUN(test_libraryfolders);
    RUN(test_older_and_odd);
    RUN(test_broken);
    RUN(test_load);
TEST_MAIN_END

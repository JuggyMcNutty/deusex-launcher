/* Mods' audit of each mod's natives by the engine (screens/mods.c, bugs.md
 * LCORE.27), the stand-in engine answering it: a mod whose packages declare
 * natives the engine lacks marked as one that does not play, its profile
 * not made; each mod asked once for an engine and a game, again for
 * another; an engine without audit-natives, or one that fails or answers
 * in part, marking nothing; the window's worker handed the words. */
#include "scratch.h"
#include "upkg_writer.h"
#include "core/argv.h"
#include "screens/shell.h"

static dxl_shell s;
static char *root, *count_file;

/* A package's file as the game's begin: its tag, version and empty tables. */
static void package(const char *rel) {
    buf b = { 0 };
    u32(&b, 0x9E2A83C1u);
    u16(&b, 61);
    u16(&b, 0);
    for (int i = 0; i < 7; i++) u32(&b, 0);
    char *path = scratch_path(root, rel);
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(b.d, 1, b.n, f);
        fclose(f);
    }
    free(path);
    free(b.d);
}

/* The audits the engine has run so far. */
static long audits(void) {
    FILE *f = fopen(count_file, "rb");
    if (!f) return 0;
    long n = 0;
    while (fgetc(f) != EOF) n++;
    fclose(f);
    return n;
}

static int row_of(const char *name) {
    for (int i = 0; i < s.mods.count; i++)
        if (!strcmp(s.mods.items[i].name, name)) return i;
    return -1;
}

static char **handed;
static int worker(dxl_shell *sh, char **argv) {
    (void)sh;
    size_t n = 0;
    while (argv[n]) n++;
    handed = dxl_xmalloc((n + 1) * sizeof *handed);
    for (size_t i = 0; i <= n; i++) handed[i] = argv[i] ? dxl_xstrdup(argv[i]) : NULL;
    return 0;
}

static void test_audit(void) {
    root = dxl_xstrdup(scratch_dir("shell-mods"));
    char *home = scratch_path(root, "home");
    count_file = scratch_path(root, "audits");
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_STUB_COUNT", count_file);
    dxl_env_set("DXL_STUB_FEATURES", "userdir audit-natives");
    package("Deus Ex/Good/System/Good.u");
    package("Deus Ex/Good/Textures/GoodTex.utx");
    package("Deus Ex/Natives/System/NativeCode.u");
    package("Deus Ex/Natives/System/Plain.u");
    scratch_write(root, "Deus Ex/Natives/System/NativeCode.dll", "MZ");

    static dxl_run r;
    memset(&r, 0, sizeof r);
    dxl_err e;
    CHECK_INT(dxl_session_open(&r.session, NULL, &e), 0);
    free(r.session.engine);
    r.session.engine = dxl_xstrdup(DXL_STUB_ENGINE);
    r.session.install.root = scratch_path(root, "Deus Ex");
    r.session.install.system_dir = scratch_path(root, "Deus Ex/System");
    r.session.install.ok = 1;
    dxl_shell_init(&s, "test");
    s.run = &r;
    dxl_shell_ask_engine(&s);
    CHECK_INT(s.engine_ok, 1);
    s.page = DXL_PAGE_MODS;

    /* Offscreen, each asked at once as the page opens. */
    dxl_page_mods_enter(&s);
    CHECK_INT(s.mods.count, 2);
    int good = row_of("Good"), bad = row_of("Natives");
    CHECK(good >= 0 && bad >= 0);
    if (good < 0 || bad < 0) return;
    CHECK_INT(audits(), 2);
    CHECK_INT(s.mod_rows[good].plays_not, 0);
    CHECK_INT(s.mod_rows[bad].plays_not, 1);
    CHECK(strstr(s.mod_rows[bad].detail, "does not play: native code of its own, 2 functions DXEngine lacks "
                                         "(NativeCode.NativeCode.ProbeByName, NativeCode.NativeCode.ProbeNumbered "
                                         "(4090))") != NULL);
    const dxl_mod_natives *n = dxl_shell_mod_natives(&s, s.mods.items[good].dir);
    CHECK(n && n->known && n->declared == 1 && n->lacking == 0);
    n = dxl_shell_mod_natives(&s, s.mods.items[bad].dir);
    CHECK(n && n->known && n->declared == 4 && n->lacking == 2);

    /* No profile made of it: a message says why, nothing begun. */
    dxl_shell_make_mod(&s, &s.mods.items[bad]);
    CHECK_INT(s.dialog, DXL_DIALOG_MESSAGE);
    CHECK_STR(s.message_title, "This mod does not play");
    CHECK_INT(s.making, 0);
    dxl_shell_close_dialog(&s);

    /* Kept: the page again asks nothing. */
    dxl_page_mods_enter(&s);
    CHECK_INT(audits(), 2);
    CHECK_INT(s.mod_rows[row_of("Natives")].plays_not, 1);

    /* Another game (the same folder by another name, its mods the same):
     * asked again. An engine that fails marks nothing, and is not asked
     * again. */
    free(r.session.install.root);
    r.session.install.root = scratch_path(root, "Deus Ex/.");
    dxl_env_set("DXL_STUB_AUDIT", "fail");
    dxl_page_mods_enter(&s);
    CHECK_INT(audits(), 4);
    CHECK_INT(s.mod_rows[row_of("Natives")].plays_not, 0);
    n = dxl_shell_mod_natives(&s, s.mods.items[row_of("Natives")].dir);
    CHECK(n && !n->known);
    dxl_page_mods_enter(&s);
    CHECK_INT(audits(), 4);

    /* An answer cut short is none either. */
    free(r.session.install.root);
    r.session.install.root = scratch_path(root, "Deus Ex/./.");
    dxl_env_set("DXL_STUB_AUDIT", "cut");
    dxl_page_mods_enter(&s);
    CHECK_INT(audits(), 6);
    CHECK_INT(s.mod_rows[row_of("Natives")].plays_not, 0);
    dxl_env_set("DXL_STUB_AUDIT", NULL);

    /* The window's worker: handed the words, one mod at a time, the row
     * waiting on it; its answer kept, and the next one handed on. */
    free(r.session.install.root);
    r.session.install.root = scratch_path(root, "Deus Ex/./././.");
    s.start_audit = worker;
    dxl_page_mods_enter(&s);
    CHECK(handed != NULL && s.auditing != NULL);
    if (handed && s.auditing) {
        char want[1200];
        snprintf(want, sizeof want, "--audit-natives=%s", s.mods.items[row_of("Good")].system_dir);
        CHECK_STR(handed[0], DXL_STUB_ENGINE);
        CHECK_STR(handed[1], want);
        CHECK_STR(handed[2], r.session.install.root);
        CHECK(handed[3] == NULL);
        CHECK_STR(s.auditing, s.mods.items[row_of("Good")].dir);
        dxl_shell_make_mod(&s, &s.mods.items[row_of("Good")]);
        CHECK_INT(s.making, 0);
        dxl_mod_natives none;
        memset(&none, 0, sizeof none);
        none.known = 1;
        none.declared = 1;
        dxl_argv_free(handed);
        handed = NULL;
        dxl_shell_audited(&s, &none);
        CHECK(handed != NULL);
        CHECK_STR(s.auditing, s.mods.items[row_of("Natives")].dir);
        dxl_argv_free(handed);
        handed = NULL;
    }
    CHECK_INT(audits(), 6);
    s.start_audit = NULL;

    /* An engine without audit-natives: none asked, nothing marked. */
    free(s.auditing);
    s.auditing = NULL;
    dxl_env_set("DXL_STUB_FEATURES", "userdir");
    s.engine_asked = 0;
    dxl_shell_ask_engine(&s);
    free(r.session.install.root);
    r.session.install.root = scratch_path(root, "Deus Ex");
    dxl_page_mods_enter(&s);
    CHECK_INT(audits(), 6);
    CHECK(dxl_shell_mod_natives(&s, s.mods.items[row_of("Natives")].dir) == NULL);

    dxl_shell_free(&s);
    dxl_session_close(&r.session);
    dxl_env_set("DXL_STUB_FEATURES", NULL);
    dxl_env_set("DXL_STUB_COUNT", NULL);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(root);
    free(count_file);
    free(home);
    free(root);
}

TEST_MAIN_BEGIN
    RUN(test_audit);
TEST_MAIN_END

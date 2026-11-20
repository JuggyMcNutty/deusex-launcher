/* An install chosen for the profile (screens/shell.h, dxl_shell_choose):
 * a repoint whose save fails leaves the profile on the install it had, in
 * memory as on disk, so no later save writes the other (bugs.md LUI.8). */
#include "scratch.h"
#include "screens/shell.h"

static dxl_shell s;

/* The repoint with launcher.ini a folder, so its save fails; had, the
 * profile's install before it, or NULL. */
static void repoint_not_kept(const char *tag, const char *had) {
    const char *home = scratch_dir(tag);
    dxl_env_set("DXL_HOME", home);
    static dxl_run r;
    memset(&r, 0, sizeof r);
    dxl_err e;
    CHECK_INT(dxl_session_open(&r.session, NULL, &e), 0);
    if (had) dxl_profile_set(&r.session.profile, "Install", had);
    CHECK_INT(dxl_path_mkdirs(r.session.ini_path), 0);
    dxl_shell_init(&s, "test");
    s.run = &r;
    s.repoint = 1;
    dxl_install in;
    memset(&in, 0, sizeof in);
    in.root = "/elsewhere/Deus Ex";
    in.ok = 1;
    dxl_shell_choose(&s, &in);
    CHECK_INT(s.dialog, DXL_DIALOG_MESSAGE);
    CHECK_STR(s.message_title, "The profile was not changed");
    const char *now = dxl_profile_get(&r.session.profile, "Install");
    if (had) CHECK_STR(now ? now : "(none)", had);
    else CHECK(now == NULL);
    dxl_session_close(&r.session);
    dxl_env_set("DXL_HOME", NULL);
    CHECK_INT(dxl_fs_remove_tree(home), 0);
}

/* A profile whose install's folder launcher.ini kept, launcher.ini gone:
 * The game says the launcher has lost it, not that the profile has
 * no game (bugs.md LAPP.25). */
static void test_repoint_unknown(void) {
    const char *home = scratch_dir("repoint-unknown");
    dxl_env_set("DXL_HOME", home);
    static dxl_run r;
    memset(&r, 0, sizeof r);
    dxl_err e;
    CHECK_INT(dxl_session_open(&r.session, NULL, &e), 0);
    dxl_shell_init(&s, "test");
    s.run = &r;
    s.repoint = 1;
    char line[1600];
    dxl_page_find_repoint_line(&s, line, sizeof line);
    CHECK_STR(line, "The profile's game is not there to play: it has none.");
    dxl_profile_set(&r.session.profile, "Install", "1112fm-0badf00d");
    CHECK_INT(dxl_session_check_install(&r.session), -1);
    dxl_page_find_repoint_line(&s, line, sizeof line);
    CHECK(strstr(line, "has lost where the profile's game is") != NULL);
    dxl_session_close(&r.session);
    dxl_env_set("DXL_HOME", NULL);
    CHECK_INT(dxl_fs_remove_tree(home), 0);
}

static void test_repoint_not_kept(void) {
    repoint_not_kept("repoint-had", "1112fm-0badf00d");
    repoint_not_kept("repoint-none", NULL);
}

/* The engine asked again for an install of the other kind: one known by
 * its packages wants game-by-packages, which the stub engine lacks, and a
 * stock one again does not (bugs.md D.89). */
static void test_engine_by_packages(void) {
    static dxl_run r;
    memset(&r, 0, sizeof r);
    dxl_shell_init(&s, "test");
    s.run = &r;
    r.session.engine = DXL_STUB_ENGINE;
    dxl_shell_ask_engine(&s);
    CHECK_INT(s.engine_ok, 1);
    r.session.install.by_packages = 1;
    dxl_shell_ask_engine(&s);
    CHECK_INT(s.engine_ok, 0);
    CHECK(strstr(s.engine_brief, "without game-by-packages") != NULL);
    r.session.install.by_packages = 0;
    dxl_shell_ask_engine(&s);
    CHECK_INT(s.engine_ok, 1);
}

/* A mod's profile made of root/<mod>, on the game at root/<game>: its
 * DeusEx.ini's paths as made, the mod's first, its own saves; profile.ini's
 * Install old and Mod, launcher.ini's old its game's folder. The game
 * itself is no folder: what is checked is what is written. */
static void mod_profile(dxl_run *r, const char *root, const char *game, const char *mod) {
    dxl_err e;
    CHECK_INT(dxl_session_open(&r->session, NULL, &e), 0);
    dxl_profile *p = &r->session.profile;
    CHECK_INT(dxl_profile_make_dirs(p, NULL), 0);
    char text[4096];
    snprintf(text, sizeof text,
             "[Core.System]\nSavePath=%s\nCachePath=%s\nPaths=%s/%s/System/*.u\nPaths=%s/%s/Maps/*.dx\n"
             "Paths=..\\System\\*.u\nPaths=%s/%s/System/*.u\n\n[DeusEx.Player]\nName=Jos\xe9\n",
             p->save_dir, p->cache_dir, root, mod, root, mod, root, game);
    FILE *f = fopen(p->system_ini, "wb");
    if (f) { fputs(text, f); fclose(f); }
    scratch_write(p->save_dir, "Save0001/SaveInfo.dxs", "the profile's");
    snprintf(text, sizeof text, "%s/%s", root, game);
    dxl_ini_set(r->session.ini, "Install.old", "Path", text);
    dxl_profile_set(p, "Install", "old");
    snprintf(text, sizeof text, "%s/%s", root, mod);
    dxl_profile_set(p, "Mod", text);
    CHECK_INT(dxl_session_save(&r->session, &e), 0);
}

/* The profile's DeusEx.ini's paths, as the file has them, against want's,
 * each root/<want's>, but a relative one as it is. */
static void paths_are(const dxl_profile *p, const char *root, const char *const *want, size_t n) {
    dxl_ini *sys = dxl_ini_load(p->system_ini, NULL);
    CHECK(sys != NULL);
    if (!sys) return;
    const char *v[16];
    CHECK_INT(dxl_ini_get_all(sys, "Core.System", "Paths", v, 16), n);
    for (size_t i = 0; i < n && i < 16; i++) {
        char full[1024];
        snprintf(full, sizeof full, "%s/%s", root, want[i]);
        CHECK_STR(v[i], want[i][0] == '.' ? want[i] : full);
    }
    CHECK_STR(dxl_ini_get(sys, "Core.System", "SavePath"), p->save_dir);
    CHECK_STR(dxl_ini_get(sys, "DeusEx.Player", "Name"), "Jos\xc3\xa9");
    dxl_ini_free(sys);
}

/* A mod's profile pointed at its mod moved (bugs.md LUI.10): Mod and the
 * mod's paths written for where it is, in DeusEx.ini and the run's open
 * copy of it, the game's paths and the saves as they were; a profile.ini
 * that cannot be written leaves Mod as it was, in memory too. */
static void test_mod_moved(void) {
    char *root = dxl_xstrdup(scratch_dir("mod-moved"));
    char *home = scratch_path(root, "home");
    dxl_env_set("DXL_HOME", home);
    static dxl_run r;
    memset(&r, 0, sizeof r);
    mod_profile(&r, root, "Deus Ex", "Old/MyMod");
    dxl_profile *p = &r.session.profile;
    scratch_write(root, "Deus Ex/System/Core.u", "pkg");
    scratch_write(root, "New Place/MyMod/System/MyMod.u", "pkg");
    scratch_write(root, "New Place/MyMod/Maps/M.dx", "map");
    dxl_install *in = &r.session.install;
    in->root = scratch_path(root, "Deus Ex");
    in->system_dir = scratch_path(root, "Deus Ex/System");
    in->ok = 1;
    r.config = dxl_config_open_files(in->system_dir, DXL_GAME_PACKAGE, p->system_ini, p->user_ini);
    dxl_shell_init(&s, "test");
    s.run = &r;
    dxl_shell_profile_mod(&s);
    CHECK_INT(s.profile_mod, -1);

    dxl_mod m;
    char *to = scratch_path(root, "New Place/MyMod");
    CHECK_INT(dxl_mod_check(to, &m, NULL), 0);
    CHECK_INT(dxl_shell_choose_mod(&s, &m), 1);
    CHECK_INT(s.dialog, DXL_DIALOG_NONE);
    CHECK_INT(s.profile_mod, 1);
    CHECK_STR(dxl_profile_get(p, "Mod"), to);
    dxl_profile q;
    dxl_profile_open(&q, &r.session.xdg, p->id, NULL);
    CHECK_STR(dxl_profile_get(&q, "Mod"), to);
    dxl_profile_free(&q);
    const char *want[] = { "New Place/MyMod/System/*.u", "New Place/MyMod/Maps/*.dx", "..\\System\\*.u",
                           "Deus Ex/System/*.u" };
    paths_are(p, root, want, 4);
    const char *v[1];
    char first[1024];
    snprintf(first, sizeof first, "%s/%s", root, want[0]);
    CHECK(dxl_ini_get_all(dxl_config_ini(r.config), "Core.System", "Paths", v, 1) >= 1 && !strcmp(v[0], first));
    CHECK_INT(scratch_exists(p->save_dir, "Save0001/SaveInfo.dxs"), 1);
    CHECK_INT(dxl_path_exists(p->user_ini), 0);
    dxl_mod_free(&m);

    /* Another, profile.ini a folder: not kept. */
    scratch_write(root, "Third/MyMod/System/MyMod.u", "pkg");
    char *third = scratch_path(root, "Third/MyMod");
    CHECK_INT(dxl_mod_check(third, &m, NULL), 0);
    CHECK_INT(remove(p->ini_path), 0);
    CHECK_INT(dxl_path_mkdirs(p->ini_path), 0);
    CHECK_INT(dxl_shell_choose_mod(&s, &m), 0);
    CHECK_INT(s.dialog, DXL_DIALOG_MESSAGE);
    CHECK_STR(s.message_title, "The profile was not changed");
    CHECK_STR(dxl_profile_get(p, "Mod"), to);
    CHECK(dxl_ini_get_all(dxl_config_ini(r.config), "Core.System", "Paths", v, 1) >= 1 && !strcmp(v[0], first));
    dxl_mod_free(&m);

    /* A mod of another folder name: asked first, nothing changed; Keep
     * looking leaves it, Use it takes the folder asked about. */
    dxl_err saved;
    CHECK_INT(dxl_fs_remove_tree(p->ini_path), 0);
    CHECK_INT(dxl_session_save(&r.session, &saved), 0);
    scratch_write(root, "Third/Other/System/Other.u", "pkg");
    char *other = scratch_path(root, "Third/Other");
    CHECK_INT(dxl_mod_check(other, &m, NULL), 0);
    s.dialog = DXL_DIALOG_NONE;
    CHECK_INT(dxl_shell_choose_mod(&s, &m), 0);
    CHECK_INT(s.dialog, DXL_DIALOG_CONFIRM);
    CHECK_STR(s.ask_title, "Another mod?");
    CHECK(strstr(s.message, "made for MyMod") && strstr(s.message, "this folder is Other"));
    CHECK_STR(dxl_profile_get(p, "Mod"), to);
    s.dialog = DXL_DIALOG_NONE;
    s.answered = 1;
    s.answer = 0;
    dxl_shell_mod_answer(&s);
    CHECK_STR(dxl_profile_get(p, "Mod"), to);
    CHECK_STR(s.mod_other, "");
    CHECK_INT(dxl_shell_choose_mod(&s, &m), 0);
    s.dialog = DXL_DIALOG_NONE;
    s.answered = 1;
    s.answer = 1;
    dxl_shell_mod_answer(&s);
    CHECK_INT(s.dialog, DXL_DIALOG_NONE);
    CHECK_STR(dxl_profile_get(p, "Mod"), other);
    CHECK_INT(scratch_exists(p->save_dir, "Save0001/SaveInfo.dxs"), 1);
    dxl_mod_free(&m);

    dxl_config_free(r.config);
    dxl_session_close(&r.session);
    dxl_env_set("DXL_HOME", NULL);
    CHECK_INT(dxl_fs_remove_tree(root), 0);
    free(other); free(third); free(to); free(home); free(root);
}

/* The game pointed anew, the profile's mod in its old folder: found at its
 * place in the new one, Mod and the paths under the old folder moved with
 * it; one still where it was, or at no such place, left (bugs.md LUI.10). */
static void test_mod_follows(void) {
    char *root = dxl_xstrdup(scratch_dir("mod-follows"));
    char *home = scratch_path(root, "home");
    dxl_env_set("DXL_HOME", home);
    static dxl_run r;
    memset(&r, 0, sizeof r);
    mod_profile(&r, root, "Deus Ex", "Deus Ex/MyMod");
    dxl_profile *p = &r.session.profile;
    scratch_write(root, "Games/Deus Ex/System/Core.u", "pkg");
    scratch_write(root, "Games/Deus Ex/MyMod/System/MyMod.u", "pkg");
    dxl_shell_init(&s, "test");
    s.run = &r;
    s.repoint = 1;
    dxl_install in;
    memset(&in, 0, sizeof in);
    in.root = scratch_path(root, "Games/Deus Ex");
    in.system_dir = scratch_path(root, "Games/Deus Ex/System");
    in.ok = 1;
    dxl_shell_choose(&s, &in);
    CHECK_INT(s.result, 1);
    char *mod = scratch_path(root, "Games/Deus Ex/MyMod");
    CHECK_STR(dxl_profile_get(p, "Mod"), mod);
    const char *want[] = { "Games/Deus Ex/MyMod/System/*.u", "Games/Deus Ex/MyMod/Maps/*.dx", "..\\System\\*.u",
                           "Games/Deus Ex/System/*.u" };
    paths_are(p, root, want, 4);
    CHECK_INT(scratch_exists(p->save_dir, "Save0001/SaveInfo.dxs"), 1);

    /* Again from there to a folder with no mod at its place: Mod as it is. */
    scratch_write(root, "Bare/Deus Ex/System/Core.u", "pkg");
    char *bare = scratch_path(root, "Bare/Deus Ex"), *bare_sys = scratch_path(root, "Bare/Deus Ex/System");
    dxl_err e;
    CHECK_INT(dxl_fs_remove_tree(mod), 0);
    CHECK_INT(dxl_run_mod_follows(&r, bare_sys, in.root, bare, &e), 0);
    CHECK_STR(dxl_profile_get(p, "Mod"), mod);
    /* The mod still where it was: left there. */
    scratch_write(root, "Games/Deus Ex/MyMod/System/MyMod.u", "pkg");
    scratch_write(root, "Bare/Deus Ex/MyMod/System/MyMod.u", "pkg");
    CHECK_INT(dxl_run_mod_follows(&r, bare_sys, in.root, bare, &e), 0);
    CHECK_STR(dxl_profile_get(p, "Mod"), mod);

    free(in.root); free(in.system_dir);
    dxl_session_close(&r.session);
    dxl_env_set("DXL_HOME", NULL);
    CHECK_INT(dxl_fs_remove_tree(root), 0);
    free(bare); free(bare_sys); free(mod); free(home); free(root);
}

/* The game pointed anew, the profile's mod elsewhere and run from its own
 * System: Mod as it is, its paths into the old game's folder given the new
 * one's, none into the old left (bugs.md LUI.10). */
static void test_own_system_follows(void) {
    char *root = dxl_xstrdup(scratch_dir("own-system-follows"));
    char *home = scratch_path(root, "home");
    dxl_env_set("DXL_HOME", home);
    static dxl_run r;
    memset(&r, 0, sizeof r);
    scratch_write(root, "Elsewhere/OwnMod/System/OwnMod.u", "pkg");
    scratch_write(root, "Elsewhere/OwnMod/System/Core.dll", "dll");
    scratch_write(root, "Elsewhere/OwnMod/Maps/M.dx", "map");
    scratch_write(root, "Games/Deus Ex/System/Core.u", "pkg");
    mod_profile(&r, root, "Deus Ex", "Elsewhere/OwnMod");
    dxl_profile *p = &r.session.profile;
    dxl_shell_init(&s, "test");
    s.run = &r;
    s.repoint = 1;
    dxl_install in;
    memset(&in, 0, sizeof in);
    in.root = scratch_path(root, "Games/Deus Ex");
    in.system_dir = scratch_path(root, "Games/Deus Ex/System");
    in.ok = 1;
    dxl_shell_choose(&s, &in);
    CHECK_INT(s.result, 1);
    char *mod = scratch_path(root, "Elsewhere/OwnMod");
    CHECK_STR(dxl_profile_get(p, "Mod"), mod);
    dxl_ini *sys = dxl_ini_load(p->system_ini, NULL);
    CHECK(sys != NULL);
    if (sys) {
        const char *v[16];
        size_t n = dxl_ini_get_all(sys, "Core.System", "Paths", v, 16);
        char *now = scratch_path(root, "Games/Deus Ex/System/*.u"), *old = scratch_path(root, "Deus Ex/");
        int found = 0, stale = 0;
        for (size_t i = 0; i < n && i < 16; i++) {
            if (!strcmp(v[i], now)) found = 1;
            if (!strncmp(v[i], old, strlen(old))) stale = 1;
        }
        CHECK_INT(found, 1);
        CHECK_INT(stale, 0);
        free(now);
        free(old);
        dxl_ini_free(sys);
    }
    free(in.root); free(in.system_dir);
    dxl_session_close(&r.session);
    dxl_env_set("DXL_HOME", NULL);
    CHECK_INT(dxl_fs_remove_tree(root), 0);
    free(mod); free(home); free(root);
}

TEST_MAIN_BEGIN
    RUN(test_repoint_not_kept);
    RUN(test_mod_moved);
    RUN(test_mod_follows);
    RUN(test_own_system_follows);
    RUN(test_repoint_unknown);
    RUN(test_engine_by_packages);
TEST_MAIN_END

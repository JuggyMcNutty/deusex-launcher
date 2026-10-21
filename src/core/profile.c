#define _GNU_SOURCE
#include "core/profile.h"
#include "core/engine_sections.h"
#include "core/paths.h"
#include "platform/fs.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The engine's own sections of the system ini, which it reads once they are
 * there (DXEngine's ENGINE.md, gameplay: the audio and render devices). */
static const char *const engine_sections[] = {
    "Engine.DXEngineClient", "Engine.DXEngineAudioDevice", "Engine.DXEngineRenderDevice",
    "Engine.DXEngineWindowSystem",
};

/* The game's master server for its Join screen is GameSpy's, which closed;
 * 333networks' answers in its place. */
#define DEAD_MASTER "master0.gamespy.com"
#define LIVE_MASTER "master.333networks.com"

static int id_valid(const char *id) {
    if (!id || !*id || strlen(id) > 64) return 0;
    for (const char *c = id; *c; c++)
        if (!isalnum((unsigned char)*c) && *c != '-' && *c != '_') return 0;
    return 1;
}

static char *join3(const char *a, const char *b, const char *c) {
    char *ab = dxl_path_join(a, b);
    char *abc = dxl_path_join(ab, c);
    free(ab);
    return abc;
}

int dxl_profile_open(dxl_profile *p, const dxl_xdg *x, const char *id, dxl_err *err) {
    memset(p, 0, sizeof *p);
    if (!id_valid(id)) {
        dxl_err_set(err, "a profile's id is letters, digits, '-' and '_': not \"%s\"", id ? id : "");
        return -1;
    }
    p->id = dxl_xstrdup(id);
    p->dir = join3(x->config, "profiles", id);
    p->ini_path = dxl_path_join(p->dir, "profile.ini");
    p->ini = dxl_ini_load(p->ini_path, NULL);
    if (!p->ini) {
        p->ini = dxl_ini_new();
        dxl_ini_set_eol(p->ini, "\n");
    }
    p->system_ini = dxl_path_join(p->dir, "DeusEx.ini");
    p->user_ini = dxl_path_join(p->dir, "User.ini");
    p->engine_dir = dxl_path_join(p->dir, "engine");
    char *data = join3(x->data, "profiles", id);
    p->save_dir = dxl_path_join(data, "Save");
    /* The engine's own Screenshots, where it is a folder, not a link: a
     * profile made on a filesystem without links (dxl_profile_make_dirs). */
    char *own = dxl_path_join(p->engine_dir, "Screenshots");
    if (!dxl_fs_is_link(own) && dxl_path_is_dir(own)) {
        p->shots_dir = own;
    } else {
        free(own);
        p->shots_dir = dxl_path_join(data, "Screenshots");
    }
    free(data);
    p->state_dir = join3(x->state, "profiles", id);
    p->logs_dir = dxl_path_join(p->state_dir, "logs");
    char *cache = join3(x->cache, "profiles", id);
    p->cache_dir = dxl_path_join(cache, "Cache");
    free(cache);
    return 0;
}

void dxl_profile_free(dxl_profile *p) {
    if (!p) return;
    dxl_ini_free(p->ini);
    free(p->id); free(p->dir); free(p->ini_path); free(p->system_ini); free(p->user_ini);
    free(p->engine_dir); free(p->save_dir); free(p->shots_dir); free(p->state_dir);
    free(p->logs_dir); free(p->cache_dir);
    memset(p, 0, sizeof *p);
}

int dxl_profile_exists(const dxl_profile *p) {
    return dxl_path_exists(p->ini_path) && dxl_path_exists(p->system_ini);
}

int (*dxl_profile_link)(const char *target, const char *link) = dxl_fs_link_dir;

int dxl_profile_make_dirs(dxl_profile *p, dxl_err *err) {
    const char *dirs[] = { p->dir, p->engine_dir, p->save_dir, p->shots_dir, p->state_dir,
                           p->logs_dir, p->cache_dir };
    for (size_t i = 0; i < sizeof dirs / sizeof *dirs; i++) {
        if (dxl_path_mkdirs(dirs[i]) != 0) {
            dxl_err_set(err, "cannot create %s", dirs[i]);
            return -1;
        }
    }
    /* The engine writes its shots into its folder's Screenshots (--userdir):
     * a link to the profile's own. A filesystem with no links (exFAT, FAT, a
     * handheld's card) refuses it: that folder is then the profile's shots
     * folder, and the other, empty, goes. */
    char *link = dxl_path_join(p->engine_dir, "Screenshots");
    int rc = 0;
    if (!dxl_path_exists(link) && dxl_profile_link(p->shots_dir, link) != 0) {
        if (errno != EPERM && errno != EOPNOTSUPP && errno != ENOSYS) {
            dxl_err_set(err, "cannot link %s to %s: %s", link, p->shots_dir, strerror(errno));
            rc = -1;
        } else if (dxl_path_mkdirs(link) != 0) {
            dxl_err_set(err, "cannot create %s", link);
            rc = -1;
        } else {
            rmdir(p->shots_dir);
            free(p->shots_dir);
            p->shots_dir = link;
            link = NULL;
        }
    }
    free(link);
    return rc;
}

int dxl_profile_count_saves(const dxl_profile *p) {
    DIR *d = p->save_dir ? opendir(p->save_dir) : NULL;
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (dxl_strnicmp(e->d_name, "Save", 4) == 0 && e->d_name[4] >= '0' && e->d_name[4] <= '9') n++;
        else if (dxl_stricmp(e->d_name, "QuickSave") == 0) n++;
    }
    closedir(d);
    return n;
}

void dxl_profile_folders(const dxl_profile *p, char *out[4]) {
    out[0] = dxl_xstrdup(p->dir);
    out[1] = dxl_path_dirname(p->save_dir);
    out[2] = dxl_xstrdup(p->state_dir);
    out[3] = dxl_path_dirname(p->cache_dir);
}

static int by_id(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

size_t dxl_profiles_list(const dxl_xdg *x, char ids[][DXL_PROFILE_ID_MAX + 1], size_t max) {
    char *dir = dxl_path_join(x->config, "profiles");
    DIR *d = opendir(dir);
    size_t n = 0;
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (e->d_name[0] == '.' || !id_valid(e->d_name)) continue;
        dxl_profile p;
        if (dxl_profile_open(&p, x, e->d_name, NULL) != 0) continue;
        if (dxl_profile_exists(&p)) {
            if (n < max) snprintf(ids[n], DXL_PROFILE_ID_MAX + 1, "%.64s", e->d_name);
            n++;
        }
        dxl_profile_free(&p);
    }
    if (d) closedir(d);
    free(dir);
    qsort(ids, n < max ? n : max, sizeof ids[0], by_id);
    return n;
}

/* Whether any of the four folders of a profile of that id is there: what
 * a deleted one left would be the new one's. */
static int id_taken(const dxl_xdg *x, const char *id) {
    dxl_profile p;
    if (dxl_profile_open(&p, x, id, NULL) != 0) return 1;
    char *dirs[4];
    dxl_profile_folders(&p, dirs);
    int taken = 0;
    for (int i = 0; i < 4; i++) {
        taken |= dxl_path_exists(dirs[i]);
        free(dirs[i]);
    }
    dxl_profile_free(&p);
    return taken;
}

void dxl_profile_new_id(const dxl_xdg *x, const char *name, char *out, size_t size) {
    char base[DXL_PROFILE_ID_MAX - 3];
    size_t n = 0;
    int dash = 0;
    for (const unsigned char *c = (const unsigned char *)(name ? name : ""); *c && n + 1 < sizeof base; c++) {
        if (isalnum(*c) && *c < 0x80) {
            if (dash && n) base[n++] = '-';
            if (n + 1 < sizeof base) base[n++] = (char)tolower(*c);
            dash = 0;
        } else {
            dash = 1;
        }
    }
    base[n] = '\0';
    if (!n) snprintf(base, sizeof base, "profile");
    for (int k = 1; k < 1000; k++) {
        if (k == 1) snprintf(out, size, "%s", base);
        else snprintf(out, size, "%s-%d", base, k);
        if (!id_taken(x, out)) break;
    }
}

/* The engine folder's own files worth a copy: Settings.json and a class's
 * config files, not its log or crash reports. */
static int engine_file_kept(const char *name) {
    size_t n = strlen(name);
    return !dxl_stricmp(name, "Settings.json") || (n > 4 && !dxl_stricmp(name + n - 4, ".ini"));
}

int dxl_profile_copy(const dxl_profile *from, dxl_profile *to, dxl_err *err) {
    if (dxl_profile_exists(to)) {
        dxl_err_set(err, "profile %s is made already", to->id);
        return -1;
    }
    dxl_ini *sys = dxl_ini_load(from->system_ini, err), *usr = sys ? dxl_ini_load(from->user_ini, err) : NULL;
    int rc = -1;
    if (!sys || !usr) goto out;
    dxl_ini_set(sys, "Core.System", "SavePath", to->save_dir);
    dxl_ini_set(sys, "Core.System", "CachePath", to->cache_dir);
    if (dxl_profile_make_dirs(to, err) != 0) goto out;
    if (dxl_ini_save(sys, to->system_ini, err) != 0 || dxl_ini_save(usr, to->user_ini, err) != 0) goto out;
    DIR *d = opendir(from->engine_dir);
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (e->d_name[0] == '.' || !engine_file_kept(e->d_name)) continue;
        char *a = dxl_path_join(from->engine_dir, e->d_name), *b = dxl_path_join(to->engine_dir, e->d_name);
        int c = dxl_path_is_dir(a) ? 0 : dxl_path_copy(a, b, err);
        free(a);
        free(b);
        if (c != 0) {
            closedir(d);
            goto out;
        }
    }
    if (d) closedir(d);
    /* profile.ini last: what makes it a profile. */
    dxl_ini *meta = dxl_ini_load(from->ini_path, err);
    if (!meta) goto out;
    rc = dxl_ini_save(meta, to->ini_path, err);
    dxl_ini_free(meta);
out:
    dxl_ini_free(sys);
    dxl_ini_free(usr);
    return rc;
}

const char *dxl_profile_get(const dxl_profile *p, const char *key) {
    return dxl_ini_get(p->ini, DXL_PROFILE_SECTION, key);
}

void dxl_profile_set(dxl_profile *p, const char *key, const char *value) {
    dxl_ini_set(p->ini, DXL_PROFILE_SECTION, key, value);
}

int dxl_profile_save(dxl_profile *p, dxl_err *err) {
    if (dxl_path_mkdirs(p->dir) != 0) {
        dxl_err_set(err, "cannot create %s", p->dir);
        return -1;
    }
    return dxl_ini_save(p->ini, p->ini_path, err);
}

/* ---- the import ------------------------------------------------------ */

static void copy_str(char *dst, size_t size, const char *src) {
    snprintf(dst, size, "%s", src ? src : "");
}

/* The newer of two files that may be missing; NULL when both are. */
static const char *newer(const char *a, const char *b) {
    if (!a) return b;
    if (!b) return a;
    return dxl_path_mtime(b) > dxl_path_mtime(a) ? b : a;
}

/* The game's save folder: SavePath of the ini the profile is made from,
 * relative to System as the engine resolves it. */
static char *game_save_dir(const char *system_dir, const dxl_ini *sys_ini) {
    const char *v = dxl_ini_get(sys_ini, "Core.System", "SavePath");
    char *rel = dxl_path_from_ini(v && *v ? v : "../Save");
    char *dir;
    if (dxl_fs_is_absolute(rel)) {
        dir = dxl_xstrdup(rel);
    } else {
        char *joined = dxl_path_join(system_dir, rel);
        dir = dxl_fs_real(joined);
        if (!dir) dir = joined;
        else free(joined);
    }
    free(rel);
    return dir;
}

static int is_shot(const char *name) {
    size_t n = strlen(name);
    return n > 8 && dxl_strnicmp(name, "Shot", 4) == 0 && dxl_stricmp(name + n - 4, ".bmp") == 0;
}

/* The saves to carry over: every folder of the game's but Current, the
 * level the player is in, which the engine makes again from a save. */
static int each_save(const char *save_dir, const char *to, dxl_err *err) {
    DIR *d = opendir(save_dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.' || dxl_stricmp(e->d_name, "Current") == 0) continue;
        char *src = dxl_path_join(save_dir, e->d_name);
        if (dxl_path_is_dir(src)) {
            if (to) {
                char *dst = dxl_path_join(to, e->d_name);
                int rc = dxl_path_copy_dir(src, dst, NULL, err);
                free(dst);
                if (rc != 0) { free(src); closedir(d); return -1; }
            }
            n++;
        }
        free(src);
    }
    closedir(d);
    return n;
}

static int each_shot(const char *system_dir, const char *to, dxl_err *err) {
    DIR *d = opendir(system_dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!is_shot(e->d_name)) continue;
        if (to) {
            char *src = dxl_path_join(system_dir, e->d_name);
            char *dst = dxl_path_join(to, e->d_name);
            int rc = dxl_path_copy(src, dst, err);
            free(src);
            free(dst);
            if (rc != 0) { closedir(d); return -1; }
        }
        n++;
    }
    closedir(d);
    return n;
}

/* [Extension.InputExt]'s keys over [Engine.Input]'s, but its key 0 (the
 * original writes it StrProperty0), and its own lines then removed: the
 * bindings in one place, the one the engine reads. With fold 0 only
 * counted. Returns how many. */
static int fold_bindings(dxl_ini *usr, int fold) {
    enum { MAX = 600 };
    const char **k = dxl_xmalloc(sizeof *k * MAX), **v = dxl_xmalloc(sizeof *v * MAX);
    size_t n = dxl_ini_pairs(usr, "Extension.InputExt", k, v, MAX);
    char (*keys)[32] = dxl_xmalloc(sizeof *keys * MAX), (*values)[256] = dxl_xmalloc(sizeof *values * MAX);
    size_t m = 0;
    for (size_t i = 0; i < n && i < MAX; i++) {
        if (!dxl_stricmp(k[i], "StrProperty0")) continue;
        snprintf(keys[m], sizeof keys[m], "%s", k[i]);
        snprintf(values[m], sizeof values[m], "%s", v[i]);
        m++;
    }
    for (size_t i = 0; fold && i < m; i++) dxl_ini_set(usr, "Engine.Input", keys[i], values[i]);
    if (fold && n) dxl_ini_empty_section(usr, "Extension.InputExt");
    free(keys);
    free(values);
    free(k);
    free(v);
    return (int)m;
}

dxl_ini *dxl_profile_user_preview(const char *path) {
    dxl_ini *usr = path && *path ? dxl_ini_load(path, NULL) : NULL;
    if (usr) fold_bindings(usr, 1);
    return usr;
}

void dxl_profile_defaults(const char *game_system_dir, dxl_ini **system, dxl_ini **user) {
    char *def_ini = dxl_path_resolve_ci(game_system_dir, "Default.ini");
    char *def_user = dxl_path_resolve_ci(game_system_dir, "DefUser.ini");
    *system = def_ini ? dxl_ini_load(def_ini, NULL) : NULL;
    *user = def_user ? dxl_ini_load(def_user, NULL) : NULL;
    if (*system) {
        const char *master = dxl_ini_get(*system, "DeusEx.MenuScreenJoinGame", "MasterServerAddress");
        if (master && dxl_stricmp(master, DEAD_MASTER) == 0)
            dxl_ini_set(*system, "DeusEx.MenuScreenJoinGame", "MasterServerAddress", LIVE_MASTER);
        dxl_engine_sections_seed(*system);
    }
    free(def_ini);
    free(def_user);
}

/* A mod's own saves: the folder its ini's SavePath names when that is a
 * folder of the mod's (core/mods.h), not the mod's own or its System; NULL
 * for none. */
static char *mod_save_dir(const char *game_system_dir, const dxl_mod *m, const dxl_ini *mod_ini) {
    const char *v = mod_ini ? dxl_ini_get(mod_ini, "Core.System", "SavePath") : NULL;
    char *dir = v && *v ? dxl_mod_folder(v, game_system_dir, m) : NULL;
    if (dir && (!strcmp(dir, m->dir) || !strcmp(dir, m->system_dir))) {
        free(dir);
        dir = NULL;
    }
    return dir;
}

/* The import, of the game or with a mod. */
static int import_from(dxl_profile *p, const char *game_system_dir, const dxl_mod *mod, int dry, dxl_import *r,
                       dxl_err *err) {
    memset(r, 0, sizeof *r);
    int rc = -1;
    char *game_ini = dxl_path_resolve_ci(game_system_dir, "DeusEx.ini");
    char *def_ini = dxl_path_resolve_ci(game_system_dir, "Default.ini");
    char *dxe_ini = dxl_path_resolve_ci(game_system_dir, "DXE-DeusEx.ini");
    char *user = dxl_path_resolve_ci(game_system_dir, "User.ini");
    char *dxe_user = dxl_path_resolve_ci(game_system_dir, "DXE-User.ini");
    char *def_user = dxl_path_resolve_ci(game_system_dir, "DefUser.ini");
    dxl_ini *sys = NULL, *usr = NULL, *dxe = NULL;
    char *saves = NULL;

    /* DeusEx.ini: a mod's own, else the game's own, else Default.ini; one
     * with no Paths (a stub the engine cannot start from) is Default.ini
     * with its values. */
    const char *sys_from = mod && mod->ini ? mod->ini : game_ini ? game_ini : def_ini;
    sys = sys_from ? dxl_ini_load(sys_from, err) : NULL;
    if (!sys) {
        if (!sys_from) dxl_err_set(err, "no DeusEx.ini or Default.ini in %s", game_system_dir);
        goto out;
    }
    if (!dxl_ini_get(sys, "Core.System", "Paths") && def_ini && sys_from != def_ini) {
        dxl_ini *base = dxl_ini_load(def_ini, err);
        if (!base) goto out;
        dxl_ini_overlay(base, sys);
        dxl_ini_free(sys);
        sys = base;
    }
    copy_str(r->system_from, sizeof r->system_from, sys_from);

    /* The engine's own sections: from DXE-DeusEx.ini, the file the engine
     * writes without --ini, for each the game's ini lacks. */
    dxe = dxe_ini ? dxl_ini_load(dxe_ini, NULL) : NULL;
    for (size_t i = 0; dxe && i < sizeof engine_sections / sizeof *engine_sections; i++) {
        if (dxl_ini_has_section(sys, engine_sections[i]) || !dxl_ini_has_section(dxe, engine_sections[i]))
            continue;
        copy_str(r->engine_from, sizeof r->engine_from, dxe_ini);
        if (!dry) dxl_ini_copy_section(sys, dxe, engine_sections[i]);
    }
    /* Every key of them there before the first run (core/engine_sections.h). */
    if (!dry) dxl_engine_sections_seed(sys);

    /* User.ini: a mod's own, else the newer of the game's and the engine's
     * own, else DefUser.ini. */
    const char *user_from = mod && mod->user_ini ? mod->user_ini : newer(user, dxe_user);
    if (!user_from) user_from = def_user;
    usr = user_from ? dxl_ini_load(user_from, err) : NULL;
    if (!usr) {
        if (!user_from) dxl_err_set(err, "no User.ini or DefUser.ini in %s", game_system_dir);
        goto out;
    }
    copy_str(r->user_from, sizeof r->user_from, user_from);

    /* A mod's: its own saves, none of the game's, and its paths; the
     * game's shots are not its. */
    saves = mod ? mod_save_dir(game_system_dir, mod, mod->ini ? sys : NULL) : game_save_dir(game_system_dir, sys);
    copy_str(r->saves_from, sizeof r->saves_from, saves);
    r->saves = saves ? each_save(saves, NULL, NULL) : 0;
    r->shots = mod ? 0 : each_shot(game_system_dir, NULL, NULL);
    if (mod) dxl_mod_set_paths(sys, game_system_dir, mod);
    const char *master = dxl_ini_get(sys, "DeusEx.MenuScreenJoinGame", "MasterServerAddress");
    r->master_replaced = master && dxl_stricmp(master, DEAD_MASTER) == 0;
    /* The bindings the original game wrote: its input object is
     * Extension.InputExt, which it reads over [Engine.Input] and saves to
     * its own section -- the engine reads [Engine.Input] alone. */
    r->bindings_folded = fold_bindings(usr, 0);
    if (dry) {
        rc = 0;
        goto out;
    }

    fold_bindings(usr, 1);

    dxl_ini_set(sys, "Core.System", "SavePath", p->save_dir);
    dxl_ini_set(sys, "Core.System", "CachePath", p->cache_dir);
    if (r->master_replaced) dxl_ini_set(sys, "DeusEx.MenuScreenJoinGame", "MasterServerAddress", LIVE_MASTER);
    dxl_ini_set_eol(sys, "\n");
    dxl_ini_set_eol(usr, "\n");

    if (dxl_profile_make_dirs(p, err) != 0) goto out;
    if (dxl_ini_save(sys, p->system_ini, err) != 0) goto out;
    if (dxl_ini_save(usr, p->user_ini, err) != 0) goto out;
    if (saves && each_save(saves, p->save_dir, err) < 0) goto out;
    if (!mod && each_shot(game_system_dir, p->shots_dir, err) < 0) goto out;
    rc = 0;
out:
    dxl_ini_free(sys);
    dxl_ini_free(usr);
    dxl_ini_free(dxe);
    free(game_ini); free(def_ini); free(dxe_ini); free(user); free(dxe_user); free(def_user);
    free(saves);
    return rc;
}

int dxl_profile_import(dxl_profile *p, const char *game_system_dir, int dry, dxl_import *r,
                       dxl_err *err) {
    return import_from(p, game_system_dir, NULL, dry, r, err);
}

int dxl_profile_import_mod(dxl_profile *p, const char *game_system_dir, const dxl_mod *mod, int dry,
                           dxl_import *r, dxl_err *err) {
    return import_from(p, game_system_dir, mod, dry, r, err);
}

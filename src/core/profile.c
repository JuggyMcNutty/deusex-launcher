#define _GNU_SOURCE
#include "core/profile.h"
#include "core/engine_sections.h"
#include "core/paths.h"
#include "core/saves.h"
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
    p->ini = dxl_ini_load_utf8(p->ini_path, NULL);
    if (!p->ini) {
        p->ini = dxl_ini_new_utf8();
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

static int make_dir(const char *path, const char *id, dxl_made *m, dxl_err *err);
static void made_add(dxl_made *m, const char *path, char *old, size_t old_len, int had);
static int write_ini(const dxl_ini *ini, const char *path, dxl_made *m, dxl_err *err);

/* The folders, and the link, each made recorded in m (may be NULL). */
static int make_dirs(dxl_profile *p, dxl_made *m, dxl_err *err) {
    const char *dirs[] = { p->dir, p->engine_dir, p->save_dir, p->shots_dir, p->state_dir,
                           p->logs_dir, p->cache_dir };
    for (size_t i = 0; i < sizeof dirs / sizeof *dirs; i++)
        if (make_dir(dirs[i], p->id, m, err) != 0) return -1;
    /* The engine writes its shots into its folder's Screenshots (--userdir):
     * a link to the profile's own. A filesystem with no links (exFAT, FAT, a
     * handheld's card) refuses it: that folder is then the profile's shots
     * folder, and the other, empty, goes. */
    char *link = dxl_path_join(p->engine_dir, "Screenshots");
    int rc = 0;
    int there = dxl_path_exists(link);
    if (!there && dxl_profile_link(p->shots_dir, link) == 0) {
        made_add(m, link, NULL, 0, 0);
    } else if (!there) {
        if (errno != EPERM && errno != EOPNOTSUPP && errno != ENOSYS) {
            dxl_err_set(err, "cannot link %s to %s: %s", link, p->shots_dir, strerror(errno));
            rc = -1;
        } else if (make_dir(link, p->id, m, err) != 0) {
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

int dxl_profile_make_dirs(dxl_profile *p, dxl_err *err) { return make_dirs(p, NULL, err); }

int dxl_profile_count_saves(const dxl_profile *p) { return dxl_saves_count(p->save_dir); }

void dxl_profile_folders(const dxl_profile *p, char *out[4]) {
    out[0] = dxl_xstrdup(p->dir);
    out[1] = dxl_path_dirname(p->save_dir);
    out[2] = dxl_xstrdup(p->state_dir);
    out[3] = dxl_path_dirname(p->cache_dir);
}

size_t dxl_profiles_list(const dxl_xdg *x, char ids[][DXL_PROFILE_ID_MAX + 1], size_t max) {
    char *dir = dxl_path_join(x->config, "profiles");
    DIR *d = opendir(dir);
    size_t n = 0;
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (e->d_name[0] == '.' || !id_valid(e->d_name)) continue;
        dxl_profile p;
        if (dxl_profile_open(&p, x, e->d_name, NULL) != 0) continue;
        /* Kept in order as they are read, the first max by id: never which
         * the folder gives first. */
        if (dxl_profile_exists(&p)) {
            if (n < max || (max && strcmp(e->d_name, ids[max - 1]) < 0)) {
                size_t k = n < max ? n : max - 1;
                for (; k > 0 && strcmp(ids[k - 1], e->d_name) > 0; k--) memcpy(ids[k], ids[k - 1], sizeof ids[0]);
                snprintf(ids[k], DXL_PROFILE_ID_MAX + 1, "%.64s", e->d_name);
            }
            n++;
        }
        dxl_profile_free(&p);
    }
    if (d) closedir(d);
    free(dir);
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

int dxl_profile_new_id(const dxl_xdg *x, const char *name, char *out, size_t size) {
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
        if (!id_taken(x, out)) return 0;
    }
    return -1;
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
    /* What the copy makes, taken away again where it fails half-way, as an
     * import's is: no folder or file of a profile that is none is left
     * (bugs.md LCORE.44). */
    dxl_made made = { 0 };
    if (!sys || !usr) goto out;
    dxl_ini_set(sys, "Core.System", "SavePath", to->save_dir);
    dxl_ini_set(sys, "Core.System", "CachePath", to->cache_dir);
    if (make_dirs(to, &made, err) != 0) goto out;
    if (write_ini(sys, to->system_ini, &made, err) != 0 || write_ini(usr, to->user_ini, &made, err) != 0) goto out;
    DIR *d = opendir(from->engine_dir);
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (e->d_name[0] == '.' || !engine_file_kept(e->d_name)) continue;
        char *a = dxl_path_join(from->engine_dir, e->d_name), *b = dxl_path_join(to->engine_dir, e->d_name);
        int c = 0;
        if (!dxl_path_is_dir(a)) {
            int was = dxl_path_exists(b);
            c = dxl_path_copy(a, b, err);
            if (c == 0 && !was) made_add(&made, b, NULL, 0, 0);
        }
        free(a);
        free(b);
        if (c != 0) {
            closedir(d);
            goto out;
        }
    }
    if (d) closedir(d);
    /* profile.ini last: what makes it a profile. */
    dxl_ini *meta = dxl_ini_load_utf8(from->ini_path, err);
    if (!meta) goto out;
    rc = write_ini(meta, to->ini_path, &made, err);
    dxl_ini_free(meta);
out:
    if (rc != 0) dxl_profile_unmake(&made);
    else dxl_made_free(&made);
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

/* value with from, the whole of it or a folder it begins with, made to;
 * NULL where it does not begin so. */
static char *rebased(const char *value, const char *from, const char *to) {
    size_t n = strlen(from);
    if (!value || !n || strncmp(value, from, n) != 0 || (value[n] && value[n] != '/' && value[n] != '\\'))
        return NULL;
    size_t size = strlen(to) + strlen(value + n) + 1;
    char *out = dxl_xmalloc(size);
    snprintf(out, size, "%s%s", to, value + n);
    return out;
}

/* The folder value was made under, where it ends as now does after root
 * (a profile's Save after the data folder) and that folder is not root;
 * else NULL. */
static char *moved_root(const char *value, const char *now, const char *root) {
    size_t r = strlen(root);
    if (!value || strncmp(now, root, r) != 0) return NULL;
    const char *tail = now + r;
    size_t v = strlen(value), t = strlen(tail);
    if (v <= t || strcmp(value + v - t, tail) != 0 || (v - t == r && !strncmp(value, root, r))) return NULL;
    char *old = dxl_xmalloc(v - t + 1);
    memcpy(old, value, v - t);
    old[v - t] = '\0';
    return old;
}

int dxl_profile_rehome(dxl_profile *p, const dxl_xdg *x, dxl_err *err) {
    dxl_ini *sys = dxl_ini_load(p->system_ini, NULL);
    if (!sys) return 0;
    char *data = moved_root(dxl_ini_get(sys, "Core.System", "SavePath"), p->save_dir, x->data);
    char *cache = moved_root(dxl_ini_get(sys, "Core.System", "CachePath"), p->cache_dir, x->cache);
    int n = 0, rc = 0;
    if (data) {
        char *mod = rebased(dxl_profile_get(p, "Mod"), data, x->data);
        if (mod) {
            dxl_profile_set(p, "Mod", mod);
            free(mod);
            n++;
            if (dxl_profile_save(p, err) != 0) {
                rc = -1;
                goto out;
            }
        }
        size_t k = dxl_ini_get_all(sys, "Core.System", "Paths", NULL, 0), moved = 0;
        const char **values = dxl_xmalloc((k + 1) * sizeof *values);
        dxl_ini_get_all(sys, "Core.System", "Paths", values, k);
        char **now = dxl_xmalloc((k + 1) * sizeof *now);
        for (size_t i = 0; i < k; i++) {
            now[i] = rebased(values[i], data, x->data);
            if (now[i]) moved++;
            else now[i] = dxl_xstrdup(values[i]);
        }
        free(values);
        if (moved) {
            dxl_ini_remove(sys, "Core.System", "Paths");
            for (size_t i = 0; i < k; i++) dxl_ini_append(sys, "Core.System", "Paths", now[i]);
        }
        for (size_t i = 0; i < k; i++) free(now[i]);
        free(now);
        dxl_ini_set(sys, "Core.System", "SavePath", p->save_dir);
        n += (int)moved + 1;
    }
    if (cache) {
        dxl_ini_set(sys, "Core.System", "CachePath", p->cache_dir);
        n++;
    }
    if (n && dxl_ini_save(sys, p->system_ini, err) != 0) rc = -1;
out:
    free(data);
    free(cache);
    dxl_ini_free(sys);
    return rc ? -1 : n;
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

/* rel against dir as the engine resolves a SavePath: "." and ".." by the
 * text, then each name in any case (DXEngine's Package/PackageManager.cpp,
 * ScanPaths). Caller frees. */
static char *resolve_any_case(const char *dir, const char *rel) {
    char *cur = dxl_xstrdup(dir), *copy = dxl_xstrdup(rel);
    for (char *save = NULL, *part = strtok_r(copy, "/", &save); part; part = strtok_r(NULL, "/", &save)) {
        if (!strcmp(part, ".")) continue;
        char *next = !strcmp(part, "..") ? dxl_path_dirname(cur) : dxl_path_resolve_ci(cur, part);
        if (!next) next = dxl_path_join(cur, part);
        free(cur);
        cur = next;
    }
    free(copy);
    return cur;
}

/* The game's save folder: SavePath of the ini the profile is made from,
 * relative to System as the engine resolves it, in any case. */
static char *game_save_dir(const char *system_dir, const dxl_ini *sys_ini) {
    const char *v = dxl_ini_get(sys_ini, "Core.System", "SavePath");
    char *rel = dxl_path_from_ini(v && *v ? v : "../Save");
    char *dir;
    if (dxl_fs_is_absolute(rel)) {
        dir = dxl_xstrdup(rel);
    } else {
        char *joined = dxl_path_join(system_dir, rel);
        dir = dxl_fs_real(joined);
        if (!dir) {
            free(joined);
            joined = resolve_any_case(system_dir, rel);
            dir = dxl_fs_real(joined);
        }
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

/* ---- what an import made ------------------------------------------- */

struct dxl_made_one {
    char  *path;
    char  *old;        /* a file there before: its bytes, put back */
    size_t old_len;
    int    had;
};

static void made_add(dxl_made *m, const char *path, char *old, size_t old_len, int had) {
    if (!m) {
        free(old);
        return;
    }
    if (m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 16;
        m->items = dxl_xrealloc(m->items, m->cap * sizeof *m->items);
    }
    m->items[m->count++] = (struct dxl_made_one){ dxl_xstrdup(path), old, old_len, had };
}

void dxl_made_free(dxl_made *m) {
    if (!m) return;
    for (size_t i = 0; i < m->count; i++) {
        free(m->items[i].path);
        free(m->items[i].old);
    }
    free(m->items);
    memset(m, 0, sizeof *m);
}

void dxl_profile_unmake(dxl_made *m) {
    if (!m) return;
    /* As far as it goes: what cannot be taken away stays, the rest goes. */
    for (size_t i = m->count; i-- > 0;) {
        struct dxl_made_one *o = &m->items[i];
        if (o->had) dxl_path_write_atomic(o->path, o->old ? o->old : "", o->old_len, NULL);
        else dxl_fs_remove_tree(o->path);
    }
    dxl_made_free(m);
}

/* path made a folder, and its parents: the first of them that was not
 * there recorded, all made under it this import's -- but none above the
 * profile's own folder, the one its id names: the launcher's folders over
 * it, made here on a first run, stay, whatever else has come to lie in them
 * by the time an import is taken away. */
static int make_dir(const char *path, const char *id, dxl_made *m, dxl_err *err) {
    char *top = NULL, *at = dxl_xstrdup(path);
    /* A link that leads nowhere, to a drive not mounted, is there: the
     * player's own, never one of the folders made here, so never taken
     * away with them. */
    while (!dxl_path_exists(at) && !dxl_fs_is_link(at)) {
        char *up = dxl_path_dirname(at);
        free(top);
        top = at;
        at = up;
        if (!strcmp(at, top) || !strcmp(dxl_path_basename(top), id)) break;
    }
    free(at);
    int rc = dxl_path_mkdirs(path);
    if (rc != 0) {
        dxl_err_set(err, "cannot create %s: %s", path, strerror(errno));
        if (top) dxl_fs_remove_tree(top);
    } else if (top) {
        made_add(m, top, NULL, 0, 0);
    }
    free(top);
    return rc;
}

/* A file's bytes; NULL when it cannot be read. */
static char *read_whole(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    dxl_buf b;
    dxl_buf_init(&b);
    char chunk[16384];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) dxl_buf_add(&b, chunk, n);
    int bad = ferror(f);
    fclose(f);
    if (bad) {
        dxl_buf_free(&b);
        return NULL;
    }
    *len = b.len;
    return b.data ? b.data : dxl_xstrdup("");
}

/* An ini written, recorded: one there before kept to be put back. */
static int write_ini(const dxl_ini *ini, const char *path, dxl_made *m, dxl_err *err) {
    size_t len = 0;
    int had = dxl_path_exists(path);
    /* A link to a file not there yet is written through, the file made
     * where it leads (dxl_path_write_atomic): what this import made is that
     * file, the link the player's own. */
    int linked = !had && dxl_fs_is_link(path);
    char *old = had ? read_whole(path, &len) : NULL;
    if (had && !old) {
        dxl_err_set(err, "cannot read %s: %s", path, strerror(errno));
        return -1;
    }
    if (dxl_ini_save(ini, path, err) != 0) {
        free(old);
        return -1;
    }
    if (linked) {
        char *real = dxl_fs_real(path);
        if (real) made_add(m, real, NULL, 0, 0);
        free(real);
        return 0;
    }
    made_add(m, path, old, len, had);
    return 0;
}

/* ---- the copy ------------------------------------------------------- */

/* The temporary name a save folder or a shot is copied under, beside its
 * own: a dot name, which the Saves page and the engine pass over (a save is
 * Save%04d or QuickSave to the one, a folder named Save* to the other), and
 * Screenshots too. */
#define COPYING ".dxl-copy-"

typedef struct {
    dxl_made           *made;
    dxl_import_progress progress;
    void               *ctx;
    int                 files, files_total;
    long long           bytes, bytes_total;
} copy_job;

/* Each file copied: the progress told. */
static int copied_one(void *ctx, long long size) {
    copy_job *j = ctx;
    j->files++;
    if (size > 0) j->bytes += size;
    return j->progress ? j->progress(j->ctx, j->files, j->files_total, j->bytes, j->bytes_total) : 0;
}

/* The files and bytes of a file, or of a folder as dxl_path_copy_dir copies
 * it: its linked folders not descended. */
static void tally(const char *path, int *files, long long *bytes) {
    if (!dxl_path_is_dir(path)) {
        long long n = dxl_path_size(path);
        (*files)++;
        if (n > 0) *bytes += n;
        return;
    }
    if (dxl_fs_is_link(path)) return;
    DIR *d = opendir(path);
    struct dirent *e;
    while (d && (e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char *in = dxl_path_join(path, e->d_name);
        tally(in, files, bytes);
        free(in);
    }
    if (d) closedir(d);
}

/* What an import cut short left in dir: its temporary names taken away. */
static void sweep(const char *dir) {
    DIR *d = opendir(dir);
    struct dirent *e;
    while (d && (e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, COPYING, strlen(COPYING)) != 0) continue;
        char *left = dxl_path_join(dir, e->d_name);
        dxl_fs_remove_tree(left);
        free(left);
    }
    if (d) closedir(d);
}

/* src, a save folder or a shot, copied to dst whole or not at all: under
 * its temporary name, put in its place once whole, then recorded. A shot
 * keeps its time: taken when the game wrote it, as Screenshots dates and
 * orders a shot. */
static int copy_whole(const char *src, const char *dst, copy_job *j, dxl_err *err) {
    char *dir = dxl_path_dirname(dst);
    size_t n = strlen(COPYING) + strlen(dxl_path_basename(dst)) + 1;
    char *leaf = dxl_xmalloc(n);
    snprintf(leaf, n, COPYING "%s", dxl_path_basename(dst));
    char *tmp = dxl_path_join(dir, leaf);
    int folder = dxl_path_is_dir(src);
    int rc = folder ? dxl_path_copy_dir(src, tmp, copied_one, j, err) : dxl_path_copy(src, tmp, err);
    if (rc == 0 && !folder) {
        long when = dxl_path_mtime(src);
        if (when >= 0) dxl_fs_set_mtime(tmp, when);
        if (copied_one(j, dxl_path_size(tmp)) != 0) {
            dxl_err_set(err, "the copy was cancelled");
            rc = -1;
        }
    }
    /* rename, not dxl_fs_replace: dst is not there, and a folder is
     * renamed so on Windows too. */
    if (rc == 0 && rename(tmp, dst) != 0) {
        dxl_err_set(err, "cannot rename %s to %s: %s", tmp, dst, strerror(errno));
        rc = -1;
    }
    if (rc == 0) made_add(j->made, dst, NULL, 0, 0);
    else dxl_fs_remove_tree(tmp);
    free(tmp);
    free(leaf);
    free(dir);
    return rc;
}

/* The saves to carry over: every folder of the game's but Current, the
 * level the player is in, which the engine makes again from a save. A
 * save the profile has already in to is its own, the later: never copied
 * into, its files mixed with the game's, but counted into kept. With copy
 * 0 those to copy are only counted, their files and bytes into j's
 * totals. Returns how many are copied, or would be; into saves, how many
 * of them are saves the Saves page lists (core/saves.h), into kept, of
 * those it has. */
static int each_save(const char *save_dir, const char *to, int copy, copy_job *j, int *saves, int *kept,
                     dxl_err *err) {
    DIR *d = opendir(save_dir);
    if (!d) return 0;
    int n = 0, rc = 0;
    struct dirent *e;
    while (rc == 0 && (e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.' || dxl_stricmp(e->d_name, "Current") == 0) continue;
        char *src = dxl_path_join(save_dir, e->d_name);
        if (dxl_path_is_dir(src) && !dxl_fs_is_link(src)) {   /* a linked folder is no save's */
            char *dst = dxl_path_join(to, e->d_name);
            int slot, listed = dxl_save_slot_of(e->d_name, &slot);
            if (dxl_path_exists(dst)) {
                if (kept && listed) (*kept)++;
            } else {
                if (copy) rc = copy_whole(src, dst, j, err);
                else tally(src, &j->files_total, &j->bytes_total);
                n++;
                if (saves && listed) (*saves)++;
            }
            free(dst);
        }
        free(src);
    }
    closedir(d);
    return rc ? -1 : n;
}

/* The shots, System/Shot*.bmp, likewise; with to NULL only counted. */
static int each_shot(const char *system_dir, const char *to, int copy, copy_job *j, int *kept, dxl_err *err) {
    DIR *d = opendir(system_dir);
    if (!d) return 0;
    int n = 0, rc = 0;
    struct dirent *e;
    while (rc == 0 && (e = readdir(d)) != NULL) {
        if (!is_shot(e->d_name)) continue;
        if (to) {
            char *src = dxl_path_join(system_dir, e->d_name);
            char *dst = dxl_path_join(to, e->d_name);
            int there = dxl_path_exists(dst);   /* the profile's own of that name: kept */
            if (there && kept) (*kept)++;
            else if (!there && copy) rc = copy_whole(src, dst, j, err);
            else if (!there) tally(src, &j->files_total, &j->bytes_total);
            free(src);
            free(dst);
            if (there) continue;
        }
        n++;
    }
    closedir(d);
    return rc ? -1 : n;
}

/* [Extension.InputExt]'s keys over [Engine.Input]'s, but its key 0 (the
 * original writes it StrProperty0), and its own lines then removed: the
 * bindings in one place, the one the engine reads. With fold 0 only
 * counted. Returns how many. */
static int fold_bindings(dxl_ini *usr, int fold) {
    enum { MAX = 600 };
    const char **k = dxl_xmalloc(sizeof *k * MAX), **v = dxl_xmalloc(sizeof *v * MAX);
    size_t n = dxl_ini_pairs(usr, "Extension.InputExt", k, v, MAX);
    /* Copies, whole: the section's own go as it is emptied. */
    char **keys = dxl_xmalloc(sizeof *keys * MAX), **values = dxl_xmalloc(sizeof *values * MAX);
    size_t m = 0;
    for (size_t i = 0; i < n && i < MAX; i++) {
        if (!dxl_stricmp(k[i], "StrProperty0")) continue;
        keys[m] = dxl_xstrdup(k[i]);
        values[m] = dxl_xstrdup(v[i]);
        m++;
    }
    for (size_t i = 0; fold && i < m; i++) dxl_ini_set(usr, "Engine.Input", keys[i], values[i]);
    if (fold && n) dxl_ini_empty_section(usr, "Extension.InputExt");
    for (size_t i = 0; i < m; i++) {
        free(keys[i]);
        free(values[i]);
    }
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

void dxl_profile_defaults(const char *game_system_dir, const dxl_mod *mod, dxl_ini **system, dxl_ini **user) {
    char *def_ini = dxl_path_resolve_ci(game_system_dir, "Default.ini");
    char *def_user = dxl_path_resolve_ci(game_system_dir, "DefUser.ini");
    /* A mod's: its own settings and controls, as its import takes them, one
     * with no Paths over Default.ini; the game's where it has none. */
    const char *sys_from = mod && mod->ini ? mod->ini : def_ini;
    const char *user_from = mod && mod->user_ini ? mod->user_ini : def_user;
    *system = sys_from ? dxl_ini_load(sys_from, NULL) : NULL;
    *user = user_from ? dxl_ini_load(user_from, NULL) : NULL;
    if (*system && def_ini && sys_from != def_ini && !dxl_ini_get(*system, "Core.System", "Paths")) {
        dxl_ini *base = dxl_ini_load(def_ini, NULL);
        if (base) {
            dxl_ini_overlay(base, *system);
            dxl_ini_free(*system);
            *system = base;
        }
    }
    if (*user) fold_bindings(*user, 1);
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
    /* A mod that runs from its own System has its SavePath from there. */
    char *dir = v && *v ? dxl_mod_folder(v, m->own_system ? m->system_dir : game_system_dir, m) : NULL;
    if (dir && (!strcmp(dir, m->dir) || !strcmp(dir, m->system_dir))) {
        free(dir);
        dir = NULL;
    }
    return dir;
}

/* The import, of the game or with a mod; progress and made as
 * dxl_profile_import_watched has them. */
static int import_from(dxl_profile *p, const char *game_system_dir, const dxl_mod *mod, int dry,
                       dxl_import_progress progress, void *ctx, dxl_made *made, dxl_import *r, dxl_err *err) {
    if (made) memset(made, 0, sizeof *made);
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
    r->saves = 0;
    r->shots = mod ? 0 : each_shot(game_system_dir, NULL, 0, NULL, NULL, NULL);
    /* What the copy will be, counted before it begins: its progress's
     * totals, and the plan's size. */
    dxl_made own = { 0 };
    copy_job j = { made ? made : &own, progress, ctx, 0, 0, 0, 0 };
    if (saves) each_save(saves, p->save_dir, 0, &j, &r->saves, NULL, NULL);
    if (!mod) each_shot(game_system_dir, p->shots_dir, 0, &j, NULL, NULL);
    r->files = j.files_total;
    r->bytes = j.bytes_total;
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

    if (make_dirs(p, j.made, err) != 0) goto undo;
    sweep(p->save_dir);
    sweep(p->shots_dir);
    if (write_ini(sys, p->system_ini, j.made, err) != 0) goto undo;
    if (write_ini(usr, p->user_ini, j.made, err) != 0) goto undo;
    r->saves = 0;
    if (saves && each_save(saves, p->save_dir, 1, &j, &r->saves, &r->saves_kept, err) < 0) goto undo;
    if (!mod && each_shot(game_system_dir, p->shots_dir, 1, &j, &r->shots_kept, err) < 0) goto undo;
    /* On the disk before profile.ini makes it a profile: the copy synced
     * once for its file system, not file by file (the inis are, as they
     * are written). */
    const char *copied_to[2] = { p->save_dir, p->shots_dir };
    for (int i = 0; i < 2 && j.files; i++) {
        if (dxl_fs_sync_all(copied_to[i]) != 0) {
            dxl_err_set(err, "cannot sync %s: %s", copied_to[i], strerror(errno));
            goto undo;
        }
    }
    r->shots -= r->shots_kept < r->shots ? r->shots_kept : r->shots;
    rc = 0;
    dxl_made_free(&own);
    goto out;
undo:
    dxl_profile_unmake(j.made);
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
    return import_from(p, game_system_dir, NULL, dry, NULL, NULL, NULL, r, err);
}

int dxl_profile_import_mod(dxl_profile *p, const char *game_system_dir, const dxl_mod *mod, int dry,
                           dxl_import *r, dxl_err *err) {
    return import_from(p, game_system_dir, mod, dry, NULL, NULL, NULL, r, err);
}

int dxl_profile_import_watched(dxl_profile *p, const char *game_system_dir, const dxl_mod *mod,
                               dxl_import_progress progress, void *ctx, dxl_made *made, dxl_import *r,
                               dxl_err *err) {
    return import_from(p, game_system_dir, mod, 0, progress, ctx, made, r, err);
}

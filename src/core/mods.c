#define _GNU_SOURCE
#include "core/mods.h"
#include "core/paths.h"
#include "platform/fs.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The name's extension is ext ("u", "ini"), in any case. */
static int has_ext(const char *name, const char *ext) {
    const char *dot = strrchr(name, '.');
    return dot && dot != name && dxl_stricmp(dot + 1, ext) == 0;
}

static int count_ext(const char *dir, const char *ext) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.' || !has_ext(e->d_name, ext)) continue;
        char *path = dxl_path_join(dir, e->d_name);
        if (!dxl_path_is_dir(path)) n++;
        free(path);
    }
    closedir(d);
    return n;
}

static int by_name_ci(const void *a, const void *b) {
    return dxl_stricmp(*(char *const *)a, *(char *const *)b);
}

/* The names in dir, in name order, any case; NULL-ended, caller frees each
 * and it. */
static char **names_in(const char *dir) {
    size_t n = 0, cap = 16;
    char **v = dxl_xmalloc(cap * sizeof *v);
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (e->d_name[0] == '.') continue;
            if (n + 1 >= cap) v = dxl_xrealloc(v, (cap *= 2) * sizeof *v);
            v[n++] = dxl_xstrdup(e->d_name);
        }
        closedir(d);
    }
    qsort(v, n, sizeof *v, by_name_ci);
    v[n] = NULL;
    return v;
}

static void free_names(char **v) {
    for (size_t i = 0; v && v[i]; i++) free(v[i]);
    free(v);
}

/* A System ini with [Core.System] Paths: the settings of a game or mod. */
static int is_system_ini(const char *path) {
    dxl_ini *ini = dxl_ini_load(path, NULL);
    int yes = ini && dxl_ini_get(ini, "Core.System", "Paths") != NULL;
    dxl_ini_free(ini);
    return yes;
}

/* The ini's stem is name, in any case. */
static int stem_is(const char *ini, size_t stem, const char *name) {
    return name && strlen(name) == stem && dxl_strnicmp(ini, name, stem) == 0;
}

/* The best settings among a folder's inis, by rank: 4 named after the mod's
 * folder (its link's name or its own), 3 with its own <stem>User.ini beside
 * it, 2 another, 1 Default.ini; 0, *best NULL, for none. */
static int rank_inis(const dxl_mod *m, const char *dir, const char *target, char **best) {
    char **names = names_in(dir);
    int rank = 0;
    *best = NULL;
    for (size_t i = 0; names[i]; i++) {
        if (!has_ext(names[i], "ini")) continue;
        size_t stem = strlen(names[i]) - 4;
        int r;
        if (stem_is(names[i], stem, m->name) || stem_is(names[i], stem, target)) {
            r = 4;
        } else if (dxl_stricmp(names[i], "Default.ini") == 0) {
            r = 1;
        } else {
            char user[300];
            snprintf(user, sizeof user, "%.*sUser.ini", (int)stem, names[i]);
            char *beside = dxl_path_resolve_ci(dir, user);
            r = beside ? 3 : 2;
            free(beside);
        }
        if (r <= rank) continue;
        char *path = dxl_path_join(dir, names[i]);
        if (is_system_ini(path)) {
            free(*best);
            *best = path;
            rank = r;
        } else {
            free(path);
        }
    }
    free_names(names);
    return rank;
}

/* The controls beside an ini of settings: its <stem>User.ini; for a
 * launcher's <name>Default.ini (Launch's: Revision's, Transcended's),
 * <name>User.ini, else <name>DefUser.ini; for DeusEx.ini, User.ini; else
 * DefUser.ini. NULL for none. */
static char *user_ini_for(const char *ini) {
    char *dir = dxl_path_dirname(ini);
    const char *name = dxl_path_basename(ini);
    size_t stem = strlen(name) - 4;
    char user[300];
    char *found = NULL;
    if (dxl_stricmp(name, "Default.ini") != 0) {
        snprintf(user, sizeof user, "%.*sUser.ini", (int)stem, name);
        found = dxl_path_resolve_ci(dir, user);
    }
    if (!found && stem > 7 && dxl_strnicmp(name + stem - 7, "Default", 7) == 0) {
        snprintf(user, sizeof user, "%.*sUser.ini", (int)(stem - 7), name);
        found = dxl_path_resolve_ci(dir, user);
        if (!found) {
            snprintf(user, sizeof user, "%.*sDefUser.ini", (int)(stem - 7), name);
            found = dxl_path_resolve_ci(dir, user);
        }
    }
    if (!found && dxl_stricmp(name, "DeusEx.ini") == 0) found = dxl_path_resolve_ci(dir, "User.ini");
    if (!found) found = dxl_path_resolve_ci(dir, "DefUser.ini");
    free(dir);
    return found;
}

/* The mod's settings, and its controls beside them: in its System, else in
 * a System folder of one of its folders, as GMDX's GMDXv9\System holds the
 * ini its shortcut names (INI=). */
static void own_inis(dxl_mod *m) {
    char *real = dxl_fs_real(m->dir);
    const char *target = real ? dxl_path_basename(real) : NULL;
    char *best = NULL;
    int rank = rank_inis(m, m->system_dir, target, &best);
    if (!rank) {
        char **subs = names_in(m->dir);
        for (size_t i = 0; subs[i]; i++) {
            char *sub = dxl_path_join(m->dir, subs[i]);
            char *sys = dxl_path_is_dir(sub) ? dxl_path_resolve_ci(sub, "System") : NULL;
            if (sys && dxl_path_is_dir(sys) && strcmp(sys, m->system_dir) != 0) {
                char *found = NULL;
                int r = rank_inis(m, sys, target, &found);
                if (r > rank) {
                    free(best);
                    best = found;
                    rank = r;
                } else {
                    free(found);
                }
            }
            free(sys);
            free(sub);
        }
        free_names(subs);
    }
    if (best) {
        m->ini = best;
        m->user_ini = user_ini_for(best);
    }
    free(real);
}

char *dxl_mod_own_system(const char *dir) {
    char *system = dxl_path_resolve_ci(dir, "System");
    char *core = system ? dxl_path_resolve_ci(system, "Core.dll") : NULL;
    if (!core) {
        free(system);
        system = NULL;
    }
    free(core);
    return system;
}

/* dir made absolute, by the text: the working folder before a relative
 * one, "." and ".." taken out, no '/' at its end but the root's; a link in
 * it kept, the name the player gave the mod. Caller frees. */
static char *absolute(const char *dir) {
    char *cwd = dxl_fs_is_absolute(dir) ? NULL : dxl_fs_cwd();
    char *joined = cwd ? dxl_path_join(cwd, dir) : dxl_xstrdup(dir);
    free(cwd);
    /* The root as it is ("/", "C:/"), the names after it one by one. */
    size_t root = dxl_fs_root_len(joined);
    if (root && joined[root - 1] == '/') root--;
    char *out = dxl_xmalloc(strlen(joined) + 2);
    memcpy(out, joined, root);
    out[root] = '\0';
    size_t len = root;
    for (char *save = NULL, *part = strtok_r(joined + root, "/", &save); part; part = strtok_r(NULL, "/", &save)) {
        if (!strcmp(part, ".")) continue;
        if (!strcmp(part, "..")) {
            char *up = strrchr(out + root, '/');
            if (up) *up = '\0';
            len = strlen(out);
            continue;
        }
        out[len++] = '/';
        strcpy(out + len, part);
        len += strlen(part);
    }
    if (len == root) strcpy(out + root, "/");
    free(joined);
    return out;
}

int dxl_mod_check(const char *dir, dxl_mod *m, dxl_err *err) {
    memset(m, 0, sizeof *m);
    char *real = absolute(dir);
    if (!dxl_path_is_dir(real)) {
        dxl_err_set(err, "%s is not a folder", dir);
        free(real);
        return -1;
    }
    char *system = dxl_path_resolve_ci(real, "System");
    if (!system || !dxl_path_is_dir(system)) {
        dxl_err_set(err, "%s has no System folder of its own", real);
        free(system);
        free(real);
        return -1;
    }
    char *exe = dxl_path_resolve_ci(system, "DeusEx.exe");
    if (exe) {
        dxl_err_set(err, "%s is a copy of the game, not a mod", real);
        free(exe);
        free(system);
        free(real);
        return -1;
    }
    int packages = count_ext(system, "u");
    if (!packages) {
        dxl_err_set(err, "%s has no packages in its System folder", real);
        free(system);
        free(real);
        return -1;
    }
    m->dir = real;
    m->name = dxl_xstrdup(dxl_path_basename(real));
    m->system_dir = system;
    m->packages = packages;
    m->where = DXL_MOD_ELSEWHERE;
    char *own = dxl_mod_own_system(real);
    m->own_system = own != NULL;
    free(own);
    own_inis(m);
    return 0;
}

void dxl_mod_free(dxl_mod *m) {
    if (!m) return;
    free(m->dir); free(m->name); free(m->system_dir); free(m->ini); free(m->user_ini);
    memset(m, 0, sizeof *m);
}

const char *dxl_mod_where_text(dxl_mod_where where) {
    switch (where) {
    case DXL_MOD_GAME:     return "in the game's folder";
    case DXL_MOD_LAUNCHER: return "in the launcher's mods folder";
    default:               return "elsewhere";
    }
}

void dxl_mod_copy(dxl_mod *to, const dxl_mod *from) {
    to->dir = dxl_xstrdup(from->dir);
    to->name = dxl_xstrdup(from->name);
    to->system_dir = dxl_xstrdup(from->system_dir);
    to->ini = from->ini ? dxl_xstrdup(from->ini) : NULL;
    to->user_ini = from->user_ini ? dxl_xstrdup(from->user_ini) : NULL;
    to->packages = from->packages;
    to->own_system = from->own_system;
    to->where = from->where;
}

int dxl_mod_list_add(dxl_mod_list *l, const dxl_mod *m) {
    for (int i = 0; i < l->count; i++)
        if (strcmp(l->items[i].dir, m->dir) == 0) return -1;
    if (l->count >= DXL_MODS_MAX) return -1;
    dxl_mod_copy(&l->items[l->count++], m);
    return 0;
}

static void find_in(dxl_mod_list *l, const char *dir, dxl_mod_where where) {
    if (!dir) return;
    char **names = names_in(dir);
    for (size_t i = 0; names[i]; i++) {
        char *path = dxl_path_join(dir, names[i]);
        dxl_mod m;
        if (dxl_path_is_dir(path) && dxl_mod_check(path, &m, NULL) == 0) {
            m.where = where;
            dxl_mod_list_add(l, &m);
            dxl_mod_free(&m);
        }
        free(path);
    }
    free_names(names);
}

void dxl_mods_find(dxl_mod_list *l, const char *game_root, const char *mods_dir) {
    memset(l, 0, sizeof *l);
    find_in(l, game_root, DXL_MOD_GAME);
    find_in(l, mods_dir, DXL_MOD_LAUNCHER);
}

void dxl_mod_list_free(dxl_mod_list *l) {
    for (int i = 0; i < l->count; i++) dxl_mod_free(&l->items[i]);
    l->count = 0;
}

/* rel against base as the engine takes a folder of the paths, in any case:
 * each name the one there of it, whatever its case, else as it is; "." and
 * ".." taken out. An absolute rel is itself. Caller frees. */
static char *resolve(const char *base, const char *rel) {
    char *cur = dxl_fs_is_absolute(rel) ? dxl_xstrndup(rel, dxl_fs_root_len(rel)) : dxl_xstrdup(base);
    rel += dxl_fs_root_len(rel);
    char *copy = dxl_xstrdup(rel);
    for (char *save = NULL, *part = strtok_r(copy, "/", &save); part; part = strtok_r(NULL, "/", &save)) {
        if (!strcmp(part, ".")) continue;
        if (!strcmp(part, "..")) {
            char *up = dxl_path_dirname(cur);
            free(cur);
            cur = up;
            continue;
        }
        char *next = dxl_path_resolve_ci(cur, part);
        if (!next) next = dxl_path_join(cur, part);
        free(cur);
        cur = next;
    }
    free(copy);
    return cur;
}

/* path is dir or in it. */
static int inside(const char *path, const char *dir) {
    size_t n = strlen(dir);
    return strncmp(path, dir, n) == 0 && (path[n] == '\0' || path[n] == '/');
}

/* A path's last part is a pattern: it has a wildcard. */
static int is_pattern(const char *part) {
    return strpbrk(part, "*?") != NULL;
}

char *dxl_mod_folder(const char *value, const char *game_system_dir, const dxl_mod *m) {
    char *v = dxl_path_from_ini(value);
    /* A path's pattern ("*.u") off: what is named is its folder. */
    char *slash = strrchr(v, '/');
    if (slash && is_pattern(slash + 1)) *slash = '\0';
    else if (!slash && is_pattern(v)) v[0] = '\0';
    size_t len = strlen(v);
    while (len > 1 && v[len - 1] == '/') v[--len] = '\0';
    if (!*v) {
        free(v);
        return NULL;
    }
    /* The mod's folder by the name it was found by, a link's kept: its own
     * folder's names under it. */
    char *folder = resolve(game_system_dir, v);
    char *real = dxl_fs_real(folder), *mine = dxl_fs_real(m->dir);
    char *found = NULL;
    if (real && mine && inside(real, mine)) {
        const char *under = real + strlen(mine);
        found = dxl_xmalloc(strlen(m->dir) + strlen(under) + 1);
        sprintf(found, "%s%s", m->dir, under);
    } else if (inside(folder, m->dir)) {
        found = dxl_xstrdup(folder);
    } else if (strncmp(v, "../", 3) == 0) {
        /* ..\<its name>\<rest>: its ini's path for it in the game's folder,
         * where it is not -- its name, or the name of the folder a link of
         * it is to. */
        char *name = v + 3, *rest = strchr(name, '/');
        if (rest && !strstr(rest, "/..") && !strstr(rest, "/./")) {
            *rest++ = '\0';
            if (dxl_stricmp(name, m->name) == 0 || (mine && dxl_stricmp(name, dxl_path_basename(mine)) == 0)) {
                char *ours = resolve(m->dir, rest);
                if (dxl_path_is_dir(ours)) found = ours;
                else free(ours);
            }
        }
    }
    free(mine);
    free(real);
    free(folder);
    free(v);
    return found;
}

/* A path's folder and its pattern apart: ..\X\*.u gives "../X" and "*.u";
 * a value with no pattern NULL. Caller frees the folder. */
static char *folder_of(const char *value, const char **pattern) {
    char *v = dxl_path_from_ini(value);
    char *slash = strrchr(v, '/');
    if (!slash || !is_pattern(slash + 1)) {
        free(v);
        return NULL;
    }
    *slash = '\0';
    const char *b = strrchr(value, '\\'), *f = strrchr(value, '/');
    const char *last = b > f ? b : f;
    *pattern = last ? last + 1 : value;
    return v;
}

/* A path's folder as the engine resolves it from a System folder. NULL for a
 * value with no pattern. Caller frees. */
static char *folder_in(const char *value, const char *system_dir) {
    const char *pattern;
    char *v = folder_of(value, &pattern);
    if (!v) return NULL;
    char *folder = resolve(system_dir, v);
    free(v);
    return folder;
}

/* A mod laid out as its installer lays it over the game, in a folder of its
 * own (The Zodiac's System, ZodiacMaps, Textures and Music; GMDX's GMDXv9,
 * HDTP, New Vision and System): its ini's ..\<folder>\<rest>, the game's
 * folder that its files join, is its own copy of that folder, when it has
 * one. NULL for none. */
static char *overlay_folder(const char *value, const dxl_mod *m) {
    const char *pattern;
    char *v = folder_of(value, &pattern);
    char *found = NULL;
    if (v && strncmp(v, "../", 3) == 0 && v[3] && !strstr(v + 3, "..") && !strstr(v + 3, "./")) {
        char *ours = resolve(m->dir, v + 3);
        if (dxl_path_is_dir(ours) && inside(ours, m->dir)) found = ours;
        else free(ours);
    }
    free(v);
    return found;
}

/* A path of a mod that runs from its own System, as that System makes it:
 * its absolute path where the folder is there; else, one climbing out of the
 * mod's folder, the game's folder of the names after the climb, where the
 * game has it (Transcended's ..\..\..\System, the mod anywhere); else NULL,
 * the value kept as it is. */
static char *own_system_path(const char *value, const char *game_system_dir, const dxl_mod *m) {
    const char *pattern;
    char *v = folder_of(value, &pattern);
    if (!v) return NULL;
    char *out = NULL;
    char *folder = resolve(m->system_dir, v);
    if (dxl_path_is_dir(folder)) {
        out = dxl_path_join(folder, pattern);
    } else if (strncmp(v, "../", 3) == 0) {
        const char *rest = v;
        while (strncmp(rest, "../", 3) == 0) rest += 3;
        char *game_root = dxl_path_dirname(game_system_dir);
        char *g = resolve(game_root, rest);
        if (*rest && !strstr(rest, "..") && dxl_path_is_dir(g)) out = dxl_path_join(g, pattern);
        free(g);
        free(game_root);
    }
    free(folder);
    free(v);
    return out;
}

/* The pattern of a path's value: what follows its folder ("*.u"). */
static const char *pattern_of(const char *value) {
    const char *b = strrchr(value, '\\'), *f = strrchr(value, '/');
    const char *last = b > f ? b : f;
    return last ? last + 1 : value;
}

static int listed(char **v, size_t n, const char *s, int any_case) {
    for (size_t i = 0; i < n; i++)
        if (any_case ? dxl_stricmp(v[i], s) == 0 : strcmp(v[i], s) == 0) return 1;
    return 0;
}

void dxl_mod_set_paths(dxl_ini *sys, const char *game_system_dir, const dxl_mod *m) {
    /* A mod that runs from its own System has its paths from there. */
    const char *base = m->own_system ? m->system_dir : game_system_dir;
    size_t n = dxl_ini_get_all(sys, "Core.System", "Paths", NULL, 0);
    const char **values = dxl_xmalloc((n + 1) * sizeof *values);
    dxl_ini_get_all(sys, "Core.System", "Paths", values, n);
    /* The ini's strings change as it does: copies of them. */
    char **ours = dxl_xmalloc((2 * n + 6) * sizeof *ours), **theirs = dxl_xmalloc((n + 1) * sizeof *theirs);
    size_t no = 0, nt = 0, named = 0;
    for (size_t i = 0; i < n; i++) {
        /* A path names a folder's files by a pattern; one that does not is
         * kept as it is. */
        int patterned = is_pattern(pattern_of(values[i]));
        char *folder = patterned ? dxl_mod_folder(values[i], base, m) : NULL;
        char *keep = NULL;
        if (folder) {
            named++;
        } else if (patterned && !m->own_system) {
            /* Laid out as over the game: its own copy of the folder, and
             * the game's after it where the game has one. */
            folder = overlay_folder(values[i], m);
            if (folder) {
                char *game_folder = folder_in(values[i], game_system_dir);
                if (game_folder && dxl_path_is_dir(game_folder)) keep = dxl_xstrdup(values[i]);
                free(game_folder);
            }
        } else if (patterned) {
            keep = own_system_path(values[i], game_system_dir, m);
        }
        if (folder) {
            char *path = dxl_path_join(folder, pattern_of(values[i]));
            if (!listed(ours, no, path, 0)) ours[no++] = path;
            else free(path);
            free(folder);
        } else if (!keep) {
            keep = dxl_xstrdup(values[i]);
        }
        if (keep && !listed(theirs, nt, keep, 1)) theirs[nt++] = keep;
        else free(keep);
    }
    free(values);
    /* No path its ini names by the mod's own folder: its System, Maps,
     * Textures, Sounds and Music first, those it has. */
    if (!named) {
        const char *ext = dxl_ini_get(sys, "URL", "MapExt");
        char maps[64];
        snprintf(maps, sizeof maps, "*.%.40s", ext && *ext ? ext : "dx");
        const char *const folders[][2] = {
            { "System", "*.u" }, { "Maps", maps }, { "Textures", "*.utx" }, { "Sounds", "*.uax" }, { "Music", "*.umx" },
        };
        char **first = dxl_xmalloc((no + 6) * sizeof *first);
        size_t nf = 0;
        for (size_t i = 0; i < sizeof folders / sizeof *folders; i++) {
            char *dir = dxl_path_resolve_ci(m->dir, folders[i][0]);
            if (dir && dxl_path_is_dir(dir)) {
                char *path = dxl_path_join(dir, folders[i][1]);
                if (!listed(ours, no, path, 0)) first[nf++] = path;
                else free(path);
            }
            free(dir);
        }
        /* The overlay's folders in the ini's order, the others after them. */
        for (size_t i = 0; i < nf; i++) ours[no++] = first[i];
        free(first);
    }
    dxl_ini_remove(sys, "Core.System", "Paths");
    for (size_t i = 0; i < no; i++) {
        dxl_ini_append(sys, "Core.System", "Paths", ours[i]);
        free(ours[i]);
    }
    for (size_t i = 0; i < nt; i++) {
        dxl_ini_append(sys, "Core.System", "Paths", theirs[i]);
        free(theirs[i]);
    }
    free(ours);
    free(theirs);
}

#define _GNU_SOURCE
#include "core/mods.h"
#include "core/paths.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

/* The mod's settings, and its controls beside them. */
static void own_inis(dxl_mod *m) {
    char **names = names_in(m->system_dir);
    char *real = realpath(m->dir, NULL);
    const char *target = real ? dxl_path_basename(real) : NULL;
    const char *best = NULL;
    /* 4 named after the mod's folder (its link's name or its own), 3 with
     * its own <stem>User.ini beside it, 2 another, 1 Default.ini. */
    int rank = 0;
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
            char *beside = dxl_path_resolve_ci(m->system_dir, user);
            r = beside ? 3 : 2;
            free(beside);
        }
        if (r <= rank) continue;
        char *path = dxl_path_join(m->system_dir, names[i]);
        if (is_system_ini(path)) {
            best = names[i];
            rank = r;
        }
        free(path);
    }
    if (best) {
        m->ini = dxl_path_join(m->system_dir, best);
        size_t stem = strlen(best) - 4;
        char user[300];
        snprintf(user, sizeof user, "%.*sUser.ini", (int)stem, best);
        if (dxl_stricmp(best, "Default.ini") != 0) m->user_ini = dxl_path_resolve_ci(m->system_dir, user);
        if (!m->user_ini && dxl_stricmp(best, "DeusEx.ini") == 0)
            m->user_ini = dxl_path_resolve_ci(m->system_dir, "User.ini");
        if (!m->user_ini) m->user_ini = dxl_path_resolve_ci(m->system_dir, "DefUser.ini");
    }
    free(real);
    free_names(names);
}

/* dir made absolute, by the text: the working folder before a relative
 * one, "." and ".." taken out, no '/' at its end; a link in it kept, the
 * name the player gave the mod. Caller frees. */
static char *absolute(const char *dir) {
    char cwd[PATH_MAX];
    char *joined = dir[0] == '/' ? dxl_xstrdup(dir) : dxl_path_join(getcwd(cwd, sizeof cwd) ? cwd : "/", dir);
    char *out = dxl_xmalloc(strlen(joined) + 2);
    size_t len = 0;
    out[0] = '\0';
    for (char *save = NULL, *part = strtok_r(joined, "/", &save); part; part = strtok_r(NULL, "/", &save)) {
        if (!strcmp(part, ".")) continue;
        if (!strcmp(part, "..")) {
            char *up = strrchr(out, '/');
            if (up) *up = '\0';
            len = strlen(out);
            continue;
        }
        out[len++] = '/';
        strcpy(out + len, part);
        len += strlen(part);
    }
    if (!len) strcpy(out, "/");
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
    char *cur = dxl_xstrdup(rel[0] == '/' ? "/" : base);
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
    char *real = realpath(folder, NULL), *mine = realpath(m->dir, NULL);
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
    size_t n = dxl_ini_get_all(sys, "Core.System", "Paths", NULL, 0);
    const char **values = dxl_xmalloc((n + 1) * sizeof *values);
    dxl_ini_get_all(sys, "Core.System", "Paths", values, n);
    /* The ini's strings change as it does: copies of them. */
    char **ours = dxl_xmalloc((n + 6) * sizeof *ours), **theirs = dxl_xmalloc((n + 1) * sizeof *theirs);
    size_t no = 0, nt = 0;
    for (size_t i = 0; i < n; i++) {
        /* A path names a folder's files by a pattern; one that does not is
         * kept as it is. */
        char *folder = is_pattern(pattern_of(values[i])) ? dxl_mod_folder(values[i], game_system_dir, m) : NULL;
        if (folder) {
            char *path = dxl_path_join(folder, pattern_of(values[i]));
            if (!listed(ours, no, path, 0)) ours[no++] = path;
            else free(path);
            free(folder);
        } else if (!listed(theirs, nt, values[i], 1)) {
            theirs[nt++] = dxl_xstrdup(values[i]);
        }
    }
    free(values);
    if (!no) {
        const char *ext = dxl_ini_get(sys, "URL", "MapExt");
        char maps[64];
        snprintf(maps, sizeof maps, "*.%.40s", ext && *ext ? ext : "dx");
        const char *const folders[][2] = {
            { "System", "*.u" }, { "Maps", maps }, { "Textures", "*.utx" }, { "Sounds", "*.uax" }, { "Music", "*.umx" },
        };
        for (size_t i = 0; i < sizeof folders / sizeof *folders; i++) {
            char *dir = dxl_path_resolve_ci(m->dir, folders[i][0]);
            if (dir && dxl_path_is_dir(dir)) ours[no++] = dxl_path_join(dir, folders[i][1]);
            free(dir);
        }
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

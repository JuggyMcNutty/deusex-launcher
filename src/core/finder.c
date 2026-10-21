#define _GNU_SOURCE
#include "core/finder_places.h"
#include "core/json.h"
#include "core/paths.h"
#include "core/vdf.h"
#include "platform/fs.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Deus Ex: Game of the Year Edition, on Steam, and the folder Steam gives
 * it. */
#define STEAM_APP    "6910"
#define STEAM_FOLDER "Deus Ex"
/* A folder's folders looked at, at most: more than a games folder has. */
#define CHILDREN_MAX 1024
/* Heroic's list of its games, read at most. */
#define JSON_MAX   (8L << 20)

#define FOUND_HEROIC   "in Heroic's games"

int dxl_finder_stopped(const finder *f) {
    return (f->stop && atomic_load(f->stop)) || f->seen_count >= DXL_FOUND_MAX;
}

/* A regular file's text, if it is no larger than max: a link to a device
 * or a file grown wild is not read. */
char *dxl_finder_read_small(const char *path, long max) {
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size > max) return NULL;
    FILE *in = fopen(path, "rb");
    if (!in) return NULL;
    char *text = dxl_xmalloc((size_t)st.st_size + 1);
    size_t len = fread(text, 1, (size_t)st.st_size, in);
    fclose(in);
    text[len] = '\0';
    return text;
}

/* dir checked and listed, once: by the folder it really is. always: the
 * folder given, listed whatever is there. */
void dxl_finder_add(finder *f, const char *source, const char *dir, int always) {
    if (dxl_finder_stopped(f) || !dir || !*dir) return;
    char *real = dxl_fs_real(dir);
    if (!real) {
        if (!always) return;
        real = dxl_xstrdup(dir);
    } else if (!always && !dxl_path_is_dir(real)) {
        free(real);
        return;
    }
    for (int i = 0; i < f->seen_count; i++)
        if (!strcmp(f->seen[i], real)) {
            free(real);
            return;
        }
    dxl_found it;
    memset(&it, 0, sizeof it);
    it.source = source;
    dxl_install_check(dir, &it.install);
    if (always || it.install.found) {
        f->seen[f->seen_count++] = real;
        real = NULL;
        f->found(f->ctx, &it);
    }
    free(real);
    dxl_install_free(&it.install);
}

/* Whether dir holds the game: System/DeusEx.exe, by that name, in a System
 * of any case, as core/install.h takes it. */
int dxl_finder_has_game(const char *dir) {
    char *sys = dxl_path_resolve_ci(dir, "System");
    if (!sys) return 0;
    char *exe = dxl_path_join(sys, "DeusEx.exe");
    int there = dxl_path_exists(exe) && !dxl_path_is_dir(exe);
    free(exe);
    free(sys);
    return there;
}

/* dir's folders, but the hidden ones, in name order, so the list is the
 * same each time: NULL-terminated, freed with names_free. */
static char **children(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return NULL;
    char **names = NULL;
    size_t n = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < CHILDREN_MAX) {
        if (e->d_name[0] == '.') continue;
        char *path = dxl_path_join(dir, e->d_name);
        int is_dir = dxl_path_is_dir(path);
        free(path);
        if (!is_dir) continue;
        if (n + 1 >= cap) {
            cap = cap ? cap * 2 : 16;
            names = dxl_xrealloc(names, cap * sizeof *names);
        }
        names[n++] = dxl_xstrdup(e->d_name);
    }
    closedir(d);
    if (!names) return NULL;
    names[n] = NULL;
    for (size_t i = 1; i < n; i++)
        for (size_t j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
            char *t = names[j];
            names[j] = names[j - 1];
            names[j - 1] = t;
        }
    return names;
}

static void names_free(char **names) {
    for (size_t i = 0; names && names[i]; i++) free(names[i]);
    free(names);
}

/* Each folder in dir that holds the game. */
void dxl_finder_add_children(finder *f, const char *source, const char *dir) {
    char **names = children(dir);
    for (size_t i = 0; names && names[i] && !dxl_finder_stopped(f); i++) {
        char *child = dxl_path_join(dir, names[i]);
        if (dxl_finder_has_game(child)) dxl_finder_add(f, source, child, 0);
        free(child);
    }
    names_free(names);
}

/* dir itself, and its folders depth deep. */
void dxl_finder_within(finder *f, const char *source, const char *dir, int depth) {
    if (dxl_finder_stopped(f)) return;
    if (dxl_finder_has_game(dir)) {
        dxl_finder_add(f, source, dir, 0);
        return;
    }
    if (depth <= 0) return;
    char **names = children(dir);
    for (size_t i = 0; names && names[i] && !dxl_finder_stopped(f); i++) {
        char *child = dxl_path_join(dir, names[i]);
        dxl_finder_within(f, source, child, depth - 1);
        free(child);
    }
    names_free(names);
}

/* ---- a Windows drive: a Wine prefix's drive_c, or one mounted ------------ */

/* Where Windows' installers put the game, from the drive's top: the CD's
 * folder, Steam's library, Windows' Steam's own and one of its own. */
static const char *const drive_games[] = {
    "DeusEx", "Program Files (x86)/Steam/steamapps/common/" STEAM_FOLDER,
    "Program Files/Steam/steamapps/common/" STEAM_FOLDER, "SteamLibrary/steamapps/common/" STEAM_FOLDER,
};
/* The folders a game is one of: GOG's installer's, GOG Galaxy's, a player's,
 * spruceOS's Ports folder on a handheld's card. */
static const char *const drive_folders[] = {
    "GOG Games", "Program Files (x86)/GOG Galaxy/Games", "Program Files/GOG Galaxy/Games", "Games", "Roms/PORTS",
};

void dxl_finder_drive(finder *f, const char *source, const char *root) {
    for (size_t i = 0; i < sizeof drive_games / sizeof *drive_games && !dxl_finder_stopped(f); i++) {
        char *p = dxl_path_resolve_ci(root, drive_games[i]);
        if (p) dxl_finder_add(f, source, p, 0);
        free(p);
    }
    for (size_t i = 0; i < sizeof drive_folders / sizeof *drive_folders && !dxl_finder_stopped(f); i++) {
        char *p = dxl_path_resolve_ci(root, drive_folders[i]);
        if (p) dxl_finder_add_children(f, source, p);
        free(p);
    }
}

/* A Wine prefix: its drive_c, and a Windows Steam there, with the
 * libraries its list puts on the prefix's drives. */
void dxl_finder_prefix(finder *f, const char *source, const char *dir) {
    char *c = dir && *dir ? dxl_path_resolve_ci(dir, "drive_c") : NULL;
    if (c && dxl_path_is_dir(c)) {
        dxl_finder_drive(f, source, c);
        static const char *const steams[] = { "Program Files (x86)/Steam", "Program Files/Steam" };
        for (size_t i = 0; i < sizeof steams / sizeof *steams && !dxl_finder_stopped(f); i++) {
            char *s = dxl_path_resolve_ci(c, steams[i]);
            if (s) dxl_finder_steam(f, source, s, dir);
            free(s);
        }
    }
    free(c);
}

/* Each folder in dir that is a Wine prefix. */
void dxl_finder_prefixes_in(finder *f, const char *source, const char *dir) {
    char **names = children(dir);
    for (size_t i = 0; names && names[i] && !dxl_finder_stopped(f); i++) {
        char *child = dxl_path_join(dir, names[i]);
        dxl_finder_prefix(f, source, child);
        free(child);
    }
    names_free(names);
}

/* ---- Steam ----------------------------------------------------------- */

/* A library: the game where its manifest puts it, else in Steam's folder
 * for it. */
static void steam_library(finder *f, const char *source, const char *lib) {
    char *apps = dxl_path_resolve_ci(lib, "steamapps");
    if (!apps) return;
    char *acf = dxl_path_join(apps, "appmanifest_" STEAM_APP ".acf");
    dxl_vdf *m = dxl_path_exists(acf) ? dxl_vdf_load(acf, NULL) : NULL;
    const char *dir = dxl_vdf_get_string(dxl_vdf_get(m, "AppState"), "installdir");
    char *common = dxl_path_join(apps, "common");
    if (dir && *dir && !strchr(dir, '/')) {
        char *game = dxl_path_join(common, dir);
        dxl_finder_add(f, source, game, 0);
        free(game);
    }
    char *game = dxl_path_join(common, STEAM_FOLDER);
    dxl_finder_add(f, source, game, 0);
    free(game);
    free(common);
    dxl_vdf_free(m);
    free(acf);
    free(apps);
}

/* A Windows path, X:\..., where a Wine prefix's dosdevices/x: leads, each
 * part found in any case; NULL for any other (a UNC path) or none there. */
static char *wine_path(const char *wine, const char *path) {
    if (!isalpha((unsigned char)path[0]) || path[1] != ':' || (path[2] && path[2] != '\\' && path[2] != '/'))
        return NULL;
    char device[16];
    snprintf(device, sizeof device, "dosdevices/%c:", tolower((unsigned char)path[0]));
    char *drive_root = dxl_path_join(wine, device);
    char *rest = dxl_xstrdup(path + 2);
    for (char *p = rest; *p; p++)
        if (*p == '\\') *p = '/';
    const char *r = rest + strspn(rest, "/");
    char *at = *r ? dxl_path_resolve_ci(drive_root, r) : dxl_path_is_dir(drive_root) ? dxl_xstrdup(drive_root) : NULL;
    free(rest);
    free(drive_root);
    return at;
}

/* A Steam: its own library, then each its libraryfolders.vdf lists -- by
 * a block's "path", or an older Steam's key's value -- under number keys.
 * A Windows Steam's, in the Wine prefix wine (NULL: a native Steam), names
 * its libraries as Windows paths. */
void dxl_finder_steam(finder *f, const char *source, const char *root, const char *wine) {
    if (!dxl_path_is_dir(root)) return;
    steam_library(f, source, root);
    static const char *const lists[] = { "steamapps/libraryfolders.vdf", "config/libraryfolders.vdf" };
    for (size_t i = 0; i < sizeof lists / sizeof *lists && !dxl_finder_stopped(f); i++) {
        char *path = dxl_path_join(root, lists[i]);
        dxl_vdf *v = dxl_path_exists(path) ? dxl_vdf_load(path, NULL) : NULL;
        const dxl_vdf *top = dxl_vdf_get(v, "libraryfolders");
        for (size_t k = 0; k < dxl_vdf_count(top) && !dxl_finder_stopped(f); k++) {
            const dxl_vdf *lib = dxl_vdf_at(top, k);
            const char *key = dxl_vdf_key(lib);
            if (!key || !*key || strspn(key, "0123456789") != strlen(key)) continue;
            const char *at = dxl_vdf_value(lib) ? dxl_vdf_value(lib) : dxl_vdf_get_string(lib, "path");
            if (at && *at == '/') {
                steam_library(f, source, at);
            } else if (at && wine) {
                char *windows = wine_path(wine, at);
                if (windows) steam_library(f, source, windows);
                free(windows);
            } else if (at) {
                /* A Steam of this machine's own naming its libraries from
                 * another root, Windows' C:\\: as this machine takes it. */
                char *native = dxl_path_from_ini(at);
                if (dxl_fs_is_absolute(native)) steam_library(f, source, native);
                free(native);
            }
        }
        dxl_vdf_free(v);
        free(path);
    }
}

/* ---- Heroic ---------------------------------------------------------- */

/* Its GOG games, from its config folder: gog_store/installed.json's
 * "installed", each with its install_path. */
void dxl_finder_heroic(finder *f, const char *config) {
    char *path = dxl_path_join(config, "heroic/gog_store/installed.json");
    char *text = dxl_finder_read_small(path, JSON_MAX);
    dxl_json *j = text ? dxl_json_parse(text, strlen(text), NULL) : NULL;
    free(text);
    dxl_json *installed = dxl_json_get(j, "installed");
    for (size_t i = 0; i < dxl_json_count(installed) && !dxl_finder_stopped(f); i++) {
        const char *at = dxl_json_get_string(dxl_json_at(installed, i), "install_path", NULL);
        char *dir = at ? dxl_path_from_ini(at) : NULL;
        if (dir && dxl_fs_is_absolute(dir)) dxl_finder_add(f, FOUND_HEROIC, dir, 0);
        free(dir);
    }
    dxl_json_free(j);
    free(path);
}

/* ---- the run ------------------------------------------------------------ */

static void teardown(finder *f) {
    for (int i = 0; i < 3; i++) free(f->own[i]);
    for (int i = 0; i < f->seen_count; i++) free(f->seen[i]);
}

void dxl_finder_run(const dxl_finder_where *w, dxl_found_fn found, void *ctx, const atomic_int *stop) {
    finder f;
    memset(&f, 0, sizeof f);
    f.found = found;
    f.ctx = ctx;
    f.stop = stop;
    dxl_finder_setup(&f, w);

    if (w->given && *w->given) dxl_finder_add(&f, "the folder given", w->given, 1);
    for (int i = 0; w->known && w->known[i]; i++) dxl_finder_add(&f, "played before", w->known[i], 0);
    dxl_finder_places(&f);
    teardown(&f);
}

static void collect(void *ctx, const dxl_found *f) { dxl_found_list_add(ctx, f); }

void dxl_finder_find(dxl_found_list *out, const dxl_finder_where *where) {
    memset(out, 0, sizeof *out);
    dxl_finder_run(where, collect, out, NULL);
}

int dxl_found_list_add(dxl_found_list *l, const dxl_found *f) {
    if (l->count >= DXL_FOUND_MAX) return -1;
    dxl_found *to = &l->items[l->count++];
    *to = *f;
    to->install.root = f->install.root ? dxl_xstrdup(f->install.root) : NULL;
    to->install.system_dir = f->install.system_dir ? dxl_xstrdup(f->install.system_dir) : NULL;
    return 0;
}

void dxl_found_list_free(dxl_found_list *l) {
    for (int i = 0; i < l->count; i++) dxl_install_free(&l->items[i].install);
    l->count = 0;
}

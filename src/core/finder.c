#define _GNU_SOURCE
#include "core/finder.h"
#include "core/json.h"
#include "core/paths.h"
#include "core/vdf.h"

#include <dirent.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Deus Ex: Game of the Year Edition, on Steam, and the folder Steam gives
 * it. */
#define STEAM_APP    "6910"
#define STEAM_FOLDER "Deus Ex"
/* A folder's folders looked at, at most: more than a games folder has. */
#define CHILDREN_MAX 1024
/* A Lutris game's file, read at most. */
#define YML_MAX (256 * 1024)

#define FOUND_STEAM    "in Steam's library"
#define FOUND_HEROIC   "in Heroic's games"
#define FOUND_LUTRIS   "in Lutris' games"
#define FOUND_BOTTLES  "in Bottles"
#define FOUND_PREFIX   "in a Wine prefix"
#define FOUND_DRIVE    "on a mounted drive"

typedef struct {
    const char        *home, *config, *data, *wineprefix;
    const char *const *media;
    char              *own[3];      /* the paths made up for those */
    char              *media_own[6];
    dxl_found_fn       found;
    void              *ctx;
    const volatile int *stop;
    char              *seen[DXL_FOUND_MAX];   /* the folders listed, as they really are */
    int                seen_count;
} finder;

static int stopped(const finder *f) { return (f->stop && *f->stop) || f->seen_count >= DXL_FOUND_MAX; }

/* dir checked and listed, once: by the folder it really is. always: the
 * folder given, listed whatever is there. */
static void add(finder *f, const char *source, const char *dir, int always) {
    if (stopped(f) || !dir || !*dir) return;
    char real[PATH_MAX];
    if (!realpath(dir, real)) {
        if (!always) return;
        snprintf(real, sizeof real, "%s", dir);
    } else if (!always && !dxl_path_is_dir(real)) {
        return;
    }
    for (int i = 0; i < f->seen_count; i++)
        if (!strcmp(f->seen[i], real)) return;
    dxl_found it;
    memset(&it, 0, sizeof it);
    it.source = source;
    dxl_install_check(dir, &it.install);
    if (always || it.install.found) {
        f->seen[f->seen_count++] = dxl_xstrdup(real);
        f->found(f->ctx, &it);
    }
    dxl_install_free(&it.install);
}

/* Whether dir holds the game: System/DeusEx.exe, by that name, in a System
 * of any case, as core/install.h takes it. */
static int has_game(const char *dir) {
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
static void add_children(finder *f, const char *source, const char *dir) {
    char **names = children(dir);
    for (size_t i = 0; names && names[i] && !stopped(f); i++) {
        char *child = dxl_path_join(dir, names[i]);
        if (has_game(child)) add(f, source, child, 0);
        free(child);
    }
    names_free(names);
}

/* dir itself, and its folders depth deep. */
static void within(finder *f, const char *source, const char *dir, int depth) {
    if (stopped(f)) return;
    if (has_game(dir)) {
        add(f, source, dir, 0);
        return;
    }
    if (depth <= 0) return;
    char **names = children(dir);
    for (size_t i = 0; names && names[i] && !stopped(f); i++) {
        char *child = dxl_path_join(dir, names[i]);
        within(f, source, child, depth - 1);
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
/* The folders a game is one of: GOG's installer's, GOG Galaxy's, a player's. */
static const char *const drive_folders[] = {
    "GOG Games", "Program Files (x86)/GOG Galaxy/Games", "Program Files/GOG Galaxy/Games", "Games",
};

static void drive(finder *f, const char *source, const char *root) {
    for (size_t i = 0; i < sizeof drive_games / sizeof *drive_games && !stopped(f); i++) {
        char *p = dxl_path_resolve_ci(root, drive_games[i]);
        if (p) add(f, source, p, 0);
        free(p);
    }
    for (size_t i = 0; i < sizeof drive_folders / sizeof *drive_folders && !stopped(f); i++) {
        char *p = dxl_path_resolve_ci(root, drive_folders[i]);
        if (p) add_children(f, source, p);
        free(p);
    }
}

/* A Wine prefix: its drive_c. */
static void prefix(finder *f, const char *source, const char *dir) {
    char *c = dir && *dir ? dxl_path_resolve_ci(dir, "drive_c") : NULL;
    if (c && dxl_path_is_dir(c)) drive(f, source, c);
    free(c);
}

/* Each folder in dir that is a Wine prefix. */
static void prefixes_in(finder *f, const char *source, const char *dir) {
    char **names = children(dir);
    for (size_t i = 0; names && names[i] && !stopped(f); i++) {
        char *child = dxl_path_join(dir, names[i]);
        prefix(f, source, child);
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
        add(f, source, game, 0);
        free(game);
    }
    char *game = dxl_path_join(common, STEAM_FOLDER);
    add(f, source, game, 0);
    free(game);
    free(common);
    dxl_vdf_free(m);
    free(acf);
    free(apps);
}

/* A Steam: its own library, then each its libraryfolders.vdf lists -- by
 * a block's "path", or an older Steam's key's value -- under number keys. */
static void steam(finder *f, const char *source, const char *root) {
    if (!dxl_path_is_dir(root)) return;
    steam_library(f, source, root);
    static const char *const lists[] = { "steamapps/libraryfolders.vdf", "config/libraryfolders.vdf" };
    for (size_t i = 0; i < sizeof lists / sizeof *lists && !stopped(f); i++) {
        char *path = dxl_path_join(root, lists[i]);
        dxl_vdf *v = dxl_path_exists(path) ? dxl_vdf_load(path, NULL) : NULL;
        const dxl_vdf *top = dxl_vdf_get(v, "libraryfolders");
        for (size_t k = 0; k < dxl_vdf_count(top) && !stopped(f); k++) {
            const dxl_vdf *lib = dxl_vdf_at(top, k);
            const char *key = dxl_vdf_key(lib);
            if (!key || !*key || strspn(key, "0123456789") != strlen(key)) continue;
            const char *at = dxl_vdf_value(lib) ? dxl_vdf_value(lib) : dxl_vdf_get_string(lib, "path");
            if (at && *at == '/') steam_library(f, source, at);
        }
        dxl_vdf_free(v);
        free(path);
    }
}

/* ---- Heroic ---------------------------------------------------------- */

/* Its GOG games, from its config folder: gog_store/installed.json's
 * "installed", each with its install_path. */
static void heroic(finder *f, const char *config) {
    char *path = dxl_path_join(config, "heroic/gog_store/installed.json");
    dxl_json *j = dxl_path_exists(path) ? dxl_json_load(path, NULL) : NULL;
    dxl_json *installed = dxl_json_get(j, "installed");
    for (size_t i = 0; i < dxl_json_count(installed) && !stopped(f); i++) {
        const char *at = dxl_json_get_string(dxl_json_at(installed, i), "install_path", NULL);
        if (at && *at == '/') add(f, FOUND_HEROIC, at, 0);
    }
    dxl_json_free(j);
    free(path);
}

/* ---- Lutris ---------------------------------------------------------- */

/* A YAML scalar as Lutris writes one, into out: bare, its comment cut off,
 * or in quotes -- '' a quote in single ones, \ an escape in double. */
static void yml_scalar(const char *in, char *out, size_t size) {
    size_t n = 0;
    while (*in == ' ' || *in == '\t') in++;
    if (*in == '\'' || *in == '"') {
        char q = *in++;
        for (; *in && n + 1 < size; in++) {
            if (*in == q) {
                if (q == '\'' && in[1] == '\'') { out[n++] = *in++; continue; }
                break;
            }
            if (q == '"' && *in == '\\' && in[1]) in++;
            out[n++] = *in;
        }
    } else {
        for (; *in && *in != '\r' && n + 1 < size; in++) {
            if (*in == '#' && n && (out[n - 1] == ' ' || out[n - 1] == '\t')) break;
            out[n++] = *in;
        }
        while (n && (out[n - 1] == ' ' || out[n - 1] == '\t')) n--;
    }
    out[n] = '\0';
}

/* One game's file: the game: section's exe -- the game's own folder if it
 * is the game's System/DeusEx.exe, or beside it -- and its Wine prefix. */
static void lutris_game(finder *f, const char *path) {
    FILE *in = fopen(path, "rb");
    if (!in) return;
    char *text = dxl_xmalloc(YML_MAX + 1);
    size_t len = fread(text, 1, YML_MAX, in);
    fclose(in);
    text[len] = '\0';
    char exe[PATH_MAX] = "", pfx[PATH_MAX] = "";
    int in_game = 0;
    char *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (line[0] != ' ' && line[0] != '\t') {
            if (line[0] && line[0] != '#') in_game = !strncmp(line, "game:", 5);
            continue;
        }
        if (!in_game) continue;
        const char *t = line + strspn(line, " \t");
        if (!strncmp(t, "exe:", 4)) yml_scalar(t + 4, exe, sizeof exe);
        else if (!strncmp(t, "prefix:", 7)) yml_scalar(t + 7, pfx, sizeof pfx);
    }
    free(text);
    if (exe[0] == '/') {
        char *dir = dxl_path_dirname(exe);
        if (!dxl_stricmp(dxl_path_basename(dir), "System")) {
            char *root = dxl_path_dirname(dir);
            add(f, FOUND_LUTRIS, root, 0);
            free(root);
        } else if (has_game(dir)) {
            add(f, FOUND_LUTRIS, dir, 0);
        }
        free(dir);
    }
    if (pfx[0] == '/') prefix(f, FOUND_LUTRIS, pfx);
}

/* Each game's file in its games folder, in name order. */
static void lutris(finder *f, const char *games) {
    DIR *d = opendir(games);
    if (!d) return;
    char *names[CHILDREN_MAX];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < CHILDREN_MAX) {
        size_t len = strlen(e->d_name);
        if (e->d_name[0] != '.' && len > 4 && !strcmp(e->d_name + len - 4, ".yml")) names[n++] = dxl_xstrdup(e->d_name);
    }
    closedir(d);
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
            char *t = names[j];
            names[j] = names[j - 1];
            names[j - 1] = t;
        }
    for (int i = 0; i < n; i++) {
        if (!stopped(f)) {
            char *path = dxl_path_join(games, names[i]);
            lutris_game(f, path);
            free(path);
        }
        free(names[i]);
    }
}

/* ---- the drives -------------------------------------------------------- */

static int is_media(const finder *f, const char *dir) {
    for (size_t i = 0; f->media && f->media[i]; i++)
        if (!strcmp(f->media[i], dir)) return 1;
    return 0;
}

/* Each drive mounted in dir -- but those of another folder looked in, a
 * user's in /run/media -- at its top and two folders down, and where
 * Windows' installers put the game. */
static void drives(finder *f, const char *dir) {
    char **names = children(dir);
    for (size_t i = 0; names && names[i] && !stopped(f); i++) {
        char *mount = dxl_path_join(dir, names[i]);
        if (!is_media(f, mount)) {
            within(f, FOUND_DRIVE, mount, 2);
            drive(f, FOUND_DRIVE, mount);
        }
        free(mount);
    }
    names_free(names);
}

/* ---- the run ------------------------------------------------------------ */

/* An absolute path from the environment, else NULL. */
static const char *env_dir(const char *name) {
    const char *v = getenv(name);
    return v && *v == '/' ? v : NULL;
}

static void setup(finder *f, const dxl_finder_where *w) {
    int own = !w->home;
    f->home = w->home ? w->home : getenv("HOME");
    f->config = w->config_home ? w->config_home : own ? env_dir("XDG_CONFIG_HOME") : NULL;
    f->data = w->data_home ? w->data_home : own ? env_dir("XDG_DATA_HOME") : NULL;
    if (!f->config && f->home) f->config = f->own[0] = dxl_path_join(f->home, ".config");
    if (!f->data && f->home) f->data = f->own[1] = dxl_path_join(f->home, ".local/share");
    f->wineprefix = w->wineprefix ? w->wineprefix : own ? env_dir("WINEPREFIX") : NULL;
    f->media = w->media;
    if (!f->media && own) {
        char user[256] = "";
        struct passwd pw, *got = NULL;
        char buf[4096];
        if (getpwuid_r(getuid(), &pw, buf, sizeof buf, &got) == 0 && got && got->pw_name)
            snprintf(user, sizeof user, "%s", got->pw_name);
        int n = 0;
        if (*user) f->media_own[n++] = dxl_path_join("/run/media", user);
        f->media_own[n++] = dxl_xstrdup("/run/media");
        if (*user) f->media_own[n++] = dxl_path_join("/media", user);
        f->media_own[n++] = dxl_xstrdup("/media");
        f->media_own[n++] = dxl_xstrdup("/mnt");
        f->media_own[n] = NULL;
        f->media = (const char *const *)f->media_own;
    }
}

static void teardown(finder *f) {
    for (int i = 0; i < 3; i++) free(f->own[i]);
    for (int i = 0; i < 6; i++) free(f->media_own[i]);
    for (int i = 0; i < f->seen_count; i++) free(f->seen[i]);
}

/* A folder under the home, for the places' walk. */
static char *in_home(const finder *f, const char *rel) { return dxl_path_join(f->home, rel); }

void dxl_finder_run(const dxl_finder_where *w, dxl_found_fn found, void *ctx, const volatile int *stop) {
    finder f;
    memset(&f, 0, sizeof f);
    f.found = found;
    f.ctx = ctx;
    f.stop = stop;
    setup(&f, w);

    if (w->given && *w->given) add(&f, "the folder given", w->given, 1);
    for (int i = 0; w->known && w->known[i]; i++) add(&f, "played before", w->known[i], 0);
    if (!f.home || !*f.home) {
        teardown(&f);
        return;
    }

    static const struct {
        const char *source;
        const char *steam;   /* under the home */
    } steams[] = {
        { FOUND_STEAM, ".local/share/Steam" },
        { FOUND_STEAM, ".steam/steam" },
        { FOUND_STEAM, ".steam/root" },
        { FOUND_STEAM, ".steam/debian-installation" },
        { FOUND_STEAM ", Steam's Flatpak", ".var/app/com.valvesoftware.Steam/.local/share/Steam" },
        { FOUND_STEAM ", Steam's Snap", "snap/steam/common/.local/share/Steam" },
    };
    for (size_t i = 0; i < sizeof steams / sizeof *steams && !stopped(&f); i++) {
        char *root = in_home(&f, steams[i].steam);
        steam(&f, steams[i].source, root);
        free(root);
    }

    /* Heroic, natively and as a Flatpak; the folder it puts games in. */
    if (f.config) heroic(&f, f.config);
    char *p = in_home(&f, ".var/app/com.heroicgameslauncher.hgl/config");
    heroic(&f, p);
    free(p);
    p = in_home(&f, "Games/Heroic");
    add_children(&f, FOUND_HEROIC, p);
    free(p);

    p = in_home(&f, "GOG Games");
    add_children(&f, "in GOG Games", p);
    free(p);
    p = in_home(&f, "Games");
    add_children(&f, "in Games", p);
    free(p);

    /* Lutris: its config folder's games, else its data folder's; the same
     * as a Flatpak. */
    char *lutris_dirs[4] = {
        f.config ? dxl_path_join(f.config, "lutris/games") : NULL,
        f.data ? dxl_path_join(f.data, "lutris/games") : NULL,
        in_home(&f, ".var/app/net.lutris.Lutris/config/lutris/games"),
        in_home(&f, ".var/app/net.lutris.Lutris/data/lutris/games"),
    };
    for (int i = 0; i < 4; i++) {
        if (lutris_dirs[i] && !stopped(&f)) lutris(&f, lutris_dirs[i]);
        free(lutris_dirs[i]);
    }

    /* Bottles' bottles, natively and as a Flatpak. */
    if (f.data) {
        p = dxl_path_join(f.data, "bottles/bottles");
        prefixes_in(&f, FOUND_BOTTLES, p);
        free(p);
    }
    p = in_home(&f, ".var/app/com.usebottles.bottles/data/bottles/bottles");
    prefixes_in(&f, FOUND_BOTTLES, p);
    free(p);

    /* The other Wine prefixes: the environment's, Wine's own, winetricks',
     * Lutris' folder for them, Heroic's. */
    if (f.wineprefix) prefix(&f, FOUND_PREFIX, f.wineprefix);
    p = in_home(&f, ".wine");
    prefix(&f, FOUND_PREFIX, p);
    free(p);
    if (f.data) {
        p = dxl_path_join(f.data, "wineprefixes");
        prefixes_in(&f, FOUND_PREFIX, p);
        free(p);
    }
    p = in_home(&f, "Games");
    prefixes_in(&f, FOUND_PREFIX, p);
    free(p);
    p = in_home(&f, "Games/Heroic/Prefixes");
    prefixes_in(&f, FOUND_PREFIX, p);
    free(p);
    p = in_home(&f, "Games/Heroic/Prefixes/default");
    prefixes_in(&f, FOUND_PREFIX, p);
    free(p);

    for (size_t i = 0; f.media && f.media[i] && !stopped(&f); i++) drives(&f, f.media[i]);
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

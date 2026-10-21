/* Where the game is on desktop Linux: Steam's own libraries, Heroic's,
 * Lutris', Bottles', the Wine prefixes and the mounted drives, in
 * core/finder.h's order; the looking is core/finder.c's. */
#define _GNU_SOURCE
#include "core/finder_places.h"
#include "core/paths.h"
#include "platform/fs.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A folder's Lutris games looked at, at most. */
#define CHILDREN_MAX 1024
/* A Lutris game's file, read at most; the table of what is mounted. */
#define YML_MAX    (256 * 1024)
#define MOUNTS_MAX (4L << 20)
/* The drives looked at, at most. */
#define DRIVES_MAX 64

#define FOUND_STEAM    "in Steam's library"
#define FOUND_HEROIC   "in Heroic's games"
#define FOUND_LUTRIS   "in Lutris' games"
#define FOUND_BOTTLES  "in Bottles"
#define FOUND_PREFIX   "in a Wine prefix"
#define FOUND_DRIVE    "on a mounted drive"

/* ---- Lutris ---------------------------------------------------------- */

/* A code point as UTF-8 at out + *n, if it fits. */
static void put_utf8(char *out, size_t *n, size_t size, unsigned long c) {
    char b[4];
    size_t k;
    if (c < 0x80) { b[0] = (char)c; k = 1; }
    else if (c < 0x800) { b[0] = (char)(0xc0 | c >> 6); b[1] = (char)(0x80 | (c & 0x3f)); k = 2; }
    else if (c < 0x10000) {
        b[0] = (char)(0xe0 | c >> 12); b[1] = (char)(0x80 | (c >> 6 & 0x3f)); b[2] = (char)(0x80 | (c & 0x3f)); k = 3;
    } else if (c < 0x110000) {
        b[0] = (char)(0xf0 | c >> 18); b[1] = (char)(0x80 | (c >> 12 & 0x3f)); b[2] = (char)(0x80 | (c >> 6 & 0x3f));
        b[3] = (char)(0x80 | (c & 0x3f)); k = 4;
    } else {
        return;
    }
    if (*n + k >= size) return;
    memcpy(out + *n, b, k);
    *n += k;
}

/* digits hex digits at s as a number; -1 when they are not. */
static long hex_of(const char *s, int digits) {
    long v = 0;
    for (int i = 0; i < digits; i++) {
        char c = s[i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return -1;
        v = v * 16 + d;
    }
    return v;
}

/* A YAML scalar as PyYAML writes one for Lutris, into out: bare, its
 * comment cut off; in single quotes, '' a quote; in double quotes, its
 * escapes -- \xE9, é, \U000000e9 a character, as UTF-8. */
static void yml_scalar(const char *in, char *out, size_t size) {
    size_t n = 0;
    while (*in == ' ' || *in == '\t') in++;
    if (*in == '\'') {
        for (in++; *in && n + 1 < size; in++) {
            if (*in == '\'') {
                if (in[1] != '\'') break;
                in++;
            }
            out[n++] = *in;
        }
    } else if (*in == '"') {
        for (in++; *in && *in != '"' && n + 1 < size; in++) {
            if (*in != '\\' || !in[1]) {
                out[n++] = *in;
                continue;
            }
            char e = *++in;
            int digits = e == 'x' ? 2 : e == 'u' ? 4 : e == 'U' ? 8 : 0;
            long c = digits ? hex_of(in + 1, digits) : -1;
            if (c >= 0) {
                put_utf8(out, &n, size, (unsigned long)c);
                in += digits;
            } else {
                out[n++] = e == 'n' ? '\n' : e == 't' ? '\t' : e;
            }
        }
    } else {
        for (; *in && n + 1 < size; in++) {
            if (*in == '#' && n && (out[n - 1] == ' ' || out[n - 1] == '\t')) break;
            out[n++] = *in;
        }
        while (n && (out[n - 1] == ' ' || out[n - 1] == '\t')) n--;
    }
    out[n] = '\0';
}

/* One game's file: the game: section's exe -- the game's own folder if it
 * is the game's System/DeusEx.exe, or beside it -- and its Wine prefix. A
 * value PyYAML folded goes on, a space between, on the lines indented
 * under it; in double quotes, a line's end escaped joins with none. */
static void lutris_game(finder *f, const char *path) {
    char *text = dxl_finder_read_small(path, YML_MAX);
    if (!text) return;
    char exe[PATH_MAX] = "", pfx[PATH_MAX] = "";
    int in_game = 0;
    for (char *line = text, *next; line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        line[strcspn(line, "\r")] = '\0';
        if (line[0] != ' ' && line[0] != '\t') {
            if (line[0] && line[0] != '#') in_game = !strncmp(line, "game:", 5);
            continue;
        }
        size_t indent = strspn(line, " \t");
        const char *t = line + indent;
        char *want = !in_game ? NULL : !strncmp(t, "exe:", 4) ? exe : !strncmp(t, "prefix:", 7) ? pfx : NULL;
        if (!want) continue;
        dxl_buf v;
        dxl_buf_init(&v);
        dxl_buf_puts(&v, strchr(t, ':') + 1);
        while (next) {
            size_t under = strspn(next, " \t");
            if (under <= indent || next[under] == '\n' || next[under] == '\r' || !next[under]) break;
            char *after = strchr(next, '\n');
            if (after) *after++ = '\0';
            next[strcspn(next, "\r")] = '\0';
            if (v.len && v.data[v.len - 1] == '\\') v.data[--v.len] = '\0';
            else dxl_buf_add(&v, " ", 1);
            dxl_buf_puts(&v, next + under);
            next = after;
        }
        yml_scalar(v.data ? v.data : "", want, PATH_MAX);
        dxl_buf_free(&v);
    }
    free(text);
    if (exe[0] == '/') {
        char *dir = dxl_path_dirname(exe);
        if (!dxl_stricmp(dxl_path_basename(dir), "System")) {
            char *root = dxl_path_dirname(dir);
            dxl_finder_add(f, FOUND_LUTRIS, root, 0);
            free(root);
        } else if (dxl_finder_has_game(dir)) {
            dxl_finder_add(f, FOUND_LUTRIS, dir, 0);
        }
        free(dir);
    }
    if (pfx[0] == '/') dxl_finder_prefix(f, FOUND_LUTRIS, pfx);
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
        if (!dxl_finder_stopped(f)) {
            char *path = dxl_path_join(games, names[i]);
            lutris_game(f, path);
            free(path);
        }
        free(names[i]);
    }
}

/* ---- the drives -------------------------------------------------------- */

/* A mount point as mountinfo writes it, into out: \040 a space, \011 a
 * tab, \012 a line's end, \134 a backslash. */
static void mount_path(const char *in, size_t len, char *out, size_t size) {
    size_t n = 0;
    for (size_t i = 0; i < len && n + 1 < size; i++) {
        if (in[i] == '\\' && i + 3 < len && in[i + 1] >= '0' && in[i + 1] <= '3' &&
            in[i + 2] >= '0' && in[i + 2] <= '7' && in[i + 3] >= '0' && in[i + 3] <= '7') {
            out[n++] = (char)((in[i + 1] - '0') * 64 + (in[i + 2] - '0') * 8 + (in[i + 3] - '0'));
            i += 3;
        } else {
            out[n++] = in[i];
        }
    }
    out[n] = '\0';
}

/* Whether path is root or under it. */
static int under(const char *path, const char *root) {
    size_t n = strlen(root);
    return !strncmp(path, root, n) && (path[n] == '/' || !path[n] || (n == 1 && root[0] == '/'));
}

/* What mountinfo has mounted under the folders drives are mounted in --
 * those folders made real first, so a link to another is looked in once --
 * each drive once, in its order: at its top and two folders down, and
 * where Windows' installers put the game. */
static void drives(finder *f) {
    if (!f->media) return;
    char *own = NULL;
    const char *info = f->mounts;
    if (!info) {
        /* /proc's files say they are empty: read to their end. */
        FILE *in = fopen("/proc/self/mountinfo", "rb");
        if (!in) return;
        own = dxl_xmalloc((size_t)MOUNTS_MAX + 1);
        size_t len = fread(own, 1, (size_t)MOUNTS_MAX, in);
        fclose(in);
        own[len] = '\0';
        info = own;
    }
    char *roots[8];
    int nroots = 0;
    for (size_t i = 0; f->media[i] && nroots < 8; i++) {
        char *r = dxl_fs_real(f->media[i]);
        for (int k = 0; r && k < nroots; k++)
            if (!strcmp(roots[k], r)) {
                free(r);
                r = NULL;
            }
        if (r) roots[nroots++] = r;
    }
    char *walked[DRIVES_MAX];
    int nwalked = 0;
    for (const char *line = info; *line && !dxl_finder_stopped(f) && nwalked < DRIVES_MAX;) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) : strlen(line);
        /* The fifth field: where it is mounted. */
        const char *p = line, *stop_at = line + len;
        for (int k = 0; k < 4 && p; k++) {
            p = memchr(p, ' ', (size_t)(stop_at - p));
            if (p) p++;
        }
        if (p && p < stop_at) {
            const char *q = memchr(p, ' ', (size_t)(stop_at - p));
            char at[PATH_MAX];
            mount_path(p, (size_t)((q ? q : stop_at) - p), at, sizeof at);
            int in_media = 0;
            for (int k = 0; k < nroots && !in_media; k++) in_media = under(at, roots[k]);
            char *real = in_media ? dxl_fs_real(at) : NULL;
            for (int k = 0; real && k < nwalked; k++)
                if (!strcmp(walked[k], real)) {
                    free(real);
                    real = NULL;
                }
            if (real && dxl_path_is_dir(real)) {
                walked[nwalked++] = real;
                dxl_finder_within(f, FOUND_DRIVE, real, 2);
                dxl_finder_drive(f, FOUND_DRIVE, real);
            } else {
                free(real);
            }
        }
        line = end ? end + 1 : line + len;
    }
    for (int k = 0; k < nwalked; k++) free(walked[k]);
    for (int k = 0; k < nroots; k++) free(roots[k]);
    free(own);
}

/* ---- the run ------------------------------------------------------------ */

/* An absolute path from the environment, else NULL. */
static const char *env_dir(const char *name) {
    const char *v = getenv(name);
    return v && dxl_fs_is_absolute(v) ? v : NULL;
}

static const char *const media_default[] = { "/run/media", "/media", "/mnt", NULL };

void dxl_finder_setup(finder *f, const dxl_finder_where *w) {
    int own = !w->home;
    f->home = w->home ? w->home : dxl_fs_home();
    f->config = w->config_home ? w->config_home : own ? env_dir("XDG_CONFIG_HOME") : NULL;
    f->data = w->data_home ? w->data_home : own ? env_dir("XDG_DATA_HOME") : NULL;
    if (!f->config && f->home) f->config = f->own[0] = dxl_path_join(f->home, ".config");
    if (!f->data && f->home) f->data = f->own[1] = dxl_path_join(f->home, ".local/share");
    f->wineprefix = w->wineprefix ? w->wineprefix : own ? env_dir("WINEPREFIX") : NULL;
    f->media = w->media ? w->media : own ? media_default : NULL;
    f->mounts = w->mounts;
}

/* A folder under the home, for the places' walk. */
static char *in_home(const finder *f, const char *rel) { return dxl_path_join(f->home, rel); }

/* The places under the home: Steam's, Heroic's, the games folders,
 * Lutris', Bottles', the Wine prefixes there. */
static void home_places(finder *f) {
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
    for (size_t i = 0; i < sizeof steams / sizeof *steams && !dxl_finder_stopped(f); i++) {
        char *root = in_home(f, steams[i].steam);
        dxl_finder_steam(f, steams[i].source, root, NULL);
        free(root);
    }

    /* Heroic, natively and as a Flatpak; the folder it puts games in. */
    if (f->config) dxl_finder_heroic(f, f->config);
    char *p = in_home(f, ".var/app/com.heroicgameslauncher.hgl/config");
    dxl_finder_heroic(f, p);
    free(p);
    p = in_home(f, "Games/Heroic");
    dxl_finder_add_children(f, FOUND_HEROIC, p);
    free(p);

    p = in_home(f, "GOG Games");
    dxl_finder_add_children(f, "in GOG Games", p);
    free(p);
    p = in_home(f, "Games");
    dxl_finder_add_children(f, "in Games", p);
    free(p);

    /* Lutris: its config folder's games, else its data folder's; the same
     * as a Flatpak. */
    char *lutris_dirs[4] = {
        f->config ? dxl_path_join(f->config, "lutris/games") : NULL,
        f->data ? dxl_path_join(f->data, "lutris/games") : NULL,
        in_home(f, ".var/app/net.lutris.Lutris/config/lutris/games"),
        in_home(f, ".var/app/net.lutris.Lutris/data/lutris/games"),
    };
    for (int i = 0; i < 4; i++) {
        if (lutris_dirs[i] && !dxl_finder_stopped(f)) lutris(f, lutris_dirs[i]);
        free(lutris_dirs[i]);
    }

    /* Bottles' bottles, natively and as a Flatpak. */
    if (f->data) {
        p = dxl_path_join(f->data, "bottles/bottles");
        dxl_finder_prefixes_in(f, FOUND_BOTTLES, p);
        free(p);
    }
    p = in_home(f, ".var/app/com.usebottles.bottles/data/bottles/bottles");
    dxl_finder_prefixes_in(f, FOUND_BOTTLES, p);
    free(p);
}

/* The places, in the finder's order (core/finder.h): those under the home,
 * the other Wine prefixes, the drives. */
void dxl_finder_places(finder *f) {
    if (f->home && *f->home) home_places(f);

    /* The other Wine prefixes: the environment's, Wine's own, winetricks',
     * Lutris' folder for them, Heroic's. */
    if (f->wineprefix) dxl_finder_prefix(f, FOUND_PREFIX, f->wineprefix);
    int home = f->home && *f->home;
    if (home) {
        char *p = in_home(f, ".wine");
        dxl_finder_prefix(f, FOUND_PREFIX, p);
        free(p);
    }
    if (f->data) {
        char *p = dxl_path_join(f->data, "wineprefixes");
        dxl_finder_prefixes_in(f, FOUND_PREFIX, p);
        free(p);
    }
    static const char *const in_games[] = { "Games", "Games/Heroic/Prefixes", "Games/Heroic/Prefixes/default" };
    for (size_t i = 0; home && i < sizeof in_games / sizeof *in_games; i++) {
        char *p = in_home(f, in_games[i]);
        dxl_finder_prefixes_in(f, FOUND_PREFIX, p);
        free(p);
    }

    drives(f);
}

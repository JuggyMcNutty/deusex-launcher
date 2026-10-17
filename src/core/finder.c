#define _GNU_SOURCE
#include "core/finder.h"
#include "core/paths.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STEAM_GAME "steamapps/common/Deus Ex"

static const struct {
    const char *source;
    const char *steam;   /* under the home */
} steams[] = {
    { "in Steam's library", ".local/share/Steam" },
    { "in Steam's library", ".steam/steam" },
    { "in Steam's library, Steam's Flatpak", ".var/app/com.valvesoftware.Steam/.local/share/Steam" },
    { "in Steam's library, Steam's Snap", "snap/steam/common/.local/share/Steam" },
};

/* Listed once: by the folder it really is. */
static int listed(const dxl_found_list *l, const char *real) {
    for (int i = 0; i < l->count; i++) {
        char r[PATH_MAX];
        if (realpath(l->items[i].install.root, r) && strcmp(r, real) == 0) return 1;
    }
    return 0;
}

static void add(dxl_found_list *l, const char *source, const char *dir, int always) {
    if (l->count >= DXL_FOUND_MAX) return;
    char real[PATH_MAX];
    if (!realpath(dir, real)) {
        if (!always) return;
        snprintf(real, sizeof real, "%s", dir);
    } else if (!always && !dxl_path_is_dir(real)) {
        return;
    }
    if (listed(l, real)) return;
    dxl_found *f = &l->items[l->count];
    memset(f, 0, sizeof *f);
    f->source = source;
    dxl_install_check(dir, &f->install);
    if (!always && !f->install.found) {
        dxl_install_free(&f->install);
        return;
    }
    l->count++;
}

/* Each folder in dir that holds System/DeusEx.exe. */
static void add_children(dxl_found_list *l, const char *source, const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    char *names[64];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < 64) {
        if (e->d_name[0] == '.') continue;
        names[n++] = dxl_xstrdup(e->d_name);
    }
    closedir(d);
    /* In name order, so the list is the same each time. */
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
            char *t = names[j]; names[j] = names[j - 1]; names[j - 1] = t;
        }
    for (int i = 0; i < n; i++) {
        char *child = dxl_path_join(dir, names[i]);
        char *exe = dxl_path_join(child, "System/DeusEx.exe");
        if (dxl_path_exists(exe)) add(l, source, child, 0);
        free(exe);
        free(child);
        free(names[i]);
    }
}

void dxl_finder_quick(dxl_found_list *out, const char *home, const char *given, const char *const *known) {
    memset(out, 0, sizeof *out);
    if (!home) home = getenv("HOME");
    if (given && *given) add(out, "the folder given", given, 1);
    for (int i = 0; known && known[i]; i++) add(out, "played before", known[i], 0);
    if (!home || !*home) return;
    for (size_t i = 0; i < sizeof steams / sizeof *steams; i++) {
        char *steam = dxl_path_join(home, steams[i].steam);
        char *game = dxl_path_join(steam, STEAM_GAME);
        add(out, steams[i].source, game, 0);
        free(game);
        free(steam);
    }
    char *heroic = dxl_path_join(home, "Games/Heroic");
    add_children(out, "in Heroic's games", heroic);
    free(heroic);
    char *gog = dxl_path_join(home, "GOG Games");
    add_children(out, "in GOG Games", gog);
    free(gog);
}

void dxl_found_list_free(dxl_found_list *l) {
    for (int i = 0; i < l->count; i++) dxl_install_free(&l->items[i].install);
    l->count = 0;
}

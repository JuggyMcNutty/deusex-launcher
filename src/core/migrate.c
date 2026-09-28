#include "core/migrate.h"
#include "core/paths.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLOT_SECTION "UnrealShare.UnrealSlotMenu"

/* A regular file named *.usa, in any case, as FindFiles' pattern matches. */
static int is_usa(const char *dir, const char *name) {
    size_t n = strlen(name);
    if (n < 4 || dxl_stricmp(name + n - 4, ".usa") != 0) return 0;
    char *path = dxl_path_join(dir, name);
    int file = dxl_path_size(path) >= 0;
    free(path);
    return file;
}

int dxl_migrate_saves(const char *system_dir, dxl_ini *user_ini) {
    if (!user_ini) return 0;
    char *game = dxl_path_dirname(system_dir);
    char *save = dxl_path_resolve_ci(game, "Save");
    free(game);
    if (!save) return 0;

    DIR *d = opendir(save);
    if (!d) { free(save); return 0; }
    int set = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!is_usa(save, e->d_name)) continue;
        /* appAtoi on the name from its fifth character: the digits there,
         * or 0. */
        int slot = strlen(e->d_name) > 4 ? atoi(e->d_name + 4) : 0;
        char key[32];
        snprintf(key, sizeof key, "SlotNames[%d]", slot);
        const char *have = dxl_ini_get(user_ini, SLOT_SECTION, key);
        if (have && *have) continue;
        dxl_ini_set(user_ini, SLOT_SECTION, key, "Saved game");
        set++;
    }
    closedir(d);
    free(save);
    return set;
}

#include "core/gamedb.h"
#include "core/common.h"

/* DXEngine's DXEngine/UE1GameDatabase.h, its Deus Ex entries. */
static const dxl_game_build builds[] = {
    { "2a933e26aa9cfb33b37f78afe21434caa031f14a", "1112fm", "GOG", 1 },
    { "af951ddd35b38e8d9cc8501b8a50a02a3ab6cae7", "1112fm", "Steam", 1 },
    { "9f923d667a396e8243028c14dc3f5e0a6db13d84", "1002f", "the first release", 0 },
};

const dxl_game_build *dxl_gamedb_find(const char *sha1_hex) {
    if (!sha1_hex) return NULL;
    for (size_t i = 0; i < sizeof builds / sizeof *builds; i++)
        if (dxl_stricmp(builds[i].sha1, sha1_hex) == 0) return &builds[i];
    return NULL;
}

size_t dxl_gamedb_count(void) { return sizeof builds / sizeof *builds; }

const dxl_game_build *dxl_gamedb_at(size_t i) {
    return i < dxl_gamedb_count() ? &builds[i] : NULL;
}

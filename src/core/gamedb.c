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

/* An exe of no hash above beside 1112fm's packages (bugs.md D.89). */
static const dxl_game_build by_packages = { "", "1112fm", "its packages", 1 };

/* DXEngine's DXEngine/UE1GameDatabase.h's Packages1112fm: GOG's and
 * Steam's copies', the same files, read from their headers. */
static const dxl_game_package packages[] = {
    { "Core.u", "BFC9A4E444E3B749B688A99529BD75C1" },
    { "Engine.u", "51EA2E914B9530035CB94784FF411102" },
    { "DeusEx.u", "CADECA6B4EB3B7FF1F0CB59221ECCC8E" },
};

size_t dxl_gamedb_count(void) { return sizeof builds / sizeof *builds; }

const dxl_game_build *dxl_gamedb_at(size_t i) {
    return i < dxl_gamedb_count() ? &builds[i] : NULL;
}

const dxl_game_build *dxl_gamedb_by_packages(void) { return &by_packages; }

size_t dxl_gamedb_package_count(void) { return sizeof packages / sizeof *packages; }

const dxl_game_package *dxl_gamedb_package_at(size_t i) {
    return i < dxl_gamedb_package_count() ? &packages[i] : NULL;
}

int dxl_gamedb_package_known(const char *file, const char *guid) {
    if (!file || !guid) return 0;
    for (size_t i = 0; i < dxl_gamedb_package_count(); i++)
        if (dxl_stricmp(packages[i].file, file) == 0 && dxl_stricmp(packages[i].guid, guid) == 0) return 1;
    return 0;
}

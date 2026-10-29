#include "core/install.h"
#include "core/paths.h"

#include <stdlib.h>
#include <string.h>

int dxl_install_cd_ok(const char *system_dir, const char *cd_path) {
    if (!cd_path || !*cd_path) return 1;   /* nothing configured, nothing to check */

    /* CdPath is a Windows path relative to System/, the launcher's own
     * folder as it is spelled: GOG's "..\" resolves right back to the game
     * directory, Steam's "..\GOTY_1" to a folder beside it, which is why
     * the shipped check never prompts. Each of its names in any case, as
     * Windows takes them. */
    char *rel = dxl_path_from_ini(cd_path);
    char *probe;
    if (rel[0] == '/') {
        probe = dxl_path_resolve_ci(rel, "Textures/Palettes.utx");
    } else {
        char *leaf = dxl_path_join(rel, "Textures/Palettes.utx");
        probe = dxl_path_resolve_ci(system_dir, leaf);
        free(leaf);
    }
    int ok = probe != NULL && dxl_path_size(probe) > 0;

    free(probe); free(rel);
    return ok;
}

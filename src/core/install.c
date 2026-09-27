#include "core/install.h"
#include "core/paths.h"

#include <stdlib.h>
#include <string.h>

int dxl_install_cd_ok(const char *game_dir, const char *cd_path) {
    if (!cd_path || !*cd_path) return 1;   /* nothing configured, nothing to check */

    /* CdPath is a Windows path relative to System/, typically "..\" -- which
     * on a normal install resolves right back to the game directory, which is
     * why the shipped check never prompts. */
    char *rel = dxl_path_from_ini(cd_path);
    char *sys = dxl_path_join(game_dir, "System");
    char *base = dxl_path_join(sys, rel);
    char *probe = dxl_path_resolve_ci(base, "Textures/Palettes.utx");
    int ok = probe != NULL;

    free(probe); free(base); free(sys); free(rel);
    return ok;
}

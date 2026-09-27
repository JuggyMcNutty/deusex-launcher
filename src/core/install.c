#include "core/install.h"
#include "core/paths.h"

#include <stdlib.h>
#include <string.h>

/* What a playable install must have. DeusEx.u and Engine.u are the engine's
 * own script packages; Palettes.utx is what the original's CD check probed
 * for; Maps is where the game actually lives. */
static const struct { const char *rel; const char *label; int dir; } required[] = {
    { "System",                 "System directory",      1 },
    { "System/DeusEx.u",        "Deus Ex game package",  0 },
    { "System/Engine.u",        "Engine package",        0 },
    { "System/Core.u",          "Core package",          0 },
    { "Textures/Palettes.utx",  "Texture palettes",      0 },
    { "Maps",                   "Maps directory",        1 },
};

void dxl_install_probe(const char *game_dir, dxl_install *out) {
    memset(out, 0, sizeof *out);
    out->game_dir = dxl_xstrdup(game_dir ? game_dir : "");

    char *sys = dxl_path_resolve_ci(out->game_dir, "System");
    out->system_dir = sys ? sys : dxl_path_join(out->game_dir, "System");

    size_t n = sizeof required / sizeof *required;
    if (n > DXL_INSTALL_MAX_ITEMS) n = DXL_INSTALL_MAX_ITEMS;
    out->item_count = (int)n;

    for (size_t i = 0; i < n; i++) {
        dxl_install_item *it = &out->items[i];
        it->relative = required[i].rel;
        it->label    = required[i].label;
        it->is_dir   = required[i].dir;
        it->resolved = dxl_path_resolve_ci(out->game_dir, required[i].rel);
        it->found    = it->resolved &&
                       (it->is_dir ? dxl_path_is_dir(it->resolved)
                                   : dxl_path_size(it->resolved) > 0);
        if (!it->found) out->missing_count++;
    }
    out->ok = (out->missing_count == 0);
}

void dxl_install_free(dxl_install *in) {
    if (!in) return;
    for (int i = 0; i < in->item_count; i++) free(in->items[i].resolved);
    free(in->game_dir);
    free(in->system_dir);
    memset(in, 0, sizeof *in);
}

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

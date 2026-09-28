/* The CD check.
 *
 * The original loops on <CdPath>Textures\Palettes.utx, showing a modal box
 * until the disc appears (dx-reverse-info/launch-flow.md section 8). The
 * shipped DeusEx.ini sets CdPath=..\, so on a GOG or Steam install the check
 * passes against the game's own Textures\ directory and never prompts.
 *
 * Lookups are case-insensitive: the 2000-era names are mixed, and an install
 * copied from another filesystem may have any case.
 */
#ifndef DXL_INSTALL_H
#define DXL_INSTALL_H

#include "core/common.h"

/* Returns 1 if the path is empty (nothing to check) or the palette file is
 * there and not empty -- the original wants it larger than 0 bytes
 * (dx-reverse-info/launch-flow.md section 8). */
int dxl_install_cd_ok(const char *game_dir, const char *cd_path);

#endif

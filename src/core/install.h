/* Game-data validation.
 *
 * The original's equivalent is the CD check: loop on
 * <CdPath>Textures\Palettes.utx, showing a modal box until the disc appears
 * (dx-reverse-info/launch-flow.md section 8). On a handheld the real first-run failure
 * is not a missing disc, it is that the user has not copied their game files
 * across yet -- so this generalises to "is there an install here, and if not,
 * which pieces are missing".
 *
 * Lookups are case-insensitive: the SD card is exFAT, but an install copied
 * via a Linux host may have any case, and the 2000-era names are mixed.
 */
#ifndef DXL_INSTALL_H
#define DXL_INSTALL_H

#include "core/common.h"

#define DXL_INSTALL_MAX_ITEMS 8

typedef struct {
    const char *relative;   /* e.g. "System/DeusEx.u" */
    const char *label;      /* shown on the install screen */
    int         is_dir;
    int         found;
    char       *resolved;   /* actual path when found; owned by dxl_install */
} dxl_install_item;

typedef struct {
    char *game_dir;
    char *system_dir;       /* <game_dir>/System, case-resolved */
    int   ok;               /* every required item present */
    int   missing_count;
    dxl_install_item items[DXL_INSTALL_MAX_ITEMS];
    int   item_count;
} dxl_install;

/* Probes game_dir. Always fills the item table, so the install screen can
 * show precisely what is absent rather than a generic failure. */
void dxl_install_probe(const char *game_dir, dxl_install *out);
void dxl_install_free(dxl_install *in);

/* The CD check proper, kept for installs that still set CdPath. Returns 1 if
 * the path is empty (nothing to check) or the palette file is present. */
int dxl_install_cd_ok(const char *game_dir, const char *cd_path);

#endif

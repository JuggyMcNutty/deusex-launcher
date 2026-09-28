/* The game's configuration, as the engine will actually read it.
 *
 * Three ini files matter, and which one a key must go into depends on what
 * the engine has already done:
 *
 *   <Package>.ini   DeusEx.ini. The launcher's own contract keys live here
 *                   ([FirstRun], CdPath) exactly as dx-reverse-info/ini-keys.md says.
 *   SE-<Package>.ini
 *                   Surreal Engine writes this on its first clean exit and
 *                   from then on reads *only* it, with client settings under
 *                   [Engine.SurrealClient] instead of [WinDrv.WindowsClient].
 *   SE-User.ini / User.ini
 *                   Key and pad bindings; SE-User.ini wins once it exists.
 *
 * So engine-facing settings (brightness, decals, bindings) go through the
 * "client" and "user" accessors below, which pick the file the engine will
 * load next time, and only the contract keys go to <Package>.ini directly.
 *
 * A missing <Package>.ini is created from Default.ini, and a missing User.ini
 * from DefUser.ini -- what UE1's Core does at startup before the launcher
 * ever runs. A <Package>.ini with no [Core.System] Paths is not a config the
 * engine can start from (it fails with "Could not find package Core"), so it
 * is rebuilt from Default.ini with its own values laid on top.
 */
#ifndef DXL_CONFIG_H
#define DXL_CONFIG_H

#include "core/common.h"
#include "core/ini.h"

/* [FirstRun] FirstRun is an engine-version integer, not a boolean.
 * dx-reverse-info/wizard.md "The FirstRun version gates". 1100 was confirmed live to
 * be the engine version itself ("Init: Version: 1100"). */
#define DXL_FIRSTRUN_MIGRATE_BELOW 220
#define DXL_FIRSTRUN_WIZARD_BELOW  400
#define DXL_FIRSTRUN_CURRENT       1100

typedef struct dxl_config dxl_config;

/* Opens <system_dir>/<package>.ini and the engine's companions. Never fails:
 * with no ini and no Default.ini the result is an empty, writable config. */
dxl_config *dxl_config_open(const char *system_dir, const char *package);
void        dxl_config_free(dxl_config *c);

/* Writes whichever of the files changed. */
int         dxl_config_save(dxl_config *c, dxl_err *err);
/* Reads the files again from disk, as UE1's Flush(1) makes the next read
 * do -- after another process has written them. A file that is not there
 * keeps what was read before. */
void        dxl_config_reload(dxl_config *c);
int         dxl_config_dirty(const dxl_config *c);
const char *dxl_config_path(const dxl_config *c);
dxl_ini    *dxl_config_ini(dxl_config *c);

/* How <Package>.ini came to be: 0 loaded as is, 1 created from Default.ini,
 * 2 rebuilt from Default.ini because it was not a usable config. */
int         dxl_config_seeded(const dxl_config *c);

/* The ini and section the engine reads client settings from. */
dxl_ini    *dxl_config_client_ini(dxl_config *c);
const char *dxl_config_client_section(const dxl_config *c);
const char *dxl_config_client_path(const dxl_config *c);

/* The ini the engine reads bindings from (SE-User.ini, else User.ini). NULL
 * only when neither exists and there is no DefUser.ini to create one from. */
dxl_ini    *dxl_config_user_ini(dxl_config *c);
const char *dxl_config_user_path(const dxl_config *c);

int  dxl_config_first_run(const dxl_config *c);
/* Clamps up to DXL_FIRSTRUN_CURRENT, never down -- the original only ever
 * raises this value (0x1090B997). */
void dxl_config_clamp_first_run(dxl_config *c);

const char *dxl_config_render_device(const dxl_config *c);
const char *dxl_config_cd_path(const dxl_config *c);
const char *dxl_config_game_engine(const dxl_config *c);

/* Client settings the engine honours (USurrealClient). Brightness is 0..1. */
double dxl_config_brightness(dxl_config *c);
void   dxl_config_set_brightness(dxl_config *c, double v);
int    dxl_config_decals(dxl_config *c);
void   dxl_config_set_decals(dxl_config *c, int on);

/* Deletes SE-<package>.ini, SE-User.ini, <package>.ini and User.ini, so the
 * next dxl_config_open rebuilds all of it from Default.ini and DefUser.ini.
 * Refuses (returns -1, deleting nothing) when Default.ini is missing, since
 * there would be nothing to rebuild from. */
int  dxl_config_reset_files(const char *system_dir, const char *package, dxl_err *err);

#endif

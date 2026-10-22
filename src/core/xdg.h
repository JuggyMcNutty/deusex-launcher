/* The launcher's own folders, where the platform keeps an app's
 * (platform/folders.h): Windows' Known Folders (platform/win32/folders.c,
 * docs/LAUNCHER.md), and on POSIX the XDG base directories, each with a
 * dxlauncher folder in it (freedesktop.org's Base Directory spec).
 *
 *   config  $XDG_CONFIG_HOME, else ~/.config       launcher.ini; each profile's
 *                                                  inis and the engine's folder
 *   data    $XDG_DATA_HOME, else ~/.local/share    each profile's saves and shots
 *   state   $XDG_STATE_HOME, else ~/.local/state   each profile's Running.ini and
 *                                                  run logs
 *   cache   $XDG_CACHE_HOME, else ~/.cache         each profile's downloads
 *
 * A variable that is empty or not an absolute path counts as unset, as the
 * spec has it. DXL_HOME, when set, puts all four under it ($DXL_HOME/config,
 * data, state and cache): a scratch home for a first run from nothing, and
 * the tests'. Nothing is ever written to the game's folder.
 */
#ifndef DXL_XDG_H
#define DXL_XDG_H

#include "core/common.h"

#define DXL_APP_DIR "dxlauncher"

typedef struct {
    char *config;      /* <config home>/dxlauncher */
    char *data;        /* <data home>/dxlauncher */
    char *state;       /* <state home>/dxlauncher */
    char *cache;       /* <cache home>/dxlauncher */
    char *data_home;   /* <data home> itself: the desktop entry and icons go under it */
} dxl_xdg;

/* Fills x; creates nothing. -1 when there is no home to put them in. */
int  dxl_xdg_open(dxl_xdg *x, dxl_err *err);
void dxl_xdg_free(dxl_xdg *x);

/* Creates the four folders. */
int  dxl_xdg_make(const dxl_xdg *x, dxl_err *err);

#endif

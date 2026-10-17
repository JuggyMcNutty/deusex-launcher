/* A Deus Ex install: whether a folder is one, which build, and what is
 * missing from it.
 *
 * A folder is an install when System/DeusEx.exe is there by that exact
 * name, which is how the engine finds it; the engine plays it only when
 * that file's SHA1 is a build it knows (core/gamedb.h). Beside it: the
 * packages the engine cannot start without, the defaults a profile is made
 * from, and the menu's map -- System's Core.u, Engine.u, Extension.u,
 * DeusEx.u, DeusExUI.u, Default.ini and DefUser.ini, and Maps/DX.dx -- in
 * whatever case the copy left them (dxl_path_resolve_ci). Nothing is
 * written.
 */
#ifndef DXL_INSTALL_H
#define DXL_INSTALL_H

#include "core/common.h"
#include "core/gamedb.h"

typedef struct {
    char *root;           /* the folder, made absolute */
    char *system_dir;     /* <root>/System, as found */
    char  sha1[41];       /* DeusEx.exe's; "" when there is none */
    const dxl_game_build *build;   /* NULL: none the engine takes */
    int   found;          /* System/DeusEx.exe is there */
    int   ok;             /* playable: a known build, nothing missing */
    char  missing[256];   /* what is not there, ", " between */
    char  problem[320];   /* why it cannot be played, or a warning; "" for none */
} dxl_install;

/* Looks at root. 0 when it is an install the engine plays (ok), -1
 * otherwise; out is filled either way, so a screen can say what is wrong. */
int  dxl_install_check(const char *root, dxl_install *out);
void dxl_install_free(dxl_install *in);

/* A short id for launcher.ini's [Install.<id>]: the version and a hash of
 * the folder, "1112fm-1a2b3c4d". */
void dxl_install_id(const dxl_install *in, char *out, size_t size);

#endif

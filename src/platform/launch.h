/* Handing over to the game: the one step whose mechanism is the platform's.
 *
 * On POSIX (platform/posix/launch.c) the launcher's last act is to exec the
 * configured game command with the command line it was given -- exec rather
 * than fork+wait, as the original ended its own process: nothing of the
 * launcher stays resident while the game runs.
 *
 * A platform that cannot start a second program implements this another way.
 * On Android the engine has to run inside the app's own process, so there it
 * would call into the engine library instead (ports/android/README.md).
 */
#ifndef DXL_LAUNCH_H
#define DXL_LAUNCH_H

#include "core/common.h"

/* Starts the game: exe with flags split by dxl_argv_build, in workdir (may
 * be NULL). Only returns on failure. */
int dxl_platform_launch(const char *exe, const char *flags, const char *workdir, dxl_err *err);

#endif

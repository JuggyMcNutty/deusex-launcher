/* Replacing this process with another: the one step whose mechanism is the
 * platform's. Safe mode starts the launcher again with its flags this way
 * (launch/winmain.c); the game itself is a child the launcher waits for
 * (process.h).
 *
 * On POSIX (platform/posix/launch.c) it is exec, with the command line it was
 * given -- rather than fork+wait: nothing of this process stays resident.
 *
 * A platform that cannot start a second program implements this another way:
 * where the engine has to run inside the app's own process, it would call
 * into the engine library instead.
 */
#ifndef DXL_LAUNCH_H
#define DXL_LAUNCH_H

#include "core/common.h"

/* Replaces this process with exe -- the launcher again, for safe mode --
 * with flags split by dxl_argv_build, in workdir (may be NULL). Only
 * returns on failure. */
int dxl_platform_launch(const char *exe, const char *flags, const char *workdir, dxl_err *err);

#endif

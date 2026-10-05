/* Replacing this process with another: the one step whose mechanism is the
 * platform's. The port branches hand over to the game this way, and main's
 * safe mode starts the launcher again with its flags (launch/winmain.c); on
 * main the game itself is a child the launcher waits for (process.h).
 *
 * On POSIX (platform/posix/launch.c) it is exec, with the command line it was
 * given -- rather than fork+wait: nothing of this process stays resident.
 *
 * A platform that cannot start a second program implements this another way.
 * On Android the engine has to run inside the app's own process, so there it
 * would call into the engine library instead (the android branch's
 * ports/android/README.md).
 */
#ifndef DXL_LAUNCH_H
#define DXL_LAUNCH_H

#include "core/common.h"

/* Replaces this process with exe -- the game on the port branches, the
 * launcher again for main's safe mode -- with flags split by
 * dxl_argv_build, in workdir (may be NULL). Only returns on failure. */
int dxl_platform_launch(const char *exe, const char *flags, const char *workdir, dxl_err *err);

#endif

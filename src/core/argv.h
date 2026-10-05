/* The argument vector a program is started with.
 *
 * The launcher keeps its command line as one string, as the original did
 * (dx-reverse-info/cli-flags.md: appStrfind matches anywhere), and splits it
 * only here, when it starts a program: the game (platform/process.h on main,
 * the port branches' hand-over in platform/launch.h), the Renderer page's
 * detection run, and safe mode's relaunch with the flag string its eight
 * boxes build (platform/launch.h). Quotes group words and are dropped, as a
 * shell's are.
 */
#ifndef DXL_ARGV_H
#define DXL_ARGV_H

#include "core/common.h"

/* Splits a flag string into an argv for exec. Caller frees with
 * dxl_argv_free. */
char **dxl_argv_build(const char *exe, const char *flags);
void   dxl_argv_free(char **argv);

#endif

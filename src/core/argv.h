/* The argument vector the game is started with.
 *
 * The launcher keeps its command line as one string, as the original did
 * (dx-reverse-info/cli-flags.md: appStrfind matches anywhere), and splits it exactly
 * once, here, when handing over to the game. The hand-over itself is the
 * platform's: platform/launch.h.
 *
 * The original also re-executed itself for safe mode with a flag string built
 * from eight checkboxes. Surreal Engine honours none of those flags, so that
 * path is gone; dx-reverse-info/README.md records why.
 */
#ifndef DXL_ARGV_H
#define DXL_ARGV_H

#include "core/common.h"

/* Splits a flag string into an argv for exec. Caller frees with
 * dxl_argv_free. */
char **dxl_argv_build(const char *exe, const char *flags);
void   dxl_argv_free(char **argv);

#endif

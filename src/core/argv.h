/* The argument vector a program is started with.
 *
 * The launcher keeps the original's command line as one string, as the
 * original did (dx-reverse-info/cli-flags.md: appStrfind matches anywhere),
 * and passes it to the engine whole (--cmdline). It splits only a string of
 * the engine's own options here (DXL_ENGINE_ARGS; core/engine_args.h).
 * Quotes group words and are dropped, as a shell's are.
 */
#ifndef DXL_ARGV_H
#define DXL_ARGV_H

#include "core/common.h"

/* Splits a flag string into an argv for exec. Caller frees with
 * dxl_argv_free. */
char **dxl_argv_build(const char *exe, const char *flags);
void   dxl_argv_free(char **argv);

#endif

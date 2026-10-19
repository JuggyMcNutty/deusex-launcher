/* The argument vector a program is started with.
 *
 * The launcher keeps its command line as one string, as the original did
 * (dx-reverse-info/cli-flags.md: appStrfind matches anywhere), and hands it
 * on as one: a program it starts -- the game's run-game.sh (platform/
 * process.h), the Renderer page's detection run, and safe mode's relaunch
 * with the flag string its eight boxes build (platform/launch.h) -- gets the
 * line as its one argument, its quotes and spaces as they are. The launcher
 * started so takes it back whole (cmdline.h), and run-game.sh hands it to
 * the engine so (--cmdline=).
 */
#ifndef DXL_ARGV_H
#define DXL_ARGV_H

#include "core/common.h"

/* exe, then the line as one argument (none when it is empty): an argv for
 * exec. Caller frees with dxl_argv_free. */
char **dxl_argv_line(const char *exe, const char *line);
void   dxl_argv_free(char **argv);

#endif

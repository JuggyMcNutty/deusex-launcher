/* The port's game hook, port-hooks.sh beside the launcher, run around each
 * game: "before <CPU mode>" once the window has gone, "after" once the game
 * has ended, however it ended (the Smart Pro's sets the CPU mode and puts
 * it back: core/cpumode.h). Each is at most 10 s; what it says goes into
 * the run's last-run.log, with a line in the launcher's log. A hook
 * missing, failing or late never stops the game. */
#ifndef DXL_GAMEHOOK_H
#define DXL_GAMEHOOK_H

#include "app/run.h"

#define DXL_GAMEHOOK    "port-hooks.sh"
#define DXL_GAMEHOOK_MS 10000

/* when: "before" or "after"; log_path the run's log. */
void dxl_gamehook(const dxl_run *r, const char *log_path, const char *when);

#endif

/* The Smart Pro's CPU modes, spruceOS's own: how fast the CPU runs while
 * the game does. launcher.ini's [Launcher] CpuMode, the Graphics page's
 * row; set by the app's port-hooks.sh just before the game starts, and put
 * back as it ends (app/gamehook.h). */
#ifndef DXL_CPUMODE_H
#define DXL_CPUMODE_H

#include "core/ini.h"
#include "core/settings.h"

/* For none kept, or one not among the modes; port-hooks.sh's too
 * (tests/test_cpumode.c). */
#define DXL_CPU_MODE_DEFAULT "Overclock"

/* The modes, as the row's choices: ends with a NULL value. */
extern const dxl_set_choice dxl_cpu_modes[];

/* The mode launcher.ini keeps, or the default. */
const char *dxl_cpu_mode(const dxl_ini *launcher);

#endif

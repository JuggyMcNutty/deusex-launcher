#include "core/cpumode.h"

#include <string.h>

const dxl_set_choice dxl_cpu_modes[] = {
    { "Smart", "Smart",
      "The CPU speeds up and down with load, as in the spruceOS menu. Saves battery; Deus Ex runs noticeably "
      "slower." },
    { "Performance", "Performance",
      "All four cores held at 1.8 GHz while the game runs, as spruceOS does for its Ports. Put back as it was "
      "when you quit." },
    { "Overclock", "Overclock",
      "All four cores at 2.0 GHz. The fastest, but the handheld runs warmer and the battery drains sooner." },
    { NULL, NULL, NULL },
};

const char *dxl_cpu_mode(const dxl_ini *launcher) {
    const char *v = launcher ? dxl_ini_get(launcher, "Launcher", "CpuMode") : NULL;
    for (int i = 0; v && dxl_cpu_modes[i].value; i++)
        if (!strcmp(dxl_cpu_modes[i].value, v)) return dxl_cpu_modes[i].value;
    return DXL_CPU_MODE_DEFAULT;
}

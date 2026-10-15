#include "platform/machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>

/* The clock in MHz: /proc/cpuinfo's "cpu MHz" where the CPU reports one, else
 * cpufreq's maximum; 0 when neither says. */
static double cpu_mhz(void) {
    double mhz = 0;
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof line, f))
            if (strncmp(line, "cpu MHz", 7) == 0) {
                const char *colon = strchr(line, ':');
                if (colon) mhz = atof(colon + 1);
                break;
            }
        fclose(f);
    }
    if (mhz > 0) return mhz;
    f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    if (f) {
        long khz = 0;
        if (fscanf(f, "%ld", &khz) == 1) mhz = khz / 1000.0;
        fclose(f);
    }
    return mhz;
}

void dxl_machine_probe(dxl_machine *m) {
    memset(m, 0, sizeof *m);
    struct sysinfo si;
    if (sysinfo(&si) == 0)
        m->memory = (unsigned long long)si.totalram * (unsigned long long)si.mem_unit;
#if defined(__i386__) || defined(__x86_64__)
    m->mmx = __builtin_cpu_supports("mmx");
#endif
    m->cpu_mhz = cpu_mhz();
}

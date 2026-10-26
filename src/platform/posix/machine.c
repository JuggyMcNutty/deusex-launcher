#include "platform/machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>

/* The CPU's rated clock in MHz, as the original's figure is its cycle
 * counter's rate (GSecondsPerCycle; dx-reverse-info/ini-keys.md), a
 * constant: cpufreq's maximum, else, where there is no cpufreq (a VM),
 * /proc/cpuinfo's "cpu MHz", the clock its counter runs at; 0 when neither
 * says. cpu MHz on a host is a core's clock of the moment, an idle one's
 * far under its rating. */
static double cpu_mhz(void) {
    double mhz = 0;
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    if (f) {
        long khz = 0;
        if (fscanf(f, "%ld", &khz) == 1) mhz = khz / 1000.0;
        fclose(f);
    }
    if (mhz > 0) return mhz;
    f = fopen("/proc/cpuinfo", "r");
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

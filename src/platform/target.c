#include "platform/target.h"

#include "core/common.h"

int dxl_target_cpu_mode_count(const dxl_target *t) {
    int n = 0;
    if (t->cpu_modes)
        while (t->cpu_modes[n].name) n++;
    return n;
}

const dxl_cpu_mode_info *dxl_target_cpu_mode(const dxl_target *t, const char *name) {
    int n = dxl_target_cpu_mode_count(t);
    if (n == 0) return NULL;
    const dxl_cpu_mode_info *fallback = &t->cpu_modes[0];
    for (int i = 0; i < n; i++) {
        if (name && dxl_stricmp(name, t->cpu_modes[i].name) == 0) return &t->cpu_modes[i];
        if (t->cpu_mode_default && dxl_stricmp(t->cpu_mode_default, t->cpu_modes[i].name) == 0)
            fallback = &t->cpu_modes[i];
    }
    return fallback;
}

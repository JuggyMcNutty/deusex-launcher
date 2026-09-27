/* The device profile: what the launcher needs to know about where it runs.
 *
 * A port can supply its own as ports/<id>/target.c; a port without one gets
 * platform/target_default.c, a generic desktop. The build compiles exactly
 * one (DXL_PROFILE, which defaults to the port), so -DDXL_PROFILE=<id> on a
 * host build shows another device's screens -- how dxl-shots renders the
 * handheld's UI on a PC. Port Ex Machina's docs/PORTING.md says what a new device needs.
 *
 * Only facts about the device belong here. Facts about a GPU (the PowerVR's
 * MSAA resolve) follow the GPU wherever it turns up: core/renderers.h.
 */
#ifndef DXL_TARGET_H
#define DXL_TARGET_H

#include "core/renderers.h"

typedef struct {
    const char *name;   /* as stored in launcher.ini CpuMode and read by port-hooks.sh */
    const char *help;   /* the Video tab's line for it */
} dxl_cpu_mode_info;

typedef struct {
    const char *id;               /* the port's directory name */
    const char *about;            /* System tab, under the launcher version */
    /* Fonts to try before the generic desktop list (ui/theme.c).
     * NULL-terminated; the pointer itself may be NULL. */
    const char *const *fonts;
    /* The screen the launcher is laid out for: the window size when the
     * video driver reports none, and dxl-shots' canvas. */
    int panel_w, panel_h;
    /* CPU modes the port's run-game hooks can apply, first to last as the
     * Video tab cycles them, terminated by { NULL, NULL }. NULL when the
     * device offers none: the row is hidden and CpuMode is never written. */
    const dxl_cpu_mode_info *cpu_modes;
    const char *cpu_mode_default;
    /* Controls tab, with a pad connected: what the built-in controls report
     * themselves as. NULL when there is nothing device-specific to say. */
    const char *pad_note;
    /* What dxl-shots pretends the GPU probe found, so its screenshots show
     * this device's renderer list and any GPU-specific rows. */
    dxl_gpu_probe preview_gpu;
} dxl_target;

/* The profile compiled into this build. */
const dxl_target *dxl_target_get(void);

/* Number of CPU modes; 0 when the device offers none. */
int dxl_target_cpu_mode_count(const dxl_target *t);

/* The mode called name (case-insensitive), else the default mode, else NULL
 * when the device offers none. */
const dxl_cpu_mode_info *dxl_target_cpu_mode(const dxl_target *t, const char *name);

#endif

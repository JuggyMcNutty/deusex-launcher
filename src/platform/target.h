/* The device profile: what the launcher needs to know about where it runs,
 * here a desktop's (platform/target_default.c). Port Ex Machina's
 * docs/PORTING.md says what a new device needs.
 */
#ifndef DXL_TARGET_H
#define DXL_TARGET_H

#include "core/renderers.h"

typedef struct {
    const char *id;               /* which profile: "generic" */
    const char *about;            /* System tab, under the launcher version */
    /* The launcher's window, and the size assumed when the video driver
     * reports none. */
    int panel_w, panel_h;
    /* What dxl-shots pretends the GPU probe found, so its screenshots show
     * a renderer list. */
    dxl_gpu_probe preview_gpu;
} dxl_target;

/* The profile compiled into this build. */
const dxl_target *dxl_target_get(void);

#endif

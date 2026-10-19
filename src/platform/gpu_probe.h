/* GPU detection, isolated from the launcher process.
 *
 * Loads the Vulkan loader and EGL with dlopen, in a forked child, and reports
 * what they offer. A driver that crashes or hangs during initialisation takes
 * the child down, not the launcher -- the same reason the original ran
 * -testrendev in a child process (dx-reverse-info/wizard.md, Renderer page).
 *
 * Neither library is linked: a device with no Vulkan loader must still start
 * the launcher and simply show Vulkan as unavailable.
 *
 * dxl-cli --probe runs it. The launcher asks dxl-cli, a process of its own,
 * with its window up: for the renderers the Graphics page offers and its
 * anti-aliasing, and before a game that would multisample (app/run.c).
 */
#ifndef DXL_GPU_PROBE_H
#define DXL_GPU_PROBE_H

#include "core/renderers.h"

/* Fills out (always -- on failure out->probed stays 0 and out->note says
 * why). Returns 0 if the child reported back in time. */
int dxl_gpu_probe_run(dxl_gpu_probe *out, int timeout_ms);

#endif

/* Headless driver.
 *
 * The launcher's entire contract with no display attached: what would it
 * decide, and what would it write. This exists so the ini/sentinel/settings
 * behaviour can be verified on the device -- over SSH, before the SDL
 * frontend is in the picture -- and so a broken install can be diagnosed
 * without a screen.
 */
#include "app.h"
#include "core/log.h"
#include "platform/gpu_probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *argv0) {
    printf(
        "usage: %s [--dry-run [--probe] | --probe | --help] [launcher flags...]\n"
        "\n"
        "  --dry-run      print the decision, the config and the engine settings as\n"
        "                 they stand; write nothing\n"
        "  --probe        detect Vulkan and OpenGL ES on this device and print what\n"
        "                 was found (with --dry-run: include it in the renderer list)\n"
        "  --help         this text\n"
        "\n"
        "Launcher flags are passed through and parsed exactly as the original does\n"
        "(-safe, -firstrun, -changevideo, -log, -consolecommand=, -testrendev=, ...).\n",
        argv0);
}

static int probe_main(void) {
    dxl_gpu_probe p;
    int rc = dxl_gpu_probe_run(&p, 4000);
    if (rc != 0) {
        printf("probe failed: %s\n", p.note);
        return 1;
    }
    printf("vulkan     %s\n", p.vulkan ? "yes" : "no");
    if (p.vulkan) printf("  device   %s\n  api      %s\n", p.vulkan_device, p.vulkan_version);
    printf("opengl es  %s\n", p.gles ? "yes" : "no");
    if (p.gles) {
        if (p.gles_renderer[0]) printf("  renderer %s\n", p.gles_renderer);
        printf("  version  %s\n", p.gles_version);
    }
    printf("opengl     %s\n", p.gl ? "yes" : "no");
    printf("software   yes (no driver needed)\n");
    return 0;
}

int main(int argc, char **argv) {
    int dry = 0, probe = 0, first = 1;
    for (; first < argc; first++) {
        if      (!strcmp(argv[first], "--help"))    { usage(argv[0]); return 0; }
        else if (!strcmp(argv[first], "--dry-run")) dry = 1;
        else if (!strcmp(argv[first], "--probe"))   probe = 1;
        else break;
    }
    if (!dry && probe) return probe_main();
    if (!dry) { usage(argv[0]); return 2; }

    /* A dry run must not touch the install -- not even to write a log line
     * saying it did nothing. */
    dxl_log_set_echo(0);
    dxl_log_set_to_file(0);

    /* Hide our own options from the launcher's command line, so they do not
     * accidentally become part of what gets parsed or forwarded. */
    char *filtered[64];
    int n = 0;
    filtered[n++] = argv[0];
    for (int i = first; i < argc && n < 63; i++) filtered[n++] = argv[i];
    filtered[n] = NULL;

    dxl_app app;
    dxl_err err;
    if (dxl_app_init(&app, n, filtered, &err) != 0) {
        fprintf(stderr, "init failed: %s\n", dxl_err_msg(&err));
        return 1;
    }
    if (probe) dxl_app_probe_gpu(&app);
    dxl_app_dry_run(&app);
    dxl_app_shutdown(&app);
    return 0;
}

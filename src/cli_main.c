/* dxl-cli: the launcher with no window, for scripts and for looking.
 *
 *   dxl-cli --dry-run [--game=<dir>] [--profile=<id>] [words...]
 *   dxl-cli --print-command [...]     the engine's command line, nothing run
 *   dxl-cli --import --game=<dir> [--profile=<id>]
 *   dxl-cli --play [...]              play, no Home (app/run.h)
 *   dxl-cli --probe [--ini]           the GPU this machine has; --ini, for a program
 *
 * The words are the original's command line (-safe, -changevideo, a map
 * with -hax0r, ...), as deusex-launcher takes them.
 */
#define _GNU_SOURCE
#include "app/run.h"
#include "platform/gpu_probe.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DXL_VERSION
#define DXL_VERSION "unversioned"
#endif

static volatile int stop;

static void on_signal(int sig) {
    (void)sig;
    stop = 1;
}

static void usage(FILE *f) {
    fprintf(f,
        "usage: dxl-cli --dry-run | --print-command | --import | --play | --probe\n"
        "               [--game=<dir>] [--profile=<id>] [the original's command line...]\n"
        "\n"
        "  --dry-run        what the launcher would decide, make and run; nothing is written\n"
        "  --print-command  the engine's command line for this profile; nothing is run\n"
        "  --import         a profile made from --game's install, nothing run\n"
        "  --play           play, with no Home screen; a screen that is due is a message\n"
        "  --probe [--ini]  the Vulkan, OpenGL ES and OpenGL this machine offers; --ini as\n"
        "                   the [Probe] section the launcher's window reads\n"
        "  --game=<dir>     the game's folder: the first run's, or the profile's from now on\n"
        "  --profile=<id>   another profile than launcher.ini's (default: default)\n"
        "\n"
        "Files: the launcher's own under $XDG_CONFIG_HOME, $XDG_DATA_HOME, $XDG_STATE_HOME\n"
        "and $XDG_CACHE_HOME, each in dxlauncher/ (DXL_HOME=<dir>: all four under <dir>);\n"
        "nothing in the game's folder. DXL_ENGINE=<path> runs another engine, and\n"
        "DXL_ENGINE_ARGS more of its own options.\n");
}

static int probe(int as_ini) {
    dxl_gpu_probe p;
    int rc = dxl_gpu_probe_run(&p, 4000);
    if (as_ini) {
        char *text = dxl_gpu_probe_to_ini(&p);
        fputs(text, stdout);
        free(text);
        return rc != 0;
    }
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
    return 0;
}

int main(int argc, char **argv) {
    int mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { usage(stdout); return 0; }
        if (strcmp(argv[i], "--version") == 0) { printf("dxl-cli %s\n", DXL_VERSION); return 0; }
        if (strcmp(argv[i], "--probe") == 0) {
            int as_ini = i + 1 < argc && strcmp(argv[i + 1], "--ini") == 0;
            return probe(as_ini);
        }
        if (!strcmp(argv[i], "--dry-run") || !strcmp(argv[i], "--print-command") ||
            !strcmp(argv[i], "--import") || !strcmp(argv[i], "--play"))
            mode = 1;
    }
    if (!mode) {
        usage(stderr);
        return DXL_RUN_EXIT_USAGE;
    }
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;   /* no SA_RESTART: the wait ends at once */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    return dxl_run_main(argc, argv, NULL, &stop);
}

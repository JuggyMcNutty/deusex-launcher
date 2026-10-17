#define _GNU_SOURCE
#include "core/install.h"
#include "core/paths.h"
#include "core/sha1.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* What the engine opens before anything else, and what a profile is made
 * from, relative to the game's folder. */
static const char *const required[] = {
    "System/Core.u", "System/Engine.u", "System/Extension.u", "System/DeusEx.u",
    "System/DeusExUI.u", "System/Default.ini", "System/DefUser.ini", "Maps/DX.dx",
};

static void add_missing(dxl_install *in, const char *what) {
    size_t n = strlen(in->missing);
    snprintf(in->missing + n, sizeof in->missing - n, "%s%s", n ? ", " : "", what);
}

int dxl_install_check(const char *root, dxl_install *out) {
    memset(out, 0, sizeof *out);
    char *abs = realpath(root, NULL);
    out->root = abs ? abs : dxl_xstrdup(root);

    char *sys = dxl_path_resolve_ci(out->root, "System");
    out->system_dir = sys ? sys : dxl_path_join(out->root, "System");

    /* Exactly this name: the engine looks for it so. */
    char *exe = dxl_path_join(out->system_dir, "DeusEx.exe");
    out->found = dxl_path_exists(exe) && !dxl_path_is_dir(exe);
    if (out->found && dxl_sha1_file(exe, out->sha1) != 0) out->sha1[0] = '\0';
    free(exe);

    if (!out->found) {
        snprintf(out->problem, sizeof out->problem, "no System/DeusEx.exe in %s", out->root);
        return -1;
    }
    for (size_t i = 0; i < sizeof required / sizeof *required; i++) {
        char *p = dxl_path_resolve_ci(out->root, required[i]);
        if (!p) add_missing(out, required[i]);
        free(p);
    }

    out->build = dxl_gamedb_find(out->sha1);
    if (!out->build) {
        snprintf(out->problem, sizeof out->problem,
                 "System/DeusEx.exe is not a build the engine plays (SHA1 %s): it wants 1112fm",
                 out->sha1[0] ? out->sha1 : "unreadable");
    } else if (out->missing[0]) {
        snprintf(out->problem, sizeof out->problem, "files missing: %s", out->missing);
    } else if (!out->build->current) {
        snprintf(out->problem, sizeof out->problem,
                 "Deus Ex %s (%s): the engine is made against 1112fm, so this build may not play as it should",
                 out->build->version, out->build->edition);
    }
    out->ok = out->build && !out->missing[0];
    return out->ok ? 0 : -1;
}

void dxl_install_free(dxl_install *in) {
    if (!in) return;
    free(in->root);
    free(in->system_dir);
    memset(in, 0, sizeof *in);
}

void dxl_install_id(const dxl_install *in, char *out, size_t size) {
    unsigned long h = 2166136261UL;   /* FNV-1a, as instance.c's */
    for (const unsigned char *p = (const unsigned char *)(in->root ? in->root : ""); *p; p++) {
        h ^= *p;
        h *= 16777619UL;
    }
    snprintf(out, size, "%s-%08lx", in->build ? in->build->version : "unknown", h & 0xffffffffUL);
}

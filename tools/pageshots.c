/* pageshots: the launcher's screens as pictures, with no display.
 *
 *   pageshots <System dir> <out dir> [<memory MB> <MHz> <MMX 0/1>]
 *
 * Every screen is drawn into <out dir>/<name>.bmp, its client area only,
 * under the name the original's capture of it has (dx-reverse-info/tools/
 * wine/wizard-capture.sh), so the two can be laid side by side: crop the
 * capture at its client origin to the same size. The pages read the game's
 * strings and configuration from the System dir, as the launcher does, and
 * nothing is written there. The machine is the Detail page's: 512 MB, 3000
 * MHz and MMX unless given.
 */
#include "core/config.h"
#include "core/localize.h"
#include "core/paths.h"
#include "gui/msgbox.h"
#include "gui/splash.h"
#include "wizard/wizard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *out_dir;
static int failures;

static char *out_path(const char *name) {
    char leaf[128];
    snprintf(leaf, sizeof leaf, "%s.bmp", name);
    return dxl_path_join(out_dir, leaf);
}

static void save_wizard(dxl_wizard *w, const char *name) {
    char *p = out_path(name);
    if (dxl_wizard_save(w, p) != 0) { fprintf(stderr, "%s: %s\n", p, SDL_GetError()); failures++; }
    else printf("%s\n", p);
    free(p);
}

static void save_box(dxl_gui *g, const dxl_msgbox_spec *s, const char *name) {
    dxl_err e;
    dxl_dialog *d = dxl_msgbox_build(g, s, 1, &e);
    char *p = out_path(name);
    if (!d || dxl_dialog_save(d, p) != 0) { fprintf(stderr, "%s: failed\n", p); failures++; }
    else printf("%s\n", p);
    free(p);
    if (d) dxl_dialog_close(d);
}

/* A wizard as the launcher opens it for this caption, its pages pushed. */
static dxl_wizard *open_wizard(dxl_gui *g, dxl_wizard_request *req, const char *caption) {
    dxl_err e;
    req->title = dxl_loc_general(req->loc, "Startup", caption);
    dxl_wizard *w = dxl_wizard_new(g, req, 1, &e);
    if (!w) { fprintf(stderr, "pageshots: %s\n", dxl_err_msg(&e)); exit(1); }
    return w;
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 6) {
        fprintf(stderr, "usage: pageshots <System dir> <out dir> [<memory MB> <MHz> <MMX 0/1>]\n");
        return 2;
    }
    const char *system_dir = argv[1];
    out_dir = argv[2];
    dxl_path_mkdirs(out_dir);

    dxl_gui g;
    dxl_err e;
    if (dxl_gui_init(&g, 0, &e) != 0) { fprintf(stderr, "pageshots: %s\n", dxl_err_msg(&e)); return 1; }
    printf("font: %s\n", g.font_path);

    dxl_config *cfg = dxl_config_open(system_dir, "DeusEx");
    dxl_loc *loc = dxl_loc_open(system_dir,
                                dxl_ini_get(dxl_config_ini(cfg), "Engine.Engine", "Language"));
    dxl_wizard_request req;
    memset(&req, 0, sizeof req);
    req.config = cfg;
    req.loc = loc;
    req.system_dir = system_dir;
    req.package = "DeusEx";
    req.exe_path = "DeusEx";
    req.cmdline = "";
    req.machine.memory = (argc == 6 ? strtoull(argv[3], NULL, 10) : 512ULL) << 20;
    req.machine.cpu_mhz = argc == 6 ? atof(argv[4]) : 3000;
    req.machine.mmx = argc == 6 ? atoi(argv[5]) : 1;

    /* First run: the Renderer page, detecting, then listed; on through
     * Driver and Detail to FirstTime. */
    dxl_wizard *w = open_wizard(&g, &req, "FirstTime");
    dxl_wizard_push(w, DXL_PAGE_RENDERER);
    save_wizard(w, "01-renderer-detecting");
    dxl_wizard_first_paint(w, 0);
    save_wizard(w, "02-renderer-first-compatible");
    dxl_wizard_show_all(w, 1);
    save_wizard(w, "03-renderer-first-all");
    dxl_wizard_show_all(w, 0);
    dxl_wizard_push(w, DXL_PAGE_DRIVER);
    save_wizard(w, "05-after-renderer");
    dxl_wizard_push(w, DXL_PAGE_DETAIL);
    save_wizard(w, "07-detail");
    dxl_wizard_push(w, DXL_PAGE_FIRSTTIME);
    save_wizard(w, "08-firsttime");
    dxl_dialog_click(dxl_wizard_dialog(w), 3);             /* Back */
    save_wizard(w, "09-back-to-detail");
    dxl_wizard_free(w);

    /* -changevideo. */
    w = open_wizard(&g, &req, "Video");
    dxl_wizard_push(w, DXL_PAGE_RENDERER);
    dxl_wizard_first_paint(w, 0);
    save_wizard(w, "11-renderer-video");
    dxl_wizard_free(w);

    /* -safe, and its options. */
    w = open_wizard(&g, &req, "SafeMode");
    dxl_wizard_push(w, DXL_PAGE_SAFEMODE);
    save_wizard(w, "20-safemode");
    dxl_wizard_push(w, DXL_PAGE_SAFEOPTIONS);
    save_wizard(w, "21-safeoptions-defaults");
    /* "Disable 3D sound hardware" cleared as the capture's script clears it,
     * with BM_SETCHECK: no click, so the focus stays where it was. */
    dxl_dialog_find(dxl_wizard_dialog(w), 1109, 1)->checked = 0;
    save_wizard(w, "22-safeoptions-no3dsound-off");
    dxl_wizard_free(w);

    /* A crash's Running.ini. */
    w = open_wizard(&g, &req, "RecoveryMode");
    dxl_wizard_push(w, DXL_PAGE_SAFEMODE);
    save_wizard(w, "30-recoverymode");
    dxl_wizard_free(w);

    /* The CD prompt and -make's error. */
    dxl_msgbox_spec cd = {
        .title = dxl_loc_general(loc, "Window", "InsertCdTitle"),
        .text = dxl_loc_general(loc, "Window", "InsertCdText"),
        .ok_cancel = 1,
    };
    save_box(&g, &cd, "23-cd-prompt");
    dxl_msgbox_spec make = {
        .title = dxl_loc_get(loc, "Window", "Errors", "Critical", 0),
        .text = "'DeusEx -make' is obsolete, use 'ucc make' now\r\n\r\nHistory: ",
        .error_icon = 1,
    };
    save_box(&g, &make, "40-make-error");

    /* The splash, from the bitmap the launcher would pick. */
    char *game = dxl_path_dirname(system_dir);
    char *logo = dxl_path_resolve_ci(game, "Help/DeusExLogo.bmp");
    if (!logo) logo = dxl_path_resolve_ci(game, "Help/Logo.bmp");
    if (logo) {
        char *p = out_path("24-splash");
        if (dxl_splash_save(logo, p) != 0) { fprintf(stderr, "%s: failed\n", p); failures++; }
        else printf("%s\n", p);
        free(p);
    }
    free(logo);
    free(game);

    dxl_loc_free(loc);
    dxl_config_free(cfg);
    dxl_gui_quit(&g);
    return failures ? 1 : 0;
}

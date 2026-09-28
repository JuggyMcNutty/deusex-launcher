/* The config wizard: dx-reverse-info/wizard.md.
 *
 * The frame is Window.dll's WWizardDialog, template 104: a stack of pages,
 * Back from the second page on, Next with the page's own text, Cancel.
 * The six pages are templates 2017 to 2022, laid out from those templates
 * and filled with the game's own strings. Each behaves as the original's:
 *
 *   SafeMode      four buttons: Run, Safe mode, Change video, Web
 *   SafeOptions   eight boxes; Run! starts the launcher again with their flags
 *   Renderer      detection, the render devices, Next writes GameRenderDevice
 *   Driver        the Direct3D card, a link (only after Direct3D)
 *   Detail        the settings it writes, one line each
 *   FirstTime     Run! runs the game
 */
#ifndef DXL_WIZARD_H
#define DXL_WIZARD_H

#include "gui/dialog.h"
#include "launch/launch.h"

typedef enum {
    DXL_PAGE_RENDERER,      /* 2017 */
    DXL_PAGE_DETAIL,        /* 2018 */
    DXL_PAGE_FIRSTTIME,     /* 2019 */
    DXL_PAGE_SAFEMODE,      /* 2020 */
    DXL_PAGE_SAFEOPTIONS,   /* 2021 */
    DXL_PAGE_DRIVER,        /* 2022 */
} dxl_page_id;

typedef struct dxl_wizard dxl_wizard;

/* The frame, with no page yet; offscreen: drawn into a surface. */
dxl_wizard *dxl_wizard_new(dxl_gui *g, dxl_wizard_request *req, int offscreen, dxl_err *err);
void        dxl_wizard_free(dxl_wizard *w);

/* Advance: a page on the stack, shown. */
void        dxl_wizard_push(dxl_wizard *w, dxl_page_id page);

/* The Renderer page's first paint: detection (when detect is set and the
 * command line has no -nodetect), then the list. Running the wizard does it
 * on its own. */
void        dxl_wizard_first_paint(dxl_wizard *w, int detect);

/* The Renderer page's two radio buttons: "Show all devices" or not. */
void        dxl_wizard_show_all(dxl_wizard *w, int all);

/* The wizard as drawn, to a .bmp. */
int         dxl_wizard_save(dxl_wizard *w, const char *bmp_path);

/* Its window and controls, to click on and look at: for the tests and
 * tools/pageshots.c. The page's controls are layer 1, the frame's 0. */
dxl_dialog *dxl_wizard_dialog(dxl_wizard *w);

/* The page the request starts with, then the wizard until it ends: 1 when it
 * ends in Run, 0 in Cancel -- also after safe mode's relaunch or the web
 * page. */
int         dxl_wizard_run(dxl_wizard *w);

#endif

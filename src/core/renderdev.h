/* The render devices the Renderer page lists (dialog 2017), and what
 * -testrendev= leaves behind: dx-reverse-info/wizard.md "Renderer".
 *
 * A device is registered by a line in the [Public] section of a .int file,
 *   Object=(Name=D3DDrv.D3DRenderDevice,Class=Class,MetaClass=Engine.RenderDevice,Autodetect=)
 * read from every .int in System/ in name order. The page shows a device
 * when it earns a priority -- 3 if its Autodetect file is in Windows' system
 * or Windows folder, 2 if its DescFlags says certified, 1 for the software
 * renderer -- or when "Show all devices" is ticked, sorted by caption as the
 * page's list box sorts, with the last device of the best priority chosen.
 *
 * DescFlags is written by detection: -testrendev=<device> marks the device
 * incompatible and flushes, then tries to start it, and a device that starts
 * writes its own flags. No Windows driver starts here, so every device
 * tested stays incompatible, and there are no Windows folders to find an
 * Autodetect file in: the software renderer is the one chosen. That is the
 * "almost" of this page.
 */
#ifndef DXL_RENDERDEV_H
#define DXL_RENDERDEV_H

#include "core/config.h"
#include "core/localize.h"

/* DescFlags bits, the SDK's Engine/Inc/UnRenDev.h. */
#define DXL_DESCF_CERTIFIED       1
#define DXL_DESCF_INCOMPATIBLE    2
#define DXL_DESCF_LOW_DETAIL_WORLD 4
#define DXL_DESCF_LOW_DETAIL_SKINS 8
#define DXL_DESCF_LOW_DETAIL_ACTORS 16

#define DXL_SOFTWARE_DEVICE "SoftDrv.SoftwareRenderDevice"
#define DXL_D3D_DEVICE      "D3DDrv.D3DRenderDevice"

typedef struct {
    char *path;         /* Package.Class */
    char *caption;      /* <Package>.int [<Class>] ClassCaption */
    char *autodetect;   /* Autodetect=, or "" */
    int   desc_flags;   /* [<path>] DescFlags in the game's ini */
    int   priority;     /* 3, 2, 1, or 0 */
} dxl_renderdev;

typedef struct {
    dxl_renderdev *items;    /* in registration order */
    size_t         count;
} dxl_renderdev_list;

/* autodetect_dirs: the folders an Autodetect file is looked for in, NULL
 * terminated -- Windows' system and Windows folders in the original; none
 * here (NULL). */
void dxl_renderdev_load(dxl_renderdev_list *l, const char *system_dir, dxl_loc *loc,
                        const dxl_ini *game_ini, const char *const *autodetect_dirs);
void dxl_renderdev_free(dxl_renderdev_list *l);

/* What the list box holds: the devices shown -- all, or those with a
 * priority -- as indices into l->items, sorted by caption. *selected is the
 * chosen one's position in that order, or -1 when nothing is shown. */
size_t dxl_renderdev_shown(const dxl_renderdev_list *l, int all,
                           size_t *out, size_t max, int *selected);

/* The note under the list: Startup.int [Descriptions] <path>, or "". */
const char *dxl_renderdev_note(dxl_loc *loc, const char *path);

/* -testrendev=<device>: [<device>] DescFlags=2, the ini saved, then an empty
 * Detected.ini in system_dir. */
int dxl_renderdev_testrendev(dxl_config *c, const char *system_dir, const char *device,
                             dxl_err *err);

#endif

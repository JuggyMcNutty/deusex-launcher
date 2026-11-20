/* The launcher's icon as a Windows icon file: a header, a directory entry
 * for each image, then the images, each a PNG, which an icon file holds as
 * it is. One packing for the Start menu's DXLauncher.ico, made at run time
 * (platform/win32/desktop.c), and the programs' own, made by dxl-icon
 * (tools/icon.c) for their resources. No SDL: the PNGs are the caller's.
 */
#ifndef DXL_ICO_H
#define DXL_ICO_H

#include "core/common.h"

/* The sizes Windows' icon holds, in pixels: its menu's, its files', its
 * taskbar's at each scaling, Explorer's largest view. */
#define DXL_ICO_SIZES 16, 32, 48, 64, 128, 256

typedef struct {
    int         px;     /* square, 1 to 256 */
    const char *png;
    size_t      len;
} dxl_ico_image;

/* The n images (1 to 255) as one icon file, onto out's end: 0, or -1 for a
 * count or a size an icon file cannot hold, out as it was. */
int dxl_ico_pack(const dxl_ico_image *images, int n, dxl_buf *out);

#endif

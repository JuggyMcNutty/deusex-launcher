/* An icon out of a Windows executable, for the launcher's windows.
 *
 * The original's wizard shows DeusEx.exe's icon group 128 (dx-reverse-info/
 * wizard.md "The frame"). dxl_pe_icon reads a group from a PE's resources --
 * RT_GROUP_ICON <group>, of its entries the one <size> square with the most
 * colours, else the largest -- and decodes that RT_ICON's DIB: a
 * BITMAPINFOHEADER, the colour table, the colours bottom row first, then the
 * AND mask, which leaves a pixel out where its bit is set. PNG icons, which
 * Windows reads from Vista on, are not read. The file is the game's, never
 * shipped: it is read where the game is installed.
 */
#ifndef DXL_PEICON_H
#define DXL_PEICON_H

#include "core/common.h"

#include <stdint.h>

typedef struct {
    int       w, h;
    uint32_t *argb;   /* w x h, 0xAARRGGBB, top row first */
} dxl_icon;

/* 0 and *out filled, or -1 with the reason. */
int  dxl_pe_icon(const char *path, int group, int size, dxl_icon *out, dxl_err *err);
void dxl_icon_free(dxl_icon *icon);

#endif

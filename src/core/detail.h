/* The Detail page (dialog 2018): the settings it writes as it opens, and the
 * lines it shows for them. dx-reverse-info/wizard.md "Detail", ini-keys.md
 * "Detail auto-configuration".
 *
 * Its choices come from the machine -- memory, MMX, the CPU's speed -- and
 * from the chosen renderer and its DescFlags. The machine is passed in, so
 * every branch can be tested; platform/machine.h says what this computer is.
 */
#ifndef DXL_DETAIL_H
#define DXL_DETAIL_H

#include "core/ini.h"
#include "core/localize.h"

typedef struct {
    unsigned long long memory;   /* physical memory, bytes (GPhysicalMemory) */
    int                mmx;      /* GIsMMX */
    double             cpu_mhz;  /* the CPU's clock; 0 when unknown */
} dxl_machine;

/* Writes the page's settings into the game's ini and returns its text: one
 * line per choice, each ending in \r\n, as the page's edit box holds it.
 * Caller frees. */
char *dxl_detail_apply(dxl_ini *game_ini, dxl_loc *loc, const dxl_machine *m);

#endif

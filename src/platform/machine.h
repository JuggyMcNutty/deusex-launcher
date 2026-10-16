/* This computer as the original measured it for the Detail page: memory,
 * MMX and the CPU's clock (dx-reverse-info/wizard.md "Detail"). An ARM CPU
 * has no MMX, which the page reads as the original would. */
#ifndef DXL_MACHINE_H
#define DXL_MACHINE_H

#include "core/detail.h"

void dxl_machine_probe(dxl_machine *m);

#endif

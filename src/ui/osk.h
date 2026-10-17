/* The on-screen keyboard: text from a pad, in a dialog of keys the pad
 * moves across. Confirm types the key under the focus; the west button
 * erases, Start keeps the text, back leaves it as it was. */
#ifndef DXL_UI_OSK_H
#define DXL_UI_OSK_H

#include "ui/kit.h"

typedef struct {
    char original[512];   /* the text as the keyboard opened, for back */
    int  shift;
} dxl_osk_state;

void dxl_osk_open(dxl_osk_state *st, const char *buf);
/* The keyboard over over, editing buf; back is the frame's back for it.
 * 1 when the text is kept, -1 when it is put back, 0 while it is up. */
int  dxl_osk(dxl_ui *ui, dxl_osk_state *st, dxl_rect over, const char *title, char *buf, size_t size, int back);

#endif

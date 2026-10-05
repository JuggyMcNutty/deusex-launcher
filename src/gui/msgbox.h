/* The message boxes the launcher shows: the CD prompt (OK and Cancel) and
 * the critical error (OK and an error icon).
 *
 * Laid out as wine lays out the original's (its MessageBox, which the
 * captures (dx-reverse-info/wizard.md, "Observed under wine") agree with to the pixel):
 * in the message font, Tahoma 8 pt; the text wrapped at 264 pixels, 12 from
 * the left or 56 past the icon; buttons twice as wide as the widest label
 * (and at least four lines), two lines high, a third of a button apart,
 * centred under a band 16 pixels taller than the text or the icon, OK the
 * default. OK and Cancel are the system's words, not the game's. The icon
 * is wine's own where wine is installed (dxl_gui's error_icon), blended as
 * wine draws it; without wine, one drawn here.
 */
#ifndef DXL_MSGBOX_H
#define DXL_MSGBOX_H

#include "gui/dialog.h"

typedef struct {
    const char *title;
    const char *text;
    int         ok_cancel;    /* else OK alone */
    int         error_icon;
} dxl_msgbox_spec;

/* Built but not run: for pictures of it. Caller closes. */
dxl_dialog *dxl_msgbox_build(dxl_gui *g, const dxl_msgbox_spec *s, int offscreen, dxl_err *err);

/* Shown and waited for: 1 for OK, 0 for Cancel. */
int dxl_msgbox(dxl_gui *g, const dxl_msgbox_spec *s);

#endif

/* The settings' controls: each a row the width of its rect that takes the
 * focus, its label on the left, the control on the right. Left and right
 * on a focused control are its own (dxl_ui_take_lr): a slider's steps, a
 * spinner's choices, a toggle's off and on. Each returns what changed.
 */
#ifndef DXL_UI_CONTROLS_H
#define DXL_UI_CONTROLS_H

#include "ui/kit.h"

/* Off and on: activate flips it, left and right set them. 1 when *on
 * changed. */
int dxl_toggle(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, int *on, int flags);

/* *v from min to max by step: left and right step it, the pointer drags
 * it, the wheel over it steps it; shown is its value as text. 1 when it
 * changed. */
int dxl_slider(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, double *v, double min, double max,
               double step, const char *shown, int flags);

/* One of n choices: left and right step through them, the arrows too;
 * activated, DXL_SPIN_LIST, for the caller's list of them. */
#define DXL_SPIN_CHANGED 1
#define DXL_SPIN_LIST    2
int dxl_spinner(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, int *index, const char *const *labels,
                int n, int flags);

/* A line of text, edited in place with the keyboard -- Enter keeps it, Esc
 * puts it back -- or, activated from a pad, DXL_TEXT_OSK for the caller's
 * on-screen keyboard (ui/osk.h). DXL_TEXT_DONE when an edit was kept. */
#define DXL_TEXT_DONE 1
#define DXL_TEXT_OSK  2
int dxl_text_field(dxl_ui *ui, uint32_t id, dxl_rect r, const char *label, char *buf, size_t size,
                   const char *placeholder, int flags);

/* A key's slot: its name, or a dash; activated, it captures the next key,
 * mouse button or wheel notch (dxl_ui_capture_begin) and returns
 * DXL_SLOT_CAPTURED with it in ui->in.captured; Esc gives
 * DXL_SLOT_CANCELLED; Delete or the pad's west button, DXL_SLOT_CLEARED. */
#define DXL_SLOT_CAPTURED  1
#define DXL_SLOT_CANCELLED 2
#define DXL_SLOT_CLEARED   3
int dxl_key_slot(dxl_ui *ui, uint32_t id, dxl_rect r, const char *name, int flags);

/* Tabs along r: the one chosen underlined; the pad's shoulders and Ctrl+Tab
 * step through them, wrapping. 1 when *current changed. */
int dxl_tabs(dxl_ui *ui, const char *id, dxl_rect r, const char *const *labels, int n, int *current);

/* The width of the label column the controls give a row of w dp. */
float dxl_control_label_w(float w);

#endif

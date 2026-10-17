/* The widgets the screens are made of, on the kit: each draws itself from
 * its arguments, every frame, and says what the player did to it.
 *
 * Panels are the game's menu windows in our own drawing (draw.h): a steel
 * body, a title bar along the top in the theme's title colour. Buttons are
 * its menu buttons: a steel face, left-aligned on the main menu; the
 * focused one lit, and the brackets round it (the kit's).
 */
#ifndef DXL_UI_WIDGETS_H
#define DXL_UI_WIDGETS_H

#include "ui/kit.h"

#define DXL_W_DISABLED 0x01   /* dim, and takes no focus */
#define DXL_W_PRIMARY  0x02   /* the screen's main action */
#define DXL_W_LEFT     0x04   /* text from the left: a label's way, and the main menu's */
#define DXL_W_CENTER   0x08
#define DXL_W_RIGHT    0x10
#define DXL_W_MAY_CUT  0x20   /* text that may be cut short with "...", no diagnostic */
#define DXL_W_TAIL     0x40   /* cut keeping the end: a path */

/* A steel panel. */
void     dxl_panel(dxl_ui *ui, dxl_rect r);
/* A panel with a title bar; the rect inside it, under the bar, padded. */
dxl_rect dxl_window(dxl_ui *ui, dxl_rect r, const char *title);
/* A sunken well in a panel: a list's, a log's. */
void     dxl_well(dxl_ui *ui, dxl_rect r);

/* One line of text, vertically centred in r and fit to its width: cut
 * short with "..." is a diagnostic, unless DXL_W_MAY_CUT. */
void     dxl_label(dxl_ui *ui, dxl_rect r, dxl_font_role role, const char *s, dxl_rgba c, int flags);
/* Text wrapped to r's width from its top; returns the height it took. Text
 * taller than r is a diagnostic. */
float    dxl_paragraph(dxl_ui *ui, dxl_rect r, dxl_font_role role, const char *s, dxl_rgba c);

/* 1 when activated: clicked, or Enter or the pad's confirm on it. */
int      dxl_button(dxl_ui *ui, const char *id, dxl_rect r, const char *label, int flags);

/* A name and its value on a line of h dp cut off the top of *r: the name
 * in the help colour, name_w wide, the value after it. */
void     dxl_field(dxl_ui *ui, dxl_rect *r, float h, float name_w, const char *name, const char *value,
                   dxl_rgba c, int flags);

/* A list: rows of one height in r, clipped, scrolled to keep the focused
 * row in view. Up and down move along the rows -- past the visible ones
 * too -- and off either end to what is beyond it; Page Up and Page Down,
 * and the wheel over it, scroll it. */
typedef struct {
    uint32_t id;
    dxl_rect area;
    int      count;
    float    row_h;
    float    offset;        /* dp scrolled */
    int      focused;       /* the row with the focus, or -1 */
    int      scrollable;    /* more rows than fit: a scroll bar, the rows narrower by it */
} dxl_list;

void     dxl_list_begin(dxl_ui *ui, dxl_list *l, const char *id, dxl_rect r, int count, float row_h);
uint32_t dxl_list_row_id(const dxl_list *l, int i);
/* Row i, drawn: a title, and a detail under it in detail_c (or none). The
 * DXL_HIT_*s that happened to it; 0 for a row out of view. */
int      dxl_list_row(dxl_ui *ui, dxl_list *l, int i, const char *title, const char *detail, dxl_rgba detail_c,
                      int flags);
void     dxl_list_end(dxl_ui *ui, dxl_list *l);
/* Gives row i the focus, scrolled into view on the next frame. */
void     dxl_list_focus(dxl_ui *ui, dxl_list *l, int i);

/* A dialog over the part of the screen in over: that part dimmed, a w x h
 * dp window in its middle (no larger than it), the rect inside the window
 * returned. Its focusables take the focus until dxl_dialog_end. */
dxl_rect dxl_dialog_begin(dxl_ui *ui, dxl_rect over, float w, float h, const char *title);
void     dxl_dialog_end(dxl_ui *ui);

#endif

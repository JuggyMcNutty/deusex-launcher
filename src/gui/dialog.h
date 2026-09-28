/* A dialog: a window and its controls, as a Win32 dialog template makes
 * them, and the input a Win32 dialog answers -- the mouse, Tab and
 * Shift+Tab, Space, Enter, Escape, the arrows, and each control's mnemonic
 * (Alt and its underlined letter).
 *
 * Controls are placed in pixels (gui.h converts from dialog units). A
 * control's command goes to the dialog's on_command: a button's click, a
 * list's new selection or double click. Escape and closing the window send
 * IDCANCEL (2); Enter clicks the focused button, else the default one, else
 * sends IDOK (1).
 */
#ifndef DXL_DIALOG_H
#define DXL_DIALOG_H

#include "gui/gui.h"

#define DXL_IDOK     1
#define DXL_IDCANCEL 2

typedef enum {
    DXL_CTL_TEXT,       /* a static: left-aligned, wrapped */
    DXL_CTL_ETCHED,     /* SS_ETCHEDHORZ */
    DXL_CTL_GROUP,      /* a group box, its frame etched */
    DXL_CTL_BITMAP,     /* SS_BITMAP, in a raised frame (WS_EX_DLGMODALFRAME) */
    DXL_CTL_COOL,       /* Window.dll's WCoolButton */
    DXL_CTL_URL,        /* Window.dll's WUrlButton */
    DXL_CTL_PUSH,       /* a standard push button */
    DXL_CTL_CHECK,      /* BS_AUTOCHECKBOX */
    DXL_CTL_RADIO,      /* BS_AUTORADIOBUTTON */
    DXL_CTL_LIST,       /* a list box */
    DXL_CTL_EDIT,       /* a read-only multi-line edit */
    DXL_CTL_ICON,       /* a message box's error icon */
    DXL_CTL_PANE,       /* a child dialog's background, over what is under it */
} dxl_ctl_kind;

/* What a control tells its dialog. */
typedef enum { DXL_CLICKED, DXL_SELCHANGE, DXL_DBLCLK } dxl_notify;

typedef struct {
    dxl_ctl_kind kind;
    int          id;
    SDL_Rect     rc;          /* in the window */
    char        *text;
    int          layer;       /* removed together (a wizard page's controls) */
    int          visible, tabstop, group;   /* group: starts a radio group */
    int          checked;     /* a check box, a radio button */
    int          is_default;  /* a push button: Enter's */
    char       **items;       /* a list's */
    int          item_count, selected;
    SDL_Texture *bitmap;
    int          bitmap_w, bitmap_h;
} dxl_ctl;

typedef struct dxl_dialog dxl_dialog;
typedef void (*dxl_command_fn)(dxl_dialog *d, int id, dxl_notify what, void *ctx);

struct dxl_dialog {
    dxl_gui      *gui;
    TTF_Font     *font;       /* every control's (WM_SETFONT): the dialog font */
    int           line_h;     /* its height */
    SDL_Window   *win;        /* NULL when drawing into a surface */
    SDL_Renderer *ren;
    SDL_Surface  *surface;    /* the offscreen target, or NULL */
    int           w, h;       /* the client area */
    dxl_ctl      *ctls;
    int           count, cap;
    int           focus, hover, pressed;     /* control indices, or -1 */
    int           alt;        /* Alt held: mnemonics underlined */
    int           ended, result;
    dxl_command_fn on_command;
    void         *ctx;
    int           dirty;
    Uint32        last_click;
    int           last_click_ctl;
};

/* A window of that client size and title, centred; offscreen: drawn into a
 * surface instead, for tools/pageshots.c. NULL on failure. */
dxl_dialog *dxl_dialog_open(dxl_gui *g, const char *title, int w, int h, int offscreen,
                            dxl_err *err);
void        dxl_dialog_close(dxl_dialog *d);
void        dxl_dialog_set_title(dxl_dialog *d, const char *title);
/* Another font for all the controls: a message box's. */
void        dxl_dialog_set_font(dxl_dialog *d, TTF_Font *font);

dxl_ctl    *dxl_dialog_add(dxl_dialog *d, dxl_ctl_kind kind, int id, SDL_Rect rc,
                           const char *text, int layer);
dxl_ctl    *dxl_dialog_find(dxl_dialog *d, int id, int layer);
void        dxl_dialog_remove_layer(dxl_dialog *d, int layer);
void        dxl_ctl_set_text(dxl_dialog *d, dxl_ctl *c, const char *text);
void        dxl_ctl_add_item(dxl_dialog *d, dxl_ctl *c, const char *item);
void        dxl_ctl_clear_items(dxl_dialog *d, dxl_ctl *c);
/* A bitmap control takes the size of its bitmap, as SS_BITMAP does. */
void        dxl_ctl_set_bitmap(dxl_dialog *d, dxl_ctl *c, const char *bmp_path);
/* Focus to the first control that takes it. */
void        dxl_dialog_focus_first(dxl_dialog *d);

/* As the mouse would: a click on the visible control with this id (the one
 * added last, if two share it), or a list's item chosen -- twice for a
 * double click. For the tests and tools/pageshots.c. */
void        dxl_dialog_click(dxl_dialog *d, int id);
void        dxl_dialog_choose(dxl_dialog *d, int id, int item, int twice);

void        dxl_dialog_draw(dxl_dialog *d);
/* Waits up to ms for input and answers it. 1 once the dialog has ended. */
int         dxl_dialog_step(dxl_dialog *d, int ms);
void        dxl_dialog_end(dxl_dialog *d, int result);
/* The dialog as drawn, to a .bmp. */
int         dxl_dialog_save(dxl_dialog *d, const char *path);

#endif

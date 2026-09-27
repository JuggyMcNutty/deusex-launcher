/* SDL2 frontend: window, input and the drawing primitives the screens need.
 *
 * Deliberately small. Every screen is a tab bar, a list of rows and a help
 * pane, so the widget vocabulary is a label, a wrapped paragraph, a menu row
 * (plain, with a value, or with a slider), a panel for overlays and a footer
 * of button hints. There is no layout engine and no retained widget tree;
 * each screen draws itself every frame from the app state.
 */
#ifndef DXL_UI_H
#define DXL_UI_H

#include "core/common.h"
#include "theme.h"

#include <SDL.h>

typedef enum {
    DXL_ACT_NONE,
    DXL_ACT_UP, DXL_ACT_DOWN, DXL_ACT_LEFT, DXL_ACT_RIGHT,
    DXL_ACT_CONFIRM,     /* A */
    DXL_ACT_BACK,        /* B */
    DXL_ACT_ALT,         /* X: reset the focused setting */
    DXL_ACT_TAB_PREV,    /* L1 */
    DXL_ACT_TAB_NEXT,    /* R1 */
    DXL_ACT_START,       /* START: play, from anywhere */
    DXL_ACT_QUIT         /* SELECT / MENU: leave without playing */
} dxl_act;

typedef enum {
    DXL_TXT_TITLE,
    DXL_TXT_BODY,
    DXL_TXT_ITEM,
    DXL_TXT_FOOTER
} dxl_txt;

typedef struct dxl_ui dxl_ui;

dxl_ui *dxl_ui_init(dxl_err *err);
void    dxl_ui_quit(dxl_ui *ui);

int  dxl_ui_width (const dxl_ui *ui);
int  dxl_ui_height(const dxl_ui *ui);
const dxl_metrics *dxl_ui_metrics(const dxl_ui *ui);

/* The name SDL gives the controller in use, or NULL when there is none. */
const char *dxl_ui_pad_name(const dxl_ui *ui);

/* Drains SDL's queue and returns the next abstract action, or DXL_ACT_NONE.
 * Auto-repeat on the d-pad and stick is handled here so the screens do not
 * each reinvent it. */
dxl_act dxl_ui_poll(dxl_ui *ui);

void dxl_ui_frame_begin(dxl_ui *ui);
void dxl_ui_frame_end(dxl_ui *ui);

void dxl_ui_fill(dxl_ui *ui, SDL_Rect r, SDL_Color c);
void dxl_ui_text(dxl_ui *ui, int x, int y, const char *s, dxl_txt style, SDL_Color c);
int  dxl_ui_text_width(dxl_ui *ui, const char *s, dxl_txt style);
int  dxl_ui_line_height(dxl_ui *ui, dxl_txt style);

/* Word-wraps s into max_w, drawing from (x,y). Returns the y below the last
 * line, so callers can stack blocks without measuring twice. max_lines > 0
 * stops early, ending the last line with "..." when text remains. */
int  dxl_ui_paragraph(dxl_ui *ui, int x, int y, int max_w, const char *s,
                      dxl_txt style, SDL_Color c);
int  dxl_ui_paragraph_n(dxl_ui *ui, int x, int y, int max_w, const char *s,
                        dxl_txt style, SDL_Color c, int max_lines);

/* Tabs across the top, with L1/R1 hints at either end. Returns the y where
 * content may start. */
int  dxl_ui_tabbar(dxl_ui *ui, const char *const *labels, int count, int active);

/* A plain title with a rule under it, for screens without tabs. */
int  dxl_ui_header(dxl_ui *ui, const char *title);

/* Button hints along the bottom: pairs of (button, meaning), drawn as a
 * button chip followed by its meaning. Returns the y of the footer's top
 * edge, i.e. where content must stop. */
int  dxl_ui_footer(dxl_ui *ui, const char *const *pairs, int npairs);

/* One menu row. selected draws the amber bar; value is the right-aligned
 * secondary text. With arrows, a selected row shows < > around the value to
 * say left/right changes it. */
enum { DXL_ROW_ARROWS = 1, DXL_ROW_DISABLED = 2 };
void dxl_ui_row(dxl_ui *ui, int y, const char *label, const char *value,
                int selected, int flags);
/* A row whose value is a bar filled to frac (0..1), labelled value_text. */
void dxl_ui_slider_row(dxl_ui *ui, int y, const char *label, double frac,
                       const char *value_text, int selected, int flags);

/* A filled, bordered panel for overlays, over a dimmed screen. */
void dxl_ui_overlay_panel(dxl_ui *ui, SDL_Rect r);

/* True when no font could be loaded; the caller falls back to stderr rather
 * than drawing an empty screen. */
int  dxl_ui_headless(const dxl_ui *ui);

#endif

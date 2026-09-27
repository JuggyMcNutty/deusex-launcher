/* The launcher's screens, and the navigation between them.
 *
 * A controller-first home: four tabs switched with L1/R1 -- Play, Video,
 * Controls, System -- and START launches from any of them. The original's
 * six-page wizard asked one question per page because it had to fit a
 * mouse-driven Win32 dialog; on a handheld the same decisions are simply
 * settings, each with a line of help saying what it really changes.
 *
 * What the original's entry decision (dx-reverse-info/wizard.md) chose between is
 * still honoured, as where the home screen opens:
 *
 *   first run / -changevideo   Video tab
 *   -safe                      System tab
 *   crash sentinel survived    Play tab, with the crash explained and the
 *                              cursor on Troubleshoot
 *
 * The install screen stands alone: until the game files are there, there is
 * nothing else to do.
 */
#ifndef DXL_SCREENS_H
#define DXL_SCREENS_H

#include "app.h"
#include "ui.h"

typedef enum {
    DXL_TAB_PLAY,
    DXL_TAB_VIDEO,
    DXL_TAB_CONTROLS,
    DXL_TAB_SYSTEM,
    DXL_TAB_COUNT
} dxl_tab;

typedef enum {
    DXL_OVL_NONE,
    DXL_OVL_RENDERERS,   /* the renderer picker */
    DXL_OVL_CONFIRM,     /* yes/no before something destructive */
    DXL_OVL_TEXT,        /* a scrollable text: the engine log */
    DXL_OVL_REMAP,       /* every pad button and what it does */
    DXL_OVL_ACTIONS,     /* the actions one button can be given */
} dxl_overlay;

typedef enum {
    DXL_END_NONE,
    DXL_END_LAUNCH,      /* commit, then exec the game */
    DXL_END_QUIT         /* save settings and leave without playing */
} dxl_end;

typedef struct dxl_session dxl_session;
typedef void (*dxl_confirm_fn)(dxl_session *s);

struct dxl_session {
    dxl_app *app;
    dxl_ui  *ui;

    int     installing;          /* the install screen, not the tabs */
    int     install_cursor;
    dxl_tab tab;
    int     cursor[DXL_TAB_COUNT];
    int     scroll[DXL_TAB_COUNT];

    /* Facts about how this session started. */
    int  crashed;                /* the sentinel survived the last run */
    char crash_detail[192];      /* the engine's last error line, if any */

    dxl_overlay overlay;
    int  ovl_cursor, ovl_scroll;
    char ovl_title[96];
    char **ovl_lines;            /* DXL_OVL_TEXT: wrapped to the panel, owned */
    int    ovl_nlines;
    /* Remapping: the button being changed, where the button list was, and
     * the picker's rows (a group header is -1 - group start). */
    int  remap_button;
    int  remap_cursor, remap_scroll;
    int *action_rows;
    int  action_nrows;
    int  pending_preset;         /* layout to apply once a confirm is accepted */

    char confirm_body[320];
    char confirm_yes[48];
    dxl_confirm_fn confirm_fn;

    char notice[256];
    int  notice_frames;          /* counts down; the notice shows while > 0 */
    int  notice_bad;

    dxl_end end;
};

void dxl_session_start(dxl_session *s, dxl_app *app, dxl_ui *ui);
void dxl_session_free(dxl_session *s);
/* One frame. Returns 1 once the session has ended (s->end says how). */
int  dxl_session_step(dxl_session *s);
/* Draws the current state without reading input; used by the screenshot
 * tool so every screen can be rendered headlessly. */
void dxl_session_draw(dxl_session *s);
/* Feeds one action as if it came from the pad. */
void dxl_session_act(dxl_session *s, dxl_act a);

/* Where the row with this label sits among the tab's rows as currently
 * shown, for a cursor; -1 if it is not shown. For tools such as dxl-shots,
 * which must not hardcode row positions: which rows show depends on the
 * device profile. */
int  dxl_session_row_index(dxl_session *s, dxl_tab tab, const char *label);

#endif

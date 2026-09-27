/* Shared between the tab files: the row model and the session helpers.
 *
 * Every settings tab is a table of rows. A row says how to show its value
 * and what left/right, A and X do to it; the list drawing, scrolling,
 * cursor movement and help pane are written once, in screens.c. A row that
 * is disabled says why, and the reason replaces its help text -- a greyed
 * option with no explanation is the thing this launcher exists to avoid.
 */
#ifndef DXL_SCREENS_INTERNAL_H
#define DXL_SCREENS_INTERNAL_H

#include "screens.h"
#include "platform/target.h"

typedef struct dxl_row dxl_row;
struct dxl_row {
    const char *label;
    const char *help;
    /* All optional. */
    int    (*visible) (dxl_session *s, const dxl_row *r);
    int    (*enabled) (dxl_session *s, const dxl_row *r, char *why, size_t n);
    void   (*value)   (dxl_session *s, const dxl_row *r, char *out, size_t n);
    double (*slider)  (dxl_session *s, const dxl_row *r);   /* 0..1: draw a bar */
    void   (*step)    (dxl_session *s, const dxl_row *r, int dir);
    void   (*activate)(dxl_session *s, const dxl_row *r);   /* A; else step(+1) */
    void   (*reset)   (dxl_session *s, const dxl_row *r);   /* X */
    void   (*describe)(dxl_session *s, const dxl_row *r, char *out, size_t n);
    int arg;                          /* e.g. a dxl_es_field */
    const char *const *choice_labels; /* shown instead of the engine's spelling */
};

typedef struct {
    const dxl_row *rows;
    int count;
} dxl_rows;

/* The tabs. Play draws itself; the others are row tables. */
dxl_rows tab_video_rows(void);
dxl_rows tab_controls_rows(void);
dxl_rows tab_system_rows(void);
void tab_play_draw(dxl_session *s, int top, int bottom);
void tab_play_act(dxl_session *s, dxl_act a);
int  tab_play_initial_cursor(dxl_session *s);

/* Generic handlers for rows backed by a Settings.json field (arg). */
void   es_value (dxl_session *s, const dxl_row *r, char *out, size_t n);
void   es_step  (dxl_session *s, const dxl_row *r, int dir);
void   es_reset (dxl_session *s, const dxl_row *r);
double es_slider(dxl_session *s, const dxl_row *r);

/* Session helpers. */
void notice(dxl_session *s, int bad, const char *fmt, ...);
void open_confirm(dxl_session *s, const char *title, const char *body,
                  const char *yes, dxl_confirm_fn fn);
/* Takes ownership of text. */
void open_text(dxl_session *s, const char *title, char *text);
void open_renderer_picker(dxl_session *s);
void switch_tab(dxl_session *s, dxl_tab t);

/* remap.c: the Customize buttons screens. */
void open_remap(dxl_session *s);
void remap_draw(dxl_session *s);
void remap_act(dxl_session *s, dxl_act a);
void actions_draw(dxl_session *s);
void actions_act(dxl_session *s, dxl_act a);
void remap_free(dxl_session *s);

/* The last "engine exit: N" in run-game.log: 1 and the code if found. */
int  last_engine_exit(const dxl_app *app, int *code);
/* The last error the engine reported in run-game.log, if any. */
int  last_engine_error(const dxl_app *app, char *out, size_t n);
/* The tail of a text file, at most max bytes, as a new string (NULL if
 * unreadable). */
char *read_tail(const char *path, size_t max);

#endif

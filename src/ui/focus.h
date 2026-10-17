/* Where focus goes: spatial navigation between the rects a frame drew.
 *
 * Each frame the widgets that take focus say where they are (dxl_focus_add);
 * a move in a direction then goes, among those of the topmost scope (a
 * dialog's, while one is up), to the nearest whose middle lies that way,
 * by distance along the move plus twice the distance across it, one that
 * overlaps the focused rect across the move first. Tab and Shift-Tab step
 * through them in the order they were drawn. No SDL here: plain rects, so
 * the rules are tested without a display (tests/test_ui_focus.c).
 */
#ifndef DXL_UI_FOCUS_H
#define DXL_UI_FOCUS_H

#include <stdint.h>

typedef struct { float x, y, w, h; } dxl_box;

typedef enum { DXL_DIR_UP, DXL_DIR_DOWN, DXL_DIR_LEFT, DXL_DIR_RIGHT } dxl_dir;

#define DXL_FOCUS_MAX 512

typedef struct {
    uint32_t id;
    dxl_box  box;
    int      scope;     /* 0 the screen's; a dialog's higher */
} dxl_focusable;

typedef struct {
    dxl_focusable items[DXL_FOCUS_MAX];
    int           count;
    int           overflowed;   /* more than DXL_FOCUS_MAX in a frame */
} dxl_focus_list;

void dxl_focus_clear(dxl_focus_list *l);
void dxl_focus_add(dxl_focus_list *l, uint32_t id, dxl_box box, int scope);

/* The topmost scope in the list (0 when it is empty). */
int  dxl_focus_top_scope(const dxl_focus_list *l);
/* Whether id is in the list's topmost scope. */
int  dxl_focus_has(const dxl_focus_list *l, uint32_t id);

/* The focusable a move from `from` goes to, or `from` when there is none
 * that way; with `from` not in the top scope, that scope's first. */
uint32_t dxl_focus_move(const dxl_focus_list *l, uint32_t from, dxl_dir dir);
/* The next (or, step -1, the previous) in drawing order, wrapping. */
uint32_t dxl_focus_step(const dxl_focus_list *l, uint32_t from, int step);
/* The top scope's first, or 0. */
uint32_t dxl_focus_first(const dxl_focus_list *l);

#endif

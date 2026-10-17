/* What to press, drawn: the hints at the foot of each screen, for the
 * device the player last used. On the keyboard and mouse a keycap; on a
 * pad its own button, by what SDL says its label is (A, B, X, Y; cross,
 * circle, square, triangle) and where -- confirm south, back east, but
 * east and south on a Nintendo pad, whose A is east -- as vector shapes of
 * our own: no maker's logo.
 */
#ifndef DXL_UI_GLYPHS_H
#define DXL_UI_GLYPHS_H

#include "ui/kit.h"

typedef enum {
    DXL_HINT_CONFIRM,   /* Enter; the pad's confirm */
    DXL_HINT_BACK,      /* Esc; its back */
    DXL_HINT_PLAY,      /* F5; Start */
    DXL_HINT_MOVE,      /* the arrows; the d-pad */
    DXL_HINT_TAB_PREV,  /* Ctrl+Shift+Tab; the left shoulder */
    DXL_HINT_TAB_NEXT,  /* Ctrl+Tab; the right shoulder */
    DXL_HINT_CLEAR,     /* Delete; the west button */
    DXL_HINT_COUNT
} dxl_hint;

/* The glyph for h, its left at x, centred on cy, size dp high. Returns its
 * width; draw 0 only measures. */
float dxl_glyph(dxl_ui *ui, dxl_hint h, float x, float cy, float size, int draw);

typedef struct {
    dxl_hint    hint;
    const char *label;
} dxl_hint_item;

/* The hints in a row, in the order given, ending at r's right end; each is
 * clickable. Returns the hint clicked, or -1. */
int dxl_hints(dxl_ui *ui, dxl_rect r, const dxl_hint_item *items, int n);

#endif

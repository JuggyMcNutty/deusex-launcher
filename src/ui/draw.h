/* The launcher's look, drawn: everything in dp, made into the renderer's
 * own rects and geometry in pixels, so it is as sharp at any scale.
 *
 * The steel panels and buttons of the game's menus, as our own drawing: a
 * body shaded top to bottom, its corners cut, its top and left edges lit and
 * its bottom and right ones in shadow; the four corner brackets the game
 * puts round what the player can use (its "frob" highlight) round what has
 * the focus.
 */
#ifndef DXL_UI_DRAW_H
#define DXL_UI_DRAW_H

#include "ui/kit.h"

void dxl_fill(dxl_ui *ui, dxl_rect r, dxl_rgba c);
void dxl_gradient(dxl_ui *ui, dxl_rect r, dxl_rgba top, dxl_rgba bottom);
/* A frame of width w (dp) inside r. */
void dxl_outline(dxl_ui *ui, dxl_rect r, float w, dxl_rgba c);
/* A line from a to b, w dp wide about it. */
void dxl_line(dxl_ui *ui, float ax, float ay, float bx, float by, float w, dxl_rgba c);

typedef struct {
    dxl_rgba top, bottom;       /* the body */
    dxl_rgba light, shadow;     /* the edges */
    float    chamfer;           /* the cut corners, dp */
} dxl_bevel_style;

/* A steel panel: the body, its corners cut, its edges lit and shadowed. */
void dxl_bevel(dxl_ui *ui, dxl_rect r, const dxl_bevel_style *s);

/* The four corner brackets round r, each arm len dp, w dp thick. */
void dxl_brackets(dxl_ui *ui, dxl_rect r, float len, float w, dxl_rgba c);

/* The window behind everything: the theme's backdrop, shaded, with a faint
 * grid. */
void dxl_backdrop(dxl_ui *ui);

/* A filled convex polygon (dp), e.g. a glyph's arrow. */
void dxl_poly(dxl_ui *ui, const SDL_FPoint *pts, int n, dxl_rgba c);
/* A filled circle (dp). */
void dxl_disc(dxl_ui *ui, float cx, float cy, float radius, dxl_rgba c);

#endif

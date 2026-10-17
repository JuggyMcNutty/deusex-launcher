/* The pad of ui/padmesh.h drawn as the augmentation screen's hologram: in
 * the theme's accent, lit by a key light, the ambient and a rim, live --
 * a button down sinks and glows, a stick tilts, a trigger pulls. Each
 * part's triangles are moved as its control is, the whole turned by the
 * view, projected, culled, lit, sorted far to near and drawn in one
 * SDL_RenderGeometry, all on the CPU; where each control lands on the
 * screen is given back for the leader lines and the pointer.
 */
#ifndef DXL_UI_PAD3D_H
#define DXL_UI_PAD3D_H

#include "ui/kit.h"
#include "ui/padmesh.h"

typedef struct {
    float yaw, pitch;                   /* the view, radians: 0, 0 is the face on */
    int   down[DXL_PADPART_COUNT];      /* a button held */
    float lx, ly, rx, ry;               /* the sticks, SDL's: -1 to 1, right and down positive */
    float lt, rt;                       /* the triggers, 0 to 1 */
    int   chosen;                       /* a part lit as the one chosen; -1 none */
} dxl_pad3d_state;

/* Where a part's top lands, in dp, and how far round it the pointer takes it. */
typedef struct {
    float x, y, r;
    int   seen;
} dxl_pad3d_spot;

/* The height of a box w across that the pad, fitted into it, fills both ways. */
float dxl_pad3d_fit_height(float w);
/* The pad fitted into r. */
void dxl_pad3d_draw(dxl_ui *ui, const dxl_padmesh *m, dxl_rect r, const dxl_pad3d_state *st,
                    dxl_pad3d_spot spots[DXL_PADPART_COUNT]);
/* The part whose spot holds the point, the smallest; -1 for none. */
int dxl_pad3d_hit(const dxl_padmesh *m, const dxl_pad3d_spot spots[DXL_PADPART_COUNT], float x, float y);

#endif

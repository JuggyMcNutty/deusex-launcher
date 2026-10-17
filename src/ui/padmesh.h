/* The Controller page's pad as triangles, for each family of pad -- the
 * kit's Xbox-like, PlayStation-like, Nintendo-like and any other: the
 * body's outline extruded with a bevel, cylinders for the buttons and the
 * sticks, a d-pad cross, rounded shoulders and triggers. Our own shapes,
 * no maker's.
 *
 * Model space: x right, y up, z out of the face toward the player, the
 * face at z = 0 and the body behind it, some 170 across; what is on the
 * face stands on it, the shoulders and triggers are behind. Each triangle is
 * a part's, and a part moves as its control does (ui/pad3d.h): the parts
 * from the south button to the d-pad's right are SDL's buttons in SDL's
 * order, after the body.
 */
#ifndef DXL_UI_PADMESH_H
#define DXL_UI_PADMESH_H

typedef enum {
    DXL_PADPART_BODY,
    DXL_PADPART_SOUTH, DXL_PADPART_EAST, DXL_PADPART_WEST, DXL_PADPART_NORTH,
    DXL_PADPART_BACK, DXL_PADPART_GUIDE, DXL_PADPART_START,
    DXL_PADPART_LSTICK, DXL_PADPART_RSTICK,          /* the caps: they tilt, and click */
    DXL_PADPART_LSHOULDER, DXL_PADPART_RSHOULDER,
    DXL_PADPART_DPUP, DXL_PADPART_DPDOWN, DXL_PADPART_DPLEFT, DXL_PADPART_DPRIGHT,
    DXL_PADPART_LTRIGGER, DXL_PADPART_RTRIGGER,
    DXL_PADPART_TRIM,                               /* the stick wells, the d-pad's middle, a touchpad */
    DXL_PADPART_COUNT
} dxl_padpart;

/* A part's SDL button, the trigger parts after them. */
#define DXL_PADPART_OF_BUTTON(b) ((dxl_padpart)((b) + 1))

typedef struct { float x, y, z; } dxl_v3;

typedef struct {
    dxl_v3 v[3];             /* counter-clockwise seen from outside */
    unsigned char part;
} dxl_padtri;

typedef struct {
    dxl_padtri *tris;
    int count, cap;
    int    has[DXL_PADPART_COUNT];
    dxl_v3 anchor[DXL_PADPART_COUNT];   /* a part's top, where its leader line starts */
    dxl_v3 pivot[DXL_PADPART_COUNT];    /* what it turns about: a stick's ball, a trigger's hinge */
    float  radius[DXL_PADPART_COUNT];   /* its footprint, for the pointer */
} dxl_padmesh;

/* The pad of the family (dxl_pad_family's order). */
void dxl_padmesh_build(dxl_padmesh *m, int family);
void dxl_padmesh_free(dxl_padmesh *m);

#endif

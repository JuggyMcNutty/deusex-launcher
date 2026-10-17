#include "ui/pad3d.h"
#include "core/common.h"
#include "ui/draw.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CAMERA 400.0f       /* the eye's distance from the pad's middle */
#define EXTENT_X 98.0f      /* the pad's reach from its middle across, and up and down, */
#define EXTENT_Y 70.0f      /* turned any way the view turns */
#define TILT   0.33f        /* a stick at full travel, radians */
#define PULL   0.45f        /* a trigger pulled all the way */
#define SINK   1.4f

static dxl_v3 sub(dxl_v3 a, dxl_v3 b) { return (dxl_v3){ a.x - b.x, a.y - b.y, a.z - b.z }; }
static dxl_v3 add(dxl_v3 a, dxl_v3 b) { return (dxl_v3){ a.x + b.x, a.y + b.y, a.z + b.z }; }
static float dot(dxl_v3 a, dxl_v3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static dxl_v3 cross(dxl_v3 a, dxl_v3 b) {
    return (dxl_v3){ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
static dxl_v3 unit(dxl_v3 v) {
    float l = sqrtf(dot(v, v));
    return l > 1e-9f ? (dxl_v3){ v.x / l, v.y / l, v.z / l } : v;
}
/* Turned about the x axis: positive brings +y toward the eye. */
static dxl_v3 rot_x(dxl_v3 v, float a) {
    float c = cosf(a), s = sinf(a);
    return (dxl_v3){ v.x, v.y * c - v.z * s, v.y * s + v.z * c };
}
/* About the y axis: positive brings +z toward +x. */
static dxl_v3 rot_y(dxl_v3 v, float a) {
    float c = cosf(a), s = sinf(a);
    return (dxl_v3){ v.x * c + v.z * s, v.y, -v.x * s + v.z * c };
}

/* A point of a part where its control has it. */
static dxl_v3 moved(const dxl_padmesh *m, const dxl_pad3d_state *st, int part, dxl_v3 p) {
    switch (part) {
    case DXL_PADPART_BODY:
    case DXL_PADPART_TRIM:
        return p;
    case DXL_PADPART_LSTICK:
    case DXL_PADPART_RSTICK: {
        int left = part == DXL_PADPART_LSTICK;
        float sx = left ? st->lx : st->rx, sy = left ? st->ly : st->ry;
        dxl_v3 q = rot_x(rot_y(sub(p, m->pivot[part]), sx * TILT), sy * TILT);
        q = add(q, m->pivot[part]);
        if (st->down[part]) q.z -= SINK;
        return q;
    }
    case DXL_PADPART_LTRIGGER:
    case DXL_PADPART_RTRIGGER: {
        float v = part == DXL_PADPART_LTRIGGER ? st->lt : st->rt;
        if (v <= 0) return p;
        return add(rot_x(sub(p, m->pivot[part]), -v * PULL), m->pivot[part]);
    }
    case DXL_PADPART_LSHOULDER:
    case DXL_PADPART_RSHOULDER:
        if (st->down[part]) p.y -= SINK;
        return p;
    default:
        if (st->down[part]) p.z -= SINK;
        return p;
    }
}

static dxl_v3 viewed(const dxl_pad3d_state *st, dxl_v3 p) { return rot_x(rot_y(p, st->yaw), st->pitch); }

typedef struct {
    float scale, cx, cy;
} projection;

/* A view-space point on the screen, in dp. */
static void project(const projection *pr, dxl_v3 p, float *x, float *y) {
    float k = CAMERA / (CAMERA - p.z);
    *x = pr->cx + p.x * k * pr->scale;
    *y = pr->cy - p.y * k * pr->scale;
}

static SDL_FColor fcolor(float r, float g, float b, float a) {
    SDL_FColor c = { r > 1 ? 1 : r, g > 1 ? 1 : g, b > 1 ? 1 : b, a };
    return c;
}

static dxl_rgba base_of(const dxl_theme *t, const dxl_pad3d_state *st, int part) {
    const dxl_rgba white = { 255, 255, 255, 255 };
    if (part == st->chosen) return dxl_rgba_mix(t->bracket, white, 0.3f);
    if (part != DXL_PADPART_BODY && part != DXL_PADPART_TRIM) {
        int pulled = (part == DXL_PADPART_LTRIGGER && st->lt > 0.05f) || (part == DXL_PADPART_RTRIGGER && st->rt > 0.05f);
        if (st->down[part] || pulled) return dxl_rgba_mix(t->accent, white, 0.45f);
        return dxl_rgba_fade(dxl_rgba_mix(t->panel_light, t->accent, 0.55f), 0.97f);
    }
    if (part == DXL_PADPART_TRIM) return dxl_rgba_fade(dxl_rgba_mix(t->panel_bottom, t->accent, 0.3f), 0.95f);
    return dxl_rgba_fade(dxl_rgba_mix(t->panel_top, t->accent, 0.4f), 0.93f);
}

/* What is drawn under what, whatever its depth: the shoulders and triggers
 * behind the body, the body, what stands on its face. Sorting by depth
 * alone would put a large triangle of the face over a button on it. */
static int layer_of(int part) {
    switch (part) {
    case DXL_PADPART_LSHOULDER: case DXL_PADPART_RSHOULDER:
    case DXL_PADPART_LTRIGGER: case DXL_PADPART_RTRIGGER: return 0;
    case DXL_PADPART_BODY: return 1;
    default: return 2;
    }
}

typedef struct {
    int    layer;
    float  depth;
    SDL_FColor color;
    dxl_v3 v[3];
} face;

static int far_first(const void *a, const void *b) {
    const face *x = a, *y = b;
    if (x->layer != y->layer) return x->layer - y->layer;
    return x->depth < y->depth ? -1 : x->depth > y->depth;
}

#define DROP 6.0f           /* the pad's middle below the box's, its triggers above it */

float dxl_pad3d_fit_height(float w) { return w / (2 * EXTENT_X) * (2 * EXTENT_Y + 2 * DROP); }

void dxl_pad3d_draw(dxl_ui *ui, const dxl_padmesh *m, dxl_rect r, const dxl_pad3d_state *st,
                    dxl_pad3d_spot spots[DXL_PADPART_COUNT]) {
    static face *faces;
    static SDL_Vertex *verts;
    static int room;
    if (room < m->count) {
        room = m->count;
        faces = dxl_xrealloc(faces, sizeof *faces * (size_t)room);
        verts = dxl_xrealloc(verts, sizeof *verts * (size_t)room * 3);
    }
    projection pr = { fminf(r.w / (2 * EXTENT_X), r.h / (2 * EXTENT_Y)), r.x + r.w / 2, r.y + r.h / 2 };
    pr.cy += DROP * pr.scale;
    const dxl_theme *t = &ui->theme;
    const dxl_v3 light = unit((dxl_v3){ -0.35f, 0.55f, 0.76f });
    const dxl_v3 eye = { 0, 0, CAMERA };

    int k = 0;
    for (int i = 0; i < m->count; i++) {
        const dxl_padtri *tri = &m->tris[i];
        dxl_v3 v[3];
        for (int j = 0; j < 3; j++) v[j] = viewed(st, moved(m, st, tri->part, tri->v[j]));
        dxl_v3 n = unit(cross(sub(v[1], v[0]), sub(v[2], v[0])));
        dxl_v3 mid = { (v[0].x + v[1].x + v[2].x) / 3, (v[0].y + v[1].y + v[2].y) / 3, (v[0].z + v[1].z + v[2].z) / 3 };
        if (dot(n, sub(eye, mid)) <= 0) continue;
        float lum = 0.45f + 0.6f * fmaxf(0.0f, dot(n, light));
        float rim = powf(1.0f - fabsf(n.z), 2.0f) * 0.55f;
        dxl_rgba b = base_of(t, st, tri->part);
        face *f = &faces[k++];
        f->layer = layer_of(tri->part);
        f->depth = mid.z;
        memcpy(f->v, v, sizeof v);
        f->color = fcolor(b.r / 255.0f * lum + t->accent.r / 255.0f * rim, b.g / 255.0f * lum + t->accent.g / 255.0f * rim,
                          b.b / 255.0f * lum + t->accent.b / 255.0f * rim, b.a / 255.0f);
    }
    qsort(faces, (size_t)k, sizeof *faces, far_first);
    for (int i = 0; i < k; i++)
        for (int j = 0; j < 3; j++) {
            float x, y;
            project(&pr, faces[i].v[j], &x, &y);
            SDL_Vertex *vx = &verts[i * 3 + j];
            vx->position.x = dxl_pxf(ui, x);
            vx->position.y = dxl_pxf(ui, y);
            vx->color = faces[i].color;
            vx->tex_coord.x = vx->tex_coord.y = 0;
        }
    if (k) SDL_RenderGeometry(ui->renderer, NULL, verts, k * 3, NULL, 0);

    for (int p = 0; p < DXL_PADPART_COUNT; p++) {
        spots[p].seen = m->has[p] && p != DXL_PADPART_BODY && p != DXL_PADPART_TRIM;
        if (!spots[p].seen) continue;
        dxl_v3 a = viewed(st, moved(m, st, p, m->anchor[p]));
        project(&pr, a, &spots[p].x, &spots[p].y);
        spots[p].r = m->radius[p] * pr.scale * CAMERA / (CAMERA - a.z);
    }
}

int dxl_pad3d_hit(const dxl_padmesh *m, const dxl_pad3d_spot spots[DXL_PADPART_COUNT], float x, float y) {
    int best = -1;
    for (int p = 0; p < DXL_PADPART_COUNT; p++) {
        if (!m->has[p] || !spots[p].seen) continue;
        float dx = x - spots[p].x, dy = y - spots[p].y;
        if (dx * dx + dy * dy > spots[p].r * spots[p].r) continue;
        if (best < 0 || spots[p].r < spots[best].r) best = p;
    }
    return best;
}

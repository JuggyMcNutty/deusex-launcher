#include "ui/padmesh.h"
#include "core/common.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x, y; } pt2;

#define PI 3.14159265f
#define MAXPTS 256

static void add_tri(dxl_padmesh *m, dxl_v3 a, dxl_v3 b, dxl_v3 c, int part) {
    if (m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 1024;
        m->tris = dxl_xrealloc(m->tris, sizeof *m->tris * (size_t)m->cap);
    }
    dxl_padtri *t = &m->tris[m->count++];
    t->v[0] = a;
    t->v[1] = b;
    t->v[2] = c;
    t->part = (unsigned char)part;
}

static float area2(const pt2 *p, int n) {
    float a = 0;
    for (int i = 0; i < n; i++) {
        const pt2 *u = &p[i], *v = &p[(i + 1) % n];
        a += u->x * v->y - v->x * u->y;
    }
    return a;
}

static float cross3(pt2 a, pt2 b, pt2 c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); }

static int inside(pt2 p, pt2 a, pt2 b, pt2 c) {
    return cross3(a, b, p) > 0 && cross3(b, c, p) > 0 && cross3(c, a, p) > 0;
}

/* A counter-clockwise polygon's inside at height z, by ear clipping. */
static void cap(dxl_padmesh *m, const pt2 *p, int n, float z, int part) {
    int idx[MAXPTS], k = n;
    for (int i = 0; i < n; i++) idx[i] = i;
    int guard = 0;
    while (k > 3 && guard++ < 4 * MAXPTS) {
        int clipped = 0;
        for (int i = 0; i < k && !clipped; i++) {
            int a = idx[(i + k - 1) % k], b = idx[i], c = idx[(i + 1) % k];
            if (cross3(p[a], p[b], p[c]) <= 1e-6f) continue;
            int ear = 1;
            for (int j = 0; j < k && ear; j++) {
                int q = idx[j];
                if (q != a && q != b && q != c && inside(p[q], p[a], p[b], p[c])) ear = 0;
            }
            if (!ear) continue;
            add_tri(m, (dxl_v3){ p[a].x, p[a].y, z }, (dxl_v3){ p[b].x, p[b].y, z },
                    (dxl_v3){ p[c].x, p[c].y, z }, part);
            memmove(&idx[i], &idx[i + 1], sizeof *idx * (size_t)(k - i - 1));
            k--;
            clipped = 1;
        }
        if (!clipped) break;
    }
    /* What is left: as a fan, should clipping have stalled on a sliver. */
    for (int i = 1; i + 1 < k; i++)
        add_tri(m, (dxl_v3){ p[idx[0]].x, p[idx[0]].y, z }, (dxl_v3){ p[idx[i]].x, p[idx[i]].y, z },
                (dxl_v3){ p[idx[i + 1]].x, p[idx[i + 1]].y, z }, part);
}

/* The wall between ring a (higher, z1) and ring b (lower, z0) of a
 * counter-clockwise outline: its faces turned out. */
static void wall(dxl_padmesh *m, const pt2 *a, float z1, const pt2 *b, float z0, int n, int part) {
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        dxl_v3 at = { a[i].x, a[i].y, z1 }, bt = { a[j].x, a[j].y, z1 };
        dxl_v3 ab = { b[i].x, b[i].y, z0 }, bb = { b[j].x, b[j].y, z0 };
        add_tri(m, at, ab, bb, part);
        add_tri(m, at, bb, bt, part);
    }
}

/* The outline drawn in by d, along each corner's mean normal. */
static void inset(const pt2 *p, int n, float d, pt2 *out) {
    for (int i = 0; i < n; i++) {
        pt2 a = p[(i + n - 1) % n], b = p[i], c = p[(i + 1) % n];
        float e1x = b.x - a.x, e1y = b.y - a.y, e2x = c.x - b.x, e2y = c.y - b.y;
        float l1 = sqrtf(e1x * e1x + e1y * e1y), l2 = sqrtf(e2x * e2x + e2y * e2y);
        if (l1 < 1e-6f) l1 = 1;
        if (l2 < 1e-6f) l2 = 1;
        /* Outward normals of a counter-clockwise outline: the edge turned right. */
        float nx = e1y / l1 + e2y / l2, ny = -e1x / l1 - e2x / l2;
        float nl = sqrtf(nx * nx + ny * ny);
        if (nl < 1e-6f) nl = 1;
        out[i].x = b.x - nx / nl * d;
        out[i].y = b.y - ny / nl * d;
    }
}

/* An outline raised to z_top, its top bevelled by bevel, its sides down to
 * z_top - depth. Clockwise outlines are turned first. */
static void extrude(dxl_padmesh *m, const pt2 *src, int n, float z_top, float depth, float bevel, int part) {
    pt2 p[MAXPTS], in[MAXPTS];
    if (n > MAXPTS) n = MAXPTS;
    if (area2(src, n) < 0)
        for (int i = 0; i < n; i++) p[i] = src[n - 1 - i];
    else
        memcpy(p, src, sizeof *p * (size_t)n);
    inset(p, n, bevel, in);
    cap(m, in, n, z_top, part);
    wall(m, in, z_top, p, z_top - bevel, n, part);
    wall(m, p, z_top - bevel, p, z_top - depth, n, part);
}

static int circle(float cx, float cy, float r, int seg, pt2 *out) {
    for (int i = 0; i < seg; i++) {
        float a = (float)i / (float)seg * 2 * PI;
        out[i] = (pt2){ cx + cosf(a) * r, cy + sinf(a) * r };
    }
    return seg;
}

static int rounded_rect(float cx, float cy, float hw, float hh, float cr, pt2 *out) {
    enum { K = 5 };
    const float corner[4][2] = { { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 } };
    int n = 0;
    if (cr > hw) cr = hw;
    if (cr > hh) cr = hh;
    for (int c = 0; c < 4; c++) {
        float ox = cx + corner[c][0] * (hw - cr), oy = cy + corner[c][1] * (hh - cr);
        for (int i = 0; i <= K; i++) {
            float a = ((float)c + (float)i / K) * (PI / 2);
            out[n++] = (pt2){ ox + cosf(a) * cr, oy + sinf(a) * cr };
        }
    }
    return n;
}

/* Chaikin's corner cutting on a closed outline, iterations times. */
static int smooth(pt2 *p, int n, int iterations) {
    pt2 tmp[MAXPTS];
    for (int it = 0; it < iterations && n * 2 <= MAXPTS; it++) {
        int k = 0;
        for (int i = 0; i < n; i++) {
            pt2 a = p[i], b = p[(i + 1) % n];
            tmp[k++] = (pt2){ 0.75f * a.x + 0.25f * b.x, 0.75f * a.y + 0.25f * b.y };
            tmp[k++] = (pt2){ 0.25f * a.x + 0.75f * b.x, 0.25f * a.y + 0.75f * b.y };
        }
        memcpy(p, tmp, sizeof *p * (size_t)k);
        n = k;
    }
    return n;
}

/* ---- the families ------------------------------------------------------ */

typedef struct {
    const pt2 *half;          /* the body's right side, the top's middle to the bottom's */
    int half_n;
    pt2 lstick, rstick, dpad, face;
    float face_gap;
    pt2 back, start, guide;
    float small_r, guide_r;
    int has_guide, touchpad, capture;
} layout;

static const pt2 xbox_half[] = {
    { 0, 40 }, { 30, 41 }, { 52, 38 }, { 66, 30 }, { 74, 16 }, { 80, -4 }, { 82, -24 }, { 80, -42 },
    { 73, -54 }, { 62, -58 }, { 50, -54 }, { 40, -42 }, { 30, -30 }, { 15, -26 }, { 0, -26 },
};
static const pt2 playstation_half[] = {
    { 0, 36 }, { 36, 38 }, { 58, 36 }, { 72, 28 }, { 80, 14 }, { 86, -10 }, { 90, -36 }, { 88, -54 },
    { 80, -64 }, { 68, -66 }, { 58, -60 }, { 50, -46 }, { 40, -32 }, { 22, -26 }, { 0, -26 },
};
static const pt2 nintendo_half[] = {
    { 0, 40 }, { 32, 41 }, { 54, 37 }, { 68, 28 }, { 76, 12 }, { 80, -8 }, { 80, -30 }, { 76, -46 },
    { 68, -56 }, { 57, -58 }, { 47, -52 }, { 38, -40 }, { 28, -30 }, { 14, -27 }, { 0, -27 },
};
static const pt2 generic_half[] = {
    { 0, 38 }, { 40, 38 }, { 62, 34 }, { 74, 20 }, { 78, 0 }, { 78, -30 }, { 72, -46 }, { 60, -52 },
    { 48, -46 }, { 36, -32 }, { 18, -28 }, { 0, -28 },
};

#define HALF(a) a, (int)(sizeof a / sizeof *a)
static const layout layouts[4] = {
    /* Xbox-like: the left stick above the d-pad, a large guide button. */
    { HALF(xbox_half), { -46, 14 }, { 24, -14 }, { -24, -14 }, { 46, 14 }, 11,
      { -12, 17 }, { 12, 17 }, { 0, 29 }, 3.0f, 5.5f, 1, 0, 0 },
    /* PlayStation-like: the d-pad high, the sticks low, the touchpad between. */
    { HALF(playstation_half), { -23, -15 }, { 23, -15 }, { -47, 14 }, { 47, 14 }, 11.5f,
      { -29, 29 }, { 29, 29 }, { 0, -6 }, 2.4f, 3.6f, 1, 1, 0 },
    /* Nintendo-like: as Xbox's, minus and plus, home beside capture. */
    { HALF(nintendo_half), { -45, 14 }, { 23, -14 }, { -23, -14 }, { 45, 14 }, 11,
      { -14, 24 }, { 14, 24 }, { 9, 9 }, 2.6f, 3.4f, 1, 0, 1 },
    /* Any other: Xbox's places, no guide. */
    { HALF(generic_half), { -46, 12 }, { 24, -14 }, { -24, -14 }, { 46, 12 }, 11,
      { -12, 18 }, { 12, 18 }, { 0, 0 }, 3.0f, 0, 0, 0, 0 },
};

static void mark(dxl_padmesh *m, int part, float x, float y, float z, float r) {
    m->has[part] = 1;
    m->anchor[part] = (dxl_v3){ x, y, z };
    m->pivot[part] = (dxl_v3){ x, y, z };
    m->radius[part] = r;
}

static void button(dxl_padmesh *m, int part, pt2 c, float r, float z_top) {
    pt2 p[MAXPTS];
    int n = circle(c.x, c.y, r, 20, p);
    extrude(m, p, n, z_top, z_top, r * 0.22f, part);
    mark(m, part, c.x, c.y, z_top, r);
}

static void stick(dxl_padmesh *m, int part, pt2 c) {
    pt2 p[MAXPTS];
    int n = circle(c.x, c.y, 11.5f, 24, p);
    extrude(m, p, n, 0.4f, 0.4f, 0.3f, DXL_PADPART_TRIM);
    n = circle(c.x, c.y, 3.5f, 12, p);
    extrude(m, p, n, 4.5f, 4.2f, 0.3f, part);
    n = circle(c.x, c.y, 8.5f, 24, p);
    extrude(m, p, n, 9.0f, 4.5f, 2.0f, part);
    mark(m, part, c.x, c.y, 9.0f, 8.5f);
    m->pivot[part] = (dxl_v3){ c.x, c.y, -6.0f };
}

static void dpad(dxl_padmesh *m, pt2 c) {
    pt2 p[MAXPTS];
    const struct { int part; float dx, dy, hw, hh; } arms[] = {
        { DXL_PADPART_DPUP, 0, 7.6f, 3.8f, 5.0f }, { DXL_PADPART_DPDOWN, 0, -7.6f, 3.8f, 5.0f },
        { DXL_PADPART_DPLEFT, -7.6f, 0, 5.0f, 3.8f }, { DXL_PADPART_DPRIGHT, 7.6f, 0, 5.0f, 3.8f },
    };
    for (int i = 0; i < 4; i++) {
        int n = rounded_rect(c.x + arms[i].dx, c.y + arms[i].dy, arms[i].hw, arms[i].hh, 1.2f, p);
        extrude(m, p, n, 2.6f, 2.6f, 0.9f, arms[i].part);
        mark(m, arms[i].part, c.x + arms[i].dx * 1.25f, c.y + arms[i].dy * 1.25f, 2.6f, 4.5f);
    }
    int n = rounded_rect(c.x, c.y, 3.8f, 3.8f, 0.6f, p);
    extrude(m, p, n, 2.2f, 2.2f, 0.5f, DXL_PADPART_TRIM);
}

static void shoulders(dxl_padmesh *m, const layout *l) {
    pt2 p[MAXPTS];
    float top = 0;
    for (int i = 0; i < l->half_n; i++)
        if (l->half[i].y > top) top = l->half[i].y;
    for (int side = -1; side <= 1; side += 2) {
        float x = 50.0f * (float)side;
        int part = side < 0 ? DXL_PADPART_LSHOULDER : DXL_PADPART_RSHOULDER;
        int n = rounded_rect(x, top + 1.0f, 17, 3.6f, 3.0f, p);
        extrude(m, p, n, -3.0f, 10.0f, 1.5f, part);
        mark(m, part, x, top + 4.6f, -3.0f, 9.0f);
        part = side < 0 ? DXL_PADPART_LTRIGGER : DXL_PADPART_RTRIGGER;
        n = rounded_rect(x, top + 9.0f, 13, 5.5f, 4.0f, p);
        extrude(m, p, n, -9.0f, 12.0f, 2.0f, part);
        mark(m, part, x, top + 14.5f, -9.0f, 9.0f);
        m->pivot[part] = (dxl_v3){ x, top + 14.5f, -21.0f };
    }
}

void dxl_padmesh_build(dxl_padmesh *m, int family) {
    memset(m, 0, sizeof *m);
    const layout *l = &layouts[family >= 0 && family < 4 ? family : 3];

    /* The body: the right side, then the left mirrored, smoothed. */
    pt2 body[MAXPTS];
    int n = 0;
    for (int i = 0; i < l->half_n; i++) body[n++] = l->half[i];
    for (int i = l->half_n - 2; i > 0; i--) body[n++] = (pt2){ -l->half[i].x, l->half[i].y };
    n = smooth(body, n, 2);
    extrude(m, body, n, 0.0f, 24.0f, 5.0f, DXL_PADPART_BODY);
    m->has[DXL_PADPART_BODY] = 1;

    stick(m, DXL_PADPART_LSTICK, l->lstick);
    stick(m, DXL_PADPART_RSTICK, l->rstick);
    dpad(m, l->dpad);
    float g = l->face_gap;
    button(m, DXL_PADPART_SOUTH, (pt2){ l->face.x, l->face.y - g }, 5.6f, 3.5f);
    button(m, DXL_PADPART_EAST, (pt2){ l->face.x + g, l->face.y }, 5.6f, 3.5f);
    button(m, DXL_PADPART_WEST, (pt2){ l->face.x - g, l->face.y }, 5.6f, 3.5f);
    button(m, DXL_PADPART_NORTH, (pt2){ l->face.x, l->face.y + g }, 5.6f, 3.5f);
    button(m, DXL_PADPART_BACK, l->back, l->small_r, 1.8f);
    button(m, DXL_PADPART_START, l->start, l->small_r, 1.8f);
    if (l->has_guide) button(m, DXL_PADPART_GUIDE, l->guide, l->guide_r, 1.6f);
    pt2 p[MAXPTS];
    if (l->touchpad) {
        int k = rounded_rect(0, 21, 21, 12, 3, p);
        extrude(m, p, k, 1.0f, 1.0f, 0.8f, DXL_PADPART_TRIM);
    }
    if (l->capture) {
        int k = rounded_rect(-l->guide.x, l->guide.y, 2.6f, 2.6f, 0.6f, p);
        extrude(m, p, k, 1.4f, 1.4f, 0.4f, DXL_PADPART_TRIM);
    }
    shoulders(m, l);
    m->has[DXL_PADPART_TRIM] = 1;
}

void dxl_padmesh_free(dxl_padmesh *m) {
    free(m->tris);
    memset(m, 0, sizeof *m);
}

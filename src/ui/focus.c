#include "ui/focus.h"

#include <float.h>
#include <math.h>
#include <string.h>

void dxl_focus_clear(dxl_focus_list *l) {
    l->count = 0;
    l->overflowed = 0;
}

void dxl_focus_add(dxl_focus_list *l, uint32_t id, dxl_box box, int scope) {
    if (l->count >= DXL_FOCUS_MAX) {
        l->overflowed = 1;
        return;
    }
    l->items[l->count].id = id;
    l->items[l->count].box = box;
    l->items[l->count].scope = scope;
    l->items[l->count].own_lr = 0;
    l->count++;
}

int dxl_focus_top_scope(const dxl_focus_list *l) {
    int top = 0;
    for (int i = 0; i < l->count; i++)
        if (l->items[i].scope > top) top = l->items[i].scope;
    return top;
}

static const dxl_focusable *find(const dxl_focus_list *l, uint32_t id, int scope) {
    for (int i = 0; i < l->count; i++)
        if (l->items[i].id == id && l->items[i].scope == scope) return &l->items[i];
    return NULL;
}

int dxl_focus_has(const dxl_focus_list *l, uint32_t id) {
    return id && find(l, id, dxl_focus_top_scope(l)) != NULL;
}

uint32_t dxl_focus_first(const dxl_focus_list *l) {
    int top = dxl_focus_top_scope(l);
    for (int i = 0; i < l->count; i++)
        if (l->items[i].scope == top) return l->items[i].id;
    return 0;
}

/* How far apart two ranges are along an axis: 0 when they overlap. */
static float gap(float a0, float a1, float b0, float b1) {
    if (b1 < a0) return a0 - b1;
    if (b0 > a1) return b0 - a1;
    return 0;
}

uint32_t dxl_focus_move(const dxl_focus_list *l, uint32_t from, dxl_dir dir) {
    int top = dxl_focus_top_scope(l);
    const dxl_focusable *cur = find(l, from, top);
    if (!cur) return dxl_focus_first(l);
    const dxl_box a = cur->box;
    float acx = a.x + a.w / 2, acy = a.y + a.h / 2;

    uint32_t best = from;
    float best_score = FLT_MAX;
    int best_overlap = 0;
    for (int i = 0; i < l->count; i++) {
        const dxl_focusable *f = &l->items[i];
        if (f->scope != top || f->id == from) continue;
        const dxl_box b = f->box;
        float bcx = b.x + b.w / 2, bcy = b.y + b.h / 2;
        float along, across;
        int overlap;
        switch (dir) {
        case DXL_DIR_UP:
            if (bcy >= acy - 0.5f || b.y >= a.y) continue;
            along = a.y - (b.y + b.h); across = gap(a.x, a.x + a.w, b.x, b.x + b.w);
            break;
        case DXL_DIR_DOWN:
            if (bcy <= acy + 0.5f || b.y + b.h <= a.y + a.h) continue;
            along = b.y - (a.y + a.h); across = gap(a.x, a.x + a.w, b.x, b.x + b.w);
            break;
        case DXL_DIR_LEFT:
            if (bcx >= acx - 0.5f || b.x >= a.x) continue;
            along = a.x - (b.x + b.w); across = gap(a.y, a.y + a.h, b.y, b.y + b.h);
            break;
        default:
            if (bcx <= acx + 0.5f || b.x + b.w <= a.x + a.w) continue;
            along = b.x - (a.x + a.w); across = gap(a.y, a.y + a.h, b.y, b.y + b.h);
            break;
        }
        if (along < 0) along = 0;
        overlap = across == 0;
        /* The middles' distance across settles ties between overlapping ones. */
        float centre = (dir == DXL_DIR_UP || dir == DXL_DIR_DOWN) ? fabsf(bcx - acx) : fabsf(bcy - acy);
        float score = along + 2 * across + centre * 0.01f;
        if ((overlap && !best_overlap) || (overlap == best_overlap && score < best_score)) {
            best = f->id;
            best_score = score;
            best_overlap = overlap;
        }
    }
    return best;
}

void dxl_focus_own_lr(dxl_focus_list *l, uint32_t id) {
    for (int i = 0; i < l->count; i++)
        if (l->items[i].id == id) l->items[i].own_lr = 1;
}

/* Its place in the list's scope, or -1. */
static int place(const dxl_focus_list *l, uint32_t id, int scope) {
    for (int i = 0; i < l->count; i++)
        if (l->items[i].id == id && l->items[i].scope == scope) return i;
    return -1;
}

int dxl_focus_reaches(const dxl_focus_list *l, uint32_t from, uint32_t to) {
    int top = dxl_focus_top_scope(l);
    int goal = place(l, to, top), start = place(l, from, top);
    if (start < 0) start = place(l, dxl_focus_first(l), top);
    if (goal < 0 || start < 0) return 0;
    /* Every move from each place reached, breadth first, each place once. */
    int queue[DXL_FOCUS_MAX], head = 0, tail = 0;
    char seen[DXL_FOCUS_MAX];
    memset(seen, 0, sizeof seen);
    queue[tail++] = start;
    seen[start] = 1;
    while (head < tail) {
        int at = queue[head++];
        if (at == goal) return 1;
        const dxl_focusable *f = &l->items[at];
        for (int d = DXL_DIR_UP; d <= DXL_DIR_RIGHT; d++) {
            if (f->own_lr && (d == DXL_DIR_LEFT || d == DXL_DIR_RIGHT)) continue;
            int next = place(l, dxl_focus_move(l, f->id, (dxl_dir)d), top);
            if (next >= 0 && !seen[next]) {
                seen[next] = 1;
                queue[tail++] = next;
            }
        }
    }
    return 0;
}

uint32_t dxl_focus_step(const dxl_focus_list *l, uint32_t from, int step) {
    int top = dxl_focus_top_scope(l);
    int idx[DXL_FOCUS_MAX], n = 0, at = -1;
    for (int i = 0; i < l->count; i++) {
        if (l->items[i].scope != top) continue;
        if (l->items[i].id == from) at = n;
        idx[n++] = i;
    }
    if (n == 0) return 0;
    if (at < 0) return l->items[idx[step > 0 ? 0 : n - 1]].id;
    int next = ((at + step) % n + n) % n;
    return l->items[idx[next]].id;
}

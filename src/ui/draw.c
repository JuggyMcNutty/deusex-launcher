#include "ui/draw.h"

#include <math.h>

static SDL_FColor fcol(dxl_rgba c) {
    SDL_FColor f = { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f };
    return f;
}

static void set(dxl_ui *ui, dxl_rgba c) {
    SDL_SetRenderDrawBlendMode(ui->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ui->renderer, c.r, c.g, c.b, c.a);
}

/* A rect in whole pixels, so an edge is never half a pixel and blurred. */
static SDL_FRect snap(dxl_ui *ui, dxl_rect r) {
    dxl_rect p = dxl_px(ui, r);
    float x0 = floorf(p.x + 0.5f), y0 = floorf(p.y + 0.5f);
    float x1 = floorf(p.x + p.w + 0.5f), y1 = floorf(p.y + p.h + 0.5f);
    SDL_FRect s = { x0, y0, x1 - x0, y1 - y0 };
    return s;
}

/* At least one pixel. */
static float thick(dxl_ui *ui, float w) {
    float p = floorf(dxl_pxf(ui, w) + 0.5f);
    return p < 1 ? 1 : p;
}

void dxl_fill(dxl_ui *ui, dxl_rect r, dxl_rgba c) {
    if (c.a == 0) return;
    SDL_FRect p = snap(ui, r);
    set(ui, c);
    SDL_RenderFillRect(ui->renderer, &p);
}

void dxl_gradient(dxl_ui *ui, dxl_rect r, dxl_rgba top, dxl_rgba bottom) {
    SDL_FRect p = snap(ui, r);
    SDL_Vertex v[4] = {
        { { p.x, p.y }, fcol(top), { 0, 0 } },
        { { p.x + p.w, p.y }, fcol(top), { 0, 0 } },
        { { p.x + p.w, p.y + p.h }, fcol(bottom), { 0, 0 } },
        { { p.x, p.y + p.h }, fcol(bottom), { 0, 0 } },
    };
    static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(ui->renderer, NULL, v, 4, idx, 6);
}

void dxl_outline(dxl_ui *ui, dxl_rect r, float w, dxl_rgba c) {
    SDL_FRect p = snap(ui, r);
    float t = thick(ui, w);
    set(ui, c);
    SDL_FRect e[4] = {
        { p.x, p.y, p.w, t }, { p.x, p.y + p.h - t, p.w, t },
        { p.x, p.y + t, t, p.h - 2 * t }, { p.x + p.w - t, p.y + t, t, p.h - 2 * t },
    };
    SDL_RenderFillRects(ui->renderer, e, 4);
}

void dxl_line(dxl_ui *ui, float ax, float ay, float bx, float by, float w, dxl_rgba c) {
    float x0 = dxl_pxf(ui, ax), y0 = dxl_pxf(ui, ay), x1 = dxl_pxf(ui, bx), y1 = dxl_pxf(ui, by);
    float t = thick(ui, w);
    /* Straight ones as rects about the line, in whole pixels: crisp at any
     * scale. */
    if (fabsf(y0 - y1) < 0.01f || fabsf(x0 - x1) < 0.01f) {
        SDL_FRect r;
        if (fabsf(y0 - y1) < 0.01f)
            r = (SDL_FRect){ floorf(fminf(x0, x1) + 0.5f), floorf(y0 - t / 2 + 0.5f), floorf(fabsf(x1 - x0) + 0.5f), t };
        else
            r = (SDL_FRect){ floorf(x0 - t / 2 + 0.5f), floorf(fminf(y0, y1) + 0.5f), t, floorf(fabsf(y1 - y0) + 0.5f) };
        set(ui, c);
        SDL_RenderFillRect(ui->renderer, &r);
        return;
    }
    float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
    float nx = -dy / len * t / 2, ny = dx / len * t / 2;
    SDL_FColor f = fcol(c);
    SDL_Vertex v[4] = {
        { { x0 + nx, y0 + ny }, f, { 0, 0 } }, { { x1 + nx, y1 + ny }, f, { 0, 0 } },
        { { x1 - nx, y1 - ny }, f, { 0, 0 } }, { { x0 - nx, y0 - ny }, f, { 0, 0 } },
    };
    static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(ui->renderer, NULL, v, 4, idx, 6);
}

void dxl_poly(dxl_ui *ui, const SDL_FPoint *pts, int n, dxl_rgba c) {
    if (n < 3 || n > 32) return;
    SDL_Vertex v[32];
    int idx[90], k = 0;
    SDL_FColor f = fcol(c);
    for (int i = 0; i < n; i++) {
        v[i].position.x = dxl_pxf(ui, pts[i].x);
        v[i].position.y = dxl_pxf(ui, pts[i].y);
        v[i].color = f;
        v[i].tex_coord.x = v[i].tex_coord.y = 0;
    }
    for (int i = 1; i + 1 < n; i++) { idx[k++] = 0; idx[k++] = i; idx[k++] = i + 1; }
    SDL_RenderGeometry(ui->renderer, NULL, v, n, idx, k);
}

void dxl_disc(dxl_ui *ui, float cx, float cy, float radius, dxl_rgba c) {
    enum { SEG = 28 };
    SDL_Vertex v[SEG + 1];
    int idx[SEG * 3];
    SDL_FColor f = fcol(c);
    float x = dxl_pxf(ui, cx), y = dxl_pxf(ui, cy), r = dxl_pxf(ui, radius);
    v[0] = (SDL_Vertex){ { x, y }, f, { 0, 0 } };
    for (int i = 0; i < SEG; i++) {
        float a = (float)i / SEG * 6.2831853f;
        v[i + 1] = (SDL_Vertex){ { x + cosf(a) * r, y + sinf(a) * r }, f, { 0, 0 } };
        idx[i * 3] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = (i + 1) % SEG + 1;
    }
    SDL_RenderGeometry(ui->renderer, NULL, v, SEG + 1, idx, SEG * 3);
}

void dxl_bevel(dxl_ui *ui, dxl_rect r, const dxl_bevel_style *s) {
    SDL_FRect p = snap(ui, r);
    float c = floorf(dxl_pxf(ui, s->chamfer) + 0.5f);
    if (c * 2 > p.w) c = p.w / 2;
    if (c * 2 > p.h) c = p.h / 2;
    /* The octagon, clockwise from the top edge's left end. */
    SDL_FPoint o[8] = {
        { p.x + c, p.y }, { p.x + p.w - c, p.y }, { p.x + p.w, p.y + c }, { p.x + p.w, p.y + p.h - c },
        { p.x + p.w - c, p.y + p.h }, { p.x + c, p.y + p.h }, { p.x, p.y + p.h - c }, { p.x, p.y + c },
    };
    SDL_Vertex v[9];
    int idx[24];
    for (int i = 0; i < 8; i++) {
        float t = (o[i].y - p.y) / (p.h > 0 ? p.h : 1);
        dxl_rgba col = dxl_rgba_mix(s->top, s->bottom, t);
        v[i + 1] = (SDL_Vertex){ o[i], fcol(col), { 0, 0 } };
        idx[i * 3] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = (i + 1) % 8 + 1;
    }
    v[0] = (SDL_Vertex){ { p.x + p.w / 2, p.y + p.h / 2 }, fcol(dxl_rgba_mix(s->top, s->bottom, 0.5f)), { 0, 0 } };
    SDL_RenderGeometry(ui->renderer, NULL, v, 9, idx, 24);

    /* The edges, a pixel or so wide: lit up and to the left, shadowed down
     * and to the right, as the game's light falls on its panels. */
    float w = thick(ui, 1);
    set(ui, s->light);
    SDL_FRect lit[2] = { { o[0].x, p.y, o[1].x - o[0].x, w }, { p.x, o[7].y, w, o[6].y - o[7].y } };
    SDL_RenderFillRects(ui->renderer, lit, 2);
    set(ui, s->shadow);
    SDL_FRect dark[2] = { { o[5].x, p.y + p.h - w, o[4].x - o[5].x, w }, { p.x + p.w - w, o[2].y, w, o[3].y - o[2].y } };
    SDL_RenderFillRects(ui->renderer, dark, 2);
    if (c >= 2) {
        /* The cut corners: the top left lit, the bottom right in shadow, the
         * other two between. */
        dxl_rgba mid = dxl_rgba_mix(s->light, s->shadow, 0.5f);
        float k = 1 / ui->scale;
        dxl_line(ui, o[7].x * k, o[7].y * k, o[0].x * k, o[0].y * k, 1, s->light);
        dxl_line(ui, o[1].x * k, o[1].y * k, o[2].x * k, o[2].y * k, 1, mid);
        dxl_line(ui, o[3].x * k, o[3].y * k, o[4].x * k, o[4].y * k, 1, s->shadow);
        dxl_line(ui, o[5].x * k, o[5].y * k, o[6].x * k, o[6].y * k, 1, mid);
    }
}

void dxl_brackets(dxl_ui *ui, dxl_rect r, float len, float w, dxl_rgba c) {
    if (c.a == 0 || r.w <= 0 || r.h <= 0) return;
    SDL_FRect p = snap(ui, r);
    float t = thick(ui, w);
    float l = floorf(dxl_pxf(ui, len) + 0.5f);
    if (l > p.w / 2) l = floorf(p.w / 2);
    if (l > p.h / 2) l = floorf(p.h / 2);
    set(ui, c);
    SDL_FRect e[8] = {
        { p.x, p.y, l, t }, { p.x, p.y, t, l },
        { p.x + p.w - l, p.y, l, t }, { p.x + p.w - t, p.y, t, l },
        { p.x, p.y + p.h - t, l, t }, { p.x, p.y + p.h - l, t, l },
        { p.x + p.w - l, p.y + p.h - t, l, t }, { p.x + p.w - t, p.y + p.h - l, t, l },
    };
    SDL_RenderFillRects(ui->renderer, e, 8);
}

static void backdrop(dxl_ui *ui) {
    dxl_rect all = { 0, 0, ui->w, ui->h };
    dxl_gradient(ui, all, ui->theme.backdrop_top, ui->theme.backdrop_bottom);
    /* A faint grid, 48 dp apart, which never moves: no screen effects, no
     * live background. */
    for (float x = 48; x < ui->w; x += 48) dxl_line(ui, x, 0, x, ui->h, 1, ui->theme.grid);
    for (float y = 48; y < ui->h; y += 48) dxl_line(ui, 0, y, ui->w, y, 1, ui->theme.grid);
}

void dxl_backdrop(dxl_ui *ui) {
    /* Drawn into a texture once for the size: a copy each frame after, which
     * the software renderer does many times faster. */
    if (ui->backdrop && (ui->backdrop_w != ui->w_px || ui->backdrop_h != ui->h_px)) {
        SDL_DestroyTexture(ui->backdrop);
        ui->backdrop = NULL;
    }
    if (!ui->backdrop && ui->w_px > 0 && ui->h_px > 0) {
        ui->backdrop = SDL_CreateTexture(ui->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, ui->w_px,
                                         ui->h_px);
        SDL_Texture *was = SDL_GetRenderTarget(ui->renderer);
        if (ui->backdrop && SDL_SetRenderTarget(ui->renderer, ui->backdrop)) {
            SDL_SetTextureScaleMode(ui->backdrop, SDL_SCALEMODE_NEAREST);
            backdrop(ui);
            SDL_SetRenderTarget(ui->renderer, was);
            ui->backdrop_w = ui->w_px;
            ui->backdrop_h = ui->h_px;
        } else if (ui->backdrop) {
            SDL_DestroyTexture(ui->backdrop);
            ui->backdrop = NULL;
        }
    }
    if (ui->backdrop) SDL_RenderTexture(ui->renderer, ui->backdrop, NULL, NULL);
    else backdrop(ui);
}

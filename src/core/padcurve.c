#include "core/padcurve.h"

#include <math.h>

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void dxl_pad_stick(float x, float y, float dead_zone, float curve, float *out_x, float *out_y) {
    float dz = clampf(dead_zone, 0.0f, DXL_PAD_DEAD_ZONE_MAX);
    float mag = sqrtf(x * x + y * y);
    if (mag <= dz || mag <= 0.0f) {
        *out_x = *out_y = 0.0f;
        return;
    }
    float scaled = (mag - dz) / (1.0f - dz);
    if (scaled > 1.0f) scaled = 1.0f;
    scaled = powf(scaled, curve);
    *out_x = x / mag * scaled;
    *out_y = y / mag * scaled;
}

void dxl_pad_move(float x, float y, const dxl_pad_tuning *t, float *out_x, float *out_y) {
    float ox, oy;
    dxl_pad_stick(x, y, t->dead_zone, DXL_PAD_CURVE_MOVE, &ox, &oy);
    *out_x = ox;
    *out_y = -oy;
}

void dxl_pad_look(float x, float y, const dxl_pad_tuning *t, float *out_x, float *out_y) {
    float ox, oy;
    dxl_pad_stick(x, y, t->dead_zone, DXL_PAD_CURVE_LOOK, &ox, &oy);
    *out_x = ox * t->look_x;
    *out_y = -(oy * t->look_y * (t->invert_y ? -1.0f : 1.0f));
}

int dxl_pad_trigger_down(float v, int was) {
    return was ? v > DXL_PAD_TRIGGER_RELEASE : v > DXL_PAD_TRIGGER_PRESS;
}

int dxl_pad_sent(float v) { return (int)lroundf(v * 100.0f); }

/* What the game makes of a pad's sticks and triggers: DXEngine's own
 * processing (its GamepadInput.cpp), for the Controller page to draw. Axis
 * values are SDL's, -1 to 1, right and down positive; the game's are up
 * positive, a whole number to 100.
 *
 * A stick goes through a radial dead zone, its distance from the centre
 * rescaled from the zone's edge to full travel, its direction kept, then a
 * curve: none for the left stick, which moves, 1.6 for the right, which
 * looks (times the look sensitivities, its Y flipped when inverted), 2.0
 * for either as the menus' pointer. A trigger is a button past half travel,
 * released under 0.35.
 */
#ifndef DXL_PADCURVE_H
#define DXL_PADCURVE_H

#define DXL_PAD_CURVE_MOVE    1.0f
#define DXL_PAD_CURVE_LOOK    1.6f
#define DXL_PAD_CURVE_POINTER 2.0f
#define DXL_PAD_TRIGGER_PRESS   0.5f
#define DXL_PAD_TRIGGER_RELEASE 0.35f
/* Settings.json's Gamepad.DeadZone as the engine takes it. */
#define DXL_PAD_DEAD_ZONE_MAX 0.9f

typedef struct {
    float dead_zone;          /* Gamepad.DeadZone */
    float look_x, look_y;     /* Gamepad.LookSensitivityX, Y */
    int   invert_y;           /* Gamepad.InvertY */
} dxl_pad_tuning;

/* The stick at x, y through the dead zone and the curve. */
void dxl_pad_stick(float x, float y, float dead_zone, float curve, float *out_x, float *out_y);
/* The left stick as the game moves with it, the right as it looks: the
 * game's values, up positive, before the x100. */
void dxl_pad_move(float x, float y, const dxl_pad_tuning *t, float *out_x, float *out_y);
void dxl_pad_look(float x, float y, const dxl_pad_tuning *t, float *out_x, float *out_y);
/* Whether a trigger at v is down, given whether it was. */
int  dxl_pad_trigger_down(float v, int was);
/* A value as the engine sends it: x100, the nearest whole. */
int  dxl_pad_sent(float v);

#endif

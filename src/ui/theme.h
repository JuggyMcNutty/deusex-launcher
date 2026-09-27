/* Palette, metrics and font selection.
 *
 * The look follows the game's own UI: near-black ground with amber/gold text.
 * That is not decoration -- the launcher is the first thing someone sees, and
 * having it match means the handheld build does not feel bolted on.
 *
 * Every metric is expressed for the 1280x720 panel this targeted first but
 * derived from the actual surface size, so a different device is a
 * configuration change rather than a rewrite.
 */
#ifndef DXL_THEME_H
#define DXL_THEME_H

#include <SDL.h>

typedef struct {
    SDL_Color bg, panel, panel_sel;
    SDL_Color text, text_dim, accent, accent_dim;
    SDL_Color ok, warn, bad;
    SDL_Color rule;
} dxl_palette;

typedef struct {
    int pad;            /* outer gutter */
    int title_size;
    int body_size;
    int item_size;
    int footer_size;
    int item_height;
    int line_gap;
} dxl_metrics;

extern const dxl_palette DXL_PAL;

/* Scales the metrics from the surface height; 720 is the reference. */
void dxl_metrics_for(dxl_metrics *m, int height);

/* First readable font found: DXL_FONT, then the device profile's fonts
 * (platform/target.h), then the usual desktop locations -- which also lets
 * the UI be iterated on without the device in reach. Returns NULL if
 * nothing is found, which the caller must survive -- a launcher that cannot
 * find a font should still be able to say so. */
const char *dxl_theme_find_font(void);

#endif

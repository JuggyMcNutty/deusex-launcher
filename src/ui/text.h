/* Text: the launcher's fonts and SDL3_ttf's renderer text engine.
 *
 * Seven roles, each a bundled font at a size in dp (data/fonts: Barlow Semi
 * Condensed for the UI, Share Tech Mono for "terminal" text), opened at the
 * pixel size the scale makes of it, again when the scale changes. Laid-out
 * texts (TTF_Text) are kept in a cache of the most recently drawn, keyed by
 * role, wrap width and string. The typewriter reveals a text line by line,
 * the line being typed growing from the left.
 */
#ifndef DXL_UI_TEXT_H
#define DXL_UI_TEXT_H

#include "ui/kit.h"

/* Opens the fonts at ui->scale. -1 when one is missing. */
int  dxl_text_open(dxl_ui *ui);
void dxl_text_close(dxl_ui *ui);
/* The texts made with a renderer, let go before it is (the window torn
 * down for the game). */
void dxl_text_flush(dxl_ui *ui);

/* The size of s, wrapped at wrap (dp; 0 for one line), in dp. */
SDL_FPoint dxl_text_size(dxl_ui *ui, dxl_font_role role, const char *s, float wrap);
/* A line's height in dp. */
float      dxl_text_line(dxl_ui *ui, dxl_font_role role);
/* Draws s with its top left at x, y (dp), wrapped at wrap; reveal is how
 * much of it shows, 0 to 1 (the typewriter), 1 for all. Returns its size. */
SDL_FPoint dxl_text_draw(dxl_ui *ui, dxl_font_role role, const char *s, float x, float y, float wrap,
                         dxl_rgba c, float reveal);
/* The longest start of s that fits in w dp, "..." added when it is cut:
 * into out. Returns 1 when it was cut. */
int        dxl_text_fit(dxl_ui *ui, dxl_font_role role, const char *s, float w, char *out, size_t size);
/* The same keeping the end: "..." and the longest end of s that fits (a
 * path's last folders). */
int        dxl_text_fit_tail(dxl_ui *ui, dxl_font_role role, const char *s, float w, char *out, size_t size);

#endif

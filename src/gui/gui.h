/* The look: the original's Win32 controls, drawn with SDL2.
 *
 * The launcher's pages are Window.dll's dialog templates (dx-reverse-info/
 * wizard.md "Page layouts"): each control at its place in dialog units, in
 * MS Sans Serif 8 pt. A dialog unit is a quarter of that font's average
 * character width across and an eighth of its height down -- 6 and 13 pixels
 * at 96 DPI -- and Windows rounds MulDiv's way, which puts every control
 * where the original has it, pixel for pixel (checked against the original
 * running under wine: dx-reverse-info/live-verification.md).
 *
 * The colours are the system's, as the original shows them there, and the
 * text is drawn without smoothing, as its bitmap font is: wine's own MS Sans
 * Serif (sserife.fon) where wine is installed, else a common sans.
 *
 * Everything draws into an SDL_Renderer: a window's, or a software one on a
 * surface, which is how tools/pageshots.c makes pictures with no display.
 */
#ifndef DXL_GUI_H
#define DXL_GUI_H

#include "core/common.h"
#include "core/peicon.h"

#include <SDL.h>
#include <SDL_ttf.h>

typedef struct { Uint8 r, g, b; } dxl_rgb;

/* The system's colours. */
typedef struct {
    dxl_rgb face;       /* COLOR_3DFACE */
    dxl_rgb highlight;  /* COLOR_3DHILIGHT */
    dxl_rgb light;      /* COLOR_3DLIGHT */
    dxl_rgb shadow;     /* COLOR_3DSHADOW */
    dxl_rgb dkshadow;   /* COLOR_3DDKSHADOW */
    dxl_rgb window;     /* COLOR_WINDOW */
    dxl_rgb text;       /* COLOR_WINDOWTEXT, COLOR_BTNTEXT */
    dxl_rgb sel;        /* COLOR_HIGHLIGHT */
    dxl_rgb sel_text;   /* COLOR_HIGHLIGHTTEXT */
    dxl_rgb url;        /* WUrlButton's RGB(0,0,255) */
    dxl_rgb frame;      /* COLOR_WINDOWFRAME: the default button's outline */
} dxl_palette;

extern const dxl_palette dxl_colors;

typedef struct {
    TTF_Font *font;      /* the dialog font */
    TTF_Font *font_url;  /* Window.dll's hFontUrl: Arial, 9 pt, underlined */
    TTF_Font *font_msg;  /* a message box's: the system's message font, Tahoma 8 pt */
    char     *font_path;
    int       line_h;    /* the dialog font's height: 13 */
    /* A message box's error icon: wine's IDI_HAND, its user32.dll's icon
     * group 32513 at 32x32, premultiplied, where wine is installed; else
     * none, and the box draws its own. */
    dxl_icon  error_icon;
    int       ttf_ready, sdl_ready;
    /* Set (by a signal handler): every dialog ends as if its window were
     * closed. May stay NULL. */
    volatile int *quit;
} dxl_gui;

/* video 0: fonts only, for drawing into surfaces with no display. */
int  dxl_gui_init(dxl_gui *g, int video, dxl_err *err);
void dxl_gui_quit(dxl_gui *g);

/* Dialog units to pixels for MS Sans Serif 8 pt at 96 DPI, base units 6
 * and 13, rounded as MulDiv rounds. A dialog converts a control's x, y,
 * width and height each on its own (the captures agree on every control),
 * not its edges. */
static inline int dxl_dlu_x(int x) { return (x * 6 + 2) / 4; }
static inline int dxl_dlu_y(int y) { return (y * 13 + 4) / 8; }

/* ---- drawing ------------------------------------------------------------ */

void dxl_fill(SDL_Renderer *r, SDL_Rect rc, dxl_rgb c);
void dxl_hline(SDL_Renderer *r, int x0, int x1, int y, dxl_rgb c);   /* x0..x1-1 */
void dxl_vline(SDL_Renderer *r, int x, int y0, int y1, dxl_rgb c);   /* y0..y1-1 */

/* DrawEdge's borders: each a colour for the top-left and one for the
 * bottom-right, one pixel in from the last. */
enum {
    DXL_BDR_RAISEDOUTER = 1, DXL_BDR_SUNKENOUTER = 2,
    DXL_BDR_RAISEDINNER = 4, DXL_BDR_SUNKENINNER = 8,
    DXL_BF_SOFT = 16,   /* a push button's: a raised edge's top-left colours swapped */
};
#define DXL_EDGE_RAISED (DXL_BDR_RAISEDOUTER | DXL_BDR_RAISEDINNER)
#define DXL_EDGE_SUNKEN (DXL_BDR_SUNKENOUTER | DXL_BDR_SUNKENINNER)
#define DXL_EDGE_ETCHED (DXL_BDR_SUNKENOUTER | DXL_BDR_RAISEDINNER)
/* Draws the edge and returns the rectangle inside it. */
SDL_Rect dxl_edge(SDL_Renderer *r, SDL_Rect rc, int edge);

/* A dotted focus rectangle, as DrawFocusRect draws it: every other pixel
 * inverted -- here drawn over the colour under, which it inverts -- in a
 * checkerboard from the control's corner (origin). */
void dxl_focus_rect(SDL_Renderer *r, SDL_Rect rc, SDL_Point origin, dxl_rgb under);

/* ---- text --------------------------------------------------------------- */

/* A font's height as Windows gives it (tmHeight), which lines of text are
 * apart: its ascent and its descent. */
int  dxl_font_height(TTF_Font *f);

/* A control's text: & before a letter underlines it (its mnemonic), && is &. */
int  dxl_text_width(const dxl_gui *g, TTF_Font *f, const char *text);
/* One line at x, y (its top), mnemonics drawn. */
void dxl_text(SDL_Renderer *r, const dxl_gui *g, TTF_Font *f, int x, int y,
              const char *text, dxl_rgb c);

/* DrawText with word wrapping, left-aligned, inside rc; \r\n and \n break
 * lines. A line is as many whole words as fit with the spaces after them,
 * which count in its width, as wine measures it. Returns the height used.
 * With r NULL it only measures, and *width gets the widest line. */
int  dxl_text_wrapped(SDL_Renderer *r, const dxl_gui *g, TTF_Font *f, SDL_Rect rc,
                      const char *text, dxl_rgb c, int *width);

/* A control's mnemonic letter, lower case, or 0. */
int  dxl_mnemonic(const char *text);

/* ---- bitmaps ------------------------------------------------------------ */

/* A .bmp as a texture; NULL when it cannot be read. */
SDL_Texture *dxl_bitmap(SDL_Renderer *r, const char *path, int *w, int *h);

#endif

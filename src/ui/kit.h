/* The launcher's UI kit: immediate mode, on SDL3's renderer and SDL3_ttf.
 *
 * Each frame draws every widget again from the program's state, each by a
 * stable id; nothing is kept between frames but the kit's own (focus, the
 * pointer, animations, the text cache). Hit tests and navigation use the
 * rects the previous frame drew. Layout is by cutting rects (dxl_cut_*), in
 * dp, which the kit turns into pixels: dp x the display's scale x the
 * player's. Only SDL_Renderer's own drawing is used (textures, geometry,
 * render targets): it runs on OpenGL, OpenGL ES, Vulkan or the software
 * renderer alike.
 *
 * Diagnostics -- a label cut short, focusables on top of one another or off
 * the window -- are counted each frame, for dxl-shots and the tests.
 */
#ifndef DXL_UI_KIT_H
#define DXL_UI_KIT_H

#include "ui/focus.h"
#include "ui/theme.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

typedef SDL_FRect dxl_rect;   /* in dp unless a name says px */

typedef enum { DXL_INPUT_MOUSE, DXL_INPUT_KEYBOARD, DXL_INPUT_PAD } dxl_input_mode;

/* What the player did since the last frame. */
typedef struct {
    int   nav[4];          /* moves up, down, left, right (dxl_dir), repeats counted */
    int   activate, back;  /* Enter / the pad's confirm; Esc / its back */
    int   tab, untab;      /* Tab, Shift-Tab */
    int   page_up, page_down;
    int   start;           /* the pad's Start, F5: play */
    int   shoulder_l, shoulder_r;
    int   toggle_big;      /* F11 */
    int   pointer_moved;
    float px, py;          /* the pointer, dp */
    int   pressed, released;   /* the left button, this frame */
    float wheel;           /* notches, up positive */
    int   quit;            /* the window's close */
    int   resized;
    int   any;
} dxl_input;

typedef enum {
    DXL_FONT_TITLE, DXL_FONT_HEADING, DXL_FONT_BUTTON, DXL_FONT_LABEL, DXL_FONT_BODY,
    DXL_FONT_SMALL, DXL_FONT_MONO, DXL_FONT_COUNT
} dxl_font_role;

typedef struct dxl_text_cache dxl_text_cache;

/* A pad button's place, for the hints: what the pad's own labels are. */
typedef enum { DXL_PADFAMILY_XBOX, DXL_PADFAMILY_PLAYSTATION, DXL_PADFAMILY_NINTENDO, DXL_PADFAMILY_GENERIC } dxl_pad_family;

typedef struct dxl_ui {
    SDL_Window     *window;      /* NULL offscreen */
    SDL_Renderer   *renderer;
    TTF_TextEngine *engine;
    TTF_Font       *fonts[DXL_FONT_COUNT];
    dxl_text_cache *cache;
    char            fonts_dir[1024];
    float           font_scale;  /* the scale the fonts were opened at */

    dxl_theme       theme;
    float           scale;       /* px per dp */
    float           user_scale;
    int             w_px, h_px;
    float           w, h;        /* the window in dp */
    double          now;         /* seconds: the kit's clock */
    float           dt;

    dxl_input       in;
    dxl_input_mode  mode;        /* the last device used: hints and the pointer follow it */
    dxl_pad_family  pad_family;
    SDL_JoystickID  pad;         /* the pad last used, 0 for none */
    int             reduced_motion;

    /* Focus. */
    uint32_t        focus;
    uint32_t        focus_was;   /* as the frame began, before its moves */
    dxl_focus_list  prev, cur;   /* last frame's focusables, and this one's */
    int             scope;       /* the scope focusables are added to */
    int             focus_moved; /* by the keys or the pad this frame */
    uint32_t        hot, held;   /* under the pointer; pressed on */

    /* The brackets, tweened to the focused rect. */
    dxl_box         br_from, br_to, br_now;
    double          br_start;
    int             br_valid;

    int             animating;   /* something moves: draw again soon */
    unsigned long   frame;

    /* Values widgets keep from frame to frame (a list's scroll), by id. */
    struct { uint32_t id; float v; unsigned long used; } values[64];

    /* The clip rects pushed, in px. */
    SDL_Rect        clips[8];
    int             clip_count;

    /* Diagnostics, this frame. */
    int             diag_count;
    char            diag[8][192];

    uint32_t        id_seed;
    int             forget;      /* dxl_ui_forget: the next frame starts afresh */

    /* The backdrop, drawn once for the window's size (draw.h). */
    SDL_Texture    *backdrop;
    int             backdrop_w, backdrop_h;
} dxl_ui;

/* The kit on a renderer (a window's, or a software one on a surface for
 * dxl-shots), its fonts from fonts_dir. -1 when a font is missing. */
int  dxl_ui_init(dxl_ui *ui, SDL_Window *window, SDL_Renderer *renderer, const char *fonts_dir,
                 float user_scale);
void dxl_ui_free(dxl_ui *ui);
/* The renderer went and came back (the window torn down for the game). */
int  dxl_ui_attach(dxl_ui *ui, SDL_Window *window, SDL_Renderer *renderer);
void dxl_ui_detach(dxl_ui *ui);
/* As new, its fonts and renderer kept: no focus, nothing remembered from
 * a frame (dxl-shots, between shots). */
void dxl_ui_reset(dxl_ui *ui);

/* Before the frame's events: clears the input. */
void dxl_ui_input_begin(dxl_ui *ui);
/* Each event. */
void dxl_ui_event(dxl_ui *ui, const SDL_Event *e);
/* The frame: sizes, the clock, focus moves; then the widgets; then the
 * brackets and diagnostics. */
void dxl_ui_begin(dxl_ui *ui, double now);
void dxl_ui_end(dxl_ui *ui);

/* An id from a name, with the frame's seed (dxl_ui_seed) mixed in. */
uint32_t dxl_ui_id(const dxl_ui *ui, const char *name);
void     dxl_ui_seed(dxl_ui *ui, uint32_t seed);

/* A focusable: registers r for navigation and the pointer. Returns the
 * DXL_HIT_* that happened to it this frame. */
#define DXL_HIT_FOCUSED   1   /* it has the focus */
#define DXL_HIT_HOVERED   2   /* the pointer is over it */
#define DXL_HIT_HELD      4   /* the pointer's button is down on it */
#define DXL_HIT_ACTIVATED 8   /* clicked, or Enter or the pad's confirm while focused */
int  dxl_ui_focusable(dxl_ui *ui, uint32_t id, dxl_rect r, int disabled);
/* The pointer's part alone, for what takes no focus (a hint): HOVERED,
 * HELD, ACTIVATED. */
int  dxl_ui_clickable(dxl_ui *ui, uint32_t id, dxl_rect r);
/* Gives id the focus (a screen's first, a dialog's default). */
void dxl_ui_focus(dxl_ui *ui, uint32_t id);
/* What the last frame drew is gone -- a page or a dialog changed: the next
 * frame keeps the focus as it is now and moves nothing; the one after gives
 * it to the first focusable when what it names was not drawn. */
void dxl_ui_forget(dxl_ui *ui);

/* The scope new focusables go to: 0 the screen's, higher a dialog's, whose
 * focusables alone take the focus while they are there. */
void dxl_ui_scope(dxl_ui *ui, int scope);

/* A value kept for id from frame to frame, init the first time. */
float *dxl_ui_value(dxl_ui *ui, uint32_t id, float init);

/* Drawing clipped to r (within any clip already pushed), until the pop. */
void dxl_clip_push(dxl_ui *ui, dxl_rect r);
void dxl_clip_pop(dxl_ui *ui);

/* dp to px. */
dxl_rect dxl_px(const dxl_ui *ui, dxl_rect r);
float    dxl_pxf(const dxl_ui *ui, float dp);

/* Rect cuts: a strip off one side of r, which shrinks by it. */
dxl_rect dxl_cut_top(dxl_rect *r, float h);
dxl_rect dxl_cut_bottom(dxl_rect *r, float h);
dxl_rect dxl_cut_left(dxl_rect *r, float w);
dxl_rect dxl_cut_right(dxl_rect *r, float w);
dxl_rect dxl_inset(dxl_rect r, float d);
dxl_rect dxl_centered(dxl_rect within, float w, float h);

/* A diagnostic for the frame (printf-like). */
void dxl_ui_diag(dxl_ui *ui, const char *fmt, ...);

/* An animation is running: the frame after this one comes soon. */
void dxl_ui_animate(dxl_ui *ui);
/* How far an animation of dur seconds begun at start has gone, 0 to 1,
 * eased out; under reduced motion 1 at once. */
float dxl_ui_tween(dxl_ui *ui, double start, float dur);

#endif

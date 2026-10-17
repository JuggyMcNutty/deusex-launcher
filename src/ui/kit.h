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
    int   clicks;              /* its press: 1, 2 a double click's, 3 a triple's */
    int   shift_click;         /* Shift held as it was pressed */
    float wheel;           /* notches, up positive */
    int   quit;            /* the window's close */
    int   resized;
    int   any;
    int   tab_prev, tab_next;  /* Ctrl+Shift+Tab, Ctrl+Tab: a page's tabs */
    int   clear;               /* Delete; the pad's west button: a key slot emptied */

    /* While a field is edited (dxl_ui_text_begin): what was typed this
     * frame, UTF-8, the editing keys in it as they came -- DXL_EDIT_* --
     * and Enter activate, Esc back; Tab, Up and Down move on as ever. What
     * the keys type on a field that takes typing (dxl_ui_text_ready) the
     * same: its edit's beginning. Ctrl+V's text comes as typed. */
    char  text[1024];

    /* While a slot captures (dxl_ui_capture_begin): the first key, mouse
     * button, wheel notch, pad button or trigger, as it is; Esc is back. */
    int   captured;            /* DXL_CAPTURED_* */
    int   captured_code;       /* the scancode, button, notch (+1 up) or pad button */
} dxl_input;

/* The editing keys in dxl_input's text: no text SDL gives has them, and
 * no text typed keeps them (DXL_EDIT_IS_OP). */
#define DXL_EDIT_ERASE  '\b'     /* Backspace */
#define DXL_EDIT_DELETE '\x7f'   /* Delete */
#define DXL_EDIT_LEFT   '\x02'
#define DXL_EDIT_RIGHT  '\x06'
#define DXL_EDIT_HOME   '\x01'
#define DXL_EDIT_END    '\x05'
#define DXL_EDIT_WORD_LEFT   '\x10'   /* Ctrl+Left: a word at a time */
#define DXL_EDIT_WORD_RIGHT  '\x11'
#define DXL_EDIT_ERASE_WORD  '\x17'   /* Ctrl+Backspace */
#define DXL_EDIT_DELETE_WORD '\x0b'   /* Ctrl+Delete */
#define DXL_EDIT_SELECT '\x13'        /* Shift, before a move: the selection grows with it */
#define DXL_EDIT_ALL    '\x0e'        /* Ctrl+A */
#define DXL_EDIT_COPY   '\x03'        /* Ctrl+C, Ctrl+Insert */
#define DXL_EDIT_CUT    '\x18'        /* Ctrl+X, Shift+Delete */
#define DXL_EDIT_IS_OP(c) ((unsigned char)(c) < 0x20 || (c) == 0x7f)

#define DXL_CAPTURED_NONE   0
#define DXL_CAPTURED_KEY    1
#define DXL_CAPTURED_MOUSE  2
#define DXL_CAPTURED_WHEEL  3
#define DXL_CAPTURED_PAD    4   /* captured_code: SDL's gamepad button */
#define DXL_CAPTURED_TRIGGER 5  /* a trigger pulled past half: 0 the left, 1 the right */

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
    double          wake;        /* a frame due at this time with nothing else happening (a caret's blink); 0 none */
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

    /* Text entry and capture: the widget they are for, 0 for none. */
    uint32_t        text_id;
    dxl_rect        text_area;   /* where the text is, for the input method's window */
    int             text_on;     /* SDL's text input is started */
    uint32_t        text_ready;  /* a field with the focus that takes typing, as the last frame drew it */
    uint32_t        text_ready_now;   /* ...as this one draws it */
    uint32_t        text_lost;   /* a field whose edit ended as it was not drawn: kept */
    int             swallow_space;    /* Space chose a field: the text it types is not typed */
    uint32_t        capture_id;
    /* The pads' buttons and sticks drive nothing (a controller being mapped). */
    int             pads_off;

    /* The backdrop, drawn once for the window's size (draw.h). */
    SDL_Texture    *backdrop;
    int             backdrop_w, backdrop_h;

    /* The focusables' names, as the frame draws them (dxl_ui_name). */
    struct {
        int count;
        struct { uint32_t id; char name[64]; } at[160];
    } names;
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
/* A pad's family, by the label its south button carries. */
dxl_pad_family dxl_ui_pad_family(SDL_Gamepad *g);

/* What the focusable id is called this frame: its label, for a script that
 * finds a control by it (screens/uiscript.h). */
void dxl_ui_name(dxl_ui *ui, uint32_t id, const char *name);
/* Between frames, the last frame's focusable of that name, the last drawn
 * of it (a dialog's over its page's); 0 for none. */
uint32_t dxl_ui_named(const dxl_ui *ui, const char *name);
/* The pointer's part alone, for what takes no focus (a hint): HOVERED,
 * HELD, ACTIVATED. */
int  dxl_ui_clickable(dxl_ui *ui, uint32_t id, dxl_rect r);
/* Gives id the focus (a screen's first, a dialog's default). */
void dxl_ui_focus(dxl_ui *ui, uint32_t id);
/* A focused control's left and right: a move it had the focus for, taken
 * back from navigation. Returns the steps, right positive; 0 when id did
 * not have the focus or there was no such move. */
int  dxl_ui_take_lr(dxl_ui *ui, uint32_t id);
/* The same for up and down, down positive (a view that scrolls). */
int  dxl_ui_take_ud(dxl_ui *ui, uint32_t id);

/* Text entry for id, its text in area: the keys edit (dxl_input's text
 * fields), and on a window SDL's text input runs, the input method's
 * window placed by area. */
void dxl_ui_text_begin(dxl_ui *ui, uint32_t id, dxl_rect area);
void dxl_ui_text_end(dxl_ui *ui);
/* This frame, the field id has the focus and takes typing, its text in
 * area: until the next frame the keys that type text type it -- W, A, S
 * and D too, which move the focus elsewhere -- as dxl_input's text, which
 * begins its edit; Backspace, Delete, Ctrl+A and Ctrl+V begin it with what
 * they do. Space chooses it as ever. */
void dxl_ui_text_ready(dxl_ui *ui, uint32_t id, dxl_rect area);
/* The next key, mouse button, wheel notch or pad button for id, as it is
 * (dxl_input's captured), none of it navigation; Esc is back. */
void dxl_ui_capture_begin(dxl_ui *ui, uint32_t id);
void dxl_ui_capture_end(dxl_ui *ui);

/* What the last frame drew is gone -- a page or a dialog changed: the next
 * frame keeps the focus as it is now and moves nothing; the one after gives
 * it to the first focusable when what it names was not drawn. */
void dxl_ui_forget(dxl_ui *ui);

/* The scope new focusables go to: 0 the screen's, higher a dialog's, whose
 * focusables alone take the focus while they are there. */
void dxl_ui_scope(dxl_ui *ui, int scope);
/* Whether what is drawn now takes input: no dialog's scope is over its. */
int  dxl_ui_live(const dxl_ui *ui);

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
/* A frame due at the time at (the kit's clock), with no input before it:
 * a change of its own, as a caret's blink, with the window waiting till
 * then. The soonest asked for this frame stands. */
void dxl_ui_wake(dxl_ui *ui, double at);
/* How far an animation of dur seconds begun at start has gone, 0 to 1,
 * eased out; under reduced motion 1 at once. */
float dxl_ui_tween(dxl_ui *ui, double start, float dur);

#endif

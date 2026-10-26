/* The install's art the window wears, read at run time and never kept:
 * the logo's "Deus Ex" (Help/Logo.bmp's band of it, no maker's logo), and
 * the game's main menu buttons (DeusExUI.u's MenuMainButton* and
 * MenuMainLight_*), as textures of the window's renderer, made again when
 * the window is. The game draws them grey on black, tinted by its menu
 * theme and added to what is under them; so do these. Until there is an
 * install -- or one lacks a piece -- the kit's own drawing stands in.
 */
#ifndef DXL_UI_ART_H
#define DXL_UI_ART_H

#include "ui/kit.h"

typedef struct {
    char        *from;               /* the install's folder they are of; NULL or "" for none */
    SDL_Renderer *renderer;          /* the one they are textures of */
    SDL_Texture *wordmark;
    float        wordmark_w, wordmark_h;
    SDL_Texture *button[2][3];       /* normal and pressed: the left end, the middle, the right end */
    float        button_w[3], button_h;
    SDL_Texture *light[2];           /* the light before a button: off, and focused */
    float        light_w;
} dxl_art;

/* The art of the install at root (NULL: none), for ui's renderer: read
 * once for an install and a renderer, each piece it lacks NULL. */
void dxl_art_load(dxl_art *a, dxl_ui *ui, const char *root);
void dxl_art_free(dxl_art *a);
/* Whether the main menu's buttons are all there. */
int  dxl_art_has_buttons(const dxl_art *a);

/* The logo's "Deus Ex" in r, its height r's, from r's left; its width. */
float dxl_art_wordmark(dxl_ui *ui, const dxl_art *a, dxl_rect r);
/* A main menu button of the game's art, as dxl_button_id is: 1 when
 * activated. */
int   dxl_art_button(dxl_ui *ui, const dxl_art *a, uint32_t id, dxl_rect r, const char *label, int flags);

#endif

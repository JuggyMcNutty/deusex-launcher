/* Pictures made small off the window's thread, for a gallery: each file
 * asked for is read -- a BMP or a PNG, as SDL reads them -- and made to fit
 * a box of w x h px, halved until it is within twice the box (each half a
 * mean of four pixels, so a shot's detail does not alias), then scaled to
 * it, by a thread of the set's own, the files in the order asked. The
 * window takes each as it is done (dxl_thumbs_update) and makes it a
 * texture of its renderer; one that does not read is marked failed.
 */
#ifndef DXL_UI_THUMBS_H
#define DXL_UI_THUMBS_H

#include <SDL3/SDL.h>

typedef struct dxl_thumbs dxl_thumbs;

/* A set for pictures fitted to w x h px; NULL when no thread starts. */
dxl_thumbs *dxl_thumbs_new(int w, int h);
void        dxl_thumbs_free(dxl_thumbs *t);

/* path's picture asked for, once; the set's others kept. */
void        dxl_thumbs_want(dxl_thumbs *t, const char *path);
/* All of them forgotten, those not read yet never read. */
void        dxl_thumbs_clear(dxl_thumbs *t);

/* On the window's thread, each frame: the pictures done made textures of
 * r (made again for another renderer). 1 while some are still to come. */
int         dxl_thumbs_update(dxl_thumbs *t, SDL_Renderer *r);
/* path's texture and its size in px, NULL until it is there; *failed 1
 * for a file that did not read. */
SDL_Texture *dxl_thumbs_get(dxl_thumbs *t, const char *path, int *failed, int *w, int *h);
/* 1 while a picture asked for is still to be read, or read and not yet a
 * texture: the window has frames to draw for it. */
int         dxl_thumbs_busy(dxl_thumbs *t);
/* Until every picture asked for is done (dxl-shots), then the update. */
void        dxl_thumbs_wait(dxl_thumbs *t, SDL_Renderer *r);

#endif

#include "ui/thumbs.h"
#include "core/common.h"
#include "core/log.h"

#include <stdlib.h>
#include <string.h>

enum { QUEUED, READING, READ, FAILED };

typedef struct {
    char        *path;
    int          state;      /* under the lock */
    SDL_Surface *pic;        /* READ: the fitted picture, until it is a texture */
    SDL_Texture *tex;        /* the window's thread's alone */
    int          w, h;
} thumb;

struct dxl_thumbs {
    int            box_w, box_h;
    SDL_Mutex     *lock;
    SDL_Condition *more;     /* a picture asked for, or the end */
    SDL_Condition *done;     /* one read */
    SDL_Thread    *thread;
    int            quit;
    unsigned       generation;   /* a clear's: what was being read for the set before it is dropped */
    thumb         *items;
    int            count, cap;
    SDL_Renderer  *renderer;     /* the textures' */
};

static int find(const dxl_thumbs *t, const char *path) {
    for (int i = 0; i < t->count; i++)
        if (!strcmp(t->items[i].path, path)) return i;
    return -1;
}

/* path read and fitted to the box, its shape kept, never made larger. */
static SDL_Surface *fit(const char *path, int bw, int bh) {
    size_t n = strlen(path);
    SDL_Surface *s = n > 4 && !SDL_strcasecmp(path + n - 4, ".png") ? SDL_LoadPNG(path) : SDL_LoadBMP(path);
    if (!s) return NULL;
    SDL_Surface *pic = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(s);
    if (!pic || pic->w < 1 || pic->h < 1) {
        SDL_DestroySurface(pic);
        return NULL;
    }
    float kx = (float)bw / (float)pic->w, ky = (float)bh / (float)pic->h, k = kx < ky ? kx : ky;
    if (k > 1) k = 1;
    int w = (int)((float)pic->w * k + 0.5f), h = (int)((float)pic->h * k + 0.5f);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    while (pic && pic->w >= 2 * w && pic->h >= 2 * h) {
        SDL_Surface *half = SDL_ScaleSurface(pic, pic->w / 2, pic->h / 2, SDL_SCALEMODE_LINEAR);
        SDL_DestroySurface(pic);
        pic = half;
    }
    if (pic && (pic->w != w || pic->h != h)) {
        SDL_Surface *sized = SDL_ScaleSurface(pic, w, h, SDL_SCALEMODE_LINEAR);
        SDL_DestroySurface(pic);
        pic = sized;
    }
    return pic;
}

static int SDLCALL work(void *ctx) {
    dxl_thumbs *t = ctx;
    SDL_LockMutex(t->lock);
    while (!t->quit) {
        int i = 0;
        while (i < t->count && t->items[i].state != QUEUED) i++;
        if (i == t->count) {
            SDL_WaitCondition(t->more, t->lock);
            continue;
        }
        t->items[i].state = READING;
        char *path = dxl_xstrdup(t->items[i].path);
        unsigned generation = t->generation;
        SDL_UnlockMutex(t->lock);
        SDL_Surface *pic = fit(path, t->box_w, t->box_h);
        SDL_LockMutex(t->lock);
        /* Found again by its path: the list may have grown or been cleared. */
        int j = generation == t->generation ? find(t, path) : -1;
        if (j >= 0 && t->items[j].state == READING) {
            t->items[j].pic = pic;
            t->items[j].state = pic ? READ : FAILED;
            if (pic) {
                t->items[j].w = pic->w;
                t->items[j].h = pic->h;
            }
        } else {
            SDL_DestroySurface(pic);
        }
        if (!pic && j >= 0) dxl_log("thumbs: %s: %s", path, SDL_GetError());
        free(path);
        SDL_BroadcastCondition(t->done);
    }
    SDL_UnlockMutex(t->lock);
    return 0;
}

dxl_thumbs *dxl_thumbs_new(int w, int h) {
    dxl_thumbs *t = dxl_xmalloc(sizeof *t);
    memset(t, 0, sizeof *t);
    t->box_w = w;
    t->box_h = h;
    t->lock = SDL_CreateMutex();
    t->more = SDL_CreateCondition();
    t->done = SDL_CreateCondition();
    if (t->lock && t->more && t->done) t->thread = SDL_CreateThread(work, "dxl-thumbs", t);
    if (!t->thread) {
        dxl_log("thumbs: no thread: %s", SDL_GetError());
        dxl_thumbs_free(t);
        return NULL;
    }
    return t;
}

static void forget(dxl_thumbs *t) {
    for (int i = 0; i < t->count; i++) {
        free(t->items[i].path);
        SDL_DestroySurface(t->items[i].pic);
        if (t->items[i].tex) SDL_DestroyTexture(t->items[i].tex);
    }
    t->count = 0;
}

void dxl_thumbs_free(dxl_thumbs *t) {
    if (!t) return;
    if (t->thread) {
        SDL_LockMutex(t->lock);
        t->quit = 1;
        SDL_SignalCondition(t->more);
        SDL_UnlockMutex(t->lock);
        SDL_WaitThread(t->thread, NULL);
    }
    forget(t);
    free(t->items);
    if (t->done) SDL_DestroyCondition(t->done);
    if (t->more) SDL_DestroyCondition(t->more);
    if (t->lock) SDL_DestroyMutex(t->lock);
    free(t);
}

void dxl_thumbs_want(dxl_thumbs *t, const char *path) {
    if (!t || !path) return;
    SDL_LockMutex(t->lock);
    if (find(t, path) < 0) {
        if (t->count == t->cap) {
            t->cap = t->cap ? t->cap * 2 : 32;
            t->items = dxl_xrealloc(t->items, (size_t)t->cap * sizeof *t->items);
        }
        thumb *it = &t->items[t->count++];
        memset(it, 0, sizeof *it);
        it->path = dxl_xstrdup(path);
        it->state = QUEUED;
        SDL_SignalCondition(t->more);
    }
    SDL_UnlockMutex(t->lock);
}

void dxl_thumbs_clear(dxl_thumbs *t) {
    if (!t) return;
    SDL_LockMutex(t->lock);
    forget(t);
    t->generation++;
    SDL_UnlockMutex(t->lock);
}

int dxl_thumbs_update(dxl_thumbs *t, SDL_Renderer *r) {
    if (!t) return 0;
    int pending = 0;
    SDL_LockMutex(t->lock);
    for (int i = 0; i < t->count; i++) {
        thumb *it = &t->items[i];
        if (r != t->renderer && it->tex) {
            /* Another renderer's: made again from the picture, read again. */
            SDL_DestroyTexture(it->tex);
            it->tex = NULL;
            if (it->state == READ && !it->pic) it->state = QUEUED;
        }
        if (it->state == QUEUED || it->state == READING) pending = 1;
        if (it->state == READ && it->pic && r) {
            it->tex = SDL_CreateTextureFromSurface(r, it->pic);
            if (it->tex) {
                SDL_SetTextureScaleMode(it->tex, SDL_SCALEMODE_LINEAR);
            } else {
                dxl_log("thumbs: %s: no texture of %dx%d: %s", it->path, it->pic->w, it->pic->h, SDL_GetError());
                it->state = FAILED;
            }
            SDL_DestroySurface(it->pic);
            it->pic = NULL;
        }
    }
    if (r != t->renderer) {
        t->renderer = r;
        for (int i = 0; i < t->count; i++)
            if (t->items[i].state == QUEUED) SDL_SignalCondition(t->more);
    }
    SDL_UnlockMutex(t->lock);
    return pending;
}

SDL_Texture *dxl_thumbs_get(dxl_thumbs *t, const char *path, int *failed, int *w, int *h) {
    if (failed) *failed = 0;
    if (!t) return NULL;
    SDL_LockMutex(t->lock);
    int i = find(t, path);
    SDL_Texture *tex = NULL;
    if (i >= 0) {
        if (failed) *failed = t->items[i].state == FAILED;
        tex = t->items[i].tex;
        if (w) *w = t->items[i].w;
        if (h) *h = t->items[i].h;
    }
    SDL_UnlockMutex(t->lock);
    return tex;
}

int dxl_thumbs_busy(dxl_thumbs *t) {
    if (!t) return 0;
    int busy = 0;
    SDL_LockMutex(t->lock);
    for (int i = 0; i < t->count && !busy; i++)
        busy = t->items[i].state == QUEUED || t->items[i].state == READING ||
               (t->items[i].state == READ && t->items[i].pic);
    SDL_UnlockMutex(t->lock);
    return busy;
}

void dxl_thumbs_wait(dxl_thumbs *t, SDL_Renderer *r) {
    if (!t) return;
    SDL_LockMutex(t->lock);
    for (;;) {
        int pending = 0;
        for (int i = 0; i < t->count; i++)
            if (t->items[i].state == QUEUED || t->items[i].state == READING) pending = 1;
        if (!pending) break;
        SDL_WaitCondition(t->done, t->lock);
    }
    SDL_UnlockMutex(t->lock);
    dxl_thumbs_update(t, r);
}

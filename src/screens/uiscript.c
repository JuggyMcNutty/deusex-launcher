#define _GNU_SOURCE
#include "screens/uiscript.h"
#include "core/log.h"
#include "core/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dxl_uiscript {
    char   **lines;
    int      count, at;
    double   next;        /* when the next line runs; 0 before the first */
    char    *shot;        /* a shot asked for */
};

dxl_uiscript *dxl_uiscript_open(void) {
    const char *path = getenv("DXL_UI_SCRIPT");
    if (!path || !*path) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "ui script: cannot read %s\n", path);
        return NULL;
    }
    dxl_uiscript *u = dxl_xmalloc(sizeof *u);
    memset(u, 0, sizeof *u);
    char line[1024];
    while (fgets(line, sizeof line, f)) {
        char *l = dxl_trim(line);
        if (!*l || *l == '#') continue;
        u->lines = dxl_xrealloc(u->lines, sizeof *u->lines * (size_t)(u->count + 1));
        u->lines[u->count++] = dxl_xstrdup(l);
    }
    fclose(f);
    dxl_log("ui script %s: %d lines", path, u->count);
    return u;
}

void dxl_uiscript_free(dxl_uiscript *u) {
    if (!u) return;
    for (int i = 0; i < u->count; i++) free(u->lines[i]);
    free(u->lines);
    free(u->shot);
    free(u);
}

static const char *up_now(const dxl_shell *s) {
    if (s->dialog == DXL_DIALOG_BROWSER) return "browser";
    if (s->dialog == DXL_DIALOG_MESSAGE) return "message";
    switch (s->page) {
    case DXL_PAGE_HOME: return "home";
    case DXL_PAGE_WELCOME: return "welcome";
    case DXL_PAGE_FIND: return "find";
    case DXL_PAGE_IMPORT: return "import";
    case DXL_PAGE_RECOVERY: return "recovery";
    case DXL_PAGE_ABOUT: return "about";
    default: return "?";
    }
}

static void push_key(SDL_Keycode k) {
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    e.key.down = true;
    SDL_PushEvent(&e);
    e.type = SDL_EVENT_KEY_UP;
    e.key.down = false;
    SDL_PushEvent(&e);
}

double dxl_uiscript_step(dxl_uiscript *u, dxl_shell *s, double now) {
    if (!u) return -1;
    if (u->next == 0) u->next = now;
    while (u->at < u->count && now >= u->next) {
        const char *l = u->lines[u->at++];
        char word[32] = "", arg[960] = "";
        sscanf(l, "%31s %959[^\n]", word, arg);
        dxl_log("ui script: %s", l);
        if (!strcmp(word, "wait")) {
            u->next = now + atof(arg);
        } else if (!strcmp(word, "key")) {
            SDL_Keycode k = SDL_GetKeyFromName(arg);
            if (k == SDLK_UNKNOWN) dxl_log("ui script: FAILED: no key %s", arg);
            else push_key(k);
        } else if (!strcmp(word, "expect")) {
            if (strcmp(arg, up_now(s)) != 0) {
                fprintf(stderr, "ui script: FAILED: expected %s, %s is up\n", arg, up_now(s));
                dxl_log("ui script: FAILED: expected %s, %s is up", arg, up_now(s));
                u->at = u->count;
                SDL_Event e;
                SDL_zero(e);
                e.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&e);
            }
        } else if (!strcmp(word, "shot")) {
            free(u->shot);
            u->shot = dxl_xstrdup(arg);
        } else if (!strcmp(word, "quit")) {
            SDL_Event e;
            SDL_zero(e);
            e.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&e);
        } else {
            dxl_log("ui script: FAILED: no command %s", word);
        }
    }
    if (u->at >= u->count) return -1;
    return u->next > now ? u->next - now : 0;
}

void dxl_uiscript_shoot(dxl_uiscript *u, SDL_Renderer *r) {
    if (!u || !u->shot) return;
    SDL_Surface *pixels = SDL_RenderReadPixels(r, NULL);
    char *dir = dxl_path_dirname(u->shot);
    dxl_path_mkdirs(dir);
    free(dir);
    if (!pixels || !SDL_SavePNG(pixels, u->shot)) dxl_log("ui script: no shot %s: %s", u->shot, SDL_GetError());
    else dxl_log("ui script: saved %s", u->shot);
    SDL_DestroySurface(pixels);
    free(u->shot);
    u->shot = NULL;
}

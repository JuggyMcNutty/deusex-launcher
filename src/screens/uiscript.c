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
    const char *seek;     /* a find's label, sought frame by frame */
    int      seek_left;   /* the Downs it has left */
};

#define SEEK_TRIES 80

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
    switch (s->dialog) {
    case DXL_DIALOG_BROWSER: return "browser";
    case DXL_DIALOG_MESSAGE: return "message";
    case DXL_DIALOG_PICK: return "pick";
    case DXL_DIALOG_OSK: return "keyboard";
    case DXL_DIALOG_CONFIRM: return "confirm";
    default: break;
    }
    switch (s->page) {
    case DXL_PAGE_HOME: return "home";
    case DXL_PAGE_WELCOME: return "welcome";
    case DXL_PAGE_FIND: return "find";
    case DXL_PAGE_IMPORT: return "import";
    case DXL_PAGE_RECOVERY: return "recovery";
    case DXL_PAGE_ABOUT: return "about";
    case DXL_PAGE_SETTINGS: return "settings";
    case DXL_PAGE_LOG: return "log";
    default: return "?";
    }
}

/* A key down and up, with its scancode: a key the keyboard page captures
 * is named by it. */
static void push_key(SDL_Keycode k) {
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    e.key.scancode = SDL_GetScancodeFromKey(k, NULL);
    e.key.down = true;
    SDL_PushEvent(&e);
    e.type = SDL_EVENT_KEY_UP;
    e.key.down = false;
    SDL_PushEvent(&e);
}

/* The script stopped where it failed, and the window closed. */
static void fail(dxl_uiscript *u, const char *what) {
    fprintf(stderr, "ui script: FAILED: %s\n", what);
    dxl_log("ui script: FAILED: %s", what);
    u->at = u->count;
    u->seek = NULL;
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}

/* A find under way: the control focused once it is drawn, else one more
 * Down. 1 while it goes on. */
static int seek(dxl_uiscript *u, dxl_shell *s) {
    uint32_t id = dxl_ui_named(&s->ui, u->seek);
    if (id) {
        dxl_ui_focus(&s->ui, id);
        s->ui.mode = DXL_INPUT_KEYBOARD;
        u->seek = NULL;
        return 0;
    }
    if (u->seek_left-- <= 0) {
        char what[160];
        snprintf(what, sizeof what, "no %s on %s", u->seek, up_now(s));
        fail(u, what);
        return 0;
    }
    push_key(SDLK_DOWN);
    return 1;
}

double dxl_uiscript_step(dxl_uiscript *u, dxl_shell *s, double now) {
    if (!u) return -1;
    if (u->next == 0) u->next = now;
    if (u->seek && seek(u, s)) return 0;
    while (u->at < u->count && now >= u->next && !u->seek) {
        const char *l = u->lines[u->at++];
        char word[32] = "", arg[960] = "";
        sscanf(l, "%31s %959[^\n]", word, arg);
        /* The line's own text, which stays while the script does. */
        const char *rest = l + strlen(word);
        while (*rest == ' ' || *rest == '\t') rest++;
        dxl_log("ui script: %s", l);
        if (!strcmp(word, "wait")) {
            u->next = now + atof(arg);
        } else if (!strcmp(word, "key")) {
            int times = 1;
            char *star = strrchr(arg, '*');
            if (star && star > arg && star[-1] == ' ') {
                times = atoi(star + 1);
                for (star--; star >= arg && *star == ' '; star--) *star = '\0';
            }
            SDL_Keycode k = SDL_GetKeyFromName(arg);
            char what[1000];
            snprintf(what, sizeof what, "no key %s", arg);
            if (k == SDLK_UNKNOWN) fail(u, what);
            for (int n = 0; k != SDLK_UNKNOWN && n < times; n++) push_key(k);
            /* A frame takes it before the next line moves the focus. */
            return 0;
        } else if (!strcmp(word, "type")) {
            SDL_Event e;
            SDL_zero(e);
            e.type = SDL_EVENT_TEXT_INPUT;
            e.text.text = rest;
            SDL_PushEvent(&e);
            return 0;
        } else if (!strcmp(word, "find")) {
            u->seek = rest;
            u->seek_left = SEEK_TRIES;
            if (seek(u, s)) return 0;
        } else if (!strcmp(word, "expect")) {
            if (strcmp(arg, up_now(s)) != 0) {
                char what[160];
                snprintf(what, sizeof what, "expected %.60s, %s is up", arg, up_now(s));
                fail(u, what);
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
            char what[80];
            snprintf(what, sizeof what, "no command %s", word);
            fail(u, what);
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

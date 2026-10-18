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
    int      seek_left;   /* the moves it has left */
    int      seek_up;     /* going up, the bottom met */
    int      seek_tabs;   /* times the top was met too */
    int      seek_tab_left;   /* Tabs to press before Down again */
    int      seek_still;  /* moves in a row the focus stayed put */
    uint32_t seek_from;   /* the focus before the last move */
    SDL_Joystick *pad;    /* a virtual controller's, pad attach's */
};

#define SEEK_TRIES 600
#define SEEK_TABS  12

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
    if (u->pad) {
        SDL_JoystickID id = SDL_GetJoystickID(u->pad);
        SDL_CloseJoystick(u->pad);
        SDL_DetachVirtualJoystick(id);
    }
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
    case DXL_DIALOG_PADWIZARD: return "padwizard";
    case DXL_DIALOG_PADTEST: return "padtest";
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
    case DXL_PAGE_PROFILES: return "profiles";
    case DXL_PAGE_SAVES: return "saves";
    case DXL_PAGE_SHOTS: return s->shot_open ? "shot" : "shots";
    default: return "?";
    }
}

/* A key down and up, with its scancode: a key the keyboard page captures
 * is named by it; held with mod. */
static void push_key_mod(SDL_Keycode k, SDL_Keymod mod) {
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    e.key.scancode = SDL_GetScancodeFromKey(k, NULL);
    e.key.mod = mod;
    e.key.down = true;
    SDL_PushEvent(&e);
    e.type = SDL_EVENT_KEY_UP;
    e.key.down = false;
    SDL_PushEvent(&e);
}

static void push_key(SDL_Keycode k) { push_key_mod(k, SDL_KMOD_NONE); }

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

/* pad's line: a virtual controller attached, worked or taken away. */
static void pad(dxl_uiscript *u, const char *arg) {
    static const struct { const char *kind; Uint16 vendor, product; const char *name; } kinds[] = {
        { "xbox", 0x045e, 0x0b12, "Xbox Series X Controller" },
        { "dualsense", 0x054c, 0x0ce6, "DualSense Wireless Controller" },
        { "switchpro", 0x057e, 0x2009, "Nintendo Switch Pro Controller" },
        { "generic", 0x1234, 0x5678, "A virtual controller" },
    };
    char verb[16] = "", what[32] = "";
    float value = 0;
    sscanf(arg, "%15s %31s %f", verb, what, &value);
    if (!strcmp(verb, "attach")) {
        SDL_VirtualJoystickDesc d;
        SDL_INIT_INTERFACE(&d);
        d.type = SDL_JOYSTICK_TYPE_UNKNOWN;
        d.vendor_id = 0x1209;
        d.product_id = 0xd0d1;
        d.naxes = 6;
        d.nbuttons = 13;
        d.nhats = 1;
        d.name = "A pad of no maker";
        for (size_t i = 0; i < sizeof kinds / sizeof *kinds; i++)
            if (!strcmp(what, kinds[i].kind)) {
                d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
                d.vendor_id = kinds[i].vendor;
                d.product_id = kinds[i].product;
                d.naxes = SDL_GAMEPAD_AXIS_COUNT;
                d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
                d.nhats = 0;
                d.name = kinds[i].name;
            }
        if (u->pad) SDL_CloseJoystick(u->pad);
        SDL_JoystickID id = SDL_AttachVirtualJoystick(&d);
        u->pad = id ? SDL_OpenJoystick(id) : NULL;
        if (!u->pad) fail(u, "no virtual controller");
        /* Where it rests, read at once, as a real one's is when SDL opens
         * it: a virtual controller's axes are read only once one moves. */
        else SDL_SetJoystickVirtualAxis(u->pad, 0, 0);
        return;
    }
    if (!u->pad) {
        fail(u, "no controller attached");
        return;
    }
    int n = atoi(what);
    if (!strcmp(verb, "button")) {
        int down = strstr(arg, " down") != NULL;
        SDL_SetJoystickVirtualButton(u->pad, n, down);
    } else if (!strcmp(verb, "axis")) {
        if (value > 1) value = 1;
        if (value < -1) value = -1;
        SDL_SetJoystickVirtualAxis(u->pad, n, (Sint16)(value * 32767));
    } else if (!strcmp(verb, "hat")) {
        SDL_SetJoystickVirtualHat(u->pad, n, (Uint8)value);
    } else if (!strcmp(verb, "detach")) {
        SDL_JoystickID id = SDL_GetJoystickID(u->pad);
        SDL_CloseJoystick(u->pad);
        SDL_DetachVirtualJoystick(id);
        u->pad = NULL;
    } else {
        fail(u, "no such pad line");
    }
}

/* A find under way: the control focused once it is drawn, else one more
 * Down, or once the focus stays put at the bottom, Up; at the top too,
 * Tabs -- one more each time -- to a control in another column, and Down
 * again. 1 while it goes on. */
static int seek(dxl_uiscript *u, dxl_shell *s) {
    uint32_t id = dxl_ui_named(&s->ui, u->seek);
    if (id) {
        dxl_ui_focus(&s->ui, id);
        s->ui.mode = DXL_INPUT_KEYBOARD;
        u->seek = NULL;
        return 0;
    }
    if (u->seek_left-- <= 0 || u->seek_tabs > SEEK_TABS) {
        char what[160];
        snprintf(what, sizeof what, "no %s on %s", u->seek, up_now(s));
        fail(u, what);
        return 0;
    }
    if (!u->seek_tab_left) {
        u->seek_still = s->ui.focus == u->seek_from ? u->seek_still + 1 : 0;
        if (u->seek_still >= 2 && !u->seek_up) {
            u->seek_up = 1;
            u->seek_still = 0;
        } else if (u->seek_still >= 2) {
            u->seek_up = u->seek_still = 0;
            u->seek_tab_left = ++u->seek_tabs;
        }
    }
    if (u->seek_tab_left) {
        u->seek_tab_left--;
        u->seek_from = 0;
        push_key(SDLK_TAB);
        return 1;
    }
    u->seek_from = s->ui.focus;
    push_key(u->seek_up ? SDLK_UP : SDLK_DOWN);
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
            /* Ctrl+, Shift+ and Alt+ before the name: held with it. */
            SDL_Keymod mod = SDL_KMOD_NONE;
            const char *name = arg;
            for (;;) {
                if (!SDL_strncasecmp(name, "Ctrl+", 5)) { mod |= SDL_KMOD_LCTRL; name += 5; }
                else if (!SDL_strncasecmp(name, "Shift+", 6)) { mod |= SDL_KMOD_LSHIFT; name += 6; }
                else if (!SDL_strncasecmp(name, "Alt+", 4)) { mod |= SDL_KMOD_LALT; name += 4; }
                else break;
            }
            SDL_Keycode k = SDL_GetKeyFromName(name);
            char what[1000];
            snprintf(what, sizeof what, "no key %s", arg);
            if (k == SDLK_UNKNOWN) fail(u, what);
            for (int n = 0; k != SDLK_UNKNOWN && n < times; n++) push_key_mod(k, mod);
            /* A frame takes it before the next line moves the focus. */
            return 0;
        } else if (!strcmp(word, "pad")) {
            pad(u, arg);
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
            u->seek_up = u->seek_still = u->seek_tabs = u->seek_tab_left = 0;
            u->seek_from = 0;
            /* A frame draws the focus where it is before the next line: a
             * field there takes the typing that follows. */
            seek(u, s);
            return 0;
        } else if (!strcmp(word, "expect")) {
            /* "settings <tab>": its tab too. */
            char up[96];
            snprintf(up, sizeof up, "%s", up_now(s));
            if (!strncmp(arg, "settings ", 9) && s->page == DXL_PAGE_SETTINGS && s->dialog == DXL_DIALOG_NONE)
                snprintf(up, sizeof up, "settings %s", dxl_settings_page((size_t)s->settings_tab));
            if (strcmp(arg, up) != 0) {
                char what[200];
                snprintf(what, sizeof what, "expected %.60s, %s is up", arg, up);
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

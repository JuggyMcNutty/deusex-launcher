#include "wizard/wizard.h"

#include "core/cmdline.h"
#include "core/paths.h"
#include "core/renderdev.h"
#include "core/safemode.h"
#include "platform/process.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The frame's controls and the page holder, template 104 (dialog units). */
#define ID_NEXT    1004
#define ID_CANCEL  2
#define ID_BACK    3
#define ID_FINISH  1005
#define ID_LOGO    1024
#define PAGE_X     3
#define PAGE_Y     59
#define PAGE_W     344      /* the pages' own size, templates 2017 to 2022 */
#define PAGE_H     172
#define FRAME_W    349
#define FRAME_H    253

#define DETECT_MS  30000   /* how long the Renderer page waits for Detected.ini */
#define DETECT_FLAGS "testrendev=D3DDrv.D3DRenderDevice log=Detected.log"
#define MAX_SHOWN  32

typedef struct {
    dxl_ctl_kind kind;
    int          id;
    int          x, y, w, h;   /* dialog units */
    const char  *text;         /* the template's text: an IDC_ key, or NULL */
    int          tabstop;
} tmpl;

/* Window.dll's templates, in their order (dx-reverse-info/wizard.md
 * "Page layouts"). */
static const tmpl renderer_t[] = {
    { DXL_CTL_TEXT,  1102,   7,   7, 328, 33, "IDC_RenderPrompt", 0 },
    { DXL_CTL_TEXT,  1104,   7, 125, 328, 41, "IDC_RenderNote",   0 },
    { DXL_CTL_LIST,  1103,   7,  42, 330, 67, NULL,               1 },
    { DXL_CTL_RADIO, 1109,   7, 110, 147, 10, "IDC_Compatible",   0 },
    { DXL_CTL_RADIO, 1110, 190, 110, 147, 10, "IDC_All",          0 },
};
static const tmpl detail_t[] = {
    { DXL_CTL_TEXT, 1099, 7,   7, 328,  23, "IDC_DetailPrompt", 0 },
    { DXL_CTL_EDIT, 1100, 7,  32, 330, 110, NULL,               1 },
    { DXL_CTL_TEXT, 1101, 7, 145, 328,  21, "IDC_DetailNote",   0 },
};
static const tmpl firsttime_t[] = {
    { DXL_CTL_TEXT, 1002, 7, 52, 328, 75, "IDC_Prompt", 0 },
};
static const tmpl safemode_t[] = {
    { DXL_CTL_TEXT,  1105,  7,   7, 330, 65, "IDC_SafeModePrompt", 0 },
    { DXL_CTL_GROUP,   -1,  7,  74, 330, 92, NULL,                 0 },
    { DXL_CTL_COOL,  1108, 15,  87, 314, 14, "IDC_Run",            1 },
    { DXL_CTL_COOL,  1109, 15, 106, 314, 14, "IDC_SafeMode",       1 },
    { DXL_CTL_COOL,  1110, 15, 125, 314, 14, "IDC_Video",          1 },
    { DXL_CTL_COOL,  1058, 15, 144, 314, 14, "IDC_Web",            1 },
};
static const tmpl safeoptions_t[] = {
    { DXL_CTL_TEXT,  1107, 7,   7, 330, 10, "IDC_SafeOptions", 0 },
    { DXL_CTL_CHECK, 1108, 7,  33, 330, 10, "IDC_NoSound",     1 },
    { DXL_CTL_CHECK, 1109, 7,  47, 330, 10, "IDC_No3DSound",   1 },
    { DXL_CTL_CHECK, 1110, 7,  61, 330, 10, "IDC_No3dVideo",   1 },
    { DXL_CTL_CHECK, 1111, 7,  89, 330, 10, "IDC_Res",         1 },
    { DXL_CTL_CHECK, 1112, 7,  75, 330, 10, "IDC_Window",      1 },
    { DXL_CTL_CHECK, 1113, 7, 104, 330, 10, "IDC_ResetConfig", 1 },
    { DXL_CTL_CHECK, 1114, 7, 119, 330, 10, "IDC_NoProcessor", 1 },
    { DXL_CTL_CHECK, 1115, 7, 134, 330, 10, "IDC_NoJoy",       1 },
};
static const tmpl driver_t[] = {
    { DXL_CTL_TEXT, 1110,  7,   7, 330, 10, "IDC_DriverText", 0 },
    { DXL_CTL_TEXT, 1111, 33,  24, 304, 10, "IDC_Card",       0 },
    { DXL_CTL_TEXT, 1112,  7,  39, 330, 96, "IDC_DriverInfo", 0 },
    { DXL_CTL_TEXT, 1058,  7, 141, 330,  8, "IDC_Web",        0 },
    { DXL_CTL_URL,  1113,  7, 154, 330, 12, "IDC_WebButton",  1 },
};

static const struct { const tmpl *t; size_t n; const char *name; } pages[] = {
    [DXL_PAGE_RENDERER]    = { renderer_t,    5, "ConfigPageRenderer" },
    [DXL_PAGE_DETAIL]      = { detail_t,      3, "ConfigPageDetail" },
    [DXL_PAGE_FIRSTTIME]   = { firsttime_t,   1, "ConfigPageFirstTime" },
    [DXL_PAGE_SAFEMODE]    = { safemode_t,    6, "ConfigPageSafeMode" },
    [DXL_PAGE_SAFEOPTIONS] = { safeoptions_t, 9, "ConfigPageSafeOptions" },
    [DXL_PAGE_DRIVER]      = { driver_t,      5, "ConfigPageDriver" },
};

typedef struct {
    dxl_page_id        id;
    /* Renderer */
    int                all, painted;
    dxl_renderdev_list devices;
    size_t             shown[MAX_SHOWN];
    int                shown_count, selected;
    /* Detail */
    char              *detail;
    /* SafeOptions */
    dxl_safe_options   opts;
} page;

struct dxl_wizard {
    dxl_wizard_request *req;
    dxl_dialog         *d;
    page                stack[8];
    int                 depth;
    int                 shows;    /* pages shown so far */
};

/* A template's rectangle in pixels, from an origin in pixels: each of the
 * four numbers converted on its own, as a dialog creates its controls. */
static SDL_Rect map(int x, int y, int w, int h, int ox, int oy) {
    SDL_Rect r = { ox + dxl_dlu_x(x), oy + dxl_dlu_y(y), dxl_dlu_x(w), dxl_dlu_y(h) };
    return r;
}

static page *top(dxl_wizard *w) { return w->depth ? &w->stack[w->depth - 1] : NULL; }

static const char *window_text(dxl_wizard *w, const char *key) {
    return dxl_loc_general(w->req->loc, "Window", key);
}

/* ---- the frame ------------------------------------------------------------ */

static void build_frame(dxl_wizard *w) {
    static const tmpl frame[] = {
        { DXL_CTL_COOL,   ID_NEXT,   122, 236, 50, 14, "&Next >",  1 },
        { DXL_CTL_COOL,   ID_CANCEL, 241, 236, 50, 14, "Cancel",   1 },
        { DXL_CTL_COOL,   ID_BACK,    72, 236, 50, 14, "< &Back",  1 },
        { DXL_CTL_COOL,   ID_FINISH, 182, 236, 50, 14, "&Finish",  1 },
        { DXL_CTL_BITMAP, ID_LOGO,     6,   6, 19, 17, NULL,       0 },
        { DXL_CTL_ETCHED, -1,          0, 231, FRAME_W, 1, NULL,   0 },
        { DXL_CTL_ETCHED, -1,          0,  58, FRAME_W, 1, NULL,   0 },
    };
    for (size_t i = 0; i < sizeof frame / sizeof *frame; i++) {
        const tmpl *t = &frame[i];
        dxl_ctl *c = dxl_dialog_add(w->d, t->kind, t->id, map(t->x, t->y, t->w, t->h, 0, 0),
                                    t->text, 0);
        c->tabstop = t->tabstop;
    }
    /* ..\Help\LogoSmall.bmp, which the static takes the size of; the GOG
     * install has none, and the frame stays empty. */
    char *game = dxl_path_dirname(w->req->system_dir);
    char *logo = dxl_path_resolve_ci(game, "Help/LogoSmall.bmp");
    if (logo) dxl_ctl_set_bitmap(w->d, dxl_dialog_find(w->d, ID_LOGO, 0), logo);
    free(logo);
    free(game);
}

/* The page's words for Next: NULL hides it. */
static const char *next_text(dxl_wizard *w, dxl_page_id id) {
    switch (id) {
    case DXL_PAGE_SAFEMODE:    return NULL;
    case DXL_PAGE_FIRSTTIME:
    case DXL_PAGE_SAFEOPTIONS: return dxl_loc_general(w->req->loc, "Startup", "Run");
    default:                   return window_text(w, "NextButton");
    }
}

static void frame_button(dxl_wizard *w, int id, const char *text) {
    dxl_ctl *c = dxl_dialog_find(w->d, id, 0);
    c->visible = text != NULL;
    dxl_ctl_set_text(w->d, c, text ? text : "");
}

/* ---- the Renderer page's list ------------------------------------------------ */

static void refresh_list(dxl_wizard *w, page *p) {
    dxl_renderdev_free(&p->devices);
    dxl_renderdev_load(&p->devices, w->req->system_dir, w->req->loc,
                       dxl_config_ini(w->req->config), NULL);
    p->shown_count = (int)dxl_renderdev_shown(&p->devices, p->all, p->shown, MAX_SHOWN,
                                              &p->selected);
}

/* CurrentDriver: the device whose caption the list has selected. */
static const char *current_driver(const page *p) {
    if (p->selected < 0 || p->selected >= p->shown_count) return NULL;
    const char *caption = p->devices.items[p->shown[p->selected]].caption;
    for (size_t i = 0; i < p->devices.count; i++)
        if (strcmp(p->devices.items[i].caption, caption) == 0) return p->devices.items[i].path;
    return NULL;
}

static void fill_renderer(dxl_wizard *w, page *p) {
    dxl_ctl *list = dxl_dialog_find(w->d, 1103, 1);
    dxl_ctl_clear_items(w->d, list);
    if (!p->painted) {
        dxl_ctl_add_item(w->d, list, dxl_loc_general(w->req->loc, "Startup", "Detecting"));
    } else {
        for (int i = 0; i < p->shown_count; i++)
            dxl_ctl_add_item(w->d, list, p->devices.items[p->shown[i]].caption);
        list->selected = p->selected;
    }
    dxl_dialog_find(w->d, 1109, 1)->checked = !p->all;
    dxl_dialog_find(w->d, 1110, 1)->checked = p->all;
    const char *path = p->painted ? current_driver(p) : NULL;
    char *note = dxl_loc_line_format(path ? dxl_renderdev_note(w->req->loc, path) : "");
    dxl_ctl_set_text(w->d, dxl_dialog_find(w->d, 1104, 1), note);
    free(note);
}

/* ---- SafeOptions' boxes ------------------------------------------------------- */

/* The boxes in dxl_safe_options' order, the page's top to bottom. */
static const int option_ids[8] = { 1108, 1109, 1110, 1112, 1111, 1113, 1114, 1115 };

static int *option(dxl_safe_options *o, int i) {
    int *const fields[8] = { &o->no_sound, &o->no_3d_sound, &o->no_3d_video, &o->window,
                             &o->res, &o->reset_config, &o->no_processor, &o->no_joy };
    return fields[i];
}

static void read_options(dxl_wizard *w, page *p) {
    for (int i = 0; i < 8; i++)
        *option(&p->opts, i) = dxl_dialog_find(w->d, option_ids[i], 1)->checked;
}

/* ---- showing a page ---------------------------------------------------------- */

static void show_page(dxl_wizard *w) {
    page *p = top(w);
    dxl_dialog_remove_layer(w->d, 1);
    int ox = dxl_dlu_x(PAGE_X), oy = dxl_dlu_y(PAGE_Y);
    /* A page is a window over the frame's, and once the frame is up, one
     * shown later paints over the top line of the etched rule below it, as
     * the original's do: the first page leaves it whole. */
    if (w->shows++ > 0)
        dxl_dialog_add(w->d, DXL_CTL_PANE, -1, map(0, 0, PAGE_W, PAGE_H, ox, oy), "", 1);
    char section[64];
    snprintf(section, sizeof section, "IDDIALOG_%s", pages[p->id].name);
    for (size_t i = 0; i < pages[p->id].n; i++) {
        const tmpl *t = &pages[p->id].t[i];
        char *text = NULL;
        if (t->text && strncmp(t->text, "IDC_", 4) == 0)
            text = dxl_loc_line_format(dxl_loc_get(w->req->loc, "Startup", section, t->text, 0));
        dxl_ctl *c = dxl_dialog_add(w->d, t->kind, t->id, map(t->x, t->y, t->w, t->h, ox, oy),
                                    text ? text : "", 1);
        c->tabstop = t->tabstop;
        free(text);
    }
    switch (p->id) {
    case DXL_PAGE_RENDERER:
        fill_renderer(w, p);
        break;
    case DXL_PAGE_DRIVER: {
        const char *card = dxl_ini_get(dxl_config_ini(w->req->config), "D3DDrv.D3DRenderDevice",
                                       "Description");
        if (card && *card) dxl_ctl_set_text(w->d, dxl_dialog_find(w->d, 1111, 1), card);
        break;
    }
    case DXL_PAGE_DETAIL:
        dxl_ctl_set_text(w->d, dxl_dialog_find(w->d, 1100, 1), p->detail ? p->detail : "");
        break;
    case DXL_PAGE_SAFEOPTIONS:
        for (int i = 0; i < 8; i++)
            dxl_dialog_find(w->d, option_ids[i], 1)->checked = *option(&p->opts, i);
        break;
    default:
        break;
    }
    frame_button(w, ID_BACK, w->depth > 1 ? window_text(w, "BackButton") : NULL);
    frame_button(w, ID_NEXT, next_text(w, p->id));
    frame_button(w, ID_FINISH, NULL);
    frame_button(w, ID_CANCEL, window_text(w, "CancelButton"));
    dxl_dialog_focus_first(w->d);
}

void dxl_wizard_push(dxl_wizard *w, dxl_page_id id) {
    if (w->depth == (int)(sizeof w->stack / sizeof *w->stack)) return;
    page *p = &w->stack[w->depth++];
    memset(p, 0, sizeof *p);
    p->id = id;
    p->selected = -1;
    if (id == DXL_PAGE_SAFEOPTIONS) p->opts = dxl_safe_defaults();
    if (id == DXL_PAGE_DETAIL)
        p->detail = dxl_detail_apply(dxl_config_ini(w->req->config), w->req->loc, &w->req->machine);
    show_page(w);
}

static void pop(dxl_wizard *w) {
    page *p = top(w);
    dxl_renderdev_free(&p->devices);
    free(p->detail);
    w->depth--;
}

/* ---- the Renderer page's first paint ------------------------------------------ */

void dxl_wizard_first_paint(dxl_wizard *w, int detect) {
    page *p = top(w);
    if (!p || p->id != DXL_PAGE_RENDERER || p->painted) return;
    if (detect && !dxl_cmd_param(w->req->cmdline, "nodetect")) {
        dxl_err e;
        dxl_config_save(w->req->config, &e);
        char *detected = dxl_path_join(w->req->system_dir, "Detected.ini");
        remove(detected);
        pid_t pid = 0;
        int running = dxl_child_start(w->req->exe_path, DETECT_FLAGS, w->req->system_dir, &pid,
                                      &e) == 0;
        /* Up to 30 seconds for Detected.ini, in steps of 100 ms, as the
         * original waits. Input waits too, as it does there; the window
         * keeps drawing. A run that ended without it is not waited for. */
        for (int ms = DETECT_MS; ms > 0 && dxl_path_size(detected) < 0; ms -= 100) {
            if (running && dxl_child_reap(pid)) {
                running = 0;
                if (dxl_path_size(detected) < 0) break;
            }
            SDL_PumpEvents();
            dxl_dialog_draw(w->d);
            SDL_Delay(100);
        }
        /* Detected.ini is the run's last word; give it a moment to end. */
        for (int ms = 2000; running && ms > 0; ms -= 10) {
            if (dxl_child_reap(pid)) running = 0;
            else SDL_Delay(10);
        }
        free(detected);
        dxl_config_reload(w->req->config);
    }
    p->painted = 1;
    refresh_list(w, p);
    fill_renderer(w, p);
}

void dxl_wizard_show_all(dxl_wizard *w, int all) {
    page *p = top(w);
    if (!p || p->id != DXL_PAGE_RENDERER) return;
    p->all = all;
    if (p->painted) refresh_list(w, p);
    fill_renderer(w, p);
}

/* ---- Next, Back, and the pages' own buttons ---------------------------------- */

static void end(dxl_wizard *w, int result) { dxl_dialog_end(w->d, result); }

static void next(dxl_wizard *w) {
    page *p = top(w);
    if (!p || !next_text(w, p->id)) return;
    switch (p->id) {
    case DXL_PAGE_RENDERER: {
        if (!p->painted) return;
        /* The selection, if any, becomes the game's renderer; whatever the
         * game's renderer is then decides the next page. */
        const char *path = current_driver(p);
        if (path) dxl_ini_set(dxl_config_ini(w->req->config), "Engine.Engine", "GameRenderDevice", path);
        const char *device = dxl_config_render_device(w->req->config);
        dxl_wizard_push(w, device && dxl_stricmp(device, DXL_D3D_DEVICE) == 0 ? DXL_PAGE_DRIVER
                                                                              : DXL_PAGE_DETAIL);
        break;
    }
    case DXL_PAGE_DRIVER:    dxl_wizard_push(w, DXL_PAGE_DETAIL); break;
    case DXL_PAGE_DETAIL:    dxl_wizard_push(w, DXL_PAGE_FIRSTTIME); break;
    case DXL_PAGE_FIRSTTIME: end(w, 1); break;
    case DXL_PAGE_SAFEOPTIONS: {
        /* The boxes as they are now, read one by one as the original's
         * GetNext reads them -- each its own, which the original's are not. */
        dxl_err e;
        read_options(w, p);
        w->req->relaunch_flags = dxl_safe_flags(&p->opts);
        if (p->opts.reset_config)
            dxl_safe_reset_config(w->req->system_dir, w->req->package, &e);
        end(w, 0);
        break;
    }
    default:
        break;
    }
}

static void back(dxl_wizard *w) {
    if (w->depth <= 1) return;
    pop(w);
    show_page(w);
}

static void on_command(dxl_dialog *d, int id, dxl_notify what, void *ctx) {
    (void)d;
    dxl_wizard *w = ctx;
    page *p = top(w);
    if (id == ID_CANCEL || id == DXL_IDCANCEL) { end(w, 0); return; }
    if (id == ID_NEXT && what == DXL_CLICKED) { next(w); return; }
    if (id == ID_BACK && what == DXL_CLICKED) { back(w); return; }
    if (id == ID_FINISH && what == DXL_CLICKED) { end(w, 1); return; }
    if (!p) return;
    switch (p->id) {
    case DXL_PAGE_RENDERER:
        if ((id == 1109 || id == 1110) && what == DXL_CLICKED) {
            dxl_wizard_show_all(w, dxl_dialog_find(w->d, 1110, 1)->checked);
        } else if (id == 1103 && what == DXL_SELCHANGE && p->painted) {
            p->selected = dxl_dialog_find(w->d, 1103, 1)->selected;
            fill_renderer(w, p);
        } else if (id == 1103 && what == DXL_DBLCLK) {
            next(w);
        }
        break;
    case DXL_PAGE_SAFEMODE:
        if (id == 1108) end(w, 1);
        else if (id == 1110) dxl_wizard_push(w, DXL_PAGE_RENDERER);
        else if (id == 1109) dxl_wizard_push(w, DXL_PAGE_SAFEOPTIONS);
        else if (id == 1058) {
            dxl_open_url(dxl_loc_general(w->req->loc, "Startup", "WebPage"));
            end(w, 0);
        }
        break;
    case DXL_PAGE_DRIVER:
        if (id == 1113 && what == DXL_CLICKED)
            dxl_open_url(dxl_loc_general(w->req->loc, "Startup", "Direct3DWebPage"));
        break;
    default:
        break;
    }
}

/* ---- the wizard --------------------------------------------------------------- */

dxl_wizard *dxl_wizard_new(dxl_gui *g, dxl_wizard_request *req, int offscreen, dxl_err *err) {
    dxl_wizard *w = dxl_xmalloc(sizeof *w);
    memset(w, 0, sizeof *w);
    w->req = req;
    w->d = dxl_dialog_open(g, req->title, dxl_dlu_x(FRAME_W), dxl_dlu_y(FRAME_H), offscreen, err);
    if (!w->d) { free(w); return NULL; }
    w->d->on_command = on_command;
    w->d->ctx = w;
    build_frame(w);
    return w;
}

void dxl_wizard_free(dxl_wizard *w) {
    if (!w) return;
    while (w->depth) pop(w);
    dxl_dialog_close(w->d);
    free(w);
}

int dxl_wizard_save(dxl_wizard *w, const char *bmp_path) { return dxl_dialog_save(w->d, bmp_path); }

dxl_dialog *dxl_wizard_dialog(dxl_wizard *w) { return w->d; }

int dxl_wizard_run(dxl_wizard *w) {
    dxl_screen s = w->req->start;
    dxl_wizard_push(w, s == DXL_SCREEN_MAIN_SAFE || s == DXL_SCREEN_MAIN_RECOVERY
                           ? DXL_PAGE_SAFEMODE : DXL_PAGE_RENDERER);
    while (!dxl_dialog_step(w->d, 50)) {
        page *p = top(w);
        if (p && p->id == DXL_PAGE_RENDERER && !p->painted) {
            dxl_dialog_draw(w->d);
            dxl_wizard_first_paint(w, 1);
        }
    }
    return w->d->result;
}

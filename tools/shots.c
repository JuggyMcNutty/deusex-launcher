/* Renders every screen to a .bmp, so layout can be reviewed without a
 * handheld in reach. Uses SDL's offscreen/dummy driver, so it runs headless.
 *
 * It draws the device profile compiled in (platform/target.h): configure a
 * host build with -DDXL_PROFILE=<port> to see that device's screens.
 *
 * This is a development tool, not part of the launcher: it fabricates the
 * session state each screen needs and drives the same draw code the real
 * frontend does, which is the point -- a screenshot of different code would
 * prove nothing.
 */
#include "app.h"
#include "ui/screens.h"
#include "ui/ui.h"
#include "core/log.h"
#include "platform/target.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* dxl_ui keeps its internals private, so grab the renderer back out of SDL
 * rather than widening the header for a tool. */
static SDL_Renderer *renderer_of_current_window(void) {
    for (int id = 1; id < 64; id++) {
        SDL_Window *w = SDL_GetWindowFromID((Uint32)id);
        if (w) {
            SDL_Renderer *r = SDL_GetRenderer(w);
            if (r) return r;
        }
    }
    return NULL;
}

static void save(SDL_Renderer *r, const char *dir, const char *name) {
    if (!r) return;
    int w, h;
    SDL_GetRendererOutputSize(r, &w, &h);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32,
                                                    SDL_PIXELFORMAT_RGB888);
    if (!s) return;
    /* The read-back happens after a present, so on a double-buffered driver
     * whatever the buffer holds is the driver's business. Start from black so
     * an unwritten row reads as background rather than as garbage. */
    SDL_FillRect(s, NULL, SDL_MapRGB(s->format, 10, 10, 12));
    if (SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_RGB888,
                             s->pixels, s->pitch) == 0) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s.bmp", dir, name);
        SDL_SaveBMP(s, path);
        printf("  %s (%dx%d)\n", path, w, h);
    } else {
        printf("  read-back failed for %s: %s\n", name, SDL_GetError());
    }
    SDL_FreeSurface(s);
}

/* The device's GPU as its profile records it, so the shots show its
 * renderer list and any GPU-specific rows. */
static void fake_gpu(dxl_app *app) {
    app->gpu = dxl_target_get()->preview_gpu;
    dxl_renderers_resolve(&app->renderers, &app->gpu);
}

/* Two frames through the real draw path: with double buffering the buffer
 * read back is the one presented a frame earlier. */
static void shoot(dxl_session *s, SDL_Renderer *r, const char *dir, const char *name) {
    dxl_session_draw(s);
    dxl_session_draw(s);
    save(r, dir, name);
}

int main(int argc, char **argv) {
    const char *outdir = (argc > 1) ? argv[1] : ".";

    /* Offscreen first; dummy cannot read pixels back but is a useful
     * smoke test if offscreen is missing. */
    if (!SDL_getenv("SDL_VIDEODRIVER")) SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);
    /* The device's panel, not whatever the offscreen driver calls a desktop. */
    const dxl_target *t = dxl_target_get();
    if (!SDL_getenv("DXL_WINDOW")) {
        char size[32];
        snprintf(size, sizeof size, "%dx%d", t->panel_w, t->panel_h);
        SDL_setenv("DXL_WINDOW", size, 1);
    }


    dxl_log_set_echo(0);
    dxl_log_set_to_file(0);

    dxl_app app;
    dxl_err err;
    if (dxl_app_init(&app, 1, argv, &err) != 0) {
        fprintf(stderr, "app init: %s\n", dxl_err_msg(&err));
        return 1;
    }
    fake_gpu(&app);
    dxl_ui *ui = dxl_ui_init(&err);
    if (!ui) { fprintf(stderr, "ui init: %s\n", dxl_err_msg(&err)); return 1; }
    if (dxl_ui_headless(ui)) fprintf(stderr, "warning: no font; shots will be blank\n");

    SDL_Renderer *r = renderer_of_current_window();
    printf("rendering to %s\n", outdir);

    /* Nothing below writes a file: every state is set up in memory and only
     * drawn. The session is never stepped, so no input is read either. */
    dxl_session s;
    dxl_session_start(&s, &app, ui);

    s.installing = 1;
    shoot(&s, r, outdir, "01-install");
    s.installing = 0;

    s.tab = DXL_TAB_PLAY;
    s.crashed = 0;
    s.cursor[DXL_TAB_PLAY] = 0;
    shoot(&s, r, outdir, "02-play");

    s.crashed = 1;
    snprintf(s.crash_detail, sizeof s.crash_detail, "Could not find package Core");
    s.cursor[DXL_TAB_PLAY] = 1;
    shoot(&s, r, outdir, "03-play-crashed");
    s.crashed = 0;
    s.cursor[DXL_TAB_PLAY] = 0;

    s.tab = DXL_TAB_VIDEO;
    s.cursor[DXL_TAB_VIDEO] = 0;
    shoot(&s, r, outdir, "04-video");
    /* Rows by name: which rows show depends on the device profile. */
    s.cursor[DXL_TAB_VIDEO] = dxl_session_row_index(&s, DXL_TAB_VIDEO, "Anti-aliasing");
    shoot(&s, r, outdir, "05-video-aa");          /* locked on a PowerVR */
    s.cursor[DXL_TAB_VIDEO] = dxl_session_row_index(&s, DXL_TAB_VIDEO, "Brightness");
    shoot(&s, r, outdir, "06-video-brightness");
    s.cursor[DXL_TAB_VIDEO] = dxl_session_row_index(&s, DXL_TAB_VIDEO, "Distant AI");
    shoot(&s, r, outdir, "06b-video-distant-ai");

    s.cursor[DXL_TAB_VIDEO] = 0;
    dxl_session_act(&s, DXL_ACT_CONFIRM);   /* opens the picker */
    s.ovl_cursor = 1;                        /* on the second renderer */
    shoot(&s, r, outdir, "07-renderers");
    dxl_session_act(&s, DXL_ACT_BACK);

    s.tab = DXL_TAB_CONTROLS;
    s.cursor[DXL_TAB_CONTROLS] = dxl_session_row_index(&s, DXL_TAB_CONTROLS, "Layout");
    shoot(&s, r, outdir, "08-controls");
    s.cursor[DXL_TAB_CONTROLS] = dxl_session_row_index(&s, DXL_TAB_CONTROLS, "Customize buttons");
    dxl_session_act(&s, DXL_ACT_CONFIRM);
    s.ovl_cursor = 8;                        /* SELECT */
    shoot(&s, r, outdir, "09-customize");
    dxl_session_act(&s, DXL_ACT_CONFIRM);   /* its action picker */
    shoot(&s, r, outdir, "09b-actions");
    dxl_session_act(&s, DXL_ACT_BACK);      /* back to the list: nothing bound */
    dxl_session_act(&s, DXL_ACT_BACK);

    s.tab = DXL_TAB_SYSTEM;
    s.cursor[DXL_TAB_SYSTEM] = 0;
    shoot(&s, r, outdir, "10-system");
    s.cursor[DXL_TAB_SYSTEM] = dxl_session_row_index(&s, DXL_TAB_SYSTEM, "Engine log");
    dxl_session_act(&s, DXL_ACT_CONFIRM);   /* scrolled to the end */
    shoot(&s, r, outdir, "11-log");
    dxl_session_act(&s, DXL_ACT_BACK);
    s.cursor[DXL_TAB_SYSTEM] = dxl_session_row_index(&s, DXL_TAB_SYSTEM, "Reset game configuration");
    dxl_session_act(&s, DXL_ACT_CONFIRM);   /* its confirmation */
    shoot(&s, r, outdir, "12-confirm");
    dxl_session_act(&s, DXL_ACT_BACK);      /* cancel: nothing deleted */

    dxl_session_free(&s);
    dxl_ui_quit(ui);
    dxl_app_shutdown(&app);
    return 0;
}

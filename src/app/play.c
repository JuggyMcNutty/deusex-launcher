#define _GNU_SOURCE
#include "app/play.h"
#include "core/log.h"
#include "platform/process.h"

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The wait wakes on the line, on the instance's socket, and on a signal (a
 * stop), which ends poll() at once; besides, once a second, in case the
 * engine's end of the line outlives it in a child of its own, and four
 * times a second to count down to SIGKILL once the game is stopped. */
#define WAKE_MS      1000
#define STOPPING_MS  250

/* ParseToken: the first word of a line, or a quoted one. */
static int first_token(const char *s, char *out, size_t size) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s || !size) return 0;
    size_t n = 0;
    if (*s == '"') {
        for (s++; *s && *s != '"'; s++) if (n + 1 < size) out[n++] = *s;
    } else {
        for (; *s && !isspace((unsigned char)*s); s++) if (n + 1 < size) out[n++] = *s;
    }
    out[n] = '\0';
    return 1;
}

/* A line a second launch forwarded: the game comes to the front, and a URL
 * in it opens, as the original's log window runs it -- only once the
 * engine is up. */
static void forwarded(dxl_game *g, const char *msg, int ready) {
    dxl_log("forwarded: %s", msg);
    if (!ready) return;
    dxl_game_send(g, "TakeFocus");
    char url[DXL_HANDOFF_MAX];
    if (first_token(msg, url, sizeof url) && url[0] != '-') {
        char open[DXL_HANDOFF_MAX + 8];
        snprintf(open, sizeof open, "Open %s", url);
        dxl_game_send(g, open);
    }
}

void dxl_play(const dxl_play_request *req, dxl_play_result *out) {
    memset(out, 0, sizeof *out);
    dxl_game g;
    dxl_err e;
    if (dxl_game_start_argv(&g, req->argv, req->env, req->workdir, req->log_path, &e) != 0) {
        snprintf(out->error, sizeof out->error, "%s", dxl_err_msg(&e));
        dxl_log("the game did not start: %s", out->error);
        return;
    }
    out->started = 1;
    dxl_log("the game started: pid %ld", (long)g.pid);

    int since_term = 0;
    for (;;) {
        char line[1024];
        int ev = dxl_game_wait(&g, dxl_instance_fd(req->instance), out->stopped ? STOPPING_MS : WAKE_MS,
                               line, sizeof line);
        if (ev & DXL_GAME_LINE) {
            if (strcmp(line, "hello") == 0) {
                out->hello = 1;
            } else if (strcmp(line, "ready") == 0 && !out->ready) {
                out->ready = 1;
                dxl_log("the engine is up");
                if (req->on_ready) req->on_ready(req->ctx);
            } else if (strncmp(line, "relaunch", 8) == 0 && (line[8] == ' ' || !line[8])) {
                free(out->relaunch);
                out->relaunch = dxl_xstrdup(line[8] ? line + 9 : "");
                dxl_log("relaunch: %s", out->relaunch);
            }
        }
        if (ev & DXL_GAME_EXTRA) {
            char msg[DXL_HANDOFF_MAX];
            while (dxl_instance_poll(req->instance, msg, sizeof msg)) forwarded(&g, msg, out->ready);
        }
        if (ev & DXL_GAME_ENDED) break;
        if (req->stop && *req->stop && !out->stopped) {
            dxl_log("stopping the game");
            dxl_game_signal(&g, SIGTERM);
            out->stopped = 1;
        } else if (out->stopped && !out->killed && !(ev & DXL_GAME_LINE)) {
            if ((since_term += STOPPING_MS) >= DXL_PLAY_KILL_AFTER_MS) {
                dxl_log("the game did not stop: killing it");
                dxl_game_signal(&g, SIGKILL);
                out->killed = 1;
            }
        }
    }
    out->status = g.status;
    out->signal = g.signal;
    out->clean = dxl_game_clean(&g) && out->ready;
    dxl_log("the game ended: %d%s", g.status, out->clean ? ", cleanly" : "");
    if (!out->clean) {
        free(out->relaunch);
        out->relaunch = NULL;
    }
}

void dxl_play_result_free(dxl_play_result *r) {
    if (!r) return;
    free(r->relaunch);
    r->relaunch = NULL;
}

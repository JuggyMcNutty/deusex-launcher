#define _GNU_SOURCE
#include "app/browser.h"
#include "core/gamespy.h"
#include "platform/net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* How long each part may take. */
#define CONNECT_MS   5000
#define GREETING_MS  5000
#define LIST_MS      10000
#define RETRY_MS     1000    /* an answer not whole this long after its asking is asked again, */
#define ASKS         3       /* each server asked this often at most, */
#define ANSWER_MS    2000    /* and given up this long after its last asking */
#define RESOLVE_MS   5000
#define LIST_MAX     (256 * 1024)
#define LAN_MS       1000    /* the local network's beacons given this long at least */
#define BEACON_PORTS 10      /* the beacon ports asked, from the first: the game's ten */

static long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

static int stopped(const atomic_int *stop) { return stop && atomic_load(stop); }

/* What each server is asked; what each beacon. */
static const char STATUS[] = "\\status\\";
static const char REPORTQUERY[] = "REPORTQUERY";

void dxl_browse_lan_init(dxl_browse_lan *lan, int beacon_port, const char *product) {
    memset(lan, 0, sizeof *lan);
    lan->beacon_port = beacon_port > 0 && beacon_port <= 65535 - BEACON_PORTS ? beacon_port : DXL_BEACON_PORT;
    snprintf(lan->product, sizeof lan->product, "%s", product && *product ? product : DXL_BEACON_PRODUCT);
    const char *env = getenv("DXL_LAN");
    if (!env || !*env) return;
    if (!dxl_stricmp(env, "none")) {
        lan->none = 1;
        return;
    }
    snprintf(lan->to, sizeof lan->to, "%s", env);
    char *colon = strrchr(lan->to, ':');
    if (colon) {
        *colon = '\0';
        int port = atoi(colon + 1);
        if (port > 0 && port <= 65535 - BEACON_PORTS) lan->beacon_port = port;
    }
}

typedef struct {
    char          address[64];
    int           port;
    int           lan;           /* a beacon on the local network answered for it */
    long          sent, first;   /* when asked last; its answer's first packet, 0 before */
    int           asks, done;
    dxl_gs_status status;
} asked;

typedef struct {
    asked *a;
    int    count;
    const dxl_browse_fns *fns;
    void  *ctx;
} asking;

/* A server to ask; one listed again by a beacon is told so. */
static void listed_as(asking *q, const char *address, int port, int lan) {
    for (int i = 0; i < q->count; i++) {
        asked *s = &q->a[i];
        if (s->port != port || strcmp(s->address, address) != 0) continue;
        if (lan && !s->lan) {
            s->lan = 1;
            if (q->fns && q->fns->listed) q->fns->listed(q->ctx, address, port, 1);
        }
        return;
    }
    if (q->count >= DXL_BROWSE_MAX) return;
    asked *s = &q->a[q->count++];
    memset(s, 0, sizeof *s);
    snprintf(s->address, sizeof s->address, "%s", address);
    s->port = port;
    s->lan = lan;
    dxl_gs_status_init(&s->status);
    if (q->fns && q->fns->listed) q->fns->listed(q->ctx, address, port, lan);
}

static void listed(void *ctx, const char *address, int port) { listed_as(ctx, address, port, 0); }

/* The local network asked, as DeusExLocalLink asks it: REPORTQUERY to each
 * of the ten beacon ports at its broadcast addresses, or at DXL_LAN's. */
static void lan_ask(dxl_socket fd, const dxl_browse_lan *lan, const atomic_int *stop) {
    char to[16][64];
    int n = 0;
    if (lan->to[0]) {
        dxl_err e;
        if (dxl_net_resolve(lan->to, to[0], sizeof to[0], RESOLVE_MS, stop, &e) == 0) n = 1;
    } else if (dxl_udp_broadcast(fd) == 0) {
        n = dxl_net_broadcasts(to, 16);
    }
    for (int i = 0; i < n; i++)
        for (int p = 0; p < BEACON_PORTS; p++)
            dxl_udp_send(fd, to[i], lan->beacon_port + p, REPORTQUERY, sizeof REPORTQUERY - 1);
}

/* A beacon's answer, "<product> <query port>" as the game's LocalLink reads
 * it: the port, or 0 for another's. */
static int beacon_answer(const dxl_browse_lan *lan, const char *text) {
    size_t n = strlen(lan->product);
    if (dxl_strnicmp(text, lan->product, n) != 0 || text[n] != ' ') return 0;
    int port = atoi(text + n + 1);
    return port > 0 && port <= 65535 ? port : 0;
}

/* Each server asked for its status, the answers taken as they come: one
 * not whole a second after its asking -- a packet lost, or all of them --
 * asked again, its answer begun again, as the game asks till final comes.
 * With lan, what the beacons answer till lan_until at least: each server
 * one names asked too. */
static void ask_all(asking *q, dxl_socket fd, const dxl_browse_lan *lan, long lan_until, const atomic_int *stop) {
    char buf[4096], from[64];
    /* What the beacons answered while the master was asked, read whatever
     * it took and listed: their servers asked with its. */
    for (int port = 0; lan;) {
        long n = dxl_udp_recv(fd, buf, sizeof buf - 1, 0, from, sizeof from, &port);
        if (n <= 0) break;
        buf[n] = '\0';
        int beacon = beacon_answer(lan, buf);
        if (beacon) listed_as(q, from, beacon, 1);
    }
    long t = now_ms();
    for (int i = 0; i < q->count; i++) {
        dxl_udp_send(fd, q->a[i].address, q->a[i].port, STATUS, 8);
        q->a[i].sent = t;
        q->a[i].asks = 1;
    }
    int left = q->count;
    while ((left > 0 || (lan && now_ms() < lan_until)) && !stopped(stop)) {
        t = now_ms();
        long until = lan && lan_until > t ? lan_until : 0;
        for (int i = 0; i < q->count; i++) {
            asked *s = &q->a[i];
            if (s->done) continue;
            if (s->asks < ASKS && t - s->sent >= RETRY_MS) {
                dxl_gs_status_restart(&s->status);
                s->first = 0;
                s->sent = t;
                s->asks++;
                dxl_udp_send(fd, s->address, s->port, STATUS, 8);
            } else if (s->asks >= ASKS && t - s->sent >= ANSWER_MS) {
                s->done = 1;
                left--;
                continue;
            }
            long end = s->sent + (s->asks < ASKS ? RETRY_MS : ANSWER_MS);
            if (!until || end < until) until = end;
        }
        if (left <= 0 && !until) break;
        long wait = until > t ? until - t : 1;
        if (wait > 100) wait = 100;
        int port = 0;
        long n = dxl_udp_recv(fd, buf, sizeof buf - 1, (int)wait, from, sizeof from, &port);
        if (n <= 0) continue;
        buf[n] = '\0';
        int known = 0;
        for (int i = 0; i < q->count && !known; i++) known = q->a[i].port == port && !strcmp(q->a[i].address, from);
        int beacon = !known && lan ? beacon_answer(lan, buf) : 0;
        if (beacon) {
            int before = q->count;
            listed_as(q, from, beacon, 1);
            if (q->count > before) {
                asked *s = &q->a[q->count - 1];
                dxl_udp_send(fd, s->address, s->port, STATUS, 8);
                s->sent = now_ms();
                s->asks = 1;
                left++;
            }
            continue;
        }
        for (int i = 0; i < q->count; i++) {
            asked *s = &q->a[i];
            if (s->done || s->port != port || strcmp(s->address, from) != 0) continue;
            if (dxl_gs_status_add(&s->status, buf, (size_t)n) != 0) break;
            if (!s->first) s->first = now_ms();
            if (dxl_gs_status_whole(&s->status)) {
                char *text = dxl_gs_status_text(&s->status);
                if (q->fns && q->fns->answered) q->fns->answered(q->ctx, s->address, s->port, (int)(s->first - s->sent), text);
                free(text);
                s->done = 1;
                left--;
            }
            break;
        }
    }
}

/* The master's list, as the game asks for it. */
static int master_list(const char *master, asking *q, const atomic_int *stop, dxl_err *err) {
    char host[256];
    snprintf(host, sizeof host, "%s", master);
    int port = DXL_MASTER_PORT;
    char *colon = strrchr(host, ':');
    if (colon) {
        *colon = '\0';
        port = atoi(colon + 1);
    }
    if (!host[0] || port <= 0 || port > 65535) {
        dxl_err_set(err, "%s is no master server's address", master);
        return -1;
    }
    char address[64];
    if (dxl_net_resolve(host, address, sizeof address, RESOLVE_MS, stop, err) != 0) return -1;
    dxl_socket fd = dxl_tcp_connect(address, port, CONNECT_MS, err);
    if (fd == DXL_SOCKET_NONE) return -1;

    char *text = dxl_xmalloc(LIST_MAX + 1);
    text[0] = '\0';
    size_t len = 0;
    int rc = -1;
    char challenge[7] = "";
    long start = now_ms();
    /* Its greeting: the challenge. */
    while (!stopped(stop) && now_ms() - start < GREETING_MS && !dxl_gs_challenge(text, challenge)) {
        long n = dxl_tcp_recv(fd, text + len, LIST_MAX - len, 100);
        if (n == -2) continue;
        if (n <= 0) break;
        len += (size_t)n;
        text[len] = '\0';
    }
    if (!challenge[0]) {
        /* Its refusal, said: a ban, a game it does not list. */
        char why[200];
        if (dxl_gs_value(text, "error", why, sizeof why)) dxl_err_set(err, "%s: %s", master, why);
        else if (!stopped(stop)) dxl_err_set(err, "%s sent no challenge", master);
        goto done;
    }
    char answer[9], ask[160];
    dxl_gs_validate(challenge, DXL_GS_GAME, answer);
    snprintf(ask, sizeof ask, "\\gamename\\%s\\location\\0\\validate\\%s\\final\\\\list\\\\gamename\\%s\\final\\",
             DXL_GS_GAME, answer, DXL_GS_GAME);
    if (dxl_tcp_send(fd, ask, strlen(ask), CONNECT_MS) != 0) {
        dxl_err_set(err, "%s took no answer", master);
        goto done;
    }
    /* Its list, after the greeting. */
    len = 0;
    text[0] = '\0';
    start = now_ms();
    int closed = 0;
    while (!stopped(stop) && now_ms() - start < LIST_MS && len < LIST_MAX) {
        long n = dxl_tcp_recv(fd, text + len, LIST_MAX - len, 100);
        if (n == -2) continue;
        if (n <= 0) {
            closed = 1;
            break;
        }
        len += (size_t)n;
        text[len] = '\0';
        if (dxl_gs_list(text, NULL, NULL) >= 0) break;
    }
    char why[200];
    if (dxl_gs_value(text, "error", why, sizeof why)) {
        dxl_err_set(err, "%s: %s", master, why);
    } else if (dxl_gs_list(text, listed, q) >= 0) {
        rc = 0;
    } else if (!stopped(stop)) {
        dxl_err_set(err, "%s %s", master, closed ? "closed without its list" : "sent no list in time");
    }
done:
    free(text);
    dxl_net_close(fd);
    return rc;
}

static void finish(asking *q) {
    for (int i = 0; i < q->count; i++) dxl_gs_status_free(&q->a[i].status);
    free(q->a);
}

int dxl_browse(const char *master, const char *const *also, const dxl_browse_lan *lan, const dxl_browse_fns *fns,
               void *ctx, const atomic_int *stop, dxl_err *err) {
    asking q = { dxl_xmalloc(sizeof(asked) * DXL_BROWSE_MAX), 0, fns, ctx };
    /* The beacons first, their answers waiting on the socket while the
     * master is asked. */
    dxl_err e;
    dxl_socket fd = dxl_udp_open(&e);
    if (lan && lan->none) lan = NULL;
    long lan_until = 0;
    if (fd != DXL_SOCKET_NONE && lan) {
        lan_ask(fd, lan, stop);
        lan_until = now_ms() + LAN_MS;
    }
    /* The favourites first: however many the master lists, they are asked. */
    for (size_t i = 0; also && also[i] && !stopped(stop); i++) {
        char host[256], at[64];
        snprintf(host, sizeof host, "%s", also[i]);
        char *colon = strrchr(host, ':');
        int port = colon ? atoi(colon + 1) : 0;
        if (colon) *colon = '\0';
        if (port > 0 && port <= 65535 && dxl_net_resolve(host, at, sizeof at, RESOLVE_MS, stop, &e) == 0)
            listed(&q, at, port);
    }
    int rc = master && *master ? master_list(master, &q, stop, err) : 0;
    if (fd != DXL_SOCKET_NONE && (q.count || lan)) ask_all(&q, fd, lan, lan_until, stop);
    if (fd != DXL_SOCKET_NONE) dxl_net_close(fd);
    finish(&q);
    return rc;
}

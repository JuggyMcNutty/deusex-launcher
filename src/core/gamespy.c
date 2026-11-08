#include "core/gamespy.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- the challenge's answer -------------------------------------------- */

static const char *secret_of(const char *game) {
    if (!strcmp(game, "unreal")) return "DAncRK";
    if (!strcmp(game, "ut")) return "Z5Nfb0";
    if (!strcmp(game, "oldver")) return "EBodSL";
    return "      ";
}

void dxl_gs_validate(const char *challenge, const char *game, char out[9]) {
    const char *key = secret_of(game ? game : "");
    size_t key_len = strlen(key);

    /* RC4's table, shuffled by the key. */
    uint8_t s[256];
    for (int i = 0; i < 256; i++) s[i] = (uint8_t)i;
    for (int i = 0, j = 0; i < 256; i++) {
        j = (j + (uint8_t)key[i % key_len] + s[i]) & 0xff;
        uint8_t t = s[i];
        s[i] = s[j];
        s[j] = t;
    }

    /* The six characters, each XORed with the stream; GameSpy's own: the
     * first index moves on by the byte it encrypts, and one more. */
    uint8_t b[6] = { 0 };
    for (int i = 0; i < 6 && challenge && challenge[i]; i++) b[i] = (uint8_t)challenge[i];
    uint8_t x = 0, y = 0;
    for (int i = 0; i < 6; i++) {
        x = (uint8_t)(x + b[i] + 1);
        y = (uint8_t)(y + s[x]);
        uint8_t t = s[x];
        s[x] = s[y];
        s[y] = t;
        b[i] ^= s[(uint8_t)(s[x] + s[y])];
    }

    /* Six bytes are eight characters of base 64. */
    static const char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0, o = 0; i < 6; i += 3, o += 4) {
        out[o] = digits[b[i] >> 2];
        out[o + 1] = digits[(b[i] & 3) << 4 | b[i + 1] >> 4];
        out[o + 2] = digits[(b[i + 1] & 15) << 2 | b[i + 2] >> 6];
        out[o + 3] = digits[b[i + 2] & 63];
    }
    out[8] = '\0';
}

/* ---- pairs ------------------------------------------------------------ */

/* The pair at *p -- \key\value -- its key and value's starts and lengths;
 * *p moved past it. 0 at the text's end. */
static int next_pair(const char **p, const char *end, const char **k, size_t *kn, const char **v, size_t *vn) {
    const char *at = *p;
    while (at < end && *at != '\\') at++;
    if (at >= end) return 0;
    *k = ++at;
    while (at < end && *at != '\\') at++;
    *kn = (size_t)(at - *k);
    if (at < end) at++;
    *v = at;
    while (at < end && *at != '\\') at++;
    *vn = (size_t)(at - *v);
    *p = at;
    return 1;
}

int dxl_gs_next(const char **p, char *key, size_t key_size, char *value, size_t value_size) {
    const char *k, *v;
    size_t kn, vn;
    if (!next_pair(p, *p + strlen(*p), &k, &kn, &v, &vn)) return 0;
    snprintf(key, key_size, "%.*s", (int)kn, k);
    snprintf(value, value_size, "%.*s", (int)vn, v);
    return 1;
}

static int key_is(const char *k, size_t kn, const char *name) {
    return strlen(name) == kn && !dxl_strnicmp(k, name, kn);
}

int dxl_gs_value(const char *text, const char *key, char *out, size_t size) {
    const char *p = text, *end = text + strlen(text), *k, *v;
    size_t kn, vn;
    while (next_pair(&p, end, &k, &kn, &v, &vn)) {
        if (!key_is(k, kn, key)) continue;
        if (size) snprintf(out, size, "%.*s", (int)vn, v);
        return 1;
    }
    if (size) out[0] = '\0';
    return 0;
}

int dxl_gs_challenge(const char *text, char out[7]) {
    const char *at = strstr(text, "\\secure\\");
    if (!at) return 0;
    at += 8;
    size_t n = strcspn(at, "\\");
    if (n < 6) return 0;
    memcpy(out, at, 6);
    out[6] = '\0';
    return 1;
}

int dxl_gs_list(const char *text, dxl_gs_ip_fn fn, void *ctx) {
    const char *p = text, *end = text + strlen(text), *k, *v;
    size_t kn, vn;
    int whole = 0;
    while (next_pair(&p, end, &k, &kn, &v, &vn))
        if (key_is(k, kn, "final")) whole = 1;
    if (!whole) return -1;
    int count = 0;
    p = text;
    while (next_pair(&p, end, &k, &kn, &v, &vn)) {
        if (key_is(k, kn, "final")) break;
        if (!key_is(k, kn, "ip")) continue;
        char at[96];
        snprintf(at, sizeof at, "%.*s", (int)vn, v);
        char *colon = strrchr(at, ':');
        if (!colon) continue;
        *colon = '\0';
        int port = atoi(colon + 1);
        if (port <= 0 || port > 65535 || !at[0]) continue;
        if (fn) fn(ctx, at, port);
        count++;
    }
    return count;
}

/* ---- a server's status -------------------------------------------------- */

void dxl_gs_status_init(dxl_gs_status *st) {
    memset(st, 0, sizeof *st);
    st->id = st->stale = -1;
}

void dxl_gs_status_free(dxl_gs_status *st) {
    for (int i = 0; i < st->cap; i++) free(st->packet[i]);
    free(st->packet);
    dxl_gs_status_init(st);
}

void dxl_gs_status_restart(dxl_gs_status *st) {
    int stale = st->id >= 0 ? st->id : st->stale;
    dxl_gs_status_free(st);
    st->stale = stale;
    st->asked_again = 1;
}

/* Whether answer id b is a later ask's than a's: IpServer.u's
 * UdpServerQuery numbers each query it is asked, whoever asks, from 1 to
 * 100 and begins again (ReceivedText), so a later ask's is a few on from an
 * earlier one's, past 100 to 1 too; one more than half the count on is an
 * earlier ask's. */
static int later(int a, int b) {
    int on = ((b - a) % 100 + 100) % 100;
    return on > 0 && on < 50;
}

int dxl_gs_status_add(dxl_gs_status *st, const char *text, size_t len) {
    const char *p = text, *end = text + len, *k, *v;
    size_t kn, vn;
    int number = -1, id = -1, final = 0;
    dxl_buf pairs;
    dxl_buf_init(&pairs);
    while (next_pair(&p, end, &k, &kn, &v, &vn)) {
        if (key_is(k, kn, "queryid")) {
            /* <id>.<n>: the answer's, and the packet's number, from 1. */
            char q[32];
            snprintf(q, sizeof q, "%.*s", (int)vn, v);
            char *dot = strchr(q, '.');
            number = dot ? atoi(dot + 1) : 1;
            if (dot) *dot = '\0';
            id = atoi(q);
        } else if (key_is(k, kn, "final")) {
            final = 1;
        } else if (kn) {
            dxl_buf_add(&pairs, "\\", 1);
            dxl_buf_add(&pairs, k, kn);
            dxl_buf_add(&pairs, "\\", 1);
            dxl_buf_add(&pairs, v, vn);
        }
    }
    /* Once asked again, an answer a later ask's than the one gathered is
     * the latest ask's, the one gathered an earlier ask's late answer, let
     * go for it; any other id is another answer's. */
    int newer = st->asked_again && st->id >= 0 && later(st->id, id);
    int other = id == st->stale || (st->id >= 0 && id != st->id && !newer);
    if (number < 1 || number > DXL_GS_PACKETS || other) {
        dxl_buf_free(&pairs);
        return -1;
    }
    if (newer) {
        int stale = st->stale;
        dxl_gs_status_free(st);
        st->stale = stale;
        st->asked_again = 1;
    }
    if (number <= st->cap && st->packet[number - 1]) {
        dxl_buf_free(&pairs);
        return -1;
    }
    if (number > st->cap) {
        int cap = st->cap ? st->cap : 8;
        while (cap < number) cap *= 2;
        if (cap > DXL_GS_PACKETS) cap = DXL_GS_PACKETS;
        st->packet = dxl_xrealloc(st->packet, sizeof *st->packet * (size_t)cap);
        for (int i = st->cap; i < cap; i++) st->packet[i] = NULL;
        st->cap = cap;
    }
    st->id = id;
    st->packet[number - 1] = pairs.data ? pairs.data : dxl_xstrdup("");
    st->count++;
    if (final) st->last = number;
    return newer;
}

int dxl_gs_status_whole(const dxl_gs_status *st) {
    if (!st->last || st->last > st->cap) return 0;
    for (int i = 0; i < st->last; i++)
        if (!st->packet[i]) return 0;
    return 1;
}

char *dxl_gs_status_text(const dxl_gs_status *st) {
    dxl_buf b;
    dxl_buf_init(&b);
    int n = st->last && st->last <= st->cap ? st->last : st->cap;
    for (int i = 0; i < n; i++)
        if (st->packet[i]) dxl_buf_puts(&b, st->packet[i]);
    return b.data ? b.data : dxl_xstrdup("");
}

#include "test.h"
#include "app/browser.h"
#include "platform/os.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32
typedef SOCKET sock;
#define NO_SOCK INVALID_SOCKET
#else
typedef int sock;
#define NO_SOCK (-1)
#endif

/* A socket on 127.0.0.1, any port: the socket, the port in *port. */
static sock local_socket(int type, int *port) {
    sock fd = socket(AF_INET, type, 0);
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len = sizeof sa;
    if (fd == NO_SOCK || bind(fd, (struct sockaddr *)&sa, sizeof sa) != 0 ||
        getsockname(fd, (struct sockaddr *)&sa, &len) != 0)
        return NO_SOCK;
    *port = ntohs(sa.sin_port);
    return fd;
}

/* The threads waiting on a socket woken, their waits failed: by its
 * shutdown here, then its close once they are joined; Windows wakes them
 * only by its close. */
static void wake_waits(sock fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    shutdown(fd, SHUT_RDWR);
#endif
}
static void close_woken(sock fd) {
#ifdef _WIN32
    (void)fd;
#else
    close(fd);
#endif
}
static void close_sock(sock fd) {
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
}

/* A master as 333networks is one: the challenge, the answer checked against
 * the original's for it, the list of the two servers. */
typedef struct {
    sock fd;
    int  server_port[3];
    char asked[512];     /* what the browser sent */
    int  wrong;          /* send an error, not the list */
} master;

static void *master_main(void *p) {
    master *m = p;
    sock c = accept(m->fd, NULL, NULL);
    if (c == NO_SOCK) return NULL;
    const char *hello = "\\basic\\\\secure\\abcdef";
    send(c, hello, (int)strlen(hello), 0);
    size_t len = 0;
    while (len < sizeof m->asked - 1 && !strstr(m->asked, "\\list\\")) {
        ssize_t n = recv(c, m->asked + len, (int)(sizeof m->asked - 1 - len), 0);
        if (n <= 0) break;
        len += (size_t)n;
        m->asked[len] = '\0';
    }
    char reply[256];
    if (m->wrong || !strstr(m->asked, "\\validate\\pMqFmqAG\\"))
        snprintf(reply, sizeof reply, "\\error\\Validation failed\\final\\");
    else
        snprintf(reply, sizeof reply, "\\ip\\127.0.0.1:%d\\ip\\127.0.0.1:%d\\ip\\127.0.0.1:%d\\final\\",
                 m->server_port[0], m->server_port[1], m->server_port[2]);
    /* In two parts: the browser reads on to \final\. */
    size_t half = strlen(reply) / 2;
    send(c, reply, (int)half, 0);
    dxl_os_sleep_ms(50);
    send(c, reply + half, (int)(strlen(reply) - half), 0);
    dxl_os_sleep_ms(100);
    close_sock(c);
    return NULL;
}

/* A server's query answerer, as Deus Ex's answers \status\: its basic,
 * info and rules, a packet each, then a player's, the last first; or
 * nothing at all; or its first answer with a packet lost. It answers each
 * asking, till its socket is shut. */
enum { ANSWERS, SILENT, LOSES_ONE };
typedef struct {
    sock fd;
    int mode;
    int asked;
} server;

static void *server_main(void *p) {
    server *s = p;
    char buf[256];
    for (;;) {
        struct sockaddr_in from;
        socklen_t len = sizeof from;
        ssize_t n = recvfrom(s->fd, buf, (int)(sizeof buf - 1), 0, (struct sockaddr *)&from, &len);
        if (n <= 0) break;
        buf[n] = '\0';
        if (strcmp(buf, "\\status\\") != 0) continue;
        s->asked++;
        if (s->mode == SILENT) continue;
        int id = 10 + s->asked;
        for (int k = 4; k >= 1; k--) {
            if (s->mode == LOSES_ONE && s->asked == 1 && k == 2) continue;
            char pkt[256];
            if (k == 1) snprintf(pkt, sizeof pkt, "\\gamename\\deusex\\gamever\\1100\\minnetver\\1100\\queryid\\%d.1", id);
            else if (k == 2) snprintf(pkt, sizeof pkt, "\\hostname\\Fake %s\\hostport\\7790\\numplayers\\1\\maxplayers\\8"
                                      "\\queryid\\%d.2", s->mode == LOSES_ONE ? "Two" : "One", id);
            else if (k == 3) snprintf(pkt, sizeof pkt, "\\TimeToWin\\15\\queryid\\%d.3", id);
            else snprintf(pkt, sizeof pkt, "\\player_0\\Cozmo\\frags_0\\15\\ping_0\\ 40\\final\\\\queryid\\%d.4", id);
            sendto(s->fd, pkt, (int)strlen(pkt), 0, (struct sockaddr *)&from, len);
        }
    }
    return NULL;
}

typedef struct {
    int  listed, lan, answered, ping;
    int  listed_port[4];
    int  answered_port[3];
    char status[3][512];
} seen;

static void on_listed(void *ctx, const char *address, int port, int lan) {
    (void)address;
    seen *w = ctx;
    if (w->listed < 4) w->listed_port[w->listed] = port;
    w->listed++;
    w->lan += lan;
}

static void on_answered(void *ctx, const char *address, int port, int ping, const char *status) {
    (void)address;
    seen *w = ctx;
    if (w->answered < 3) {
        w->answered_port[w->answered] = port;
        snprintf(w->status[w->answered], sizeof w->status[0], "%s", status);
    }
    w->answered++;
    w->ping = ping;
}

static server s[3];

static void run(int wrong, seen *w, int *rc, dxl_err *e) {
    master m;
    memset(&m, 0, sizeof m);
    m.wrong = wrong;
    int master_port = 0;
    s[0] = (server){ NO_SOCK, ANSWERS, 0 };
    s[1] = (server){ NO_SOCK, SILENT, 0 };
    s[2] = (server){ NO_SOCK, LOSES_ONE, 0 };
    m.fd = local_socket(SOCK_STREAM, &master_port);
    listen(m.fd, 1);
    for (int i = 0; i < 3; i++) s[i].fd = local_socket(SOCK_DGRAM, &m.server_port[i]);
    pthread_t mt, st[3];
    pthread_create(&mt, NULL, master_main, &m);
    for (int i = 0; i < 3; i++) pthread_create(&st[i], NULL, server_main, &s[i]);
    char at[64];
    snprintf(at, sizeof at, "127.0.0.1:%d", master_port);
    dxl_browse_fns fns = { on_listed, on_answered };
    memset(w, 0, sizeof *w);
    *rc = dxl_browse(at, NULL, NULL, &fns, w, NULL, e);
    pthread_join(mt, NULL);
    /* The servers wait on: their sockets shut, they end. */
    for (int i = 0; i < 3; i++) wake_waits(s[i].fd);
    for (int i = 0; i < 3; i++) {
        pthread_join(st[i], NULL);
        close_woken(s[i].fd);
    }
    close_sock(m.fd);
    if (!wrong) {
        CHECK(strstr(m.asked, "\\gamename\\deusex\\location\\0\\validate\\pMqFmqAG\\final\\") != NULL);
        CHECK(strstr(m.asked, "\\list\\\\gamename\\deusex\\final\\") != NULL);
        CHECK_INT(w->answered_port[0], m.server_port[0]);
        CHECK_INT(w->answered_port[1], m.server_port[2]);
    }
}

/* The master's list, as the game asks it; each server asked: the one that
 * answers as the game's do, its packets backwards, whole; the one whose
 * first answer lost a packet asked again and whole; the silent one asked
 * three times and given up. */
static void test_browse(void) {
    seen w;
    int rc;
    dxl_err e;
    run(0, &w, &rc, &e);
    CHECK_INT(rc, 0);
    CHECK_INT(w.listed, 3);
    CHECK_INT(w.answered, 2);
    CHECK_INT(s[0].asked, 1);
    CHECK_INT(s[1].asked, 3);
    CHECK_INT(s[2].asked, 2);
    CHECK(w.ping >= 0 && w.ping < 1000);
    CHECK_STR(w.status[0], "\\gamename\\deusex\\gamever\\1100\\minnetver\\1100\\hostname\\Fake One\\hostport\\7790"
                           "\\numplayers\\1\\maxplayers\\8\\TimeToWin\\15\\player_0\\Cozmo\\frags_0\\15\\ping_0\\ 40");
    CHECK(strstr(w.status[1], "\\hostname\\Fake Two\\") != NULL);
}

/* A master's error, said; no master there, said at once, the connect's
 * refusal, not a wait for an answer (Windows 10 1903 and 1909's WSAPoll). */
static void test_master_errors(void) {
    seen w;
    int rc;
    dxl_err e;
    run(1, &w, &rc, &e);
    CHECK_INT(rc, -1);
    CHECK(strstr(e.msg, "Validation failed") != NULL);
    CHECK_INT(w.listed, 0);
    int port = 0;
    sock fd = local_socket(SOCK_STREAM, &port);
    close_sock(fd);   /* nothing listens there now */
    char at[64];
    snprintf(at, sizeof at, "127.0.0.1:%d", port);
    dxl_browse_fns fns = { on_listed, on_answered };
    long long t0 = dxl_os_ms();
    CHECK_INT(dxl_browse(at, NULL, NULL, &fns, &w, NULL, &e), -1);
    CHECK(dxl_os_ms() - t0 < 2500);
    CHECK(e.msg[0] != '\0');
    CHECK(strstr(e.msg, "no answer") == NULL);
    CHECK_INT(dxl_browse("host:0", NULL, NULL, &fns, &w, NULL, &e), -1);
}

/* A beacon as a server's UdpBeacon is one: REPORTQUERY answered with the
 * product and its query port; anything else, nothing. */
typedef struct {
    sock fd;
    int asked;
    char answer[64];
} beacon;

static void *beacon_main(void *p) {
    beacon *b = p;
    char buf[64];
    for (;;) {
        struct sockaddr_in from;
        socklen_t len = sizeof from;
        ssize_t n = recvfrom(b->fd, buf, (int)(sizeof buf - 1), 0, (struct sockaddr *)&from, &len);
        if (n <= 0) break;
        buf[n] = '\0';
        if (strcmp(buf, "REPORTQUERY") != 0) continue;
        b->asked++;
        sendto(b->fd, b->answer, (int)strlen(b->answer), 0, (struct sockaddr *)&from, len);
    }
    return NULL;
}

/* A master that says nothing for a second and a half, then refuses: what
 * the beacons answered meanwhile waits on the browser's socket. */
static void *slow_master_main(void *p) {
    master *m = p;
    sock c = accept(m->fd, NULL, NULL);
    if (c == NO_SOCK) return NULL;
    dxl_os_sleep_ms(1500);
    const char *no = "\\error\\Try again later\\final\\";
    send(c, no, (int)strlen(no), 0);
    dxl_os_sleep_ms(100);
    close_sock(c);
    return NULL;
}

static long ms_now(void) {
#ifdef _WIN32
    return (long)GetTickCount64();
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
#endif
}

/* The local network as the game's Join LAN screen asks it, at DXL_LAN's
 * place: REPORTQUERY to the ten ports from its, each beacon's server asked
 * for its status and handed over as LAN; another game's beacon passed
 * over; a second given to the beacons, though the one server answers at
 * once; DXL_LAN=none, no beacon asked. */
static void test_lan(void) {
    beacon b[2];
    int beacon_port[2];
    for (int i = 0; i < 2; i++) {
        memset(&b[i], 0, sizeof b[i]);
        b[i].fd = local_socket(SOCK_DGRAM, &beacon_port[i]);
    }
    int query_port = 0;
    s[0] = (server){ local_socket(SOCK_DGRAM, &query_port), ANSWERS, 0 };
    snprintf(b[0].answer, sizeof b[0].answer, "DeusEx %d", query_port);
    snprintf(b[1].answer, sizeof b[1].answer, "UnrealTournament %d", query_port + 1);
    pthread_t bt[2], st;
    for (int i = 0; i < 2; i++) pthread_create(&bt[i], NULL, beacon_main, &b[i]);
    pthread_create(&st, NULL, server_main, &s[0]);

    /* The first beacon on the third of the ten ports asked. */
    char env[64];
    snprintf(env, sizeof env, "127.0.0.1:%d", beacon_port[0] - 2);
    dxl_env_set("DXL_LAN", env);
    dxl_browse_lan lan;
    dxl_browse_lan_init(&lan, 0, NULL);
    CHECK_INT(lan.beacon_port, beacon_port[0] - 2);
    CHECK_STR(lan.product, "DeusEx");
    CHECK_STR(lan.to, "127.0.0.1");
    seen w;
    memset(&w, 0, sizeof w);
    dxl_browse_fns fns = { on_listed, on_answered };
    dxl_err e;
    long t0 = ms_now();
    CHECK_INT(dxl_browse(NULL, NULL, &lan, &fns, &w, NULL, &e), 0);
    CHECK(ms_now() - t0 >= 1000);
    CHECK_INT(b[0].asked, 1);
    CHECK_INT(w.listed, 1);
    CHECK_INT(w.lan, 1);
    CHECK_INT(w.listed_port[0], query_port);
    CHECK_INT(w.answered, 1);
    CHECK(strstr(w.status[0], "\\hostname\\Fake One\\") != NULL);

    /* The other game's beacon, at its own place: its server never asked. */
    snprintf(env, sizeof env, "127.0.0.1:%d", beacon_port[1]);
    dxl_env_set("DXL_LAN", env);
    dxl_browse_lan_init(&lan, 0, NULL);
    memset(&w, 0, sizeof w);
    CHECK_INT(dxl_browse(NULL, NULL, &lan, &fns, &w, NULL, &e), 0);
    CHECK_INT(b[1].asked, 1);
    CHECK_INT(w.listed, 0);

    dxl_env_set("DXL_LAN", "none");
    dxl_browse_lan_init(&lan, 0, NULL);
    CHECK_INT(lan.none, 1);
    memset(&w, 0, sizeof w);
    t0 = ms_now();
    CHECK_INT(dxl_browse(NULL, NULL, &lan, &fns, &w, NULL, &e), 0);
    CHECK(ms_now() - t0 < 1000);
    CHECK_INT(b[0].asked, 1);

    /* A master slower than the beacons' second, which lists nothing: what
     * they answered meanwhile read, and their server asked, all the same. */
    snprintf(env, sizeof env, "127.0.0.1:%d", beacon_port[0]);
    dxl_env_set("DXL_LAN", env);
    dxl_browse_lan_init(&lan, 0, NULL);
    master m;
    memset(&m, 0, sizeof m);
    int master_port = 0;
    m.fd = local_socket(SOCK_STREAM, &master_port);
    listen(m.fd, 1);
    pthread_t mt;
    pthread_create(&mt, NULL, slow_master_main, &m);
    char at[64];
    snprintf(at, sizeof at, "127.0.0.1:%d", master_port);
    memset(&w, 0, sizeof w);
    CHECK_INT(dxl_browse(at, NULL, &lan, &fns, &w, NULL, &e), -1);
    CHECK(strstr(e.msg, "Try again later") != NULL);
    pthread_join(mt, NULL);
    close_sock(m.fd);
    CHECK_INT(b[0].asked, 2);
    CHECK_INT(w.listed, 1);
    CHECK_INT(w.lan, 1);
    CHECK_INT(w.answered, 1);
    dxl_env_set("DXL_LAN", NULL);

    for (int i = 0; i < 2; i++) wake_waits(b[i].fd);
    wake_waits(s[0].fd);
    for (int i = 0; i < 2; i++) {
        pthread_join(bt[i], NULL);
        close_woken(b[i].fd);
    }
    pthread_join(st, NULL);
    close_woken(s[0].fd);
}

TEST_MAIN_BEGIN
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    RUN(test_browse);
    RUN(test_master_errors);
    RUN(test_lan);
TEST_MAIN_END

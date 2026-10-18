#include "test.h"
#include "app/browser.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

/* A socket on 127.0.0.1, any port: its fd, the port in *port. */
static int local_socket(int type, int *port) {
    int fd = socket(AF_INET, type, 0);
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len = sizeof sa;
    if (fd < 0 || bind(fd, (struct sockaddr *)&sa, sizeof sa) != 0 || getsockname(fd, (struct sockaddr *)&sa, &len) != 0)
        return -1;
    *port = ntohs(sa.sin_port);
    return fd;
}

/* A master as 333networks is one: the challenge, the answer checked against
 * the original's for it, the list of the two servers. */
typedef struct {
    int  fd;
    int  server_port[3];
    char asked[512];     /* what the browser sent */
    int  wrong;          /* send an error, not the list */
} master;

static void *master_main(void *p) {
    master *m = p;
    int c = accept(m->fd, NULL, NULL);
    if (c < 0) return NULL;
    const char *hello = "\\basic\\\\secure\\abcdef";
    send(c, hello, strlen(hello), 0);
    size_t len = 0;
    while (len < sizeof m->asked - 1 && !strstr(m->asked, "\\list\\")) {
        ssize_t n = recv(c, m->asked + len, sizeof m->asked - 1 - len, 0);
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
    send(c, reply, half, 0);
    usleep(50000);
    send(c, reply + half, strlen(reply) - half, 0);
    usleep(100000);
    close(c);
    return NULL;
}

/* A server's query answerer, as Deus Ex's answers \status\: its basic,
 * info and rules, a packet each, then a player's, the last first; or
 * nothing at all; or its first answer with a packet lost. It answers each
 * asking, till its socket is shut. */
enum { ANSWERS, SILENT, LOSES_ONE };
typedef struct {
    int fd;
    int mode;
    int asked;
} server;

static void *server_main(void *p) {
    server *s = p;
    char buf[256];
    for (;;) {
        struct sockaddr_in from;
        socklen_t len = sizeof from;
        ssize_t n = recvfrom(s->fd, buf, sizeof buf - 1, 0, (struct sockaddr *)&from, &len);
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
            sendto(s->fd, pkt, strlen(pkt), 0, (struct sockaddr *)&from, len);
        }
    }
    return NULL;
}

typedef struct {
    int  listed, answered, ping;
    int  answered_port[3];
    char status[3][512];
} seen;

static void on_listed(void *ctx, const char *address, int port) {
    (void)address;
    (void)port;
    ((seen *)ctx)->listed++;
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
    s[0] = (server){ -1, ANSWERS, 0 };
    s[1] = (server){ -1, SILENT, 0 };
    s[2] = (server){ -1, LOSES_ONE, 0 };
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
    *rc = dxl_browse(at, NULL, &fns, w, NULL, e);
    pthread_join(mt, NULL);
    /* The servers wait on: their sockets shut, they end. */
    for (int i = 0; i < 3; i++) shutdown(s[i].fd, SHUT_RDWR);
    for (int i = 0; i < 3; i++) {
        pthread_join(st[i], NULL);
        close(s[i].fd);
    }
    close(m.fd);
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

/* A master's error, said; no master there, said. */
static void test_master_errors(void) {
    seen w;
    int rc;
    dxl_err e;
    run(1, &w, &rc, &e);
    CHECK_INT(rc, -1);
    CHECK(strstr(e.msg, "Validation failed") != NULL);
    CHECK_INT(w.listed, 0);
    int port = 0, fd = local_socket(SOCK_STREAM, &port);
    close(fd);   /* nothing listens there now */
    char at[64];
    snprintf(at, sizeof at, "127.0.0.1:%d", port);
    dxl_browse_fns fns = { on_listed, on_answered };
    CHECK_INT(dxl_browse(at, NULL, &fns, &w, NULL, &e), -1);
    CHECK(e.msg[0] != '\0');
    CHECK_INT(dxl_browse("host:0", NULL, &fns, &w, NULL, &e), -1);
}

TEST_MAIN_BEGIN
    RUN(test_browse);
    RUN(test_master_errors);
TEST_MAIN_END

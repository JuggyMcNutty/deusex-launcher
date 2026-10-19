#define _GNU_SOURCE
#include "platform/net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* A lookup on its thread: the asker's and the thread's, freed by the last
 * to let go, so one given up on ends on its own. */
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  ended;
    int             refs, done, rc;
    char            host[256], address[INET_ADDRSTRLEN], error[200];
} lookup;

static void lookup_unref(lookup *l) {
    pthread_mutex_lock(&l->lock);
    int left = --l->refs;
    pthread_mutex_unlock(&l->lock);
    if (left) return;
    pthread_cond_destroy(&l->ended);
    pthread_mutex_destroy(&l->lock);
    free(l);
}

/* The resolver asked, into l. */
static void look_up(lookup *l) {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(l->host, NULL, &hints, &res);
    pthread_mutex_lock(&l->lock);
    if (rc != 0 || !res) {
        snprintf(l->error, sizeof l->error, "%s", rc ? gai_strerror(rc) : "no address");
        l->rc = -1;
    } else {
        inet_ntop(AF_INET, &((const struct sockaddr_in *)res->ai_addr)->sin_addr, l->address, sizeof l->address);
        l->rc = 0;
    }
    l->done = 1;
    pthread_cond_broadcast(&l->ended);
    pthread_mutex_unlock(&l->lock);
    if (res) freeaddrinfo(res);
}

static void *lookup_main(void *p) {
    look_up(p);
    lookup_unref(p);
    return NULL;
}

static long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

int dxl_net_resolve(const char *host, char *address, size_t size, int timeout_ms, const atomic_int *stop,
                    dxl_err *err) {
    struct in_addr a;
    if (inet_pton(AF_INET, host, &a) == 1) {
        snprintf(address, size, "%s", host);
        return 0;
    }
    lookup *l = calloc(1, sizeof *l);
    if (!l) {
        dxl_err_set(err, "%s: no memory to look it up", host);
        return -1;
    }
    pthread_mutex_init(&l->lock, NULL);
    pthread_condattr_t ca;
    pthread_condattr_init(&ca);
    pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
    pthread_cond_init(&l->ended, &ca);
    pthread_condattr_destroy(&ca);
    snprintf(l->host, sizeof l->host, "%s", host);
    l->refs = 2;
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    pthread_t t;
    if (pthread_create(&t, &at, lookup_main, l) != 0) {
        /* No thread: the lookup here, as long as it takes. */
        l->refs = 1;
        look_up(l);
    }
    pthread_attr_destroy(&at);
    long until = now_ms() + timeout_ms;
    pthread_mutex_lock(&l->lock);
    while (!l->done && !(stop && atomic_load(stop)) && now_ms() < until) {
        struct timespec w;
        clock_gettime(CLOCK_MONOTONIC, &w);
        w.tv_nsec += 100 * 1000000L;
        if (w.tv_nsec >= 1000000000L) {
            w.tv_sec++;
            w.tv_nsec -= 1000000000L;
        }
        pthread_cond_timedwait(&l->ended, &l->lock, &w);
    }
    int rc = -1;
    if (l->done && l->rc == 0) {
        snprintf(address, size, "%s", l->address);
        rc = 0;
    } else {
        dxl_err_set(err, "%s: %s", host, l->done ? l->error : stop && atomic_load(stop) ? "not looked up" : "no answer in time");
    }
    pthread_mutex_unlock(&l->lock);
    lookup_unref(l);
    return rc;
}

static int to_sockaddr(const char *address, int port, struct sockaddr_in *sa) {
    memset(sa, 0, sizeof *sa);
    sa->sin_family = AF_INET;
    sa->sin_port = htons((uint16_t)port);
    return inet_pton(AF_INET, address, &sa->sin_addr) == 1 ? 0 : -1;
}

/* Waits on fd for what (POLLIN, POLLOUT): 1 ready, 0 timed out, -1. */
static int wait_for(int fd, short what, int timeout_ms) {
    struct pollfd p = { fd, what, 0 };
    for (;;) {
        int rc = poll(&p, 1, timeout_ms);
        if (rc < 0 && errno == EINTR) continue;
        return rc < 0 ? -1 : rc > 0;
    }
}

int dxl_tcp_connect(const char *address, int port, int timeout_ms, dxl_err *err) {
    struct sockaddr_in sa;
    if (to_sockaddr(address, port, &sa) != 0) {
        dxl_err_set(err, "%s is no IPv4 address", address);
        return -1;
    }
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        dxl_err_set(err, "no socket: %s", strerror(errno));
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&sa, sizeof sa) != 0 && errno != EINPROGRESS) {
        dxl_err_set(err, "%s:%d: %s", address, port, strerror(errno));
        close(fd);
        return -1;
    }
    int ready = wait_for(fd, POLLOUT, timeout_ms);
    int so = 0;
    socklen_t len = sizeof so;
    if (ready <= 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &so, &len) != 0 || so != 0) {
        dxl_err_set(err, "%s:%d: %s", address, port, ready == 0 ? "no answer" : strerror(so ? so : errno));
        close(fd);
        return -1;
    }
    return fd;
}

int dxl_tcp_send(int fd, const char *data, size_t len, int timeout_ms) {
    while (len) {
        if (wait_for(fd, POLLOUT, timeout_ms) <= 0) return -1;
        ssize_t n = send(fd, data, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            return -1;
        }
        data += n;
        len -= (size_t)n;
    }
    return 0;
}

long dxl_tcp_recv(int fd, char *buf, size_t size, int timeout_ms) {
    int ready = wait_for(fd, POLLIN, timeout_ms);
    if (ready < 0) return -1;
    if (ready == 0) return -2;
    for (;;) {
        ssize_t n = recv(fd, buf, size, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return errno == EAGAIN ? -2 : -1;
        return (long)n;
    }
}

int dxl_udp_open(dxl_err *err) {
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) dxl_err_set(err, "no socket: %s", strerror(errno));
    return fd;
}

int dxl_udp_broadcast(int fd) {
    int on = 1;
    return setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof on) == 0 ? 0 : -1;
}

int dxl_net_broadcasts(char (*out)[64], int max) {
    int n = 0;
    if (n < max) snprintf(out[n++], 64, "255.255.255.255");
    struct ifaddrs *all = NULL;
    if (getifaddrs(&all) != 0) return n;
    for (struct ifaddrs *i = all; i && n < max; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || !(i->ifa_flags & IFF_UP) ||
            !(i->ifa_flags & IFF_BROADCAST) || !i->ifa_broadaddr)
            continue;
        char a[64];
        if (!inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_broadaddr)->sin_addr, a, sizeof a)) continue;
        int seen = 0;
        for (int k = 0; k < n; k++) seen |= !strcmp(out[k], a);
        if (!seen) snprintf(out[n++], 64, "%s", a);
    }
    freeifaddrs(all);
    return n;
}

int dxl_udp_send(int fd, const char *address, int port, const char *data, size_t len) {
    struct sockaddr_in sa;
    if (to_sockaddr(address, port, &sa) != 0) return -1;
    for (;;) {
        ssize_t n = sendto(fd, data, len, 0, (struct sockaddr *)&sa, sizeof sa);
        if (n < 0 && errno == EINTR) continue;
        return n == (ssize_t)len ? 0 : -1;
    }
}

long dxl_udp_recv(int fd, char *buf, size_t size, int timeout_ms, char *from, size_t from_size, int *from_port) {
    int ready = wait_for(fd, POLLIN, timeout_ms);
    if (ready < 0) return -1;
    if (ready == 0) return -2;
    struct sockaddr_in sa;
    socklen_t len = sizeof sa;
    for (;;) {
        ssize_t n = recvfrom(fd, buf, size, 0, (struct sockaddr *)&sa, &len);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return errno == EAGAIN ? -2 : -1;
        if (from) inet_ntop(AF_INET, &sa.sin_addr, from, (socklen_t)from_size);
        if (from_port) *from_port = ntohs(sa.sin_port);
        return (long)n;
    }
}

void dxl_net_close(int fd) {
    if (fd >= 0) close(fd);
}

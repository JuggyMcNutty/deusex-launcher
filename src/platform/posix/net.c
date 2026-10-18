#define _GNU_SOURCE
#include "platform/net.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int dxl_net_resolve(const char *host, char *address, size_t size, dxl_err *err) {
    struct in_addr a;
    if (inet_pton(AF_INET, host, &a) == 1) {
        snprintf(address, size, "%s", host);
        return 0;
    }
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(host, NULL, &hints, &res);
    if (rc != 0 || !res) {
        dxl_err_set(err, "%s: %s", host, rc ? gai_strerror(rc) : "no address");
        return -1;
    }
    const struct sockaddr_in *in = (const struct sockaddr_in *)res->ai_addr;
    inet_ntop(AF_INET, &in->sin_addr, address, (socklen_t)size);
    freeaddrinfo(res);
    return 0;
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

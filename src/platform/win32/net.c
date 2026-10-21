/* The network on Windows (platform/net.h): Winsock 2, started on first
 * use, as POSIX's sockets are used in platform/posix/net.c -- a lookup on a
 * thread of its own, sockets that never block, each wait a WSAPoll with
 * its time, and the local network's broadcast addresses from each adapter's
 * IPv4 address and prefix (GetAdaptersAddresses).
 */
#include "platform/net.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static INIT_ONCE started = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK start_winsock(PINIT_ONCE once, PVOID p, PVOID *ctx) {
    (void)once; (void)p; (void)ctx;
    WSADATA d;
    return WSAStartup(MAKEWORD(2, 2), &d) == 0;
}

static int winsock(void) { return InitOnceExecuteOnce(&started, start_winsock, NULL, NULL) ? 0 : -1; }

/* A lookup on its thread: the asker's and the thread's, freed by the last
 * to let go, so one given up on ends on its own. */
typedef struct {
    SRWLOCK            lock;
    CONDITION_VARIABLE ended;
    int                refs, done, rc;
    char               host[256], address[INET_ADDRSTRLEN], error[200];
} lookup;

static void lookup_unref(lookup *l) {
    AcquireSRWLockExclusive(&l->lock);
    int left = --l->refs;
    ReleaseSRWLockExclusive(&l->lock);
    if (!left) free(l);
}

/* The resolver asked, into l. */
static void look_up(lookup *l) {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(l->host, NULL, &hints, &res);
    AcquireSRWLockExclusive(&l->lock);
    if (rc != 0 || !res) {
        snprintf(l->error, sizeof l->error, "%s", rc ? gai_strerrorA(rc) : "no address");
        l->rc = -1;
    } else {
        inet_ntop(AF_INET, &((const struct sockaddr_in *)res->ai_addr)->sin_addr, l->address, sizeof l->address);
        l->rc = 0;
    }
    l->done = 1;
    WakeAllConditionVariable(&l->ended);
    ReleaseSRWLockExclusive(&l->lock);
    if (res) freeaddrinfo(res);
}

static DWORD WINAPI lookup_main(LPVOID p) {
    look_up(p);
    lookup_unref(p);
    return 0;
}

int dxl_net_resolve(const char *host, char *address, size_t size, int timeout_ms, const atomic_int *stop,
                    dxl_err *err) {
    if (winsock() != 0) {
        dxl_err_set(err, "%s: no Winsock", host);
        return -1;
    }
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
    InitializeSRWLock(&l->lock);
    InitializeConditionVariable(&l->ended);
    snprintf(l->host, sizeof l->host, "%s", host);
    l->refs = 2;
    HANDLE t = CreateThread(NULL, 0, lookup_main, l, 0, NULL);
    if (t) {
        CloseHandle(t);
    } else {
        /* No thread: the lookup here, as long as it takes. */
        l->refs = 1;
        look_up(l);
    }
    ULONGLONG until = GetTickCount64() + (ULONGLONG)(timeout_ms > 0 ? timeout_ms : 0);
    AcquireSRWLockExclusive(&l->lock);
    while (!l->done && !(stop && atomic_load(stop)) && GetTickCount64() < until)
        SleepConditionVariableSRW(&l->ended, &l->lock, 100, 0);
    int rc = -1;
    if (l->done && l->rc == 0) {
        snprintf(address, size, "%s", l->address);
        rc = 0;
    } else {
        dxl_err_set(err, "%s: %s", host, l->done ? l->error : stop && atomic_load(stop) ? "not looked up" : "no answer in time");
    }
    ReleaseSRWLockExclusive(&l->lock);
    lookup_unref(l);
    return rc;
}

static int to_sockaddr(const char *address, int port, struct sockaddr_in *sa) {
    memset(sa, 0, sizeof *sa);
    sa->sin_family = AF_INET;
    sa->sin_port = htons((u_short)port);
    return inet_pton(AF_INET, address, &sa->sin_addr) == 1 ? 0 : -1;
}

/* Waits on s for what (POLLRDNORM, POLLWRNORM): 1 ready, 0 timed out, -1. */
static int wait_for(SOCKET s, short what, int timeout_ms) {
    WSAPOLLFD p = { s, what, 0 };
    int rc = WSAPoll(&p, 1, timeout_ms);
    return rc < 0 ? -1 : rc > 0;
}

/* A socket of type, not inherited by the programs the launcher starts, not
 * blocking. */
static SOCKET open_socket(int type, int protocol, dxl_err *err) {
    if (winsock() != 0) {
        dxl_err_set(err, "no Winsock");
        return INVALID_SOCKET;
    }
    SOCKET s = WSASocketW(AF_INET, type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
    if (s == INVALID_SOCKET) {
        dxl_err_set(err, "no socket: error %d", WSAGetLastError());
        return INVALID_SOCKET;
    }
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
    return s;
}

dxl_socket dxl_tcp_connect(const char *address, int port, int timeout_ms, dxl_err *err) {
    struct sockaddr_in sa;
    if (to_sockaddr(address, port, &sa) != 0) {
        dxl_err_set(err, "%s is no IPv4 address", address);
        return DXL_SOCKET_NONE;
    }
    SOCKET s = open_socket(SOCK_STREAM, IPPROTO_TCP, err);
    if (s == INVALID_SOCKET) return DXL_SOCKET_NONE;
    if (connect(s, (struct sockaddr *)&sa, sizeof sa) != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
        dxl_err_set(err, "%s:%d: error %d", address, port, WSAGetLastError());
        closesocket(s);
        return DXL_SOCKET_NONE;
    }
    int ready = wait_for(s, POLLWRNORM, timeout_ms);
    int so = 0, len = sizeof so;
    if (ready <= 0 || getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&so, &len) != 0 || so != 0) {
        if (ready == 0) dxl_err_set(err, "%s:%d: no answer", address, port);
        else dxl_err_set(err, "%s:%d: error %d", address, port, so ? so : WSAGetLastError());
        closesocket(s);
        return DXL_SOCKET_NONE;
    }
    return (dxl_socket)s;
}

int dxl_tcp_send(dxl_socket s, const char *data, size_t len, int timeout_ms) {
    SOCKET fd = (SOCKET)s;
    while (len) {
        if (wait_for(fd, POLLWRNORM, timeout_ms) <= 0) return -1;
        int n = send(fd, data, len > 0x10000 ? 0x10000 : (int)len, 0);
        if (n < 0) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
            return -1;
        }
        data += n;
        len -= (size_t)n;
    }
    return 0;
}

long dxl_tcp_recv(dxl_socket s, char *buf, size_t size, int timeout_ms) {
    SOCKET fd = (SOCKET)s;
    int ready = wait_for(fd, POLLRDNORM, timeout_ms);
    if (ready < 0) return -1;
    if (ready == 0) return -2;
    int n = recv(fd, buf, size > 0x10000 ? 0x10000 : (int)size, 0);
    if (n < 0) return WSAGetLastError() == WSAEWOULDBLOCK ? -2 : -1;
    return (long)n;
}

dxl_socket dxl_udp_open(dxl_err *err) {
    SOCKET s = open_socket(SOCK_DGRAM, IPPROTO_UDP, err);
    return s == INVALID_SOCKET ? DXL_SOCKET_NONE : (dxl_socket)s;
}

int dxl_udp_broadcast(dxl_socket s) {
    BOOL on = TRUE;
    return setsockopt((SOCKET)s, SOL_SOCKET, SO_BROADCAST, (const char *)&on, sizeof on) == 0 ? 0 : -1;
}

int dxl_net_broadcasts(char (*out)[64], int max) {
    int n = 0;
    if (n < max) snprintf(out[n++], 64, "255.255.255.255");
    ULONG size = 16 * 1024;
    IP_ADAPTER_ADDRESSES *all = NULL;
    ULONG rc = ERROR_BUFFER_OVERFLOW;
    for (int tries = 0; tries < 3 && rc == ERROR_BUFFER_OVERFLOW; tries++) {
        free(all);
        all = malloc(size);
        if (!all) return n;
        rc = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                  NULL, all, &size);
    }
    for (IP_ADAPTER_ADDRESSES *a = rc == NO_ERROR ? all : NULL; a && n < max; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (IP_ADAPTER_UNICAST_ADDRESS *u = a->FirstUnicastAddress; u && n < max; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family != AF_INET || u->OnLinkPrefixLength > 30) continue;
            struct in_addr ip = ((struct sockaddr_in *)u->Address.lpSockaddr)->sin_addr;
            ULONG mask = u->OnLinkPrefixLength ? htonl(~0UL << (32 - u->OnLinkPrefixLength)) : 0;
            struct in_addr b;
            b.s_addr = (ip.s_addr & mask) | ~mask;
            char text[64];
            if (!inet_ntop(AF_INET, &b, text, sizeof text)) continue;
            int seen = 0;
            for (int k = 0; k < n; k++) seen |= !strcmp(out[k], text);
            if (!seen) snprintf(out[n++], 64, "%s", text);
        }
    }
    free(all);
    return n;
}

int dxl_udp_send(dxl_socket s, const char *address, int port, const char *data, size_t len) {
    struct sockaddr_in sa;
    if (to_sockaddr(address, port, &sa) != 0) return -1;
    int n = sendto((SOCKET)s, data, (int)len, 0, (struct sockaddr *)&sa, sizeof sa);
    return n == (int)len ? 0 : -1;
}

long dxl_udp_recv(dxl_socket s, char *buf, size_t size, int timeout_ms, char *from, size_t from_size,
                  int *from_port) {
    SOCKET fd = (SOCKET)s;
    int ready = wait_for(fd, POLLRDNORM, timeout_ms);
    if (ready < 0) return -1;
    if (ready == 0) return -2;
    struct sockaddr_in sa;
    int len = sizeof sa;
    int n = recvfrom(fd, buf, size > 0x10000 ? 0x10000 : (int)size, 0, (struct sockaddr *)&sa, &len);
    if (n < 0) return WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAECONNRESET ? -2 : -1;
    if (from) inet_ntop(AF_INET, &sa.sin_addr, from, from_size);
    if (from_port) *from_port = ntohs(sa.sin_port);
    return (long)n;
}

void dxl_net_close(dxl_socket s) {
    if (s != DXL_SOCKET_NONE) closesocket((SOCKET)s);
}

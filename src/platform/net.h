/* The network as the server browser needs it (app/browser.h): a name
 * looked up, a TCP connection and UDP datagrams, each wait bounded. IPv4
 * alone: the game's master servers and servers are.
 *
 * Every call takes milliseconds to wait at most; none waits longer, so a
 * worker asked to stop stops within one. A socket is an fd, -1 for none.
 */
#ifndef DXL_NET_H
#define DXL_NET_H

#include "core/common.h"

#include <stdatomic.h>

/* host -- a name or a.b.c.d -- as a.b.c.d, within timeout_ms, or until
 * *stop is set (stop may be NULL). A lookup the resolver is still on is
 * left to end on a thread of its own. */
int  dxl_net_resolve(const char *host, char *address, size_t size, int timeout_ms, const atomic_int *stop,
                     dxl_err *err);

/* A TCP connection to address:port, made within timeout_ms; -1 and why. */
int  dxl_tcp_connect(const char *address, int port, int timeout_ms, dxl_err *err);
/* All of data sent, each part within timeout_ms; -1 when it is not. */
int  dxl_tcp_send(int fd, const char *data, size_t len, int timeout_ms);
/* What has come, up to size, waiting timeout_ms for any: the bytes read, 0
 * when the other side has closed, -1 on an error, -2 when nothing came. */
long dxl_tcp_recv(int fd, char *buf, size_t size, int timeout_ms);

/* A UDP socket on any port. */
int  dxl_udp_open(dxl_err *err);
/* It may send to a broadcast address too (SO_BROADCAST); -1 when not. */
int  dxl_udp_broadcast(int fd);
/* The local network's broadcast addresses, a.b.c.d each into out, at most
 * max: 255.255.255.255, then each interface's that is up (getifaddrs),
 * each once. How many. */
int  dxl_net_broadcasts(char (*out)[64], int max);
/* A datagram to address:port; -1 when it did not go. */
int  dxl_udp_send(int fd, const char *address, int port, const char *data, size_t len);
/* A datagram, waiting timeout_ms for one: its length, its sender's address
 * and port; -2 when none came, -1 on an error. */
long dxl_udp_recv(int fd, char *buf, size_t size, int timeout_ms, char *from, size_t from_size, int *from_port);

void dxl_net_close(int fd);

#endif

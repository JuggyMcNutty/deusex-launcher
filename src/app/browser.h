/* The server browser's look (screens/servers.c, dxl-cli --servers): the
 * profile's master server asked for Deus Ex's servers as the game asks it
 * (core/gamespy.h), then each server for its status -- all at once, on one
 * UDP socket, one whose answer is not whole a second after it was asked
 * asked again, three times at most -- each answer handed over as it comes.
 */
#ifndef DXL_BROWSER_H
#define DXL_BROWSER_H

#include "core/common.h"

#include <stdatomic.h>

#define DXL_MASTER_PORT  28900
#define DXL_BROWSE_MAX   256      /* the servers asked, at most */

typedef struct {
    /* Each server the master lists, by its query port. */
    void (*listed)(void *ctx, const char *address, int query_port);
    /* A server's whole answer, and its ping: its first packet's time. */
    void (*answered)(void *ctx, const char *address, int query_port, int ping_ms, const char *status);
} dxl_browse_fns;

/* master: a host, or host:port (28900 when none); also: more servers to
 * ask with its, "host:query port" each, NULL-terminated (may be NULL) --
 * the favourites, asked whether the master lists them or not. 0 once each
 * server has answered or had its time; -1 and why when the master gave no
 * list, the others asked all the same. Stops early once *stop is set
 * (stop may be NULL). */
int dxl_browse(const char *master, const char *const *also, const dxl_browse_fns *fns, void *ctx, const atomic_int *stop,
               dxl_err *err);

#endif

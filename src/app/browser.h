/* The server browser's look (screens/servers.c, dxl-cli --servers and
 * --lan): the local network's servers asked for as the game's Join LAN
 * screen asks (DeusEx.DeusExLocalLink: REPORTQUERY to ten beacon ports, a
 * beacon's answer its server's query port), and the profile's master server
 * for Deus Ex's servers as the game asks it (core/gamespy.h); then each
 * server for its status -- all at once, on one UDP socket, one whose answer
 * is not whole a second after it was asked asked again, three times at most
 * -- each answer handed over as it comes.
 */
#ifndef DXL_BROWSER_H
#define DXL_BROWSER_H

#include "core/common.h"
#include "core/gamespy.h"

#include <stdatomic.h>

#define DXL_BROWSE_MAX   256      /* the servers asked, at most */

/* The local network's look, the profile's [DeusEx.DeusExLocalLink]. */
typedef struct {
    int  beacon_port;        /* ServerBeaconPort: the first of ten asked */
    char product[64];        /* BeaconProduct: a beacon's answer begins with it */
    char to[64];             /* where REPORTQUERY goes: "", the network's broadcast addresses */
    int  none;               /* no look on the local network */
} dxl_browse_lan;

#define DXL_BEACON_PORT    8777
#define DXL_BEACON_PRODUCT "DeusEx"

/* The game's defaults, then DXL_LAN's: "none", or host[:port], the one
 * place asked (a fake beacon's, for the tests). */
void dxl_browse_lan_init(dxl_browse_lan *lan, int beacon_port, const char *product);

typedef struct {
    /* Each server the master lists, by its query port; lan, one a beacon
     * on the local network answered for. */
    void (*listed)(void *ctx, const char *address, int query_port, int lan);
    /* A server's whole answer, and its ping: its first packet's time. */
    void (*answered)(void *ctx, const char *address, int query_port, int ping_ms, const char *status);
} dxl_browse_fns;

/* master: a host, or host:port (28900 when none), or NULL or "" for none;
 * also: more servers to ask with its, "host:query port" each,
 * NULL-terminated (may be NULL) -- the favourites, asked whether the master
 * lists them or not; lan: the local network's look, first (NULL: none),
 * its beacons given a second at least. 0 once each server has answered or
 * had its time; -1 and why when the master gave no list, the others asked
 * all the same. Stops early once *stop is set (stop may be NULL). */
int dxl_browse(const char *master, const char *const *also, const dxl_browse_lan *lan, const dxl_browse_fns *fns,
               void *ctx, const atomic_int *stop, dxl_err *err);

#endif

/* GameSpy's words, as the game speaks them to a master server and to a
 * server (dx-reverse-info's ipdrv-dll.md, "The master server"): text of
 * \key\value pairs.
 *
 *   master, over TCP     \basic\\secure\<six characters>
 *   the game's answer    \gamename\deusex\location\0\validate\<eight>\final\
 *                        \list\\gamename\deusex\final\
 *   master               \ip\a.b.c.d:<query port>\ip\...\final\
 *   a server, over UDP   asked \status\: its \basic\, \info\, \rules\ and
 *                        \players\, in packets each ending
 *                        \queryid\<id>.<n>, the last with \final\ too,
 *                        in whatever order they come
 *
 * Nothing here touches a socket (app/browser.h does).
 */
#ifndef DXL_GAMESPY_H
#define DXL_GAMESPY_H

#include "core/common.h"

#define DXL_GS_GAME "deusex"

/* The answer to a master server's challenge for a game: the challenge's
 * six characters through GameSpy's RC4, keyed by the game's secret -- the
 * table of three IpDrv.dll has, Unreal's, Unreal Tournament's and an old
 * version's; six spaces for any other, Deus Ex's -- as eight characters of
 * base 64, into out[9]. */
void dxl_gs_validate(const char *challenge, const char *game, char out[9]);

/* The pair at *p of a \key\value text, into key and value, each cut to
 * its size; *p moved past it. 0 at the text's end. */
int dxl_gs_next(const char **p, char *key, size_t key_size, char *value, size_t value_size);

/* The value of key in a \key\value text (keys matched without regard to
 * case, the first), into out; 0 when it is not there. */
int dxl_gs_value(const char *text, const char *key, char *out, size_t size);

/* The challenge in a master's greeting, \basic\\secure\<six>, into out[7];
 * 0 when the text is no greeting (yet). */
int dxl_gs_challenge(const char *text, char out[7]);

/* The master's list: each \ip\a.b.c.d:port, in order, handed to fn; how
 * many, or -1 when the text has no \final\ yet -- the list is read on. */
typedef void (*dxl_gs_ip_fn)(void *ctx, const char *address, int port);
int dxl_gs_list(const char *text, dxl_gs_ip_fn fn, void *ctx);

/* A server's answer to \status\, as its packets come: the pairs of each,
 * by the packet's number, and whether all of them are in. */
#define DXL_GS_PACKETS 16
typedef struct {
    char  *packet[DXL_GS_PACKETS];   /* each packet's pairs, its \queryid\ and \final\ taken out */
    int    count;                     /* the packets in */
    int    last;                      /* the number of the one with \final\, 0 before it comes */
} dxl_gs_status;

void dxl_gs_status_init(dxl_gs_status *st);
void dxl_gs_status_free(dxl_gs_status *st);
/* A packet of it, as it came; -1 for one that is not of a status answer
 * (no \queryid\), or one already in. */
int  dxl_gs_status_add(dxl_gs_status *st, const char *text, size_t len);
/* Whether each packet from the first to the one with \final\ is in. */
int  dxl_gs_status_whole(const dxl_gs_status *st);
/* The packets' pairs together, in their order, as one \key\value text;
 * the caller frees. */
char *dxl_gs_status_text(const dxl_gs_status *st);

#endif

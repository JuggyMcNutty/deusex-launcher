/* What POSIX's platform files share among themselves and no one else: the
 * single instance's channel, a descriptor the waits poll. */
#ifndef DXL_POSIX_INTERNAL_H
#define DXL_POSIX_INTERNAL_H

#include "platform/instance.h"

/* The channel's descriptor; -1 when the instance has none. */
int dxl_instance_fd(const dxl_instance *inst);

/* The mark's descriptor, which the game keeps across its exec, so that the
 * mark lives as long as the game does, as the original's mutex is the
 * game's process's (bugs.md D.95); -1 when the instance has none. */
int dxl_instance_mark_fd(const dxl_instance *inst);
/* Marks, through that descriptor, a game that takes a second start's line
 * playing (on 1), or ended (0): an instance with a channel's alone. */
void dxl_instance_mark_game(dxl_instance *inst, int on);

#endif

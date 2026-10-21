/* What POSIX's platform files share among themselves and no one else: the
 * single instance's channel, a descriptor the waits poll. */
#ifndef DXL_POSIX_INTERNAL_H
#define DXL_POSIX_INTERNAL_H

#include "platform/instance.h"

/* The channel's descriptor; -1 when the instance has none. */
int dxl_instance_fd(const dxl_instance *inst);

#endif

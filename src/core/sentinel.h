/* Running.ini -- the entire crash-detection mechanism.
 *
 * Created after the wizard, deleted on clean exit. If it survives, the next
 * launch shows the recovery screen. There is nothing else to it: no signal
 * handler, no log parsing, no heuristics. dx-reverse-info/launch-flow.md sections 5
 * and 9.
 *
 * The one subtlety, from the original: it is created AFTER the wizard, not
 * before. A wizard sitting open for three minutes must not look like a crash
 * if the user gives up and kills it -- confirmed live, the file stayed absent
 * through 2m47s of open wizard.
 */
#ifndef DXL_SENTINEL_H
#define DXL_SENTINEL_H

#include "core/common.h"

typedef struct {
    char *path;
    int   created;   /* did *this* process create it? */
} dxl_sentinel;

void dxl_sentinel_init(dxl_sentinel *s, const char *system_dir);
void dxl_sentinel_free(dxl_sentinel *s);

int  dxl_sentinel_exists(const dxl_sentinel *s);
int  dxl_sentinel_create(dxl_sentinel *s, dxl_err *err);
/* Safe to call unconditionally on the exit path. */
void dxl_sentinel_remove(dxl_sentinel *s);

const char *dxl_sentinel_path(const dxl_sentinel *s);

#endif

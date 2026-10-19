/* What the launcher deletes of the player's -- a profile's folders, a save,
 * a screenshot -- removed for good, once the page has asked: the Smart Pro
 * has no desktop trash, nor a file manager to put it back from.
 */
#ifndef DXL_DELETE_H
#define DXL_DELETE_H

#include "core/common.h"

/* path gone: a file, or a folder with everything in it, its own links
 * removed, never followed. 0, or -1 and why, with what could not go still
 * there. */
int dxl_delete(const char *path, dxl_err *err);

#endif

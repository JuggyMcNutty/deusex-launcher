/* Where the platform keeps an app's own files, for the launcher's folders
 * (core/xdg.h): POSIX's are freedesktop.org's base directories, in
 * platform/posix/folders.c.
 */
#ifndef DXL_FOLDERS_H
#define DXL_FOLDERS_H

#include "core/common.h"

typedef struct {
    char *config;      /* its settings */
    char *data;        /* what the player makes: saves, shots */
    char *state;       /* what its runs leave: logs, the last run */
    char *cache;       /* what can be made again */
    char *data_home;   /* where the desktop's entry for it and its icons go */
} dxl_app_folders;

/* app's folders, each named after it where the platform's are shared;
 * creates nothing. -1 when there is no home to put them in. */
int  dxl_app_folders_get(const char *app, dxl_app_folders *f, dxl_err *err);
void dxl_app_folders_free(dxl_app_folders *f);

#endif

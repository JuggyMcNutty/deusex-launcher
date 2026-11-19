/* Windows' Recycle Bin, where what the launcher deletes of the player's
 * goes (platform/trash.h), to be restored from it, never removed outright:
 * the shell's delete with undo (SHFileOperationW, FOF_ALLOWUNDO). A drive
 * with no Recycle Bin of its own -- a network share, some removable ones --
 * takes nothing. The Recycle Bin says nothing back about where it put a
 * thing, so the launcher cannot take one back itself: all of several, or
 * none, is each first moved aside and back, which fails for one in use
 * with nothing gone, then all of them recycled together, each by its own
 * name, which the Recycle Bin restores it to.
 */
#include "platform/trash.h"
#include "platform/win32/internal.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/fs.h"

#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int dxl_trash_can(const char *path, const char *data_home) {
    (void)data_home;
    if (!dxl_path_exists(path)) return 0;
    char *real = dxl_fs_real(path);
    if (!real) return 0;
    wchar_t *w = dxl_w(real, 1);
    free(real);
    /* The drive's root, "C:\", or the share's. */
    wchar_t root[MAX_PATH];
    int ok = GetVolumePathNameW(w, root, MAX_PATH);
    free(w);
    if (!ok) return 0;
    UINT type = GetDriveTypeW(root);
    if (type == DRIVE_REMOTE || type == DRIVE_CDROM || type == DRIVE_RAMDISK) return 0;
    SHQUERYRBINFO info;
    memset(&info, 0, sizeof info);
    info.cbSize = sizeof info;
    return SHQueryRecycleBinW(root, &info) == S_OK;
}

int (*dxl_trash_test_op)(struct _SHFILEOPSTRUCTW *op);

/* The window the shell's question stands in front of: this thread's first
 * top-level window shown, the launcher's; NULL for none. */
static BOOL CALLBACK first_shown(HWND w, LPARAM out) {
    if (!IsWindowVisible(w) || GetWindow(w, GW_OWNER)) return TRUE;
    *(HWND *)out = w;
    return FALSE;
}

static HWND owner(void) {
    HWND w = NULL;
    EnumThreadWindows(GetCurrentThreadId(), first_shown, (LPARAM)&w);
    return w;
}

/* paths, n of them, into the Recycle Bin in one go. The shell takes them
 * one by one and stops at the first that fails, those before it gone, or
 * passes over one its question was answered No for: each looked for after
 * a failure, and what went said (bugs.md LPORT.14). SHFileOperationW's
 * answer is its own code, no GetLastError's: said as a number. */
static int recycle(wchar_t *const *paths, int n, dxl_err *err) {
    size_t len = 2;
    for (int i = 0; i < n; i++) len += wcslen(paths[i]) + 1;
    wchar_t *from = dxl_xmalloc(len * sizeof *from), *p = from;
    for (int i = 0; i < n; i++) {
        wcscpy(p, paths[i]);
        p += wcslen(paths[i]) + 1;
    }
    *p = 0;   /* the list ends with two NULs */
    SHFILEOPSTRUCTW op;
    memset(&op, 0, sizeof op);
    /* Its question in front of the launcher's window, whose frames stop
     * until it is answered. */
    op.hwnd = owner();
    op.wFunc = FO_DELETE;
    op.pFrom = from;
    /* FOF_WANTNUKEWARNING: an item the bin will not take (the bin off for
     * the drive, one over its size) is asked about, not deleted outright
     * without a word, as trash.h promises. */
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_WANTNUKEWARNING | FOF_NOERRORUI | FOF_SILENT;
    int rc = dxl_trash_test_op ? dxl_trash_test_op(&op) : SHFileOperationW(&op);
    free(from);
    if (rc == 0 && !op.fAnyOperationsAborted) return 0;
    int gone = 0, left = -1;
    for (int i = 0; i < n; i++) {
        DWORD e = GetFileAttributesW(paths[i]) == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0;
        if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) {
            char *u = dxl_u8(paths[i], 1);
            dxl_log("the Recycle Bin took %s", u);
            free(u);
            gone++;
        } else if (left < 0) {
            left = i;
        }
    }
    /* Each gone, the failure past the last: what was asked is done. */
    if (left < 0) return 0;
    if (!gone) {
        dxl_err_set(err, "the Recycle Bin took nothing (error 0x%X)", (unsigned)rc);
    } else {
        char *u = dxl_u8(paths[left], 1);
        if (n - gone > 1)
            dxl_err_set(err, "the Recycle Bin took %d of %d and stopped (error 0x%X): %s and %d more stay", gone, n,
                        (unsigned)rc, u, n - gone - 1);
        else
            dxl_err_set(err, "the Recycle Bin took %d of %d and stopped (error 0x%X): %s stays", gone, n,
                        (unsigned)rc, u);
        free(u);
    }
    return -1;
}

int dxl_trash(const char *path, const char *data_home, char *name, size_t name_size, dxl_err *err) {
    if (!dxl_trash_can(path, data_home)) {
        dxl_err_set(err, "%s cannot go to the Recycle Bin", path);
        return -1;
    }
    if (name && name_size) name[0] = '\0';
    wchar_t *w = dxl_w(path, 1);
    int rc = recycle(&w, 1, err);
    free(w);
    return rc;
}

int dxl_trash_undo(const char *data_home, const char *name, const char *path, dxl_err *err) {
    (void)data_home; (void)name;
    dxl_err_set(err, "%s is in the Recycle Bin: restore it from there", path);
    return -1;
}

int dxl_trash_all(char *const paths[], int n, const char *data_home, dxl_err *err) {
    for (int i = 0; i < n; i++)
        if (dxl_path_exists(paths[i]) && !dxl_trash_can(paths[i], data_home)) {
            dxl_err_set(err, "%s cannot go to the Recycle Bin: nothing was deleted", paths[i]);
            return -1;
        }
    /* Each moved aside first: in use, it cannot be, and nothing has gone;
     * then each back by its name. */
    size_t m = (size_t)(n > 0 ? n : 1);
    char **staged = dxl_xmalloc(m * sizeof *staged);
    wchar_t **wide = dxl_xmalloc(m * sizeof *wide);
    memset(staged, 0, m * sizeof *staged);
    int count = 0, rc = 0;
    for (int i = 0; i < n && rc == 0; i++) {
        if (!dxl_path_exists(paths[i])) continue;
        size_t len = strlen(paths[i]) + 32;
        staged[i] = dxl_xmalloc(len);
        snprintf(staged[i], len, "%s.deleting-%lu", paths[i], (unsigned long)GetCurrentProcessId());
        if (dxl_fs_replace(paths[i], staged[i]) != 0) {
            dxl_err_set(err, "%s is in use: nothing was deleted", paths[i]);
            free(staged[i]);
            staged[i] = NULL;
            rc = -1;
            break;
        }
        wide[count++] = dxl_w(paths[i], 1);
    }
    for (int i = 0; i < n; i++)
        if (staged[i] && dxl_fs_replace(staged[i], paths[i]) != 0) {
            dxl_log("warning: %s stays as %s", paths[i], staged[i]);
            if (rc == 0) dxl_err_set(err, "%s could not be put back: nothing was deleted", paths[i]);
            rc = -1;
        }
    if (rc == 0 && count) rc = recycle(wide, count, err);
    for (int i = 0; i < count; i++) free(wide[i]);
    for (int i = 0; i < n; i++) free(staged[i]);
    free(wide);
    free(staged);
    return rc;
}

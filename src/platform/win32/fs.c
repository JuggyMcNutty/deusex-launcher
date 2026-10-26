/* The file system on Windows (platform/fs.h): its wide calls, each path
 * UTF-16 with '\' on the way in and UTF-8 with '/' on the way back. A file
 * replaced whole by a new one written beside it, flushed and moved over it
 * (MoveFileExW); a link to a folder a symbolic one, which Windows lets a
 * user make only in its developer mode.
 */
#include "platform/fs.h"
#include "platform/win32/internal.h"
#include "core/paths.h"

#include <errno.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int sep(char c) { return c == '/' || c == '\\'; }

/* "C:/", "//server/share/" (the share's '/' if there), or a rooted "/",
 * on the current drive. */
size_t dxl_fs_root_len(const char *path) {
    if (!path) return 0;
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':' &&
        sep(path[2]))
        return 3;
    if (sep(path[0]) && sep(path[1]) && path[2] && !sep(path[2])) {
        size_t n = 2;
        while (path[n] && !sep(path[n])) n++;          /* the server */
        if (!path[n]) return n;
        n++;
        while (path[n] && !sep(path[n])) n++;          /* the share */
        return path[n] ? n + 1 : n;
    }
    return sep(path[0]) ? 1 : 0;
}

int dxl_fs_is_absolute(const char *path) { return dxl_fs_root_len(path) > 0; }

/* A known folder's path, in UTF-8 with '/'; NULL when there is none. */
static char *known(REFKNOWNFOLDERID id) {
    PWSTR w = NULL;
    char *out = NULL;
    if (SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, NULL, &w) == S_OK) out = dxl_u8(w, 1);
    CoTaskMemFree(w);
    return out;
}

const char *dxl_fs_home(void) {
    /* Found once and kept: the finder's thread holds the pointer while the
     * window's thread asks every frame, and USERPROFILE does not change. */
    static char *home;
    if (home) return home;
    const wchar_t *v = _wgetenv(L"USERPROFILE");
    char *h = v && *v ? dxl_u8(v, 1) : known(&FOLDERID_Profile);
    if (!h || !dxl_fs_is_absolute(h)) {
        free(h);
        return NULL;
    }
    if (InterlockedCompareExchangePointer((void *volatile *)&home, h, NULL) != NULL) free(h);   /* another thread's first */
    return home;
}

/* Each of leaf's names by the one the folder stores. */
char *dxl_fs_stored(const char *dir, const char *leaf) {
    char *cur = dxl_xstrdup(dir);
    char *rest = dxl_xstrdup(leaf), *save = NULL;
    dxl_path_normalize(rest);
    for (char *name = strtok_r(rest, "/", &save); name && cur; name = strtok_r(NULL, "/", &save)) {
        char *p = dxl_path_join(cur, name);
        wchar_t *w = dxl_w(p, 1);
        WIN32_FIND_DATAW fd;
        HANDLE f = wcspbrk(w, L"*?") ? INVALID_HANDLE_VALUE : FindFirstFileW(w, &fd);
        free(w);
        free(p);
        char *next = NULL;
        if (f != INVALID_HANDLE_VALUE) {
            char *stored = dxl_u8(fd.cFileName, 0);
            next = !strcmp(name, ".") || !strcmp(name, "..") ? dxl_path_join(cur, name) : dxl_path_join(cur, stored);
            free(stored);
            FindClose(f);
        }
        free(cur);
        cur = next;
    }
    free(rest);
    return cur;
}

char *dxl_fs_path(const char *outside) {
    char *p = dxl_xstrdup(outside ? outside : "");
    for (char *s = p; *s; s++)
        if (*s == '\\') *s = '/';
    return p;
}

char *dxl_fs_self(void) {
    wchar_t buf[32768];
    DWORD n = GetModuleFileNameW(NULL, buf, sizeof buf / sizeof *buf);
    return n && n < sizeof buf / sizeof *buf ? dxl_u8(buf, 1) : NULL;
}

/* "\\?\C:\x" as "C:/x", "\\?\UNC\host\share" as "//host/share". */
static char *plain(const wchar_t *w) {
    if (!wcsncmp(w, L"\\\\?\\UNC\\", 8)) {
        char *rest = dxl_u8(w + 8, 1);
        size_t n = strlen(rest) + 3;
        char *out = dxl_xmalloc(n);
        snprintf(out, n, "//%s", rest);
        free(rest);
        return out;
    }
    return dxl_u8(!wcsncmp(w, L"\\\\?\\", 4) ? w + 4 : w, 1);
}

char *dxl_fs_real(const char *path) {
    if (!path || !*path) return NULL;
    wchar_t *w = dxl_w(path, 1);
    /* Opened for nothing but its name, a folder too, links followed. */
    HANDLE h = CreateFileW(w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS, NULL);
    free(w);
    if (h == INVALID_HANDLE_VALUE) {
        dxl_win_errno(GetLastError());
        return NULL;
    }
    wchar_t buf[32768];
    DWORD n = GetFinalPathNameByHandleW(h, buf, sizeof buf / sizeof *buf, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(h);
    if (!n || n >= sizeof buf / sizeof *buf) return NULL;
    char *out = plain(buf);
    /* No '/' at its end but a root's. */
    size_t len = strlen(out), root = dxl_fs_root_len(out);
    while (len > root && out[len - 1] == '/') out[--len] = '\0';
    return out;
}

char *dxl_fs_cwd(void) {
    wchar_t buf[32768];
    DWORD n = GetCurrentDirectoryW(sizeof buf / sizeof *buf, buf);
    return n && n < sizeof buf / sizeof *buf ? dxl_u8(buf, 1) : NULL;
}

int dxl_fs_mkdir(const char *path) {
    wchar_t *w = dxl_w(path, 1);
    BOOL ok = CreateDirectoryW(w, NULL);
    DWORD e = GetLastError();
    free(w);
    if (ok) return 0;
    dxl_win_errno(e);
    return -1;
}

int dxl_fs_replace(const char *from, const char *to) {
    wchar_t *f = dxl_w(from, 1), *t = dxl_w(to, 1);
    BOOL ok = MoveFileExW(f, t, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    DWORD e = GetLastError();
    free(f);
    free(t);
    if (ok) return 0;
    dxl_win_errno(e);
    return -1;
}

int dxl_fs_is_link(const char *path) {
    if (!path) return 0;
    wchar_t *w = dxl_w(path, 1);
    DWORD a = GetFileAttributesW(w);
    free(w);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT);
}

/* A symbolic link, which Windows lets a user make only in its developer
 * mode; else EPERM, and the caller does without. */
int dxl_fs_link_dir(const char *target, const char *link) {
    wchar_t *t = dxl_w(target, 1), *l = dxl_w(link, 1);
    BOOLEAN ok = CreateSymbolicLinkW(l, t, SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
    DWORD e = GetLastError();
    free(t);
    free(l);
    if (ok) return 0;
    errno = e == ERROR_ALREADY_EXISTS ? EEXIST : EPERM;
    return -1;
}

/* Windows runs a file by its name, with no bit to say it may be: a file
 * that is there. */
int dxl_fs_is_program(const char *path) {
    if (!path) return 0;
    wchar_t *w = dxl_w(path, 1);
    DWORD a = GetFileAttributesW(w);
    free(w);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* "N": not inherited by a program the launcher starts. */
FILE *dxl_fs_open_log(const char *path) {
    wchar_t *w = dxl_w(path, 1);
    FILE *f = _wfopen(w, L"abN");
    free(w);
    return f;
}

/* A temporary file beside it, made new (CREATE_NEW), written whole, synced
 * (FlushFileBuffers), then moved over it, which on one volume replaces it
 * at once. Windows' files have no modes to keep. */
int dxl_fs_write_atomic(const char *path, const void *data, size_t len, dxl_err *err) {
    static volatile LONG seq;
    char *dir = dxl_path_dirname(path);
    size_t n = strlen(dir) + strlen(dxl_path_basename(path)) + 48;
    char *tmp = dxl_xmalloc(n);
    wchar_t *wt = NULL, *wp = NULL;
    HANDLE h = INVALID_HANDLE_VALUE;
    int rc = -1;
    char why[256];
    for (int tries = 0; tries < 16 && h == INVALID_HANDLE_VALUE; tries++) {
        snprintf(tmp, n, "%s/.%s.%lu.%ld", dir, dxl_path_basename(path), (unsigned long)GetCurrentProcessId(),
                 (long)InterlockedIncrement(&seq));
        free(wt);
        wt = dxl_w(tmp, 1);
        h = CreateFileW(wt, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_EXISTS) break;
    }
    if (h == INVALID_HANDLE_VALUE) {
        dxl_err_set(err, "cannot write in %s: %s", dir, dxl_win_error(GetLastError(), why, sizeof why));
        goto out;
    }
    const char *p = data;
    size_t left = len;
    while (left > 0) {
        DWORD chunk = left > 0x40000000 ? 0x40000000 : (DWORD)left, wrote = 0;
        if (!WriteFile(h, p, chunk, &wrote, NULL) || !wrote) break;
        p += wrote;
        left -= wrote;
    }
    DWORD e = left ? GetLastError() : 0;
    int ok = left == 0 && FlushFileBuffers(h);
    if (ok == 0 && !e) e = GetLastError();
    CloseHandle(h);
    if (!ok) {
        dxl_err_set(err, "cannot write %s: %s", tmp, dxl_win_error(e, why, sizeof why));
        DeleteFileW(wt);
        goto out;
    }
    wp = dxl_w(path, 1);
    if (!MoveFileExW(wt, wp, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        dxl_err_set(err, "cannot replace %s: %s", path, dxl_win_error(GetLastError(), why, sizeof why));
        DeleteFileW(wt);
        goto out;
    }
    rc = 0;
out:
    free(wt);
    free(wp);
    free(tmp);
    free(dir);
    return rc;
}

char **dxl_fs_roots(void) {
    wchar_t drives[512];
    DWORD n = GetLogicalDriveStringsW(sizeof drives / sizeof *drives, drives);
    size_t count = 0;
    char **roots = dxl_xmalloc(sizeof *roots);
    for (const wchar_t *d = drives; n && n < sizeof drives / sizeof *drives && *d; d += wcslen(d) + 1) {
        roots = dxl_xrealloc(roots, (count + 2) * sizeof *roots);
        roots[count++] = dxl_u8(d, 1);
    }
    roots[count] = NULL;
    return roots;
}

void dxl_fs_roots_free(char **roots) {
    for (size_t i = 0; roots && roots[i]; i++) free(roots[i]);
    free(roots);
}

char *dxl_fs_temp_dir(void) {
    wchar_t buf[MAX_PATH + 2];
    DWORD n = GetTempPathW(sizeof buf / sizeof *buf, buf);
    char *t = n && n < sizeof buf / sizeof *buf ? dxl_u8(buf, 1) : dxl_xstrdup("C:/Windows/Temp");
    size_t len = strlen(t), root = dxl_fs_root_len(t);
    while (len > root && t[len - 1] == '/') t[--len] = '\0';
    return t;
}

int dxl_fs_set_mtime(const char *path, long when) {
    wchar_t *w = dxl_w(path, 1);
    HANDLE h = CreateFileW(w, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    free(w);
    if (h == INVALID_HANDLE_VALUE) {
        dxl_win_errno(GetLastError());
        return -1;
    }
    /* 100 ns since 1601. */
    ULONGLONG t = ((ULONGLONG)when + 11644473600ULL) * 10000000ULL;
    FILETIME ft = { (DWORD)t, (DWORD)(t >> 32) };
    BOOL ok = SetFileTime(h, NULL, &ft, &ft);
    CloseHandle(h);
    return ok ? 0 : -1;
}

/* A link to a folder is removed itself, never what it leads to. */
static int remove_w(const wchar_t *w) {
    DWORD a = GetFileAttributesW(w);
    if (a == INVALID_FILE_ATTRIBUTES) return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND ? 0 : -1;
    if (a & FILE_ATTRIBUTE_READONLY) SetFileAttributesW(w, a & ~FILE_ATTRIBUTE_READONLY);
    if (!(a & FILE_ATTRIBUTE_DIRECTORY)) return DeleteFileW(w) ? 0 : -1;
    if (!(a & FILE_ATTRIBUTE_REPARSE_POINT)) {
        size_t n = wcslen(w);
        wchar_t *pattern = dxl_xmalloc((n + 3) * sizeof *pattern);
        swprintf(pattern, n + 3, L"%ls\\*", w);
        WIN32_FIND_DATAW fd;
        HANDLE f = FindFirstFileW(pattern, &fd);
        free(pattern);
        if (f != INVALID_HANDLE_VALUE) {
            do {
                if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
                size_t m = n + wcslen(fd.cFileName) + 2;
                wchar_t *child = dxl_xmalloc(m * sizeof *child);
                swprintf(child, m, L"%ls\\%ls", w, fd.cFileName);
                remove_w(child);
                free(child);
            } while (FindNextFileW(f, &fd));
            FindClose(f);
        }
    }
    return RemoveDirectoryW(w) ? 0 : -1;
}

int dxl_fs_remove_tree(const char *path) {
    wchar_t *w = dxl_w(path, 1);
    int rc = remove_w(w);
    free(w);
    return rc;
}

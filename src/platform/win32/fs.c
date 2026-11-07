/* The file system on Windows (platform/fs.h): its wide calls, each path
 * UTF-16 with '\' on the way in and UTF-8 with '/' on the way back. A file
 * replaced whole by a new one written beside it, flushed and moved over it
 * (MoveFileExW), a symbolic link written through to where it leads; a link
 * to a folder a symbolic one, which Windows lets a user make only in its
 * developer mode.
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
#include <winioctl.h>

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
    if (h == INVALID_HANDLE_VALUE) {
        dxl_win_errno(GetLastError());
        free(w);
        return NULL;
    }
    wchar_t buf[32768];
    DWORD n = GetFinalPathNameByHandleW(h, buf, sizeof buf / sizeof *buf, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    CloseHandle(h);
    /* Where the volume has no DOS name to give (an ImDisk RAM disk's, some
     * network redirectors'), the path as given, made full: it is there, as
     * the open showed, and POSIX's realpath names it. */
    if (!n) n = GetFullPathNameW(w, sizeof buf / sizeof *buf, buf, NULL);
    free(w);
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

/* A symbolic link's reparse point as FSCTL_GET_REPARSE_POINT gives it: the
 * layout of the DDK's REPARSE_DATA_BUFFER (ntifs.h), which the SDK's
 * headers lack, its names after it -- the substitute name, which Windows
 * follows, and the print name, for show. */
typedef struct {
    ULONG  tag;
    USHORT data_length;
    USHORT reserved;
    USHORT substitute_offset;
    USHORT substitute_length;
    USHORT print_offset;
    USHORT print_length;
    ULONG  flags;
} symlink_point;

/* An absolute link's target, its substitute name, as the launcher names a
 * path: "\??\C:\x" as "C:/x", "\??\UNC\host\share\x" as "//host/share/x", a
 * volume's "\??\Volume{...}\x" through Win32's "\\?\"; NULL for one of the
 * object manager's own names ("\Device\..."), which no Win32 path names. */
static char *link_target(const wchar_t *name) {
    if (!wcsncmp(name, L"\\??\\", 4) || !wcsncmp(name, L"\\\\?\\", 4)) {
        const wchar_t *rest = name + 4;
        int unc = !wcsncmp(rest, L"UNC\\", 4);
        if (!unc && rest[0] && rest[1] == L':') return dxl_u8(rest, 1);
        char *tail = dxl_u8(unc ? rest + 4 : rest, 1);
        size_t n = strlen(tail) + 5;
        char *out = dxl_xmalloc(n);
        snprintf(out, n, "%s%s", unc ? "//" : "//?/", tail);
        free(tail);
        return out;
    }
    if (name[0] == L'\\' && name[1] != L'\\') return NULL;
    return dxl_u8(name, 1);
}

/* A relative target of the link at at from the link's own folder, '\' as
 * '/'; one that starts at a root ("\x") from the link's drive's. Caller
 * frees. */
static char *beside(const char *at, const wchar_t *name) {
    char *rel = dxl_u8(name, 1);
    char *base = rel[0] == '/' ? dxl_xstrndup(at, dxl_fs_root_len(at)) : dxl_path_dirname(at);
    char *out = dxl_path_join(base, rel);
    free(base);
    free(rel);
    return out;
}

/* The reparse point at w read (FSCTL_GET_REPARSE_POINT), the point itself
 * opened (FILE_FLAG_OPEN_REPARSE_POINT; a folder's with
 * FILE_FLAG_BACKUP_SEMANTICS): 1, *to a symbolic link's target as the
 * launcher names a path, a relative one from the link's folder at's (caller
 * frees); 0 for another kind of point; -1 where it cannot be read. */
static int read_point(const wchar_t *w, const char *at, char **to) {
    HANDLE h = CreateFileW(w, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    BYTE *buf = dxl_xmalloc(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
    DWORD got = 0;
    BOOL ok = DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL, 0, buf, MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &got, NULL);
    CloseHandle(h);
    int rc = -1;
    ULONG tag = 0;
    symlink_point p;
    if (ok && got >= sizeof tag) memcpy(&tag, buf, sizeof tag);
    if (ok && got >= sizeof tag && tag != IO_REPARSE_TAG_SYMLINK) {
        rc = 0;
    } else if (ok && got >= sizeof p) {
        memcpy(&p, buf, sizeof p);
        if (p.substitute_length && p.substitute_length % sizeof(wchar_t) == 0 &&
            (size_t)p.substitute_offset + p.substitute_length <= got - sizeof p) {
            size_t n = p.substitute_length / sizeof(wchar_t);
            wchar_t *name = dxl_xmalloc((n + 1) * sizeof *name);
            memcpy(name, buf + sizeof p + p.substitute_offset, n * sizeof *name);
            name[n] = L'\0';
            *to = p.flags & SYMLINK_FLAG_RELATIVE ? beside(at, name) : link_target(name);
            free(name);
            rc = *to ? 1 : -1;
        }
    }
    free(buf);
    return rc;
}

/* The reparse tag of the entry at w as its folder lists it (FindFirstFileW's
 * dwReserved0); 0 for none. */
static DWORD listed_tag(const wchar_t *w) {
    WIN32_FIND_DATAW fd;
    HANDLE f = wcspbrk(w, L"*?") ? INVALID_HANDLE_VALUE : FindFirstFileW(w, &fd);
    if (f == INVALID_HANDLE_VALUE) return 0;
    FindClose(f);
    return fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ? fd.dwReserved0 : 0;
}

/* What the symbolic link at at leads to: 1, *to its target as the launcher
 * names a path (caller frees); 0 where at is no symbolic link -- a file or
 * a folder, nothing yet, another kind of reparse point (a junction, a cloud
 * file's placeholder, an app's alias), each written as any other; -1 for a
 * link whose point cannot be read. The point is asked for where at's
 * attributes have one (GetFileAttributesW, the link's own); one that cannot
 * be read is a link's where its folder lists it as one. */
static int read_link(const char *at, char **to) {
    *to = NULL;
    wchar_t *w = dxl_w(at, 1);
    DWORD a = GetFileAttributesW(w);
    int rc = 0;
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_REPARSE_POINT)) {
        rc = read_point(w, at, to);
        if (rc < 0 && listed_tag(w) != IO_REPARSE_TAG_SYMLINK) rc = 0;
    }
    free(w);
    return rc;
}

/* Where a symbolic link leads, followed link by link as Windows follows
 * one, a relative target from its link's folder: the end, there or not yet,
 * path itself where it is no link (caller frees); NULL for a loop, or a
 * link whose point cannot be read. 63 links at most, the most Windows
 * follows on a path. */
static char *link_end(const char *path) {
    char *at = dxl_xstrdup(path);
    for (int hops = 0; hops <= 63; hops++) {
        char *to = NULL;
        int r = read_link(at, &to);
        if (r == 0) return at;
        free(at);
        if (r < 0) return NULL;
        at = to;
    }
    free(at);
    return NULL;
}

/* A temporary file beside it, made new (CREATE_NEW), written whole, synced
 * (FlushFileBuffers), then moved over it, which on one volume replaces it
 * at once. Windows' files have no modes to keep. */
int dxl_fs_write_atomic(const char *path, const void *data, size_t len, dxl_err *err) {
    static volatile LONG seq;
    /* A symbolic link (a dotfiles repository's) is written where it leads,
     * as DXEngine's CreateFile and the original's write through one: the
     * move would put a file in the link's place, its target stale. One that
     * leads nowhere yet is made where it leads; one that cannot be followed,
     * a loop, is refused. */
    char *target = link_end(path);
    if (!target) {
        dxl_err_set(err, "%s: its link cannot be followed", path);
        return -1;
    }
    path = target;
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
    free(target);
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

/* Processes on Windows (platform/os.h), and what the other win32 files
 * share (platform/win32/internal.h): UTF-8 and UTF-16, Windows' errors as
 * errno's and as text, a word quoted as CommandLineToArgvW splits a line,
 * and the names of the codes a program ends with when it does not exit by
 * itself.
 */
#include "platform/os.h"
#include "platform/win32/internal.h"
#include "core/common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- UTF-8 and UTF-16 ------------------------------------------------- */

wchar_t *dxl_w(const char *s, int backslashes) {
    if (!s) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = dxl_xmalloc((size_t)(n > 0 ? n : 1) * sizeof *w);
    if (n <= 0 || !MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n)) w[0] = 0;
    if (backslashes)
        for (wchar_t *p = w; *p; p++)
            if (*p == L'/') *p = L'\\';
    return w;
}

char *dxl_u8(const wchar_t *w, int slashes) {
    if (!w) return NULL;
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = dxl_xmalloc((size_t)(n > 0 ? n : 1));
    if (n <= 0 || !WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL)) s[0] = '\0';
    if (slashes)
        for (char *p = s; *p; p++)
            if (*p == '\\') *p = '/';
    return s;
}

void dxl_win_errno(DWORD error) {
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_BAD_NETPATH:    errno = ENOENT; break;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION: errno = EACCES; break;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:    errno = EEXIST; break;
    case ERROR_DIR_NOT_EMPTY:  errno = ENOTEMPTY; break;
    case ERROR_PRIVILEGE_NOT_HELD: errno = EPERM; break;
    case ERROR_NOT_SUPPORTED:
    case ERROR_INVALID_FUNCTION: errno = EOPNOTSUPP; break;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL: errno = ENOSPC; break;
    case ERROR_NOT_SAME_DEVICE: errno = EXDEV; break;
    default:                   errno = EIO; break;
    }
}

const char *dxl_win_error(DWORD error, char *buf, size_t size) {
    wchar_t text[512];
    DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0, text,
                             sizeof text / sizeof *text, NULL);
    if (!n) {
        snprintf(buf, size, "error %lu", (unsigned long)error);
        return buf;
    }
    char *u = dxl_u8(text, 0);
    size_t len = strlen(u);
    while (len && (u[len - 1] == '\n' || u[len - 1] == '\r' || u[len - 1] == ' ' || u[len - 1] == '.')) u[--len] = '\0';
    snprintf(buf, size, "%s", u);
    free(u);
    return buf;
}

int dxl_win_quote(wchar_t *line, size_t size, const wchar_t *word) {
    size_t n = wcslen(line);
    /* Room for every character doubled, the quotes, a space and the end. */
    if (n + 2 * wcslen(word) + 4 > size) return 0;
    if (n) line[n++] = L' ';
    if (*word && !wcspbrk(word, L" \t\n\v\"")) {
        wcscpy(line + n, word);
        return 1;
    }
    line[n++] = L'"';
    for (const wchar_t *p = word;; p++) {
        size_t slashes = 0;
        while (*p == L'\\') { p++; slashes++; }
        if (!*p) {
            /* Backslashes before the closing quote, doubled. */
            for (size_t i = 0; i < 2 * slashes; i++) line[n++] = L'\\';
            break;
        }
        if (*p == L'"') {
            for (size_t i = 0; i < 2 * slashes + 1; i++) line[n++] = L'\\';
        } else {
            for (size_t i = 0; i < slashes; i++) line[n++] = L'\\';
        }
        line[n++] = *p;
    }
    line[n++] = L'"';
    line[n] = 0;
    return 1;
}

/* ---- the process's own lot -------------------------------------------- */

int dxl_env_set(const char *name, const char *value) {
    wchar_t *n = dxl_w(name, 0), *v = dxl_w(value ? value : "", 0);
    /* An empty value takes it out, in the C library's copy and Windows'. */
    int rc = _wputenv_s(n, v) == 0 ? 0 : -1;
    free(n);
    free(v);
    return rc;
}

long dxl_os_pid(void) { return (long)GetCurrentProcessId(); }

void dxl_os_sleep_ms(int ms) { Sleep(ms > 0 ? (DWORD)ms : 0); }

/* ---- how a program ended ---------------------------------------------- */

/* A program the launcher ended at once, by TerminateProcess: the debugger's
 * code for a killed process, which no program exits with by itself. */
#define DXL_KILLED 0x40010004u

static const struct {
    unsigned code;
    const char *name;
    int crash;
} ends[] = {
    { 0xC0000005u, "an access violation", 1 },
    { 0xC00000FDu, "a stack overflow", 1 },
    { 0xC0000409u, "an abort (a fast fail: the C runtime's abort, or a stack buffer overrun)", 1 },
    { 0xC000001Du, "an illegal instruction", 1 },
    { 0xC0000096u, "a privileged instruction", 1 },
    { 0xC0000094u, "an integer division by zero", 1 },
    { 0xC0000095u, "an integer overflow", 1 },
    { 0xC000008Eu, "a floating-point division by zero", 1 },
    { 0xC0000374u, "a corrupted heap", 1 },
    { 0xC0000006u, "a page it could not read in", 1 },
    { 0x80000003u, "a breakpoint", 1 },
    { 0xC0000135u, "a DLL it needs not found", 0 },
    { 0xC0000139u, "a DLL's entry point not found", 0 },
    { 0xC000007Bu, "an image for another machine", 0 },
    { 0xC0000142u, "a DLL that failed to start", 0 },
    { 0xC000013Au, "Ctrl+C", 0 },
    { DXL_KILLED, "the launcher", 0 },
};

const char *dxl_end_name(int end, char *buf, size_t size) {
    for (size_t i = 0; i < sizeof ends / sizeof *ends; i++)
        if (ends[i].code == (unsigned)end) {
            snprintf(buf, size, "%s (0x%08X)", ends[i].name, (unsigned)end);
            return buf;
        }
    snprintf(buf, size, "exception 0x%08X", (unsigned)end);
    return buf;
}

int dxl_end_crashed(int end) {
    for (size_t i = 0; i < sizeof ends / sizeof *ends; i++)
        if (ends[i].code == (unsigned)end) return ends[i].crash;
    return (unsigned)end >= 0xC0000000u;
}

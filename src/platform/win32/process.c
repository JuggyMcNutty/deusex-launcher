/* The processes the launcher starts, on Windows (platform/process.h).
 *
 * The game's line is a named pipe, \\.\pipe\dxl-line-<pid>-<n>, both ways:
 * the launcher holds its server end, overlapped, and the game the client
 * end, inherited -- the only handle it inherits but its output's, by the
 * list CreateProcessW is given -- its value in DXL_LAUNCHER_FD. Its
 * command line is quoted as CommandLineToArgvW, the engine's, splits it.
 * Stopping it asks first -- "Quit" on the line, WM_CLOSE to its windows --
 * and killing it is TerminateProcess, with the code platform/os.h names.
 */
#include "platform/process.h"
#include "platform/os.h"
#include "platform/win32/internal.h"

#include <objbase.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define DXL_KILLED 0x40010004u

struct dxl_game_os {
    HANDLE     process;
    HANDLE     line;        /* the launcher's end, or INVALID_HANDLE_VALUE once broken */
    OVERLAPPED ov;
    int        reading;
};

/* ---- making a program's start ------------------------------------------ */

/* argv as one command line, each word quoted as it needs. Caller frees. */
static wchar_t *command_line(char *const argv[]) {
    size_t size = 64;
    for (size_t i = 0; argv[i]; i++) size += 2 * strlen(argv[i]) + 4;
    wchar_t *line = dxl_xmalloc(size * sizeof *line);
    line[0] = 0;
    for (size_t i = 0; argv[i]; i++) {
        wchar_t *w = dxl_w(argv[i], 0);
        dxl_win_quote(line, size, w);
        free(w);
    }
    return line;
}

/* This process's environment as a block, the changes in env made to it
 * ("NAME=value" sets one, "NAME" alone takes it out), name dropped first
 * and set to value when value is not NULL. Caller frees. */
static wchar_t *environment(char *const env[], const wchar_t *name, const wchar_t *value) {
    wchar_t *mine = GetEnvironmentStringsW();
    size_t cap = 4096, len = 0;
    wchar_t *block = dxl_xmalloc(cap * sizeof *block);
    size_t nchanges = 0;
    while (env && env[nchanges]) nchanges++;
    wchar_t **changes = dxl_xmalloc((nchanges + 1) * sizeof *changes);
    for (size_t i = 0; i < nchanges; i++) changes[i] = dxl_w(env[i], 0);
    for (const wchar_t *v = mine; v && *v; v += wcslen(v) + 1) {
        const wchar_t *eq = wcschr(v + 1, L'=');    /* "=C:=C:\..." keeps its first '=' */
        size_t nlen = eq ? (size_t)(eq - v) : wcslen(v);
        int drop = name && wcslen(name) == nlen && !_wcsnicmp(v, name, nlen);
        for (size_t i = 0; !drop && i < nchanges; i++) {
            const wchar_t *ceq = wcschr(changes[i], L'=');
            size_t clen = ceq ? (size_t)(ceq - changes[i]) : wcslen(changes[i]);
            drop = clen == nlen && !_wcsnicmp(v, changes[i], nlen);
        }
        if (drop) continue;
        size_t vl = wcslen(v) + 1;
        while (len + vl + 2 > cap) block = dxl_xrealloc(block, (cap *= 2) * sizeof *block);
        memcpy(block + len, v, vl * sizeof *v);
        len += vl;
    }
    FreeEnvironmentStringsW(mine);
    for (size_t i = 0; i <= nchanges; i++) {
        wchar_t set[1024];
        if (i < nchanges) {
            if (!wcschr(changes[i], L'=')) continue;
            swprintf(set, sizeof set / sizeof *set, L"%ls", changes[i]);
        } else if (name && value) {
            swprintf(set, sizeof set / sizeof *set, L"%ls=%ls", name, value);
        } else {
            continue;
        }
        size_t vl = wcslen(set) + 1;
        while (len + vl + 2 > cap) block = dxl_xrealloc(block, (cap *= 2) * sizeof *block);
        memcpy(block + len, set, vl * sizeof *set);
        len += vl;
    }
    block[len] = 0;
    if (!len) block[++len] = 0;
    for (size_t i = 0; i < nchanges; i++) free(changes[i]);
    free(changes);
    return block;
}

/* A program started with only handles of inherit[] inherited, its
 * standard handles those three. 0, or -1 and why. */
static int start(char *const argv[], const wchar_t *env, const char *workdir, HANDLE in, HANDLE out, HANDLE err_h,
                 HANDLE *inherit, DWORD ninherit, PROCESS_INFORMATION *pi, dxl_err *err) {
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &size);
    LPPROC_THREAD_ATTRIBUTE_LIST list = dxl_xmalloc(size);
    int rc = -1;
    if (!InitializeProcThreadAttributeList(list, 1, 0, &size)) {
        free(list);
        dxl_err_set(err, "cannot run %s: no attribute list", argv[0]);
        return -1;
    }
    UpdateProcThreadAttribute(list, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, ninherit * sizeof *inherit, NULL,
                              NULL);
    STARTUPINFOEXW si;
    memset(&si, 0, sizeof si);
    si.StartupInfo.cb = sizeof si;
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = in;
    si.StartupInfo.hStdOutput = out;
    si.StartupInfo.hStdError = err_h;
    si.lpAttributeList = list;
    wchar_t *app = dxl_w(argv[0], 1), *line = command_line(argv), *dir = workdir && *workdir ? dxl_w(workdir, 1) : NULL;
    if (CreateProcessW(app, line, NULL, NULL, TRUE,
                       CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW,
                       (LPVOID)env, dir, &si.StartupInfo, pi)) {
        rc = 0;
    } else {
        char why[256];
        dxl_err_set(err, "cannot run %s: %s", argv[0], dxl_win_error(GetLastError(), why, sizeof why));
    }
    DeleteProcThreadAttributeList(list);
    free(list);
    free(app);
    free(line);
    free(dir);
    return rc;
}

static HANDLE inheritable(HANDLE h) {
    SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    return h;
}

static HANDLE null_device(DWORD access) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    return CreateFileW(L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
}

/* A run's end: its exit code, or what ended it -- a code above 0xC0000000,
 * an exception's, or the launcher's kill (platform/os.h names them). */
static void ended_with(DWORD code, int *status, int *signal) {
    *status = (int)code;
    *signal = code >= 0xC0000000u || code == 0x80000003u || code == DXL_KILLED ? (int)code : 0;
}

/* ---- the game ---------------------------------------------------------- */

int dxl_game_start_argv(dxl_game *g, char *const argv[], char *const env[], const char *workdir,
                        const char *log_path, dxl_err *err) {
    static volatile LONG seq;
    memset(g, 0, sizeof *g);
    wchar_t name[96];
    swprintf(name, sizeof name / sizeof *name, L"\\\\.\\pipe\\dxl-line-%lu-%ld", (unsigned long)GetCurrentProcessId(),
             (long)InterlockedIncrement(&seq));
    HANDLE server = CreateNamedPipeW(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                     PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
                                     4096, 4096, 0, NULL);
    char why[256];
    if (server == INVALID_HANDLE_VALUE) {
        dxl_err_set(err, "no line for the game: %s", dxl_win_error(GetLastError(), why, sizeof why));
        return -1;
    }
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE client = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, NULL);
    HANDLE nul = null_device(GENERIC_READ), log = INVALID_HANDLE_VALUE;
    if (log_path) {
        wchar_t *w = dxl_w(log_path, 1);
        log = CreateFileW(w, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &sa,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        free(w);
        if (log == INVALID_HANDLE_VALUE) {
            dxl_err_set(err, "cannot open %s: %s", log_path, dxl_win_error(GetLastError(), why, sizeof why));
            CloseHandle(server);
            if (client != INVALID_HANDLE_VALUE) CloseHandle(client);
            if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
            return -1;
        }
    }
    HANDLE out = log != INVALID_HANDLE_VALUE ? log : inheritable(GetStdHandle(STD_OUTPUT_HANDLE));
    HANDLE errh = log != INVALID_HANDLE_VALUE ? log : inheritable(GetStdHandle(STD_ERROR_HANDLE));
    HANDLE list[4];
    DWORD n = 0;
    if (client != INVALID_HANDLE_VALUE) list[n++] = client;
    if (nul != INVALID_HANDLE_VALUE) list[n++] = nul;
    if (out && out != INVALID_HANDLE_VALUE) list[n++] = out;
    if (errh && errh != INVALID_HANDLE_VALUE && errh != out) list[n++] = errh;
    wchar_t value[32];
    swprintf(value, sizeof value / sizeof *value, L"%llu", (unsigned long long)(uintptr_t)client);
    wchar_t *fd_name = dxl_w(DXL_LAUNCHER_FD_ENV, 0);
    wchar_t *block = environment(env, fd_name, client != INVALID_HANDLE_VALUE ? value : NULL);
    free(fd_name);
    PROCESS_INFORMATION pi;
    int rc = start(argv, block, workdir, nul, out, errh, list, n, &pi, err);
    free(block);
    if (client != INVALID_HANDLE_VALUE) CloseHandle(client);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    if (rc != 0) {
        CloseHandle(server);
        return -1;
    }
    CloseHandle(pi.hThread);
    g->os = dxl_xmalloc(sizeof *g->os);
    memset(g->os, 0, sizeof *g->os);
    g->os->process = pi.hProcess;
    g->os->line = server;
    g->os->ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    g->pid = (long)pi.dwProcessId;
    return 0;
}

void dxl_game_close(dxl_game *g) {
    free(g->pending);
    g->pending = NULL;
    g->pending_len = g->pending_cap = 0;
    if (!g->os) return;
    if (g->os->line != INVALID_HANDLE_VALUE) {
        CancelIo(g->os->line);
        CloseHandle(g->os->line);
    }
    CloseHandle(g->os->ov.hEvent);
    CloseHandle(g->os->process);
    free(g->os);
    g->os = NULL;
}

/* A line is any length; one past this with no end is taken as it is. */
#define LINE_MOST (1 << 20)

/* A whole line from what has come in, malloc'd into *line; 1 if there was
 * one. */
static int take_line(dxl_game *g, char **line) {
    char *nl = g->pending_len ? memchr(g->pending, '\n', g->pending_len) : NULL;
    size_t used;
    if (nl) {
        used = (size_t)(nl - g->pending) + 1;
    } else if (g->pending_len >= LINE_MOST) {
        used = g->pending_len;
        nl = g->pending + used;
    } else {
        return 0;
    }
    size_t n = (size_t)(nl - g->pending);
    if (n && g->pending[n - 1] == '\r') n--;
    char *out = dxl_xmalloc(n + 1);
    memcpy(out, g->pending, n);
    out[n] = '\0';
    memmove(g->pending, g->pending + used, g->pending_len - used);
    g->pending_len -= used;
    *line = out;
    return 1;
}

/* The line's end gone: the game ended, or dropped it. */
static void broken(dxl_game_os *os) {
    CloseHandle(os->line);
    os->line = INVALID_HANDLE_VALUE;
    os->reading = 0;
}

/* A read pending on the line, unless it has one or is gone. The buffer
 * grows for a line of any length, only while no read is pending on it. */
static void read_line(dxl_game *g) {
    dxl_game_os *os = g->os;
    while (os->line != INVALID_HANDLE_VALUE && !os->reading) {
        if (g->pending_len == g->pending_cap) {
            g->pending_cap = g->pending_cap ? g->pending_cap * 2 : 1024;
            g->pending = dxl_xrealloc(g->pending, g->pending_cap);
        }
        ResetEvent(os->ov.hEvent);
        DWORD got = 0;
        if (ReadFile(os->line, g->pending + g->pending_len, (DWORD)(g->pending_cap - g->pending_len), &got,
                     &os->ov)) {
            g->pending_len += got;
            if (!got) broken(os);
            return;
        }
        if (GetLastError() == ERROR_IO_PENDING) os->reading = 1;
        else broken(os);
    }
}

/* What a pending read brought, once it is done. */
static void finish_read(dxl_game *g) {
    dxl_game_os *os = g->os;
    if (!os->reading) return;
    DWORD got = 0;
    if (GetOverlappedResult(os->line, &os->ov, &got, FALSE)) {
        os->reading = 0;
        g->pending_len += got;
    } else if (GetLastError() != ERROR_IO_INCOMPLETE) {
        broken(os);
    }
}

int dxl_game_wait(dxl_game *g, dxl_instance *inst, int ms, char **line) {
    *line = NULL;
    if (take_line(g, line)) return DXL_GAME_LINE;
    dxl_game_os *os = g->os;
    read_line(g);
    if (take_line(g, line)) return DXL_GAME_LINE;
    HANDLE h[3];
    int k = 0, chan = -1, extra = -1;
    if (os->reading) { chan = k; h[k++] = os->ov.hEvent; }
    HANDLE ev = dxl_instance_event(inst);
    if (ev) { extra = k; h[k++] = ev; }
    int proc = k;
    h[k++] = os->process;
    DWORD r = WaitForMultipleObjects((DWORD)k, h, FALSE, ms < 0 ? INFINITE : (DWORD)ms);
    int seen = 0;
    if (r != WAIT_TIMEOUT && r < WAIT_OBJECT_0 + (DWORD)k) {
        if (chan >= 0 && WaitForSingleObject(h[chan], 0) == WAIT_OBJECT_0) {
            finish_read(g);
            if (take_line(g, line)) seen |= DXL_GAME_LINE;
        }
        if (extra >= 0 && WaitForSingleObject(h[extra], 0) == WAIT_OBJECT_0) seen |= DXL_GAME_EXTRA;
    }
    /* The end is told once nothing else is waiting to be read: what the
     * game wrote before it ended comes first. */
    if (!seen && WaitForSingleObject(h[proc], 0) == WAIT_OBJECT_0) {
        DWORD avail = 0;
        if (os->line != INVALID_HANDLE_VALUE && !os->reading &&
            PeekNamedPipe(os->line, NULL, 0, NULL, &avail, NULL) && avail)
            return 0;
        DWORD code = 0;
        GetExitCodeProcess(os->process, &code);
        ended_with(code, &g->status, &g->signal);
        g->ended = 1;
        seen |= DXL_GAME_ENDED;
    }
    return seen;
}

void dxl_game_send(dxl_game *g, const char *line) {
    if (!g->os || g->os->line == INVALID_HANDLE_VALUE || !line) return;
    size_t n = strlen(line);
    char *buf = dxl_xmalloc(n + 2);
    memcpy(buf, line, n);
    buf[n] = '\n';
    OVERLAPPED ov;
    memset(&ov, 0, sizeof ov);
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    DWORD wrote = 0;
    if (!WriteFile(g->os->line, buf, (DWORD)(n + 1), &wrote, &ov) && GetLastError() == ERROR_IO_PENDING) {
        /* A game that reads nothing does not hold the launcher up. */
        if (WaitForSingleObject(ov.hEvent, 1000) != WAIT_OBJECT_0) CancelIoEx(g->os->line, &ov);
        GetOverlappedResult(g->os->line, &ov, &wrote, TRUE);
    }
    CloseHandle(ov.hEvent);
    free(buf);
}

/* Each top-level window of the game's process asked to close. */
static BOOL CALLBACK close_window(HWND w, LPARAM pid) {
    DWORD owner = 0;
    GetWindowThreadProcessId(w, &owner);
    if (owner == (DWORD)pid) PostMessageW(w, WM_CLOSE, 0, 0);
    return TRUE;
}

void dxl_game_stop(dxl_game *g) {
    if (g->ended || !g->os) return;
    dxl_game_send(g, "Quit");
    EnumWindows(close_window, (LPARAM)g->pid);
}

void dxl_game_kill(dxl_game *g) {
    if (!g->ended && g->os) TerminateProcess(g->os->process, DXL_KILLED);
}

int dxl_game_clean(const dxl_game *g) { return g->ended && g->status == 0; }

/* ---- the shell --------------------------------------------------------- */

static int shell_open(const wchar_t *what) {
    HRESULT co = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    HINSTANCE r = ShellExecuteW(NULL, L"open", what, NULL, NULL, SW_SHOWNORMAL);
    if (SUCCEEDED(co)) CoUninitialize();
    return (INT_PTR)r > 32 ? 0 : -1;
}

int dxl_open_url(const char *url) {
    wchar_t *w = dxl_w(url, 0);
    int rc = shell_open(w);
    free(w);
    return rc;
}

int dxl_open_path(const char *path) {
    wchar_t *w = dxl_w(path, 1);
    int rc = shell_open(w);
    free(w);
    return rc;
}

/* ---- a program run to its end ------------------------------------------ */

/* What a pipe has, onto buf's end (cut at size, NUL-terminated); 0 at the
 * pipe's end. */
static int take_pipe(HANDLE h, char *buf, size_t size, size_t *got) {
    DWORD avail = 0;
    if (!PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL)) return 0;
    while (avail) {
        char chunk[512];
        DWORD n = 0;
        if (!ReadFile(h, chunk, avail < sizeof chunk ? avail : (DWORD)sizeof chunk, &n, NULL) || !n) return 0;
        for (DWORD i = 0; i < n && *got + 1 < size; i++) buf[(*got)++] = chunk[i];
        if (size) buf[*got] = '\0';
        avail -= n;
    }
    return 1;
}

int dxl_run_capture(char *const argv[], int timeout_ms, char *out, size_t size, char *errs, size_t errs_size,
                    int *status, dxl_err *err) {
    if (size) out[0] = '\0';
    if (errs && errs_size) errs[0] = '\0';
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE out_r, out_w, err_r = NULL, err_w = NULL;
    if (!CreatePipe(&out_r, &out_w, &sa, 1 << 16)) {
        dxl_err_set(err, "no pipe for %s", argv[0]);
        return -1;
    }
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    if (errs && CreatePipe(&err_r, &err_w, &sa, 1 << 16)) SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = null_device(GENERIC_READ | GENERIC_WRITE);
    HANDLE list[3];
    DWORD n = 0;
    list[n++] = out_w;
    if (nul != INVALID_HANDLE_VALUE) list[n++] = nul;
    if (err_w) list[n++] = err_w;
    /* Its environment this one's but the launcher's line. */
    wchar_t *fd_name = dxl_w(DXL_LAUNCHER_FD_ENV, 0);
    wchar_t *block = environment(NULL, fd_name, NULL);
    free(fd_name);
    PROCESS_INFORMATION pi;
    dxl_err why;
    int rc = start(argv, block, NULL, nul, out_w, err_w ? err_w : nul, list, n, &pi, &why);
    free(block);
    CloseHandle(out_w);
    if (err_w) CloseHandle(err_w);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (rc != 0) {
        CloseHandle(out_r);
        if (err_r) CloseHandle(err_r);
        if (status) *status = 127;
        /* Why, on the errors' line too, as POSIX's child says it there. */
        if (errs && errs_size) snprintf(errs, errs_size, "%s\n", dxl_err_msg(&why));
        if (err) *err = why;
        return -1;
    }
    CloseHandle(pi.hThread);
    size_t got = 0, got_err = 0;
    int open_out = 1, open_err = err_r != NULL, timed_out = 0;
    /* A deadline, not a count of quiet waits: a program that talks on and
     * on is stopped at it too. */
    ULONGLONG deadline = GetTickCount64() + (ULONGLONG)(timeout_ms > 0 ? timeout_ms : 0);
    for (;;) {
        if (open_out) open_out = take_pipe(out_r, out, size, &got);
        if (open_err) open_err = take_pipe(err_r, errs, errs_size, &got_err);
        if (!open_out && !open_err) break;
        ULONGLONG now = GetTickCount64();
        if (now >= deadline) {
            timed_out = 1;
            break;
        }
        /* Its end, or a little while: a pipe has no event to wait on. */
        if (WaitForSingleObject(pi.hProcess, (DWORD)(deadline - now < 20 ? deadline - now : 20)) == WAIT_OBJECT_0) {
            /* What it left in the pipes, then their ends. */
            if (open_out) take_pipe(out_r, out, size, &got);
            if (open_err) take_pipe(err_r, errs, errs_size, &got_err);
            break;
        }
    }
    CloseHandle(out_r);
    if (err_r) CloseHandle(err_r);
    if (timed_out) TerminateProcess(pi.hProcess, DXL_KILLED);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    int sig = 0, st = 0;
    ended_with(code, &st, &sig);
    if (status) *status = st;
    if (timed_out) {
        dxl_err_set(err, "%s gave no answer in %d ms", argv[0], timeout_ms);
        return DXL_RUN_TIMED_OUT;
    }
    return 0;
}

/* The single instance on Windows (platform/instance.h): each launcher marks
 * itself running with a named mutex of its own, kept to its end, as the
 * original's every instance keeps its DeusExIsRunning (CreateMutex and
 * ERROR_ALREADY_EXISTS): Local\deusex-launcher.<hash>.<n>, the first number
 * no launcher holds, gone with the last handle to it, so a crashed
 * launcher's never reads as live. Another launcher is live where a mark of
 * another number opens. The channel is a named pipe,
 * \\.\pipe\deusex-launcher.<hash>, the first client's, one message a
 * command line: the launcher that has it waits for a sender to connect,
 * reads the message, and waits again; its event is set while either is
 * ready to go on. A pipe that will not wait is given up, the mark kept
 * alone.
 */
#include "platform/instance.h"
#include "platform/win32/internal.h"
#include "core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The marks' numbers: more launchers of a home at once than any player
 * starts. */
#define MARKS 64

struct dxl_instance {
    HANDLE     mutex;      /* its mark of a launcher running; NULL for none */
    int        mark;       /* the mark's number */
    HANDLE     pipe;       /* INVALID_HANDLE_VALUE: the mark alone */
    OVERLAPPED ov;
    int        reading;    /* a read pending, else a wait for a sender */
    DWORD      error;      /* the last wait's that failed, for the log */
    char       buf[DXL_HANDOFF_MAX + 1];
};

void (*dxl_instance_test_made)(const wchar_t *pipe);

/* FNV-1a over the id, as POSIX's: two homes get two names. */
static unsigned long id_hash(const char *id) {
    unsigned long h = 2166136261UL;
    for (const unsigned char *p = (const unsigned char *)(id ? id : ""); *p; p++) {
        h ^= *p;
        h *= 16777619UL;
    }
    return h & 0xffffffffUL;
}

static void mark_name(const char *id, int n, wchar_t *out, size_t size) {
    swprintf(out, size, L"Local\\deusex-launcher.%08lx.%d", id_hash(id), n);
}

static void pipe_name(const char *id, wchar_t *out, size_t size) {
    swprintf(out, size, L"\\\\.\\pipe\\deusex-launcher.%08lx", id_hash(id));
}

/* This launcher's mark: the mutex of the first number no launcher holds,
 * made here, its number into *n. NULL when every number is held. */
static HANDLE take_mark(const char *id, int *n) {
    for (int i = 0; i < MARKS; i++) {
        wchar_t name[64];
        mark_name(id, i, name, 64);
        HANDLE m = CreateMutexW(NULL, FALSE, name);
        if (!m) continue;
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            *n = i;
            return m;
        }
        CloseHandle(m);
    }
    return NULL;
}

/* The next wait: for a sender, or, connected, for its message. 0 when it
 * leaves a call in flight, or one done, its event set; -1 when the pipe
 * will not wait, each call failing at once: none then sets the event, and
 * one set by hand would wake every wait for nothing (bugs.md LPORT.7). */
static int arm(dxl_instance *inst) {
    /* A sender gone takes a turn, the wait for the next one another: four
     * are more than a working pipe needs. */
    for (int turn = 0; turn < 4; turn++) {
        ResetEvent(inst->ov.hEvent);
        if (!inst->reading) {
            if (ConnectNamedPipe(inst->pipe, &inst->ov)) {
                inst->reading = 1;
                continue;
            }
            DWORD e = GetLastError();
            if (e == ERROR_IO_PENDING) return 0;
            /* Connected before the wait; or come, written and gone before
             * it, the pipe closing (ERROR_NO_DATA): what it wrote is read
             * all the same, the read then finding its end. */
            if (e == ERROR_PIPE_CONNECTED || e == ERROR_NO_DATA) {
                inst->reading = 1;
                continue;
            }
            /* Failed at once: what is left of a sender dropped, the wait
             * made again. */
            inst->error = e;
            DisconnectNamedPipe(inst->pipe);
            continue;
        }
        DWORD got = 0;
        if (ReadFile(inst->pipe, inst->buf, DXL_HANDOFF_MAX, &got, &inst->ov)) {
            SetEvent(inst->ov.hEvent);   /* there already */
            return 0;
        }
        DWORD e = GetLastError();
        if (e == ERROR_IO_PENDING || e == ERROR_MORE_DATA) return 0;
        /* The sender went before it said anything: wait for the next. */
        inst->error = e;
        DisconnectNamedPipe(inst->pipe);
        inst->reading = 0;
    }
    return -1;
}

/* The channel given up, the mark kept, as -server's: a second start then
 * plays beside this launcher. */
static void give_up(dxl_instance *inst) {
    char why[256];
    dxl_log("the hand-over's pipe will not wait (%s): a second start plays beside this launcher",
            dxl_win_error(inst->error, why, sizeof why));
    CancelIo(inst->pipe);
    DisconnectNamedPipe(inst->pipe);
    CloseHandle(inst->pipe);
    CloseHandle(inst->ov.hEvent);
    inst->pipe = INVALID_HANDLE_VALUE;
    inst->ov.hEvent = NULL;
}

/* The handoff pipe, into inst: the first instance of its name, so none
 * while a launcher listens on it already (the first client, or one beside
 * a server). Failing to make it is not fatal -- the instance just cannot
 * receive URLs. */
static void open_pipe(dxl_instance *inst, const wchar_t *pname) {
    HANDLE p = CreateNamedPipeW(pname, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                1, 0, DXL_HANDOFF_MAX, 0, NULL);
    HANDLE e = p != INVALID_HANDLE_VALUE ? CreateEventW(NULL, TRUE, FALSE, NULL) : NULL;
    if (e) {
        inst->pipe = p;
        inst->ov.hEvent = e;
        if (dxl_instance_test_made) dxl_instance_test_made(pname);
        if (arm(inst) != 0) give_up(inst);
    } else if (p != INVALID_HANDLE_VALUE) {
        CloseHandle(p);
    }
}

static dxl_instance *acquire(const char *id, int listen) {
    wchar_t pname[64];
    pipe_name(id, pname, 64);
    dxl_instance *inst = dxl_xmalloc(sizeof *inst);
    memset(inst, 0, sizeof *inst);
    inst->mutex = take_mark(id, &inst->mark);
    inst->pipe = INVALID_HANDLE_VALUE;
    if (listen) open_pipe(inst, pname);
    if (!inst->mutex && inst->pipe == INVALID_HANDLE_VALUE) {
        free(inst);
        return NULL;
    }
    return inst;
}

dxl_instance *dxl_instance_acquire(const char *id) { return acquire(id, 1); }
dxl_instance *dxl_instance_acquire_lock(const char *id) { return acquire(id, 0); }

int dxl_instance_listens(const dxl_instance *inst) { return inst && inst->pipe != INVALID_HANDLE_VALUE; }

int dxl_instance_locked(const dxl_instance *inst) { return inst && inst->mutex; }

HANDLE dxl_instance_event(const dxl_instance *inst) { return dxl_instance_listens(inst) ? inst->ov.hEvent : NULL; }

void dxl_instance_release(dxl_instance *inst) {
    if (!inst) return;
    if (inst->pipe != INVALID_HANDLE_VALUE) {
        CancelIo(inst->pipe);
        DisconnectNamedPipe(inst->pipe);
        CloseHandle(inst->pipe);
        CloseHandle(inst->ov.hEvent);
    }
    if (inst->mutex) CloseHandle(inst->mutex);
    free(inst);
}

int dxl_instance_other_running(const dxl_instance *inst, const char *id) {
    for (int i = 0; i < MARKS; i++) {
        if (dxl_instance_locked(inst) && inst->mark == i) continue;
        wchar_t name[64];
        mark_name(id, i, name, 64);
        HANDLE m = OpenMutexW(SYNCHRONIZE, FALSE, name);
        if (m) {
            CloseHandle(m);
            return 1;
        }
    }
    return 0;
}

/* How long a pipe that is not there is waited for: a launcher makes its
 * pipe straight after its mark, so one still starting has it by then; one
 * that holds the mark alone (-server's, or one whose pipe failed) never
 * does, and a start beside it goes on at once, as POSIX's does. */
#define NO_PIPE_MS 250

int dxl_instance_forward(const char *id, const char *message, int timeout_ms, dxl_err *err) {
    wchar_t pname[64];
    pipe_name(id, pname, 64);
    /* This start is the one in the foreground: the launcher it hands over
     * to may bring its window, or the game's, to the front, which Windows
     * lets a program in the background do only when the foreground one
     * allows it. */
    AllowSetForegroundWindow(ASFW_ANY);
    ULONGLONG start = GetTickCount64(), deadline = start + (ULONGLONG)(timeout_ms > 0 ? timeout_ms : 0);
    HANDLE h;
    char why[256];
    for (;;) {
        h = CreateFileW(pname, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) break;
        DWORD e = GetLastError();
        ULONGLONG now = GetTickCount64();
        if ((e != ERROR_PIPE_BUSY && e != ERROR_FILE_NOT_FOUND) || now >= deadline ||
            (e == ERROR_FILE_NOT_FOUND && now - start >= NO_PIPE_MS)) {
            dxl_err_set(err, "handoff failed: %s", dxl_win_error(e, why, sizeof why));
            return -1;
        }
        /* The primary between two senders: wait for it, a little at a time. */
        if (e == ERROR_PIPE_BUSY) WaitNamedPipeW(pname, (DWORD)(deadline - now < 100 ? deadline - now : 100));
        else Sleep(20);
    }
    size_t mlen = strlen(message ? message : "");
    if (mlen > DXL_HANDOFF_MAX) mlen = DXL_HANDOFF_MAX;
    DWORD wrote = 0;
    /* Into the pipe's buffer: no flush, which on a pipe waits until the
     * primary has read it. */
    BOOL ok = WriteFile(h, message ? message : "", (DWORD)mlen, &wrote, NULL);
    DWORD e = GetLastError();
    CloseHandle(h);
    if (!ok || wrote != mlen) {
        dxl_err_set(err, "handoff failed: %s", dxl_win_error(e, why, sizeof why));
        return -1;
    }
    return 0;
}

int dxl_instance_poll(dxl_instance *inst, char *buf, size_t size) {
    if (!dxl_instance_listens(inst) || size == 0) return 0;
    for (int steps = 0; steps < 4; steps++) {
        if (WaitForSingleObject(inst->ov.hEvent, 0) != WAIT_OBJECT_0) return 0;
        DWORD got = 0;
        BOOL done = GetOverlappedResult(inst->pipe, &inst->ov, &got, FALSE);
        DWORD e = done ? 0 : GetLastError();
        if (!done && e == ERROR_IO_INCOMPLETE) return 0;
        int have = 0;
        if (!inst->reading) {
            /* A sender connected, gone already or not: its message read
             * next. Or the wait failed: what is left dropped, the wait made
             * again. */
            if (done || e == ERROR_PIPE_CONNECTED || e == ERROR_NO_DATA) {
                inst->reading = 1;
            } else {
                inst->error = e;
                DisconnectNamedPipe(inst->pipe);
            }
        } else {
            /* Its message, whole or cut at the buffer; then the next sender. */
            have = done || e == ERROR_MORE_DATA;
            if (have) {
                size_t n = got < size - 1 ? got : size - 1;
                memcpy(buf, inst->buf, n);
                buf[n] = '\0';
            }
            DisconnectNamedPipe(inst->pipe);
            inst->reading = 0;
        }
        if (arm(inst) != 0) {
            give_up(inst);
            return have;
        }
        if (have) return 1;
    }
    return 0;
}

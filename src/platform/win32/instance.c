/* The single instance on Windows (platform/instance.h): the lock is a
 * named mutex, Local\deusex-launcher.<hash>, the original's own way
 * (CreateMutex and ERROR_ALREADY_EXISTS), gone with the last handle to it,
 * so a crashed launcher's never reads as live. The channel is a named pipe,
 * \\.\pipe\deusex-launcher.<hash>, one message a command line: the primary
 * waits for a sender to connect, reads the message, and waits again; its
 * event is set while either is ready to go on.
 */
#include "platform/instance.h"
#include "platform/win32/internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct dxl_instance {
    HANDLE     mutex;
    HANDLE     pipe;       /* INVALID_HANDLE_VALUE: the lock alone */
    OVERLAPPED ov;
    int        reading;    /* a read pending, else a wait for a sender */
    char       buf[DXL_HANDOFF_MAX + 1];
};

/* FNV-1a over the id, as POSIX's: two homes get two names. */
static unsigned long id_hash(const char *id) {
    unsigned long h = 2166136261UL;
    for (const unsigned char *p = (const unsigned char *)(id ? id : ""); *p; p++) {
        h ^= *p;
        h *= 16777619UL;
    }
    return h & 0xffffffffUL;
}

static void names(const char *id, wchar_t *mutex, wchar_t *pipe, size_t size) {
    swprintf(mutex, size, L"Local\\deusex-launcher.%08lx", id_hash(id));
    swprintf(pipe, size, L"\\\\.\\pipe\\deusex-launcher.%08lx", id_hash(id));
}

/* The next wait: for a sender, or, connected, for its message. */
static void arm(dxl_instance *inst) {
    for (;;) {
        ResetEvent(inst->ov.hEvent);
        if (!inst->reading) {
            if (ConnectNamedPipe(inst->pipe, &inst->ov)) {
                inst->reading = 1;
                continue;
            }
            DWORD e = GetLastError();
            if (e == ERROR_PIPE_CONNECTED) {
                inst->reading = 1;
                continue;
            }
            if (e != ERROR_IO_PENDING) SetEvent(inst->ov.hEvent);   /* to be tried again */
            return;
        }
        DWORD got = 0;
        if (ReadFile(inst->pipe, inst->buf, DXL_HANDOFF_MAX, &got, &inst->ov)) {
            SetEvent(inst->ov.hEvent);   /* there already */
            return;
        }
        DWORD e = GetLastError();
        if (e == ERROR_IO_PENDING || e == ERROR_MORE_DATA) return;
        /* The sender went before it said anything: wait for the next. */
        DisconnectNamedPipe(inst->pipe);
        inst->reading = 0;
    }
}

static dxl_instance *acquire(const char *id, int listen) {
    wchar_t mname[64], pname[64];
    names(id, mname, pname, 64);
    HANDLE m = CreateMutexW(NULL, TRUE, mname);
    if (!m) return NULL;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(m);
        return NULL;                      /* someone else holds it */
    }
    dxl_instance *inst = dxl_xmalloc(sizeof *inst);
    memset(inst, 0, sizeof *inst);
    inst->mutex = m;
    inst->pipe = INVALID_HANDLE_VALUE;
    /* The handoff channel. Failing to open it is not fatal -- we are still the
     * only instance, we just cannot receive URLs. */
    if (listen) {
        HANDLE p = CreateNamedPipeW(pname, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                    1, 0, DXL_HANDOFF_MAX, 0, NULL);
        HANDLE e = p != INVALID_HANDLE_VALUE ? CreateEventW(NULL, TRUE, FALSE, NULL) : NULL;
        if (e) {
            inst->pipe = p;
            inst->ov.hEvent = e;
            arm(inst);
        } else if (p != INVALID_HANDLE_VALUE) {
            CloseHandle(p);
        }
    }
    return inst;
}

dxl_instance *dxl_instance_acquire(const char *id) { return acquire(id, 1); }
dxl_instance *dxl_instance_acquire_lock(const char *id) { return acquire(id, 0); }

int dxl_instance_listens(const dxl_instance *inst) { return inst && inst->pipe != INVALID_HANDLE_VALUE; }

HANDLE dxl_instance_event(const dxl_instance *inst) { return dxl_instance_listens(inst) ? inst->ov.hEvent : NULL; }

void dxl_instance_release(dxl_instance *inst) {
    if (!inst) return;
    if (inst->pipe != INVALID_HANDLE_VALUE) {
        CancelIo(inst->pipe);
        DisconnectNamedPipe(inst->pipe);
        CloseHandle(inst->pipe);
        CloseHandle(inst->ov.hEvent);
    }
    ReleaseMutex(inst->mutex);
    CloseHandle(inst->mutex);
    free(inst);
}

int dxl_instance_other_running(const char *id) {
    wchar_t mname[64], pname[64];
    names(id, mname, pname, 64);
    HANDLE m = OpenMutexW(SYNCHRONIZE, FALSE, mname);
    if (!m) return 0;
    CloseHandle(m);
    return 1;
}

int dxl_instance_forward(const char *id, const char *message, int timeout_ms, dxl_err *err) {
    wchar_t mname[64], pname[64];
    names(id, mname, pname, 64);
    /* This start is the one in the foreground: the launcher it hands over
     * to may bring its window, or the game's, to the front, which Windows
     * lets a program in the background do only when the foreground one
     * allows it. */
    AllowSetForegroundWindow(ASFW_ANY);
    ULONGLONG deadline = GetTickCount64() + (ULONGLONG)(timeout_ms > 0 ? timeout_ms : 0);
    HANDLE h;
    char why[256];
    for (;;) {
        h = CreateFileW(pname, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) break;
        DWORD e = GetLastError();
        ULONGLONG now = GetTickCount64();
        if ((e != ERROR_PIPE_BUSY && e != ERROR_FILE_NOT_FOUND) || now >= deadline) {
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
        if (!inst->reading) {
            /* A sender connected (or the wait failed): read its message. */
            inst->reading = done || e == ERROR_PIPE_CONNECTED;
            arm(inst);
            continue;
        }
        /* Its message, whole or cut at the buffer; then the next sender. */
        int have = done || e == ERROR_MORE_DATA;
        if (have) {
            size_t n = got < size - 1 ? got : size - 1;
            memcpy(buf, inst->buf, n);
            buf[n] = '\0';
        }
        DisconnectNamedPipe(inst->pipe);
        inst->reading = 0;
        arm(inst);
        if (have) return 1;
    }
    return 0;
}

/* Waiting on Windows (platform/wait.h): a wake is an event, and the single
 * instance's line one more, WaitForMultipleObjects on them all.
 */
#include "platform/wait.h"
#include "platform/win32/internal.h"

#include <stdlib.h>

/* A manual-reset event: set from any thread -- the console's handler runs
 * on one of its own -- until it is cleared. */
struct dxl_wake {
    HANDLE event;
};

dxl_wake *dxl_wake_new(void) {
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!e) return NULL;
    dxl_wake *w = dxl_xmalloc(sizeof *w);
    w->event = e;
    return w;
}

void dxl_wake_free(dxl_wake *w) {
    if (!w) return;
    CloseHandle(w->event);
    free(w);
}

void dxl_wake_set(dxl_wake *w) {
    if (w) SetEvent(w->event);
}

void dxl_wake_clear(dxl_wake *w) {
    if (w) ResetEvent(w->event);
}

int dxl_wait(dxl_wake *const *wakes, int n, dxl_instance *inst, int ms) {
    HANDLE h[9];
    int bit[9], k = 0;
    for (int i = 0; i < n && k < 8; i++)
        if (wakes[i]) {
            h[k] = wakes[i]->event;
            bit[k++] = 1 << i;
        }
    HANDLE line = dxl_instance_event(inst);
    if (line) {
        h[k] = line;
        bit[k++] = DXL_WAIT_LINE;
    }
    if (!k) {
        Sleep(ms < 0 ? INFINITE : (DWORD)ms);
        return 0;
    }
    DWORD r = WaitForMultipleObjects((DWORD)k, h, FALSE, ms < 0 ? INFINITE : (DWORD)ms);
    if (r == WAIT_TIMEOUT) return 0;
    if (r >= WAIT_OBJECT_0 + (DWORD)k) return -1;
    /* Each that is set, not the first alone. */
    int got = 0;
    for (int i = 0; i < k; i++)
        if (WaitForSingleObject(h[i], 0) == WAIT_OBJECT_0) got |= bit[i];
    return got;
}

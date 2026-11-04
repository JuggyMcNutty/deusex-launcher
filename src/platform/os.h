/* Processes where POSIX and Windows differ: this one's environment, its
 * id, a pause; and how a program the launcher started ended when it did
 * not exit by itself. platform/posix/os.c is POSIX's.
 */
#ifndef DXL_OS_H
#define DXL_OS_H

#include <stddef.h>

/* Sets name to value in this process's environment; a NULL value takes it
 * out. 0, or -1. */
int  dxl_env_set(const char *name, const char *value);

/* This process's id. */
long dxl_os_pid(void);

/* Sleeps ms milliseconds. */
void dxl_os_sleep_ms(int ms);

/* Milliseconds by a clock no one sets, from some start of its own: what
 * time a wait took. */
long long dxl_os_ms(void);

/* Whether the system's devices belong to an owner this process cannot
 * name, the host's root unmapped as in a rootless container (podman's, so
 * distrobox's and toolbox's): /dev/input owned by the kernel's overflow uid.
 * There every udev message comes from that uid, which libudev drops. */
int dxl_os_host_root_unmapped(void);

/* What ended a program that did not exit by itself (dxl_game's signal,
 * LastRun.ini's Signal) -- a signal here: its name, "SIGSEGV", or "signal
 * <n>" for one without a name here. */
const char *dxl_end_name(int end, char *buf, size_t size);
/* Whether that end was the program's own crash, not a stop. */
int dxl_end_crashed(int end);

/* The virtual-key code the keyboard layout in use gives a key by its
 * platform scancode (SDL's raw): on Windows MapVirtualKeyEx's, by which
 * DXEngine's SDL3 window names a typing key there; 0 elsewhere, and for
 * none. */
int dxl_os_key_vk(int raw);

#endif

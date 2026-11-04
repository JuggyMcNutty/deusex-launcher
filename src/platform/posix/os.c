#define _GNU_SOURCE
#include "platform/os.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int dxl_env_set(const char *name, const char *value) {
    return value ? setenv(name, value, 1) : unsetenv(name);
}

long dxl_os_pid(void) { return (long)getpid(); }

void dxl_os_sleep_ms(int ms) {
    struct timespec t = { ms / 1000, (long)(ms % 1000) * 1000000L };
    while (nanosleep(&t, &t) != 0 && errno == EINTR) {}
}

long long dxl_os_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

int dxl_os_host_root_unmapped(void) {
    struct stat st;
    if (stat("/dev/input", &st) != 0) return 0;
    unsigned long overflow = 65534;
    FILE *f = fopen("/proc/sys/kernel/overflowuid", "r");
    if (f) {
        if (fscanf(f, "%lu", &overflow) != 1) overflow = 65534;
        fclose(f);
    }
    return (unsigned long)st.st_uid == overflow;
}

const char *dxl_end_name(int end, char *buf, size_t size) {
    static const struct { int sig; const char *name; } names[] = {
        { SIGHUP, "SIGHUP" }, { SIGINT, "SIGINT" }, { SIGQUIT, "SIGQUIT" }, { SIGILL, "SIGILL" },
        { SIGTRAP, "SIGTRAP" }, { SIGABRT, "SIGABRT" }, { SIGBUS, "SIGBUS" }, { SIGFPE, "SIGFPE" },
        { SIGKILL, "SIGKILL" }, { SIGSEGV, "SIGSEGV" }, { SIGPIPE, "SIGPIPE" }, { SIGALRM, "SIGALRM" },
        { SIGTERM, "SIGTERM" },
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++)
        if (names[i].sig == end) return names[i].name;
    snprintf(buf, size, "signal %d", end);
    return buf;
}

int dxl_end_crashed(int end) {
    return end == SIGSEGV || end == SIGBUS || end == SIGABRT || end == SIGILL || end == SIGFPE;
}

int dxl_os_key_vk(int raw) {
    (void)raw;
    return 0;
}

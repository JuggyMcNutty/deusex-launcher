/* A Vulkan driver that crashes as the loader loads it, as a broken driver
 * or layer does in vkCreateInstance: the GPU probe's test of a crash that
 * loses Vulkan's answers alone (test_gpu_probe.c, bugs.md LAPP.30). */
#include <stdlib.h>

__attribute__((constructor)) static void crash(void) { abort(); }

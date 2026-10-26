/* GPU detection on Windows (platform/gpu_probe.h): the Vulkan loader,
 * vulkan-1.dll, opened at run time, and desktop OpenGL through WGL on a
 * hidden window. Windows has no OpenGL ES of its own. It runs in dxl-cli,
 * a process of the launcher's own already, on a thread of its own here so
 * a driver that hangs is left behind when the time is up; one that crashes
 * takes dxl-cli with it, not the launcher.
 */
#include "platform/gpu_probe.h"
#include "platform/win32/internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- Vulkan ---------------------------------------------------------- */

/* As much of Vulkan's API as the probe asks for, by the specification's
 * layout: no SDK's headers needed. */
typedef struct VkInstance_T *VkInstance;
typedef struct VkPhysicalDevice_T *VkPhysicalDevice;
typedef int32_t VkResult;
typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(VkInstance, const char *);
typedef struct {
    int32_t sType;
    const void *pNext;
    const char *pApplicationName;
    uint32_t applicationVersion;
    const char *pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
} VkApplicationInfo;
typedef struct {
    int32_t sType;
    const void *pNext;
    uint32_t flags;
    const VkApplicationInfo *pApplicationInfo;
    uint32_t enabledLayerCount;
    const char *const *ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char *const *ppEnabledExtensionNames;
} VkInstanceCreateInfo;
/* VkPhysicalDeviceProperties: its first fields, and room for the rest. */
typedef struct {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    int32_t deviceType;
    char deviceName[256];
    uint8_t rest[2048];
} VkPhysicalDeviceProperties;
typedef VkResult (*PFN_vkCreateInstance)(const VkInstanceCreateInfo *, const void *, VkInstance *);
typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(VkInstance, uint32_t *, VkPhysicalDevice *);
typedef void (*PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice, VkPhysicalDeviceProperties *);
typedef void (*PFN_vkDestroyInstance)(VkInstance, const void *);
#define VK_SUCCESS 0
#define VK_INCOMPLETE 5
#define VK_STRUCTURE_TYPE_APPLICATION_INFO 0
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1
#define VK_PHYSICAL_DEVICE_TYPE_CPU 4

static void probe_vulkan(dxl_gpu_probe *out) {
    HMODULE lib = LoadLibraryW(L"vulkan-1.dll");
    if (!lib) return;
    PFN_vkGetInstanceProcAddr gipa = (PFN_vkGetInstanceProcAddr)(void *)GetProcAddress(lib, "vkGetInstanceProcAddr");
    PFN_vkCreateInstance create = gipa ? (PFN_vkCreateInstance)(void *)gipa(NULL, "vkCreateInstance") : NULL;
    if (!create) {
        FreeLibrary(lib);
        return;
    }
    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "deusex-launcher probe",
        .apiVersion = 1u << 22,   /* 1.0 */
    };
    VkInstanceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
    };
    VkInstance inst = NULL;
    if (create(&ci, NULL, &inst) != VK_SUCCESS || !inst) {
        FreeLibrary(lib);
        return;
    }
    PFN_vkEnumeratePhysicalDevices enumerate =
        (PFN_vkEnumeratePhysicalDevices)(void *)gipa(inst, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties props =
        (PFN_vkGetPhysicalDeviceProperties)(void *)gipa(inst, "vkGetPhysicalDeviceProperties");
    PFN_vkDestroyInstance destroy = (PFN_vkDestroyInstance)(void *)gipa(inst, "vkDestroyInstance");
    if (enumerate && props) {
        VkPhysicalDevice devs[8];
        uint32_t n = 8;
        VkResult rc = enumerate(inst, &n, devs);
        if ((rc == VK_SUCCESS || rc == VK_INCOMPLETE) && n > 0) {
            /* The first GPU's: a CPU implementation (lavapipe) is no GPU,
             * as the engine passes it over for the software device. */
            static VkPhysicalDeviceProperties p;
            for (uint32_t i = 0; i < n; i++) {
                props(devs[i], &p);
                if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) continue;
                out->vulkan = 1;
                snprintf(out->vulkan_device, sizeof out->vulkan_device, "%.127s", p.deviceName);
                snprintf(out->vulkan_version, sizeof out->vulkan_version, "%u.%u.%u", p.apiVersion >> 22,
                         (p.apiVersion >> 12) & 0x3ffu, p.apiVersion & 0xfffu);
                break;
            }
        }
    }
    if (destroy) destroy(inst, NULL);
    FreeLibrary(lib);
}

/* ---- OpenGL ------------------------------------------------------------ */

#define DXL_GL_RENDERER 0x1F01
#define DXL_GL_VERSION  0x1F02

/* Mesa's CPU renderers, as GL_RENDERER names them */
static int cpu_renderer(const char *renderer) {
    return renderer && (strstr(renderer, "llvmpipe") || strstr(renderer, "softpipe"));
}

/* Desktop OpenGL: a context on a hidden window, its GL_VERSION, the GL
 * device's as the engine's Win32 window makes one. */
static void probe_wgl(dxl_gpu_probe *out) {
    HINSTANCE self = GetModuleHandleW(NULL);
    WNDCLASSW wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = self;
    wc.lpszClassName = L"dxl-probe";
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 16, 16, NULL, NULL, self, NULL);
    if (!w) return;
    HDC dc = GetDC(w);
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof pfd);
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    int format = ChoosePixelFormat(dc, &pfd);
    if (format && SetPixelFormat(dc, format, &pfd)) {
        HGLRC ctx = wglCreateContext(dc);
        if (ctx && wglMakeCurrent(dc, ctx)) {
            typedef const unsigned char *(WINAPI *get_string_fn)(unsigned int);
            HMODULE gl = GetModuleHandleW(L"opengl32.dll");
            get_string_fn get = gl ? (get_string_fn)(void *)GetProcAddress(gl, "glGetString") : NULL;
            const char *v = get ? (const char *)get(DXL_GL_VERSION) : NULL;
            const char *renderer = get ? (const char *)get(DXL_GL_RENDERER) : NULL;
            /* The renderer list asks for desktop GL; the GL device wants 4.2,
             * and a GPU's: llvmpipe is none, as the engine takes it. */
            int major = 0, minor = 0;
            if (v && sscanf(v, "%d.%d", &major, &minor) == 2)
                out->gl = (major > 4 || (major == 4 && minor >= 2)) && !cpu_renderer(renderer);
            wglMakeCurrent(NULL, NULL);
        }
        if (ctx) wglDeleteContext(ctx);
    }
    ReleaseDC(w, dc);
    DestroyWindow(w);
}

/* ---- on a thread of its own -------------------------------------------- */

static DWORD WINAPI probe_main(LPVOID p) {
    dxl_gpu_probe *r = p;
    probe_vulkan(r);
    probe_wgl(r);
    r->probed = 1;
    return 0;
}

int dxl_gpu_probe_run(dxl_gpu_probe *out, int timeout_ms) {
    memset(out, 0, sizeof *out);
    /* Its own copy: a thread left behind still writes to it. */
    static dxl_gpu_probe got;
    memset(&got, 0, sizeof got);
    HANDLE t = CreateThread(NULL, 0, probe_main, &got, 0, NULL);
    if (!t) {
        snprintf(out->note, sizeof out->note, "no thread for the probe");
        return -1;
    }
    DWORD r = WaitForSingleObject(t, timeout_ms > 0 ? (DWORD)timeout_ms : INFINITE);
    CloseHandle(t);
    if (r != WAIT_OBJECT_0 || !got.probed) {
        snprintf(out->note, sizeof out->note, "detection timed out after %d ms", timeout_ms);
        return -1;
    }
    *out = got;
    return 0;
}

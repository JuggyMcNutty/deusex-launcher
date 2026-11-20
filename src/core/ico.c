#include "core/ico.h"

#include <stdint.h>
#include <string.h>

static void put32(unsigned char *p, uint32_t v) {
    for (int b = 0; b < 4; b++) p[b] = (unsigned char)(v >> (8 * b));
}

int dxl_ico_pack(const dxl_ico_image *images, int n, dxl_buf *out) {
    if (n < 1 || n > 255) return -1;
    for (int i = 0; i < n; i++)
        if (images[i].px < 1 || images[i].px > 256 || images[i].len > UINT32_MAX) return -1;
    unsigned char header[6] = { 0, 0, 1, 0, (unsigned char)n, 0 };   /* reserved, an icon, its count */
    dxl_buf_add(out, (const char *)header, sizeof header);
    uint32_t offset = 6 + 16 * (uint32_t)n;
    for (int i = 0; i < n; i++) {
        unsigned char e[16];
        memset(e, 0, sizeof e);
        int px = images[i].px;
        e[0] = e[1] = (unsigned char)(px >= 256 ? 0 : px);   /* 0 means 256 */
        e[4] = 1;                                            /* planes */
        e[6] = 32;                                           /* bits */
        put32(e + 8, (uint32_t)images[i].len);
        put32(e + 12, offset);
        offset += (uint32_t)images[i].len;
        dxl_buf_add(out, (const char *)e, sizeof e);
    }
    for (int i = 0; i < n; i++) dxl_buf_add(out, images[i].png, images[i].len);
    return 0;
}

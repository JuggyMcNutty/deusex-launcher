/* dxl-icon: the launcher's icon (ui/icon.h) as a Windows icon file, a PNG
 * at each of core/ico.h's sizes, packed as the Start menu's is. Run by hand
 * when the drawing changes, for the file the Windows programs' resources
 * take in (ports/windows-x86_64/win32/DXLauncher.ico, and DXEngine's copy
 * of it); test_icon fails while that file is not this drawing. Not part of
 * the app.
 *
 *   dxl-icon <file.ico>
 */
#include "core/ico.h"
#include "core/paths.h"
#include "ui/icon.h"

#include <SDL3/SDL.h>
#include <stdio.h>

static const int sizes[] = { DXL_ICO_SIZES };
#define COUNT ((int)(sizeof sizes / sizeof *sizes))

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: dxl-icon <file.ico>\n");
        return 2;
    }
    SDL_IOStream *png[COUNT] = { 0 };
    dxl_ico_image images[COUNT];
    dxl_buf out;
    dxl_buf_init(&out);
    dxl_err e = { { 0 } };
    int rc = 1;
    for (int i = 0; i < COUNT; i++) {
        SDL_Surface *s = dxl_icon(sizes[i]);
        png[i] = SDL_IOFromDynamicMem();
        int ok = s && png[i] && SDL_SavePNG_IO(s, png[i], false);
        SDL_DestroySurface(s);
        const char *bytes = ok ? SDL_GetPointerProperty(SDL_GetIOProperties(png[i]),
                                                        SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL)
                               : NULL;
        Sint64 len = ok ? SDL_GetIOSize(png[i]) : 0;
        if (!bytes || len <= 0) {
            fprintf(stderr, "dxl-icon: no %d px image: %s\n", sizes[i], SDL_GetError());
            goto done;
        }
        images[i] = (dxl_ico_image){ sizes[i], bytes, (size_t)len };
    }
    if (dxl_ico_pack(images, COUNT, &out) != 0) fprintf(stderr, "dxl-icon: the images cannot be packed\n");
    else if (dxl_path_write_atomic(argv[1], out.data, out.len, &e) != 0) fprintf(stderr, "dxl-icon: %s\n", dxl_err_msg(&e));
    else rc = 0;
done:
    dxl_buf_free(&out);
    for (int i = 0; i < COUNT; i++)
        if (png[i]) SDL_CloseIO(png[i]);
    return rc;
}

/* What to do after a run that did not end cleanly, or that the software
 * renderer drew in the chosen one's place, for Recovery: the engine's known
 * errors, from its own messages (DXEngine's Exception::Throw), and the ways
 * a run ends, each with advice and the fix Recovery offers for it. */
#ifndef DXL_ADVICE_H
#define DXL_ADVICE_H

#include "core/lastrun.h"

typedef enum {
    DXL_FIX_NONE,
    DXL_FIX_RENDERER,   /* another renderer, on Graphics or here */
    DXL_FIX_NOSOUND,    /* the next run without sound */
    DXL_FIX_INSTALL,    /* the game's folder: check it, or another */
    DXL_FIX_APP,        /* the launcher's own folder: stage or install it again */
    DXL_FIX_SAVES       /* the profile's saves: another to load, the one that did not, deleted */
} dxl_fix;

typedef struct {
    const char *advice;
    dxl_fix     fix;
} dxl_advice;

/* The advice for how the run ended; NULL for a clean end on the renderer
 * chosen, or none known. */
const dxl_advice *dxl_advice_for(const dxl_lastrun *l);

#endif

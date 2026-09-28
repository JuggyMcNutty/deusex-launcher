/* Safe mode's options: the SafeOptions page, dialog 2021
 * (dx-reverse-info/wizard.md "SafeOptions").
 *
 * The page does not change anything in place. Run! builds a string of flags
 * from the ticked boxes, deletes <Package>.ini if Reset is ticked, starts the
 * launcher again with just those flags, and ends. The flags are the original's
 * strings, each with the space before it that the original's has; what the
 * engine does with each: dx-reverse-info/cli-flags.md.
 *
 * All eight boxes are wired. In the shipped binary boxes 3, 4 and 5 read box 2,
 * so "Disable 3D sound hardware" also gave -nohard, -noddraw and -defaultres,
 * and those three boxes did nothing (seen live too); that is a bug, and is not
 * reproduced here.
 */
#ifndef DXL_SAFEMODE_H
#define DXL_SAFEMODE_H

#include "core/common.h"

/* In the page's order, top to bottom. */
typedef struct {
    int no_sound;       /* IDC_NoSound      1108  -nosound             */
    int no_3d_sound;    /* IDC_No3DSound    1109  -no3dsound           */
    int no_3d_video;    /* IDC_No3dVideo    1110  -nohard              */
    int window;         /* IDC_Window       1112  -nohard -noddraw     */
    int res;            /* IDC_Res          1111  -defaultres          */
    int reset_config;   /* IDC_ResetConfig  1113  delete <Package>.ini */
    int no_processor;   /* IDC_NoProcessor  1114  -nommx -nokni -nok6  */
    int no_joy;         /* IDC_NoJoy        1115  -nojoy               */
} dxl_safe_options;

/* How the page opens: every box ticked but Reset. */
dxl_safe_options dxl_safe_defaults(void);

/* The relaunch's command line, box by box. Caller frees. */
char *dxl_safe_flags(const dxl_safe_options *o);

/* Reset: deletes <system_dir>/<package>.ini. 0 when it is gone afterwards,
 * whether or not it was there. */
int dxl_safe_reset_config(const char *system_dir, const char *package, dxl_err *err);

#endif

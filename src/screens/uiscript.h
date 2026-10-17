/* A script of input for the window, for runs on the desktop's display
 * that no one sits at: DXL_UI_SCRIPT=<file>. Its input goes through SDL's
 * event queue, as a player's would, into the same loop and pages.
 *
 *   wait <seconds>    before the next line
 *   key <name>        a key pressed, by SDL's name for it: Down, Return, Escape, Tab, F5
 *   expect <name>     the page or dialog up: home, welcome, find, import, recovery,
 *                     about; browser, message. Not so: "ui script: FAILED" in the
 *                     launcher's log and on stderr, and the window closes
 *   shot <file>       the window's next frame as a PNG
 *   quit              the window's close
 *
 * Blank lines and those starting with # are skipped. The script runs on
 * across the window's screens -- the first run, Home, the game, Recovery --
 * a wait counting from the line before.
 */
#ifndef DXL_UISCRIPT_H
#define DXL_UISCRIPT_H

#include "screens/shell.h"

typedef struct dxl_uiscript dxl_uiscript;

/* DXL_UI_SCRIPT's, or NULL when it is not set or cannot be read. */
dxl_uiscript *dxl_uiscript_open(void);
void          dxl_uiscript_free(dxl_uiscript *u);

/* Before a frame at now: the lines whose time has come. Returns the
 * seconds until the next, or -1 when the script is done. */
double        dxl_uiscript_step(dxl_uiscript *u, dxl_shell *s, double now);
/* After the frame is drawn, before it is presented: a shot asked for. */
void          dxl_uiscript_shoot(dxl_uiscript *u, SDL_Renderer *r);

#endif

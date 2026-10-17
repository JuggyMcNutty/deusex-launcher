/* A script of input for the window, for runs on the desktop's display
 * that no one sits at: DXL_UI_SCRIPT=<file>. Its input goes through SDL's
 * event queue, as a player's would, into the same loop and pages.
 *
 *   wait <seconds>    before the next line
 *   key <name> [*<n>] a key pressed, by SDL's name for it: Down, Return, Escape, Tab,
 *                     F5; n times over
 *   type <text>       text typed, as an input method gives it
 *   find <label>      the focus on the control of that label (ui/kit.h's names):
 *                     Down pressed, a frame each, until one is drawn, then Up
 *                     once the focus stays put at the bottom, and at the top
 *                     Tabs, one more each time, into another column, and Down
 *                     again
 *   expect <name>     the page or dialog up: home, welcome, find, import, recovery,
 *                     about, settings, log; browser, message, pick, keyboard,
 *                     confirm, padwizard. Not so: "ui script: FAILED" in the
 *                     launcher's log and on stderr, and the window closes
 *   pad attach <kind> a virtual controller: xbox, dualsense, switchpro or generic,
 *                     as SDL knows them, or raw, one it has no mapping for (13
 *                     buttons, 6 axes, a hat), at rest, as SDL first reads it;
 *                     pad detach takes it away
 *   pad button <n> <down|up>   its button n (a known one's in SDL's order: 0 south)
 *   pad axis <n> <value>       its axis n, -1 to 1
 *   pad hat <n> <mask>         its hat n: 1 up, 2 right, 4 down, 8 left
 *   shot <file>       the window's next frame as a PNG
 *   quit              the window's close
 *
 * Blank lines and those starting with # are skipped. The script runs on
 * across the window's screens -- the first run, Home, the game, Recovery --
 * a wait counting from the line before; a frame takes each key and text
 * before the next line runs.
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

/* SDL's game controller mappings, as text: "<GUID>,<name>,<control>:<source>,
 * ...,platform:<system>,". A control is SDL's name for it (a for the south
 * button, b east, x west, y north, back, guide, start, the sticks' clicks,
 * the shoulders, the d-pad's four, the sticks' axes, the triggers); a
 * source the joystick's button (b3), hat (h0.4: hat 0, down -- 1 up, 2
 * right, 4 down, 8 left), axis (a2), half an axis (+a2, -a2), inverted
 * (a2~). The mapping wizard makes one for a pad SDL has none for; the
 * launcher keeps its own in gamecontrollerdb.txt, one a line, and hands
 * them to the engine with Steam's (SDL_GAMECONTROLLERCONFIG).
 *
 * Since SDL 2.26 a GUID holds a CRC of the pad's name, its bytes 2 and 3,
 * so that pads of one maker's ids but other names map apart; a mapping may
 * name it in a field instead, crc:<the 16 bits, as hex>. Older SDLs make
 * GUIDs without it, and pass over a field they do not know.
 */
#ifndef DXL_PADMAP_H
#define DXL_PADMAP_H

#include "core/common.h"

#include <stddef.h>

typedef enum { DXL_PADSRC_NONE, DXL_PADSRC_BUTTON, DXL_PADSRC_HAT, DXL_PADSRC_AXIS } dxl_padsrc_kind;

typedef struct {
    dxl_padsrc_kind kind;
    int index;      /* the button's, hat's or axis's number */
    int hat_mask;   /* a hat's direction */
    int half;       /* an axis: 0 the whole, 1 its positive half, -1 its negative */
    int invert;
} dxl_padsrc;

/* SDL's controls, in its order. */
enum {
    DXL_PADCTL_A, DXL_PADCTL_B, DXL_PADCTL_X, DXL_PADCTL_Y, DXL_PADCTL_BACK, DXL_PADCTL_GUIDE,
    DXL_PADCTL_START, DXL_PADCTL_LEFTSTICK, DXL_PADCTL_RIGHTSTICK, DXL_PADCTL_LEFTSHOULDER,
    DXL_PADCTL_RIGHTSHOULDER, DXL_PADCTL_DPUP, DXL_PADCTL_DPDOWN, DXL_PADCTL_DPLEFT, DXL_PADCTL_DPRIGHT,
    DXL_PADCTL_LEFTX, DXL_PADCTL_LEFTY, DXL_PADCTL_RIGHTX, DXL_PADCTL_RIGHTY, DXL_PADCTL_LEFTTRIGGER,
    DXL_PADCTL_RIGHTTRIGGER,
    DXL_PADCTL_COUNT
};
extern const char *const dxl_padctl_names[DXL_PADCTL_COUNT];

typedef struct {
    char guid[33];
    char name[128];
    dxl_padsrc src[DXL_PADCTL_COUNT];
} dxl_padmap;

/* A source as SDL's text has it: "b3", "h0.4", "+a2", "a1~"; "" for none. */
void dxl_padsrc_format(const dxl_padsrc *s, char *out, size_t size);
/* The mapping as SDL takes it: each control that has a source, then
 * platform:<platform>, SDL's name for the system it is made on
 * (SDL_GetPlatform()): a file of them gives SDL only the lines of its own
 * system's. A comma in the name is left out. */
void dxl_padmap_format(const dxl_padmap *m, const char *platform, char *out, size_t size);
/* How many controls it maps. */
int  dxl_padmap_controls(const dxl_padmap *m);
/* Whether the two map every control alike, whatever their GUIDs and names. */
int  dxl_padmap_same(const dxl_padmap *a, const dxl_padmap *b);
/* A mapping's text read back; controls SDL has and this does not keep
 * (paddles, misc1, touchpad) passed over. -1 for text that is no mapping. */
int  dxl_padmap_parse(const char *text, dxl_padmap *m);
/* The GUID with the CRC of its name zeroed (its bytes 2 and 3), as an SDL2
 * before 2.26 makes GUIDs: 1 when it had one. */
int  dxl_padmap_guid_no_crc(const char *guid, char out[33]);
/* The GUID's CRC as a mapping's crc: field names it ("df47" for a GUID whose
 * bytes 2 and 3 are 47 df); 0 when it has none. */
int  dxl_padmap_guid_crc(const char *guid, char out[5]);

/* gamecontrollerdb.txt's text, "" for none (caller frees); whether a text of
 * it maps the GUID. */
char *dxl_padmap_db_read(const char *path);
int   dxl_padmap_db_has(const char *text, const char *guid);
/* gamecontrollerdb.txt: the mapping in place of the line of its GUID, else
 * added, the file written whole. */
int  dxl_padmap_db_put(const char *path, const char *mapping, dxl_err *err);
/* The lines of steam's value (SDL_GAMECONTROLLERCONFIG as the launcher was
 * started with it) but those of a GUID the file maps: the launcher's
 * mapping of a controller is the one SDL takes, in the launcher and in the
 * game. "" when none is left; NULL for no value. */
char *dxl_padmap_without_ours(const char *steam, const char *db_path);
/* What SDL_GAMECONTROLLERCONFIG gives the engine: steam's value without
 * ours first (NULL for none), then each mapping of the file, and of each
 * whose GUID has a CRC a copy whose GUID has none and whose crc: field
 * names it, for an SDL2 before 2.26; one a line. NULL when there are none. */
char *dxl_padmap_engine_config(const char *steam, const char *db_path);

#endif

/* Unreal Engine 1 packages -- the game's .u, .utx, .uax, .umx and .dx files,
 * a save's .dxs -- read: our own code from the format, every read checked
 * against the bytes there are. What the launcher takes from the install at
 * run time -- the menu theme's colours, the main menu's art, the menu's
 * sounds, the title music, a save's description and snapshot -- comes
 * through here (core/upkg_assets.h); nothing read is ever written anywhere.
 *
 * A package is a header, a table of names, one of imports (objects of other
 * packages, by name) and one of exports (its own objects, each a run of
 * bytes in the file). An object reference is 0 for none, n > 0 for export
 * n - 1, n < 0 for import -n - 1. Counts and indices inside an object are
 * compact: a sign, then six bits, then seven at a time. An object's bytes
 * begin -- after a state frame, for an object with one -- with its tagged
 * properties, each a name, a type, a size and its value, ending at the name
 * None; then what its class adds (a texture's mipmaps, a sound's file).
 */
#ifndef DXL_UPKG_H
#define DXL_UPKG_H

#include "core/common.h"

#include <stdint.h>

#define DXL_UPKG_TAG 0x9E2A83C1u

typedef struct {
    int32_t class_package;   /* name index */
    int32_t class_name;      /* name index */
    int32_t package;         /* an object reference: the import it is in */
    int32_t object_name;     /* name index */
} dxl_upkg_import;

typedef struct {
    int32_t  class_ref;      /* an object reference; 0 for a class */
    int32_t  super_ref;
    int32_t  package;        /* an object reference: the export it is in, 0 for the package */
    int32_t  object_name;    /* name index */
    uint32_t flags;          /* RF_* */
    int32_t  size, offset;   /* its bytes in the file; size 0 for none */
} dxl_upkg_export;

/* An object with a state frame before its properties. */
#define DXL_RF_HAS_STACK 0x02000000u

typedef struct {
    uint8_t         *data;   /* the whole file */
    size_t           size;
    int              version, licensee;
    char           **names;
    int32_t          name_count;
    dxl_upkg_import *imports;
    int32_t          import_count;
    dxl_upkg_export *exports;
    int32_t          export_count;
} dxl_upkg;

/* The file at path, its tables read. -1 and why for a file that is no
 * package, or whose tables run past its end. */
int  dxl_upkg_load(dxl_upkg *p, const char *path, dxl_err *err);
/* The same from bytes, copied. */
int  dxl_upkg_parse(dxl_upkg *p, const void *data, size_t size, dxl_err *err);
void dxl_upkg_free(dxl_upkg *p);

/* The name of index i; "" for one out of range. */
const char *dxl_upkg_name(const dxl_upkg *p, int32_t i);
/* What an object reference names: its object's name, and its class's
 * ("Class" for a class); "" for none or one out of range. */
const char *dxl_upkg_ref_name(const dxl_upkg *p, int32_t ref);
const char *dxl_upkg_ref_class(const dxl_upkg *p, int32_t ref);
/* The export of that class and name (any case), or -1. class NULL: any. */
int32_t     dxl_upkg_find(const dxl_upkg *p, const char *class_name, const char *object_name);

/* ---- reading an object's bytes ----------------------------------------- */

typedef struct {
    const uint8_t *data;
    size_t         size, at;
    int            bad;      /* a read went past the end: every read after gives 0 */
} dxl_ureader;

/* Over export e's bytes; a reader of none for one out of range. */
void        dxl_ureader_export(dxl_ureader *r, const dxl_upkg *p, int32_t e);
uint8_t     dxl_ur_u8(dxl_ureader *r);
uint16_t    dxl_ur_u16(dxl_ureader *r);
uint32_t    dxl_ur_u32(dxl_ureader *r);
int32_t     dxl_ur_i32(dxl_ureader *r);
uint64_t    dxl_ur_u64(dxl_ureader *r);
float       dxl_ur_f32(dxl_ureader *r);
int32_t     dxl_ur_index(dxl_ureader *r);
/* n bytes, or NULL (and bad) when there are not n left. */
const uint8_t *dxl_ur_bytes(dxl_ureader *r, size_t n);
/* A string as the format keeps one: a compact length with its NUL, then
 * the bytes, into out as Latin-1 as it is; or a negative length, 16-bit
 * characters (one past 0xFF in it: the original's and DXEngine's saves), into
 * out as UTF-8, cut at a character; "" when it is none. */
void        dxl_ur_string(dxl_ureader *r, char *out, size_t size);
/* Past export e's state frame, when it has one: to its properties. */
void        dxl_ur_skip_stack(dxl_ureader *r, const dxl_upkg *p, int32_t e);

/* The tagged properties' types. */
enum {
    DXL_UPROP_BYTE = 1, DXL_UPROP_INT = 2, DXL_UPROP_BOOL = 3, DXL_UPROP_FLOAT = 4, DXL_UPROP_OBJECT = 5,
    DXL_UPROP_NAME = 6, DXL_UPROP_STRING = 7, DXL_UPROP_CLASS = 8, DXL_UPROP_ARRAY = 9, DXL_UPROP_STRUCT = 10,
    DXL_UPROP_VECTOR = 11, DXL_UPROP_ROTATOR = 12, DXL_UPROP_STR = 13, DXL_UPROP_MAP = 14, DXL_UPROP_FIXED_ARRAY = 15
};

typedef struct {
    const char *name;
    int         type;          /* DXL_UPROP_* */
    const char *struct_name;   /* DXL_UPROP_STRUCT's, else "" */
    int32_t     array_index;
    int         bool_value;
    dxl_ureader value;         /* over the value's bytes */
} dxl_uprop;

/* The next tagged property at r: 1, with r past it; 0 at None, the list's
 * end; -1 for a list that does not hold together. */
int  dxl_ur_prop(dxl_ureader *r, const dxl_upkg *p, dxl_uprop *out);

/* A property's value as its type reads: a byte's, an int's, a float's; a
 * name's or an object reference's index; a Str's text, as dxl_ur_string
 * gives it (Latin-1, or UTF-8 when 16-bit). */
int32_t dxl_uprop_int(const dxl_uprop *pr);
float   dxl_uprop_float(const dxl_uprop *pr);
int32_t dxl_uprop_index(const dxl_uprop *pr);
void    dxl_uprop_str(const dxl_uprop *pr, char *out, size_t size);

#endif

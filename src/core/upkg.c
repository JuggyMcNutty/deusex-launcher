#include "core/upkg.h"
#include "core/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The most of each table a package of the game's has many times over. */
#define MAX_NAMES   (1 << 20)
#define MAX_OBJECTS (1 << 20)

/* ---- the reader -------------------------------------------------------- */

static void ur_init(dxl_ureader *r, const uint8_t *data, size_t size) {
    r->data = data;
    r->size = size;
    r->at = 0;
    r->bad = 0;
}

const uint8_t *dxl_ur_bytes(dxl_ureader *r, size_t n) {
    if (r->bad || n > r->size - r->at) {
        r->bad = 1;
        return NULL;
    }
    const uint8_t *b = r->data + r->at;
    r->at += n;
    return b;
}

uint8_t dxl_ur_u8(dxl_ureader *r) {
    const uint8_t *b = dxl_ur_bytes(r, 1);
    return b ? b[0] : 0;
}

uint16_t dxl_ur_u16(dxl_ureader *r) {
    const uint8_t *b = dxl_ur_bytes(r, 2);
    return b ? (uint16_t)(b[0] | b[1] << 8) : 0;
}

uint32_t dxl_ur_u32(dxl_ureader *r) {
    const uint8_t *b = dxl_ur_bytes(r, 4);
    return b ? (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24 : 0;
}

int32_t dxl_ur_i32(dxl_ureader *r) { return (int32_t)dxl_ur_u32(r); }

uint64_t dxl_ur_u64(dxl_ureader *r) {
    uint64_t lo = dxl_ur_u32(r), hi = dxl_ur_u32(r);
    return lo | hi << 32;
}

float dxl_ur_f32(dxl_ureader *r) {
    uint32_t u = dxl_ur_u32(r);
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

int32_t dxl_ur_index(dxl_ureader *r) {
    uint8_t b = dxl_ur_u8(r);
    int neg = b & 0x80;
    uint32_t v = b & 0x3f;
    if (b & 0x40) {
        for (int shift = 6; shift < 34; shift += 7) {
            b = dxl_ur_u8(r);
            v |= (uint32_t)(b & 0x7f) << shift;
            if (!(b & 0x80)) break;
        }
    }
    if (v > 0x7fffffffu) {
        r->bad = 1;
        return 0;
    }
    return neg ? -(int32_t)v : (int32_t)v;
}

void dxl_ur_string(dxl_ureader *r, char *out, size_t size) {
    if (size) out[0] = '\0';
    int32_t n = dxl_ur_index(r);
    if (n <= 0) return;
    const uint8_t *b = dxl_ur_bytes(r, (size_t)n);
    if (!b || !size) return;
    size_t len = (size_t)n - 1;
    if (len >= size) len = size - 1;
    memcpy(out, b, len);
    out[len] = '\0';
}

/* ---- the tables --------------------------------------------------------- */

void dxl_upkg_free(dxl_upkg *p) {
    if (!p) return;
    for (int32_t i = 0; i < p->name_count; i++) free(p->names[i]);
    free(p->names);
    free(p->imports);
    free(p->exports);
    free(p->data);
    memset(p, 0, sizeof *p);
}

/* A table's place, checked: at off, count entries of at least min bytes. */
static int table_fits(const dxl_upkg *p, uint32_t count, uint32_t off, size_t min) {
    return off <= p->size && (size_t)count <= (p->size - off) / min;
}

static int read_tables(dxl_upkg *p, dxl_err *err) {
    dxl_ureader h;
    ur_init(&h, p->data, p->size);
    if (dxl_ur_u32(&h) != DXL_UPKG_TAG) {
        dxl_err_set(err, "not a package");
        return -1;
    }
    p->version = dxl_ur_u16(&h);
    p->licensee = dxl_ur_u16(&h);
    dxl_ur_u32(&h);   /* the package's flags */
    uint32_t ncount = dxl_ur_u32(&h), noff = dxl_ur_u32(&h);
    uint32_t ecount = dxl_ur_u32(&h), eoff = dxl_ur_u32(&h);
    uint32_t icount = dxl_ur_u32(&h), ioff = dxl_ur_u32(&h);
    if (h.bad || ncount > MAX_NAMES || ecount > MAX_OBJECTS || icount > MAX_OBJECTS ||
        !table_fits(p, ncount, noff, 5) || !table_fits(p, ecount, eoff, 8) || !table_fits(p, icount, ioff, 7)) {
        dxl_err_set(err, "a package whose tables run past its end");
        return -1;
    }

    dxl_ureader r;
    ur_init(&r, p->data, p->size);
    r.at = noff;
    p->names = dxl_xmalloc(sizeof *p->names * (ncount ? ncount : 1));
    for (uint32_t i = 0; i < ncount; i++) {
        char buf[1024];
        if (p->version >= 64) {
            dxl_ur_string(&r, buf, sizeof buf);
        } else {
            /* Before 64, NUL-terminated. */
            size_t n = 0;
            for (uint8_t c; (c = dxl_ur_u8(&r)) != 0 && !r.bad;)
                if (n + 1 < sizeof buf) buf[n++] = (char)c;
            buf[n] = '\0';
        }
        dxl_ur_u32(&r);   /* its flags */
        if (r.bad) {
            p->name_count = (int32_t)i;
            dxl_err_set(err, "a name table that runs past the end");
            return -1;
        }
        p->names[i] = dxl_xstrdup(buf);
        p->name_count = (int32_t)i + 1;
    }

    r.at = ioff;
    p->imports = dxl_xmalloc(sizeof *p->imports * (icount ? icount : 1));
    for (uint32_t i = 0; i < icount; i++) {
        dxl_upkg_import *im = &p->imports[i];
        im->class_package = dxl_ur_index(&r);
        im->class_name = dxl_ur_index(&r);
        im->package = dxl_ur_i32(&r);
        im->object_name = dxl_ur_index(&r);
    }
    p->import_count = (int32_t)icount;

    r.at = eoff;
    p->exports = dxl_xmalloc(sizeof *p->exports * (ecount ? ecount : 1));
    for (uint32_t i = 0; i < ecount; i++) {
        dxl_upkg_export *ex = &p->exports[i];
        ex->class_ref = dxl_ur_index(&r);
        ex->super_ref = dxl_ur_index(&r);
        ex->package = dxl_ur_i32(&r);
        ex->object_name = dxl_ur_index(&r);
        ex->flags = dxl_ur_u32(&r);
        ex->size = dxl_ur_index(&r);
        ex->offset = ex->size > 0 ? dxl_ur_index(&r) : 0;
        /* An object's bytes in the file, or none. */
        if (ex->size < 0 || ex->offset < 0 || (size_t)ex->offset > p->size ||
            (size_t)ex->size > p->size - (size_t)ex->offset)
            ex->size = ex->offset = 0;
    }
    p->export_count = (int32_t)ecount;
    if (r.bad) {
        dxl_err_set(err, "an object table that runs past the end");
        return -1;
    }
    return 0;
}

int dxl_upkg_parse(dxl_upkg *p, const void *data, size_t size, dxl_err *err) {
    memset(p, 0, sizeof *p);
    p->data = dxl_xmalloc(size ? size : 1);
    memcpy(p->data, data, size);
    p->size = size;
    if (read_tables(p, err) != 0) {
        dxl_upkg_free(p);
        return -1;
    }
    return 0;
}

int dxl_upkg_load(dxl_upkg *p, const char *path, dxl_err *err) {
    memset(p, 0, sizeof *p);
    FILE *f = fopen(path, "rb");
    if (!f) {
        dxl_err_set(err, "cannot open %s", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > (1L << 30)) {
        fclose(f);
        dxl_err_set(err, "%s: not a package", path);
        return -1;
    }
    p->data = dxl_xmalloc(n ? (size_t)n : 1);
    p->size = fread(p->data, 1, (size_t)n, f);
    fclose(f);
    if (read_tables(p, err) != 0) {
        char why[256];
        snprintf(why, sizeof why, "%s", dxl_err_msg(err));
        dxl_err_set(err, "%s: %s", path, why);
        dxl_upkg_free(p);
        return -1;
    }
    return 0;
}

const char *dxl_upkg_name(const dxl_upkg *p, int32_t i) {
    return i >= 0 && i < p->name_count ? p->names[i] : "";
}

const char *dxl_upkg_ref_name(const dxl_upkg *p, int32_t ref) {
    if (ref > 0 && ref <= p->export_count) return dxl_upkg_name(p, p->exports[ref - 1].object_name);
    if (ref < 0 && -(int64_t)ref <= p->import_count) return dxl_upkg_name(p, p->imports[-ref - 1].object_name);
    return "";
}

const char *dxl_upkg_ref_class(const dxl_upkg *p, int32_t ref) {
    if (ref > 0 && ref <= p->export_count) {
        int32_t c = p->exports[ref - 1].class_ref;
        return c ? dxl_upkg_ref_name(p, c) : "Class";
    }
    if (ref < 0 && -(int64_t)ref <= p->import_count) return dxl_upkg_name(p, p->imports[-ref - 1].class_name);
    return "";
}

int32_t dxl_upkg_find(const dxl_upkg *p, const char *class_name, const char *object_name) {
    for (int32_t i = 0; i < p->export_count; i++) {
        if (dxl_stricmp(dxl_upkg_name(p, p->exports[i].object_name), object_name) != 0) continue;
        if (class_name && dxl_stricmp(dxl_upkg_ref_class(p, i + 1), class_name) != 0) continue;
        return i;
    }
    return -1;
}

/* ---- an object's bytes ------------------------------------------------- */

void dxl_ureader_export(dxl_ureader *r, const dxl_upkg *p, int32_t e) {
    if (e < 0 || e >= p->export_count || p->exports[e].size <= 0) {
        ur_init(r, NULL, 0);
        return;
    }
    ur_init(r, p->data + p->exports[e].offset, (size_t)p->exports[e].size);
}

void dxl_ur_skip_stack(dxl_ureader *r, const dxl_upkg *p, int32_t e) {
    if (e < 0 || e >= p->export_count || !(p->exports[e].flags & DXL_RF_HAS_STACK)) return;
    /* The state frame: its node, its state node, the probe mask, the
     * latent action, and its code's offset when it has a node. */
    int32_t node = dxl_ur_index(r);
    dxl_ur_index(r);
    dxl_ur_u64(r);
    dxl_ur_i32(r);
    if (node) dxl_ur_index(r);
}

int dxl_ur_prop(dxl_ureader *r, const dxl_upkg *p, dxl_uprop *out) {
    memset(out, 0, sizeof *out);
    int32_t ni = dxl_ur_index(r);
    if (r->bad || ni < 0 || ni >= p->name_count) return -1;
    out->name = p->names[ni];
    if (!dxl_stricmp(out->name, "None")) return 0;
    uint8_t info = dxl_ur_u8(r);
    out->type = info & 0x0f;
    out->struct_name = "";
    if (out->type == DXL_UPROP_STRUCT) out->struct_name = dxl_upkg_name(p, dxl_ur_index(r));
    static const int sizes[5] = { 1, 2, 4, 12, 16 };
    int k = (info >> 4) & 7;
    size_t size = 0;
    if (k < 5) size = (size_t)sizes[k];
    else if (k == 5) size = dxl_ur_u8(r);
    else if (k == 6) size = dxl_ur_u16(r);
    else size = dxl_ur_u32(r);
    /* A bool's value is its tag's high bit; any other's that bit says an
     * array index follows. */
    if (out->type == DXL_UPROP_BOOL) {
        out->bool_value = (info & 0x80) != 0;
        size = 0;
    } else if (info & 0x80) {
        uint8_t b = dxl_ur_u8(r);
        if ((b & 0xc0) == 0xc0) {
            uint32_t v = (uint32_t)(b & 0x3f) << 24;
            v |= (uint32_t)dxl_ur_u8(r) << 16;
            v |= (uint32_t)dxl_ur_u8(r) << 8;
            v |= dxl_ur_u8(r);
            out->array_index = (int32_t)v;
        } else if (b & 0x80) {
            out->array_index = ((b & 0x7f) << 8) | dxl_ur_u8(r);
        } else {
            out->array_index = b;
        }
    }
    const uint8_t *v = dxl_ur_bytes(r, size);
    if (r->bad) return -1;
    ur_init(&out->value, v, size);
    return 1;
}

int32_t dxl_uprop_int(const dxl_uprop *pr) {
    dxl_ureader r = pr->value;
    if (pr->type == DXL_UPROP_BYTE) return dxl_ur_u8(&r);
    if (pr->type == DXL_UPROP_BOOL) return pr->bool_value;
    return dxl_ur_i32(&r);
}

float dxl_uprop_float(const dxl_uprop *pr) {
    dxl_ureader r = pr->value;
    return dxl_ur_f32(&r);
}

int32_t dxl_uprop_index(const dxl_uprop *pr) {
    dxl_ureader r = pr->value;
    return dxl_ur_index(&r);
}

void dxl_uprop_str(const dxl_uprop *pr, char *out, size_t size) {
    dxl_ureader r = pr->value;
    dxl_ur_string(&r, out, size);
}

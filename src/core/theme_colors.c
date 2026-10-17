#include "core/theme_colors.h"
#include "core/upkg_assets.h"

#include <stdio.h>
#include <string.h>

/* A class's Colors and themeName, as its defaults have them; has[i] for
 * each colour it sets. 0 when they read. */
static int read_defaults(const dxl_upkg *p, int32_t e, dxl_menu_theme *t, int has[DXL_THEME_COLOR_COUNT]) {
    dxl_ureader r;
    if (dxl_upkg_class_defaults(p, e, &r) != 0) return -1;
    dxl_uprop pr;
    int rc;
    while ((rc = dxl_ur_prop(&r, p, &pr)) == 1) {
        if (!dxl_stricmp(pr.name, "themeName") && pr.type == DXL_UPROP_STR) {
            dxl_uprop_str(&pr, t->name, sizeof t->name);
        } else if (!dxl_stricmp(pr.name, "Colors") && pr.type == DXL_UPROP_STRUCT && pr.array_index >= 0 &&
                   pr.array_index < DXL_THEME_COLOR_COUNT && pr.value.size == 4) {
            memcpy(t->colors[pr.array_index], pr.value.data, 4);
            has[pr.array_index] = 1;
        }
    }
    return rc == 0 ? 0 : -1;
}

int dxl_menu_themes_read(const char *path, dxl_menu_theme *out, int max, dxl_err *err) {
    dxl_upkg p;
    if (dxl_upkg_load(&p, path, err) != 0) return -1;
    /* ColorThemeMenu's own, what each theme's are over. */
    dxl_menu_theme base;
    memset(&base, 0, sizeof base);
    int base_has[DXL_THEME_COLOR_COUNT] = { 0 };
    int32_t b = dxl_upkg_find(&p, "Class", "ColorThemeMenu");
    if (b >= 0) read_defaults(&p, b, &base, base_has);
    int n = 0;
    for (int32_t i = 0; i < p.export_count && n < max; i++) {
        if (p.exports[i].class_ref != 0) continue;
        const char *name = dxl_upkg_name(&p, p.exports[i].object_name);
        if (dxl_strnicmp(name, "ColorThemeMenu_", 15) != 0) continue;
        dxl_menu_theme t = base;
        int has[DXL_THEME_COLOR_COUNT] = { 0 };
        t.name[0] = '\0';
        if (read_defaults(&p, i, &t, has) != 0) continue;
        snprintf(t.class_name, sizeof t.class_name, "%s", name);
        if (!t.name[0]) snprintf(t.name, sizeof t.name, "%s", name + 15);
        for (int c = 0; c < DXL_THEME_COLOR_COUNT; c++)
            if (!has[c] && !base_has[c]) memset(t.colors[c], 0, 4);
        out[n++] = t;
    }
    dxl_upkg_free(&p);
    if (!n) {
        dxl_err_set(err, "%s: no menu themes in it", path);
        return -1;
    }
    return n;
}

const dxl_menu_theme *dxl_menu_theme_find(const dxl_menu_theme *themes, int count, const char *name) {
    for (int i = 0; name && i < count; i++)
        if (!dxl_stricmp(themes[i].name, name)) return &themes[i];
    return NULL;
}

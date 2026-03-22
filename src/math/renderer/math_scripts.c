// math_scripts.c - Superscript/subscript attachment
// Verbatim from original render.c

#include "math_internal.h"

Box attach_sup(const Box *base, const Box *sup) {
    int sshift = base->sup_shift;
    int sup_above = sup->h - sshift;
    if (sup_above < 0) sup_above = 0;
    int above = imax(base->baseline + sup_above, base->baseline);
    int below = base->h - base->baseline - 1;
    int h = above + 1 + below;
    int w = base->w + sup->w;

    Box r = make_box(w, h, above);
    int offBy = r.baseline - base->baseline;
    for (int y = 0; y < base->h; y++)
        for (int x = 0; x < base->w; x++) put_cell(&r, x, offBy + y, base, x, y);
    int sup_y = r.baseline - base->baseline - sup->h + sshift;
    int sup_x = base->w;
    for (int y = 0; y < sup->h; y++)
        for (int x = 0; x < sup->w; x++) put_cell(&r, sup_x + x, sup_y + y, sup, x, y);
    return r;
}
Box attach_sub(const Box *base, const Box *sub) {
    int above = base->baseline;
    int shift = base->sub_shift;
    int below;
    if (shift > 0) {
        // Shifted mode (tall delimiters): subscript goes near bottom of base
        below = imax(base->h - base->baseline - 1, shift + sub->h);
    } else {
        // Normal mode: subscript goes just below baseline
        below = imax(base->h - base->baseline - 1 + sub->h, base->h - base->baseline - 1);
    }
    int h = above + 1 + below;
    int w = base->w + sub->w;

    Box r = make_box(w, h, above);
    int offBy = r.baseline - base->baseline;
    for (int y = 0; y < base->h; y++)
        for (int x = 0; x < base->w; x++) put_cell(&r, x, offBy + y, base, x, y);

    int sub_y = r.baseline + 1 + shift;
    int sub_x = base->w;
    for (int y = 0; y < sub->h; y++)
        for (int x = 0; x < sub->w; x++) put_cell(&r, sub_x + x, sub_y + y, sub, x, y);
    return r;
}

// NEW: Combined superscript + subscript renderer
// Centers both scripts vertically over the base
Box attach_supsub(const Box *base, const Box *sup, const Box *sub) {
    // Calculate required vertical space
    int sshift = base->sup_shift;
    int sup_above = sup->h - sshift;
    if (sup_above < 0) sup_above = 0;
    int above = imax(base->baseline + sup_above, base->baseline);
    int shift = base->sub_shift;
    int below;
    if (shift > 0) {
        below = imax(base->h - base->baseline - 1, shift + sub->h);
    } else {
        below = imax(base->h - base->baseline - 1 + sub->h, base->h - base->baseline - 1);
    }

    int h = above + 1 + below;

    // Width: base + max of script widths (they stack vertically)
    int script_w = imax(sup->w, sub->w);
    int w = base->w + script_w;

    Box r = make_box(w, h, above);

    // Blit base (with metadata)
    int offBy = r.baseline - base->baseline;
    for (int y = 0; y < base->h; y++)
        for (int x = 0; x < base->w; x++) put_cell(&r, x, offBy + y, base, x, y);

    // Position superscript (left-aligned, TeX convention)
    int sup_y = r.baseline - base->baseline - sup->h + sshift;
    int sup_x = base->w;
    for (int y = 0; y < sup->h; y++)
        for (int x = 0; x < sup->w; x++) put_cell(&r, sup_x + x, sup_y + y, sup, x, y);

    // Position subscript (left-aligned, TeX convention)
    int sub_y = r.baseline + 1 + shift;
    int sub_x = base->w;
    for (int y = 0; y < sub->h; y++)
        for (int x = 0; x < sub->w; x++) put_cell(&r, sub_x + x, sub_y + y, sub, x, y);

    return r;
}

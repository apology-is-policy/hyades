// math_boxes.c - Box combination utilities for math rendering
// Verbatim from original render.c

#include "math_internal.h"

// ============================================================================
// Low-level Box Operations
// ============================================================================

void put(Box *b, int x, int y, uint32_t cp) {
    if (x < 0 || y < 0 || x >= b->w || y >= b->h) return;
    b->cells[y * b->w + x] = cp;
}

// Put cell from source box to destination box (cell + metadata)
void put_cell(Box *dst, int dx, int dy, const Box *src, int sx, int sy) {
    if (dx < 0 || dy < 0 || dx >= dst->w || dy >= dst->h) return;
    if (sx < 0 || sy < 0 || sx >= src->w || sy >= src->h) return;
    dst->cells[dy * dst->w + dx] = src->cells[sy * src->w + sx];
    // Copy metadata if source has it
    if (src->meta) {
        uint8_t m = src->meta[sy * src->w + sx];
        if (m != CELL_META_NONE) {
            box_set_meta(dst, dx, dy, m);
        }
    }
}

// Copy cell and metadata from source box to destination box
static void copy_cell(Box *dst, int dx, int dy, const Box *src, int sx, int sy) {
    if (dx < 0 || dy < 0 || dx >= dst->w || dy >= dst->h) return;
    if (sx < 0 || sy < 0 || sx >= src->w || sy >= src->h) return;
    dst->cells[dy * dst->w + dx] = src->cells[sy * src->w + sx];
    // Copy metadata if source has it
    if (src->meta) {
        uint8_t m = src->meta[sy * src->w + sx];
        if (m != CELL_META_NONE) {
            box_set_meta(dst, dx, dy, m);
        }
    }
}

// Check if a codepoint is a combining diacritical mark (zero-width)
static int is_combining_mark(uint32_t cp) {
    if (cp >= 0x0300 && cp <= 0x036F) return 1;
    if (cp >= 0x1AB0 && cp <= 0x1AFF) return 1;
    if (cp >= 0x1DC0 && cp <= 0x1DFF) return 1;
    if (cp >= 0x20D0 && cp <= 0x20FF) return 1;
    return 0;
}

void put_str(Box *b, int x, int y, const char *s) {
    size_t len = strlen(s), p = 0;
    int ix = x;
    while (p < len) {
        uint32_t cp = utf8_next(s, len, &p);
        if (is_combining_mark(cp)) {
            // Combining mark: don't advance x
            // (mark is rendered on top of previous char by terminal)
            continue;
        }
        put(b, ix++, y, cp);
    }
}

int str_cols(const char *s) {
    return (int)utf8_display_width(s);
}

// ============================================================================
// Box Combination Functions
// ============================================================================

// Horizontal concatenation with baseline alignment
Box hcat(const Box *A, const Box *B, int gap) {
    int baseline = imax(A->baseline, B->baseline);
    int underA = A->h - A->baseline - 1;
    int underB = B->h - B->baseline - 1;
    int below = imax(underA, underB);
    int h = baseline + 1 + below;
    int w = A->w + gap + B->w;

    Box r = make_box(w, h, baseline);
    int offA_y = baseline - A->baseline;
    for (int y = 0; y < A->h; y++)
        for (int x = 0; x < A->w; x++) copy_cell(&r, x, offA_y + y, A, x, y);

    int offB_y = baseline - B->baseline;
    for (int y = 0; y < B->h; y++)
        for (int x = 0; x < B->w; x++) copy_cell(&r, A->w + gap + x, offB_y + y, B, x, y);

    if (B->tag_width > 0) r.tag_width = B->tag_width;
    // Propagate row_flags from A (equation) to result, mapping y-offsets
    if (A->row_flags) {
        r.row_flags = calloc(r.h, 1);
        int offA = baseline - A->baseline;
        for (int y = 0; y < A->h; y++) {
            if (A->row_flags[y] && (offA + y) >= 0 && (offA + y) < r.h)
                r.row_flags[offA + y] = A->row_flags[y];
        }
    }
    return r;
}

// Like hcat but with negative gap (overlap) support for tight placement
Box hcat_overlap(const Box *A, const Box *B, int overlap) {
    int baseline = imax(A->baseline, B->baseline);
    int underA = A->h - A->baseline - 1;
    int underB = B->h - B->baseline - 1;
    int below = imax(underA, underB);
    int h = baseline + 1 + below;
    int w = A->w + B->w - overlap;
    if (w < 1) w = 1;

    Box r = make_box(w, h, baseline);
    int offA_y = baseline - A->baseline;
    for (int y = 0; y < A->h; y++)
        for (int x = 0; x < A->w; x++) copy_cell(&r, x, offA_y + y, A, x, y);

    int offB_y = baseline - B->baseline;
    int offB_x = A->w - overlap;
    for (int y = 0; y < B->h; y++)
        for (int x = 0; x < B->w; x++) {
            uint32_t c = B->cells[y * B->w + x];
            // Only overwrite if B has non-space
            if (c != U' ') {
                copy_cell(&r, offB_x + x, offB_y + y, B, x, y);
            }
        }

    if (B->tag_width > 0) r.tag_width = B->tag_width;
    return r;
}

Box hcat_shifted(const Box *A, const Box *B, int gap, int shift_up) {
    int baseline = imax(A->baseline, B->baseline + shift_up);
    int underA = A->h - A->baseline - 1;
    int underB = B->h - (B->baseline + shift_up) - 1;
    int below = imax(underA, underB);
    int h = baseline + 1 + below;
    int w = A->w + gap + B->w;

    Box r = make_box(w, h, baseline);
    int offA_y = baseline - A->baseline;
    for (int y = 0; y < A->h; y++)
        for (int x = 0; x < A->w; x++) copy_cell(&r, x, offA_y + y, A, x, y);

    int offB_y = baseline - (B->baseline + shift_up);
    for (int y = 0; y < B->h; y++)
        for (int x = 0; x < B->w; x++) copy_cell(&r, A->w + gap + x, offB_y + y, B, x, y);

    if (B->tag_width > 0) r.tag_width = B->tag_width;
    return r;
}

// Concat on LEFT axis, with the right placed at *exactly* the left baseline (no upward shift).
Box hcat_on_left_axis(const Box *A, const Box *B, int gap) {
    int axis = A->baseline;

    // Space below each side when both baselines are aligned to 'axis'
    int underA = A->h - A->baseline - 1;
    int underB = B->h - B->baseline - 1;

    // Space above needs to cover the max "above baseline" of both sides.
    int aboveA = A->baseline;
    int aboveB = B->baseline;

    int above = imax(aboveA, aboveB);
    int below = imax(underA, underB);

    Box r = make_box(A->w + gap + B->w, above + 1 + below, /*baseline*/ above);

    // Blit A aligned so its baseline is at 'r.baseline'
    int oyA = r.baseline - A->baseline;
    for (int y = 0; y < A->h; ++y)
        for (int x = 0; x < A->w; ++x) copy_cell(&r, x, oyA + y, A, x, y);

    // Blit B aligned to the same baseline (no vertical shift)
    int oyB = r.baseline - B->baseline;
    for (int y = 0; y < B->h; ++y)
        for (int x = 0; x < B->w; ++x) copy_cell(&r, A->w + gap + x, oyB + y, B, x, y);

    if (B->tag_width > 0) r.tag_width = B->tag_width;
    return r;
}

Box vstack_centered(const Box *A, const Box *B, bool rule) {
    int rule_h = rule ? 1 : 0;
    int w = imax(A->w, B->w);
    int h = A->h + rule_h + B->h;
    Box r = make_box(w, h, A->baseline);
    // center A
    int offA_x = (w - A->w) / 2;
    for (int y = 0; y < A->h; y++)
        for (int x = 0; x < A->w; x++) copy_cell(&r, offA_x + x, y, A, x, y);
    // rule
    int uni = get_unicode_mode();
    if (rule) {
        for (int x = 0; x < w; x++) put(&r, x, A->h, uni ? U'─' : '-');
    }
    // center B
    int offB_x = (w - B->w) / 2;
    for (int y = 0; y < B->h; y++)
        for (int x = 0; x < B->w; x++) copy_cell(&r, offB_x + x, A->h + rule_h + y, B, x, y);

    int under = (A->h - A->baseline - 1) + rule_h + B->h;
    r.h = A->baseline + 1 + under;
    r.baseline = A->baseline;
    return r;
}

// ============================================================================
// Text Box Creation
// ============================================================================

// Simple text box for identifiers/symbols
Box text_box(const char *s) {
    int w = str_cols(s);
    // Allow zero-width boxes for empty content (e.g., {} empty groups in TeX)
    // This is important for tensor notation like R^\rho{}_{\sigma\mu\nu}
    if (w == 0) {
        Box b = make_box(0, 1, 0);
        return b;
    }
    Box b = make_box(w, 1, 0);
    put_str(&b, 0, 0, s);
    return b;
}

Box text_box_from_utf32(const uint32_t *cps, int n) {
    // utility to build a 1-row box from codepoints
    int w = 0;
    for (int i = 0; i < n; i++) {
        // Skip combining marks in width calculation
        if (!is_combining_mark(cps[i])) w += 1;
    }
    // Allow zero-width boxes for empty content (e.g., {} empty groups in TeX)
    // This is important for tensor notation like R^\rho{}_{\sigma\mu\nu}
    if (w == 0) {
        return make_box(0, 1, 0);
    }
    Box b = make_box(w, 1, 0);
    int col = 0;
    for (int i = 0; i < n; i++) {
        if (is_combining_mark(cps[i])) continue; // Skip combining marks
        put(&b, col++, 0, cps[i]);
    }
    return b;
}

// Make a box of N spaces
Box space_box(int n) {
    if (n <= 0) {
        Box z = make_box(0, 1, 0);
        return z;
    }
    Box b = make_box(n, 1, 0);
    for (int i = 0; i < n; i++) put(&b, i, 0, U' ');
    return b;
}

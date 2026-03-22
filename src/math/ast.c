#include "ast.h"
#include "stdlib.h"
#include "string.h"

Box make_box(int w, int h, int baseline) {
    Box b = (Box){.w = w,
                  .h = h,
                  .baseline = baseline,
                  .cells = NULL,
                  .meta = NULL,
                  .style = NULL,
                  .sub_shift = 0,
                  .sup_shift = 0,
                  .tag_width = 0,
                  .row_flags = NULL};
    if (w <= 0 || h <= 0) return b;
    b.cells = (uint32_t *)malloc((size_t)w * h * sizeof(uint32_t));
    if (!b.cells) return b; // Return box with NULL cells on allocation failure
    for (int i = 0; i < w * h; i++) b.cells[i] = U' ';
    return b;
}

void box_free(Box *b) {
    if (b) {
        if (b->cells) {
            free(b->cells);
            b->cells = NULL;
        }
        if (b->meta) {
            free(b->meta);
            b->meta = NULL;
        }
        if (b->style) {
            free(b->style);
            b->style = NULL;
        }
        if (b->row_flags) {
            free(b->row_flags);
            b->row_flags = NULL;
        }
    }
}

void box_ensure_meta(Box *b) {
    if (!b || b->meta) return; // Already allocated or invalid
    size_t size = (size_t)b->w * b->h;
    if (size == 0) return;
    b->meta = (uint8_t *)malloc(size);
    if (!b->meta) return; // Allocation failed
    memset(b->meta, CELL_META_NONE, size);
}

void box_set_meta(Box *b, int x, int y, uint8_t flags) {
    if (!b || x < 0 || y < 0 || x >= b->w || y >= b->h) return;
    box_ensure_meta(b);
    b->meta[y * b->w + x] = flags;
}

uint8_t box_get_meta(const Box *b, int x, int y) {
    if (!b || !b->meta || x < 0 || y < 0 || x >= b->w || y >= b->h) {
        return CELL_META_NONE;
    }
    return b->meta[y * b->w + x];
}

// ============================================================================
// Per-cell ANSI style helpers
// ============================================================================

void box_ensure_style(Box *b) {
    if (!b || b->style) return; // Already allocated or invalid
    size_t size = (size_t)b->w * b->h;
    if (size == 0) return;
    b->style = (uint16_t *)calloc(size, sizeof(uint16_t));
}

void box_set_cell_style(Box *b, int x, int y, uint16_t s) {
    if (!b || x < 0 || y < 0 || x >= b->w || y >= b->h) return;
    box_ensure_style(b);
    if (!b->style) return;
    b->style[y * b->w + x] = s;
}

uint16_t box_get_cell_style(const Box *b, int x, int y) {
    if (!b || !b->style || x < 0 || y < 0 || x >= b->w || y >= b->h) {
        return 0;
    }
    return b->style[y * b->w + x];
}
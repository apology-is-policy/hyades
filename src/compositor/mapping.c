// mapping.c - Input/output coordinate mapping for compositor

#include "compositor_internal.h"
#include <limits.h>
#include <stdlib.h>

// ============================================================================
// MapCtx Public API
// ============================================================================

void mapctx_init(MapCtx *mc) {
    if (!mc) return;
    mc->v = NULL;
    mc->n = mc->cap = 0;
    mc->row = mc->col = 0;
}

void mapctx_reset(MapCtx *mc) {
    if (!mc) return;
    mc->n = 0;
    mc->row = mc->col = 0;
}

void mapctx_free(MapCtx *mc) {
    if (!mc) return;
    free(mc->v);
    mc->v = NULL;
    mc->n = mc->cap = 0;
}

// ============================================================================
// MapCtx Internal Operations
// ============================================================================

static void mc_push(MapCtx *mc, MapEntry e) {
    if (!mc) return;
    if (mc->n >= mc->cap) {
        mc->cap = mc->cap ? mc->cap * 2 : 256;
        mc->v = realloc(mc->v, mc->cap * sizeof(MapEntry));
        if (!mc->v) {
            mc->n = mc->cap = 0;
            return;
        }
    }
    mc->v[mc->n++] = e;
}

void mc_newline(MapCtx *mc) {
    if (!mc) return;
    mc->row++;
    mc->col = 0;
}

void mc_emit(MapCtx *mc, int input_byte_idx, int kind) {
    if (!mc) return;
    mc_push(mc, (MapEntry){.in = input_byte_idx, .row = mc->row, .col = mc->col, .kind = kind});
}

void mc_advance_col(MapCtx *mc, int cols) {
    if (!mc) return;
    mc->col += cols;
}

// ============================================================================
// Coordinate Conversion
// ============================================================================

int input_coord_to_byte_index(const char *input, int y, int x) {
    int row = 0, col = 0;
    const unsigned char *p = (const unsigned char *)input;
    int byte_index = 0;

    while (*p) {
        if (row == y && col == x) return byte_index;
        if (*p == '\n') {
            row++;
            col = 0;
            p++;
            byte_index++;
            if (row > y) break;
            continue;
        }
        unsigned c = *p;
        int adv = ((c & 0x80) == 0)      ? 1
                  : ((c & 0xE0) == 0xC0) ? 2
                  : ((c & 0xF0) == 0xE0) ? 3
                  : ((c & 0xF8) == 0xF0) ? 4
                                         : 1;
        p += adv;
        byte_index += adv;
        col++;
    }
    return byte_index;
}

static int mc_find_nearest_by_input(const MapCtx *mc, int idx) {
    if (!mc || !mc->v || mc->n <= 0) return -1;

    int best = -1, best_delta = INT_MAX;
    for (int i = 0; i < mc->n; ++i) {
        if (mc->v[i].in == idx) return i;
        if (mc->v[i].in > idx) {
            int d = mc->v[i].in - idx;
            if (d < best_delta) {
                best_delta = d;
                best = i;
            }
        }
    }
    if (best >= 0) return best;

    best_delta = INT_MAX;
    for (int i = 0; i < mc->n; ++i) {
        if (mc->v[i].in < idx) {
            int d = idx - mc->v[i].in;
            if (d < best_delta) {
                best_delta = d;
                best = i;
            }
        }
    }
    return best;
}

int map_input_coord_to_output_mc(const char *input, int in_row, int in_col, const MapCtx *mc,
                                 int *out_row, int *out_col, int *out_kind) {
    if (!input || !mc || !mc->v || mc->n <= 0) return 0;

    int idx = input_coord_to_byte_index(input, in_row, in_col);
    int m = mc_find_nearest_by_input(mc, idx);
    if (m < 0) return 0;

    if (out_row) *out_row = mc->v[m].row;
    if (out_col) *out_col = mc->v[m].col;
    if (out_kind) *out_kind = mc->v[m].kind;
    return 1;
}

void debug_dump_input_to_output_map(const char *input, const MapCtx *mc, FILE *fp) {
    if (!mc || !mc->v || mc->n <= 0) {
        fprintf(fp, "(no map entries)\n");
        return;
    }
    fprintf(fp, "=== Input→Output Map (%d entries) ===\n", mc->n);
    for (int i = 0; i < mc->n; i++) {
        fprintf(fp, "  [%3d] in=%4d → out=(%d,%d) kind=%d\n", i, mc->v[i].in, mc->v[i].row,
                mc->v[i].col, mc->v[i].kind);
    }
}

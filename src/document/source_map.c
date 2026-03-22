// source_map.c - Implementation of source position mapping
#include "source_map.h"

#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 256

// ============================================================================
// Lifecycle
// ============================================================================

SourceMap *source_map_new(void) {
    SourceMap *sm = calloc(1, sizeof(SourceMap));
    if (!sm) return NULL;

    sm->mappings = calloc(INITIAL_CAPACITY, sizeof(SourceMapping));
    if (!sm->mappings) {
        free(sm);
        return NULL;
    }

    sm->n_mappings = 0;
    sm->capacity = INITIAL_CAPACITY;
    sm->original_source = NULL;
    sm->original_length = 0;

    return sm;
}

SourceMap *source_map_new_with_source(const char *original_source) {
    SourceMap *sm = source_map_new();
    if (!sm) return NULL;

    sm->original_source = original_source;
    sm->original_length = original_source ? (int)strlen(original_source) : 0;

    return sm;
}

void source_map_free(SourceMap *sm) {
    if (!sm) return;
    free(sm->mappings);
    free(sm);
}

// ============================================================================
// Building the map
// ============================================================================

static int ensure_capacity(SourceMap *sm) {
    if (sm->n_mappings < sm->capacity) return 1;

    int old_cap = sm->capacity;
    int new_cap = sm->capacity * 2;
    SourceMapping *new_mappings = realloc(sm->mappings, new_cap * sizeof(SourceMapping));
    if (!new_mappings) return 0;

    // Zero the new portion
    memset(new_mappings + old_cap, 0, (new_cap - old_cap) * sizeof(SourceMapping));

    sm->mappings = new_mappings;
    sm->capacity = new_cap;
    return 1;
}

void source_map_add(SourceMap *sm, int trans_pos, int orig_line, int orig_col) {
    source_map_add_with_offset(sm, trans_pos, orig_line, orig_col, -1);
}

void source_map_add_with_offset(SourceMap *sm, int trans_pos, int orig_line, int orig_col,
                                int orig_pos) {
    if (!sm || !ensure_capacity(sm)) return;

    SourceMapping *m = &sm->mappings[sm->n_mappings++];
    m->transformed_pos = trans_pos;
    m->original_line = orig_line;
    m->original_col = orig_col;
    m->original_pos = orig_pos;
}

void source_map_add_range(SourceMap *sm, int trans_start, int trans_end, int orig_line,
                          int orig_col) {
    // For a range, we add a single mapping at the start
    // The lookup will interpolate within the range
    source_map_add(sm, trans_start, orig_line, orig_col);
    // Mark the end with a sentinel (same position maps to same origin)
    // This helps with range queries
    if (trans_end > trans_start) {
        source_map_add(sm, trans_end, orig_line, orig_col);
    }
}

void source_map_add_identity(SourceMap *sm, int start_pos, int length, int start_line,
                             int start_col) {
    if (!sm || !sm->original_source || length <= 0) return;

    int line = start_line;
    int col = start_col;
    const char *src = sm->original_source;

    // Find byte offset for start position
    int byte_offset = source_map_line_col_to_offset(src, start_line, start_col);
    if (byte_offset < 0) byte_offset = 0;

    // Add mappings, tracking newlines
    for (int i = 0; i < length && (byte_offset + i) < sm->original_length; i++) {
        // Only add mapping every N characters to save space
        // (we can interpolate for positions in between)
        if (i == 0 || i == length - 1 || (i % 64) == 0) {
            source_map_add_with_offset(sm, start_pos + i, line, col, byte_offset + i);
        }

        char c = src[byte_offset + i];
        if (c == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }
}

// ============================================================================
// Querying the map
// ============================================================================

// Binary search for the largest mapping with transformed_pos <= target
static int find_mapping_index(const SourceMap *sm, int trans_pos) {
    if (!sm || sm->n_mappings == 0) return -1;

    int lo = 0;
    int hi = sm->n_mappings - 1;
    int result = -1;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (sm->mappings[mid].transformed_pos <= trans_pos) {
            result = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    return result;
}

bool source_map_lookup(const SourceMap *sm, int trans_pos, int *out_line, int *out_col) {
    return source_map_lookup_with_offset(sm, trans_pos, out_line, out_col, NULL);
}

bool source_map_lookup_with_offset(const SourceMap *sm, int trans_pos, int *out_line, int *out_col,
                                   int *out_pos) {
    int idx = find_mapping_index(sm, trans_pos);
    if (idx < 0) return false;

    const SourceMapping *m = &sm->mappings[idx];

    // Calculate offset from the mapping point
    int offset = trans_pos - m->transformed_pos;

    if (out_line) *out_line = m->original_line;
    if (out_col) *out_col = m->original_col + offset;
    if (out_pos && m->original_pos >= 0) *out_pos = m->original_pos + offset;

    // If we have the original source, we can compute exact line/col
    // accounting for newlines in the gap
    if (sm->original_source && m->original_pos >= 0 && offset > 0) {
        int line = m->original_line;
        int col = m->original_col;
        int pos = m->original_pos;

        for (int i = 0; i < offset && pos + i < sm->original_length; i++) {
            char c = sm->original_source[pos + i];
            if (c == '\n') {
                line++;
                col = 1;
            } else {
                col++;
            }
        }

        if (out_line) *out_line = line;
        if (out_col) *out_col = col;
    }

    return true;
}

const SourceMapping *source_map_get_nearest(const SourceMap *sm, int trans_pos) {
    int idx = find_mapping_index(sm, trans_pos);
    if (idx < 0) return NULL;
    return &sm->mappings[idx];
}

// ============================================================================
// Combining maps
// ============================================================================

SourceMap *source_map_compose(const SourceMap *map1, const SourceMap *map2) {
    if (!map1 || !map2) return NULL;

    SourceMap *result = source_map_new();
    if (!result) return NULL;

    // For each mapping in map2, look up its original position in map1
    for (int i = 0; i < map2->n_mappings; i++) {
        const SourceMapping *m2 = &map2->mappings[i];
        int orig_line, orig_col, orig_pos = -1;

        if (source_map_lookup_with_offset(map1, m2->original_pos, &orig_line, &orig_col,
                                          &orig_pos)) {
            source_map_add_with_offset(result, m2->transformed_pos, orig_line, orig_col, orig_pos);
        } else {
            // No mapping in map1, preserve map2's mapping
            source_map_add_with_offset(result, m2->transformed_pos, m2->original_line,
                                       m2->original_col, m2->original_pos);
        }
    }

    result->original_source = map1->original_source;
    result->original_length = map1->original_length;

    return result;
}

// ============================================================================
// Utility
// ============================================================================

void source_map_offset_to_line_col(const char *text, int offset, int *out_line, int *out_col) {
    if (!text || offset < 0) {
        if (out_line) *out_line = 1;
        if (out_col) *out_col = 1;
        return;
    }

    int line = 1;
    int col = 1;

    for (int i = 0; i < offset && text[i]; i++) {
        if (text[i] == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
    }

    if (out_line) *out_line = line;
    if (out_col) *out_col = col;
}

int source_map_line_col_to_offset(const char *text, int line, int col) {
    if (!text || line < 1 || col < 1) return -1;

    int current_line = 1;
    int current_col = 1;
    int offset = 0;

    while (text[offset]) {
        if (current_line == line && current_col == col) {
            return offset;
        }

        if (text[offset] == '\n') {
            if (current_line == line) {
                // Requested column is past end of line
                return offset;
            }
            current_line++;
            current_col = 1;
        } else {
            current_col++;
        }
        offset++;
    }

    // If we're looking for a position at the end
    if (current_line == line && current_col == col) {
        return offset;
    }

    return -1;
}

int source_map_count(const SourceMap *sm) {
    return sm ? sm->n_mappings : 0;
}

void source_map_clear(SourceMap *sm) {
    if (sm) sm->n_mappings = 0;
}

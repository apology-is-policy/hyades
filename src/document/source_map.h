// source_map.h - Map positions in transformed text back to original source
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// A single mapping from transformed position to original position
typedef struct {
    int transformed_pos; // Byte offset in transformed text
    int original_line;   // 1-based line in original source
    int original_col;    // 1-based column in original source
    int original_pos;    // Byte offset in original source
} SourceMapping;

// Collection of source mappings for a transformed document
typedef struct {
    SourceMapping *mappings;
    int n_mappings;
    int capacity;
    const char *original_source; // Reference to original source (not owned)
    int original_length;
} SourceMap;

// ============================================================================
// Lifecycle
// ============================================================================

// Create a new empty source map
SourceMap *source_map_new(void);

// Create a source map with reference to original source
SourceMap *source_map_new_with_source(const char *original_source);

// Free a source map
void source_map_free(SourceMap *sm);

// ============================================================================
// Building the map
// ============================================================================

// Add a mapping entry
// trans_pos: position in transformed text
// orig_line, orig_col: position in original source (1-based)
void source_map_add(SourceMap *sm, int trans_pos, int orig_line, int orig_col);

// Add mapping with byte offset
void source_map_add_with_offset(SourceMap *sm, int trans_pos, int orig_line, int orig_col,
                                int orig_pos);

// Mark a range in transformed text as mapping to a single point in original
// (useful for expansions where many output chars come from one input point)
void source_map_add_range(SourceMap *sm, int trans_start, int trans_end, int orig_line,
                          int orig_col);

// Add an identity mapping (trans_pos maps to same position in original)
// Useful when copying text unchanged
void source_map_add_identity(SourceMap *sm, int start_pos, int length, int start_line,
                             int start_col);

// ============================================================================
// Querying the map
// ============================================================================

// Look up original position for a transformed position
// Returns true if found, false if no mapping exists
bool source_map_lookup(const SourceMap *sm, int trans_pos, int *out_line, int *out_col);

// Look up original position with byte offset
bool source_map_lookup_with_offset(const SourceMap *sm, int trans_pos, int *out_line, int *out_col,
                                   int *out_pos);

// Get the nearest mapping at or before trans_pos
// Returns NULL if no mapping exists
const SourceMapping *source_map_get_nearest(const SourceMap *sm, int trans_pos);

// ============================================================================
// Combining maps
// ============================================================================

// Compose two source maps: result maps positions through both transformations
// If map1 transforms A→B and map2 transforms B→C, result maps A→C
SourceMap *source_map_compose(const SourceMap *map1, const SourceMap *map2);

// ============================================================================
// Utility
// ============================================================================

// Compute line and column from byte offset in text
void source_map_offset_to_line_col(const char *text, int offset, int *out_line, int *out_col);

// Compute byte offset from line and column
int source_map_line_col_to_offset(const char *text, int line, int col);

// Get number of mappings
int source_map_count(const SourceMap *sm);

// Clear all mappings (but keep allocated capacity)
void source_map_clear(SourceMap *sm);

#ifdef __cplusplus
}
#endif

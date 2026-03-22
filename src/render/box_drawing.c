#include "box_drawing.h"
#include <stddef.h>

// ============================================================================
// Box Drawing Character Database
// ============================================================================
//
// Unicode box-drawing characters live in U+2500..U+257F
// We also handle a few related characters from other blocks.
//
// Each entry maps a codepoint to its arm flags.
// ============================================================================

typedef struct {
    uint32_t cp;
    uint8_t arms;
} BoxCharEntry;

// The table is sorted by codepoint for binary search (optional optimization)
// For now, linear search is fine given the small table size (~80 entries)

static const BoxCharEntry box_char_table[] = {
    // ============ Light lines ============
    // Horizontals
    {U'─', ARM_EAST | ARM_WEST}, // U+2500
    {U'━', ARM_EAST | ARM_WEST}, // U+2501 heavy (treat as single for now)

    // Verticals
    {U'│', ARM_NORTH | ARM_SOUTH}, // U+2502
    {U'┃', ARM_NORTH | ARM_SOUTH}, // U+2503 heavy

    // Light corners
    {U'┌', ARM_SOUTH | ARM_EAST}, // U+250C
    {U'┐', ARM_SOUTH | ARM_WEST}, // U+2510
    {U'└', ARM_NORTH | ARM_EAST}, // U+2514
    {U'┘', ARM_NORTH | ARM_WEST}, // U+2518

    // Light T-junctions
    {U'├', ARM_NORTH | ARM_SOUTH | ARM_EAST}, // U+251C
    {U'┤', ARM_NORTH | ARM_SOUTH | ARM_WEST}, // U+2524
    {U'┬', ARM_SOUTH | ARM_EAST | ARM_WEST},  // U+252C
    {U'┴', ARM_NORTH | ARM_EAST | ARM_WEST},  // U+2534

    // Light cross
    {U'┼', ARM_NORTH | ARM_SOUTH | ARM_EAST | ARM_WEST}, // U+253C

    // ============ Double lines ============
    // Horizontals
    {U'═', ARM_EAST | ARM_WEST | STYLE_DOUBLE_H}, // U+2550

    // Verticals
    {U'║', ARM_NORTH | ARM_SOUTH | STYLE_DOUBLE_V}, // U+2551

    // Double corners
    {U'╔', ARM_SOUTH | ARM_EAST | STYLE_DOUBLE_ALL}, // U+2554
    {U'╗', ARM_SOUTH | ARM_WEST | STYLE_DOUBLE_ALL}, // U+2557
    {U'╚', ARM_NORTH | ARM_EAST | STYLE_DOUBLE_ALL}, // U+255A
    {U'╝', ARM_NORTH | ARM_WEST | STYLE_DOUBLE_ALL}, // U+255D

    // Double T-junctions
    {U'╠', ARM_NORTH | ARM_SOUTH | ARM_EAST | STYLE_DOUBLE_ALL}, // U+2560
    {U'╣', ARM_NORTH | ARM_SOUTH | ARM_WEST | STYLE_DOUBLE_ALL}, // U+2563
    {U'╦', ARM_SOUTH | ARM_EAST | ARM_WEST | STYLE_DOUBLE_ALL},  // U+2566
    {U'╩', ARM_NORTH | ARM_EAST | ARM_WEST | STYLE_DOUBLE_ALL},  // U+2569

    // Double cross
    {U'╬', ARM_NORTH | ARM_SOUTH | ARM_EAST | ARM_WEST | STYLE_DOUBLE_ALL}, // U+256C

    // ============ Mixed single/double ============
    // Single vertical, double horizontal corners
    {U'╒', ARM_SOUTH | ARM_EAST | STYLE_DOUBLE_E}, // U+2552
    {U'╓', ARM_SOUTH | ARM_EAST | STYLE_DOUBLE_S}, // U+2553
    {U'╕', ARM_SOUTH | ARM_WEST | STYLE_DOUBLE_W}, // U+2555
    {U'╖', ARM_SOUTH | ARM_WEST | STYLE_DOUBLE_S}, // U+2556
    {U'╘', ARM_NORTH | ARM_EAST | STYLE_DOUBLE_E}, // U+2558
    {U'╙', ARM_NORTH | ARM_EAST | STYLE_DOUBLE_N}, // U+2559
    {U'╛', ARM_NORTH | ARM_WEST | STYLE_DOUBLE_W}, // U+255B
    {U'╜', ARM_NORTH | ARM_WEST | STYLE_DOUBLE_N}, // U+255C

    // Mixed T-junctions (single vertical, double horizontal)
    {U'╞', ARM_NORTH | ARM_SOUTH | ARM_EAST | STYLE_DOUBLE_E}, // U+255E
    {U'╟', ARM_NORTH | ARM_SOUTH | ARM_EAST | STYLE_DOUBLE_V}, // U+255F
    {U'╡', ARM_NORTH | ARM_SOUTH | ARM_WEST | STYLE_DOUBLE_W}, // U+2561
    {U'╢', ARM_NORTH | ARM_SOUTH | ARM_WEST | STYLE_DOUBLE_V}, // U+2562
    {U'╤', ARM_SOUTH | ARM_EAST | ARM_WEST | STYLE_DOUBLE_H},  // U+2564
    {U'╥', ARM_SOUTH | ARM_EAST | ARM_WEST | STYLE_DOUBLE_S},  // U+2565
    {U'╧', ARM_NORTH | ARM_EAST | ARM_WEST | STYLE_DOUBLE_H},  // U+2567
    {U'╨', ARM_NORTH | ARM_EAST | ARM_WEST | STYLE_DOUBLE_N},  // U+2568

    // Mixed crosses
    {U'╪',
     ARM_NORTH | ARM_SOUTH | ARM_EAST | ARM_WEST | STYLE_DOUBLE_H}, // U+256A single V, double H
    {U'╫',
     ARM_NORTH | ARM_SOUTH | ARM_EAST | ARM_WEST | STYLE_DOUBLE_V}, // U+256B double V, single H

    // ============ Light dashed/dotted (treat as solid) ============
    {U'┄', ARM_EAST | ARM_WEST},   // U+2504
    {U'┆', ARM_NORTH | ARM_SOUTH}, // U+2506
    {U'┈', ARM_EAST | ARM_WEST},   // U+2508
    {U'┊', ARM_NORTH | ARM_SOUTH}, // U+250A

    // ============ Rounded corners ============
    {U'╭', ARM_SOUTH | ARM_EAST}, // U+256D
    {U'╮', ARM_SOUTH | ARM_WEST}, // U+256E
    {U'╯', ARM_NORTH | ARM_WEST}, // U+256F
    {U'╰', ARM_NORTH | ARM_EAST}, // U+2570

    // ============ Half lines (useful for detection) ============
    {U'╴', ARM_WEST},  // U+2574 light left
    {U'╵', ARM_NORTH}, // U+2575 light up
    {U'╶', ARM_EAST},  // U+2576 light right
    {U'╷', ARM_SOUTH}, // U+2577 light down
};

static const int box_char_table_size = sizeof(box_char_table) / sizeof(box_char_table[0]);

// ============================================================================
// Lookup Functions
// ============================================================================

uint8_t box_char_get_arms(uint32_t cp) {
    for (int i = 0; i < box_char_table_size; i++) {
        if (box_char_table[i].cp == cp) {
            return box_char_table[i].arms;
        }
    }
    return 0;
}

bool box_char_has_arm(uint32_t cp, uint8_t direction) {
    uint8_t arms = box_char_get_arms(cp);
    return (arms & direction) != 0;
}

bool is_box_drawing_char(uint32_t cp) {
    return box_char_get_arms(cp) != 0;
}

// ============================================================================
// Junction Synthesis
// ============================================================================
//
// Given a combination of arms (and styles), find the best matching character.
// Priority: exact match > close match with same directions
// ============================================================================

uint32_t arms_to_box_char(uint8_t arms) {
    if ((arms & ARM_MASK) == 0) {
        return 0; // No arms, no character
    }

    // First, try exact match
    for (int i = 0; i < box_char_table_size; i++) {
        if (box_char_table[i].arms == arms) {
            return box_char_table[i].cp;
        }
    }

    // No exact match - try to find best match with same directions
    // (ignore style bits for fallback)
    uint8_t dirs = arms & ARM_MASK;
    uint8_t style = arms & ~ARM_MASK;

    // Preference order for style fallback:
    // 1. Same directions, all-double style
    // 2. Same directions, all-single style
    // 3. Same directions, any style

    // Try all-double first if we have any double style
    if (style != 0) {
        uint8_t all_double = dirs | STYLE_DOUBLE_ALL;
        for (int i = 0; i < box_char_table_size; i++) {
            if (box_char_table[i].arms == all_double) {
                return box_char_table[i].cp;
            }
        }
    }

    // Try all-single (no style bits)
    for (int i = 0; i < box_char_table_size; i++) {
        if (box_char_table[i].arms == dirs) {
            return box_char_table[i].cp;
        }
    }

    // Last resort: any character with same directions
    for (int i = 0; i < box_char_table_size; i++) {
        if ((box_char_table[i].arms & ARM_MASK) == dirs) {
            return box_char_table[i].cp;
        }
    }

    return 0; // No match found
}

// ============================================================================
// Box Post-Processing
// ============================================================================

// Helper: get character at (x, y) with bounds checking
static uint32_t get_cell(const Box *b, int x, int y) {
    if (!b || !b->cells || x < 0 || y < 0 || x >= b->w || y >= b->h) {
        return 0;
    }
    return b->cells[y * b->w + x];
}

// Helper: set character at (x, y)
static void set_cell(Box *b, int x, int y, uint32_t cp) {
    if (!b || !b->cells || x < 0 || y < 0 || x >= b->w || y >= b->h) {
        return;
    }
    b->cells[y * b->w + x] = cp;
}

void box_fixup_junctions(Box *b) {
    if (!b || !b->cells || b->w == 0 || b->h == 0) {
        return;
    }

    // We need to scan the entire box and detect where lines meet.
    // For each cell, we look at its 4 neighbors and determine what arms
    // are "pointing at" this cell from each direction.
    //
    // Cells marked with CELL_META_NO_CONNECT are skipped - they don't
    // participate in junction formation. This is used for content bars
    // in math expressions (like │ in bra-ket notation).

    for (int y = 0; y < b->h; y++) {
        for (int x = 0; x < b->w; x++) {
            uint32_t center = get_cell(b, x, y);

            // Skip cells marked with NO_CONNECT - they don't participate in junction formation
            uint8_t center_meta = box_get_meta(b, x, y);
            if (center_meta & CELL_META_NO_CONNECT) {
                continue;
            }

            // Get neighbors
            uint32_t north_char = get_cell(b, x, y - 1);
            uint32_t south_char = get_cell(b, x, y + 1);
            uint32_t east_char = get_cell(b, x + 1, y);
            uint32_t west_char = get_cell(b, x - 1, y);

            // Get metadata for neighbors (NO_CONNECT cells don't participate)
            uint8_t north_meta = box_get_meta(b, x, y - 1);
            uint8_t south_meta = box_get_meta(b, x, y + 1);
            uint8_t east_meta = box_get_meta(b, x + 1, y);
            uint8_t west_meta = box_get_meta(b, x - 1, y);

            // Determine which arms point INTO this cell from neighbors
            uint8_t incoming_arms = 0;

            // North neighbor has south arm → arm coming from north
            // Skip if neighbor has NO_CONNECT flag
            uint8_t north_arms = box_char_get_arms(north_char);
            if ((north_arms & ARM_SOUTH) && !(north_meta & CELL_META_NO_CONNECT)) {
                incoming_arms |= ARM_NORTH;
                if (north_arms & STYLE_DOUBLE_S) incoming_arms |= STYLE_DOUBLE_N;
            }

            // South neighbor has north arm → arm coming from south
            uint8_t south_arms = box_char_get_arms(south_char);
            if ((south_arms & ARM_NORTH) && !(south_meta & CELL_META_NO_CONNECT)) {
                incoming_arms |= ARM_SOUTH;
                if (south_arms & STYLE_DOUBLE_N) incoming_arms |= STYLE_DOUBLE_S;
            }

            // East neighbor has west arm → arm coming from east
            uint8_t east_arms = box_char_get_arms(east_char);
            if ((east_arms & ARM_WEST) && !(east_meta & CELL_META_NO_CONNECT)) {
                incoming_arms |= ARM_EAST;
                if (east_arms & STYLE_DOUBLE_W) incoming_arms |= STYLE_DOUBLE_E;
            }

            // West neighbor has east arm → arm coming from west
            uint8_t west_arms = box_char_get_arms(west_char);
            if ((west_arms & ARM_EAST) && !(west_meta & CELL_META_NO_CONNECT)) {
                incoming_arms |= ARM_WEST;
                if (west_arms & STYLE_DOUBLE_E) incoming_arms |= STYLE_DOUBLE_W;
            }

            // Count incoming directions
            int incoming_count = 0;
            if (incoming_arms & ARM_NORTH) incoming_count++;
            if (incoming_arms & ARM_SOUTH) incoming_count++;
            if (incoming_arms & ARM_EAST) incoming_count++;
            if (incoming_arms & ARM_WEST) incoming_count++;

            // Only process if we have 2+ incoming arms (potential junction)
            if (incoming_count < 2) {
                continue;
            }

            // The merged arms are simply the incoming arms.
            // We don't merge with center's arms because that would add arms
            // that don't connect to anything (e.g., an hrule's west arm when
            // there's nothing to the west).
            //
            // The incoming_arms already represent all the connections:
            // - If the center has an east arm AND there's a neighbor to the east
            //   with a west arm, then ARM_EAST is already in incoming_arms.
            // - If the center has an east arm but nothing connects from the east,
            //   we shouldn't include ARM_EAST in the junction.

            uint8_t merged_arms = incoming_arms;

            // Find the appropriate junction character
            uint32_t junction = arms_to_box_char(merged_arms);

            if (junction != 0 && junction != center) {
                // Preserve intentionally-placed chars with matching arms
                // (e.g., rounded corners ╭╮╰╯ placed by table border caps)
                uint8_t center_arms = box_char_get_arms(center);
                if (center_arms != 0 && center_arms == incoming_arms) {
                    continue;
                }
                set_cell(b, x, y, junction);
            }
        }
    }
}
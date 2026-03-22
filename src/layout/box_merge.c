// box_merge.c - Box merging operations for Hyades layout system
//
// Functions for combining multiple boxes into composite boxes
// (horizontal and vertical arrangement).

#include "diagnostics/diagnostics.h"
#include "layout_internal.h"
#include "layout_types.h"
#include "math/ast.h"

#include <stdlib.h>
#include <string.h>

// ============================================================================
// Horizontal Merge
// ============================================================================

// Merge boxes horizontally (side by side)
// v_align: ALIGN_TOP for top-alignment (tables), other values for baseline alignment (math)
Box *hbox_merge(Box **boxes, int n, Alignment v_align) {
    if (n == 0) {
        // Return empty box
        Box *empty = malloc(sizeof(Box));
        *empty = make_box(1, 1, 0);
        return empty;
    }

    if (n == 1) {
        // Return copy of single box (including metadata and style)
        Box *copy = calloc(1, sizeof(Box));
        copy->w = boxes[0]->w;
        copy->h = boxes[0]->h;
        copy->baseline = boxes[0]->baseline;
        copy->cells = malloc(copy->w * copy->h * sizeof(uint32_t));
        if (copy->cells && boxes[0]->cells) {
            memcpy(copy->cells, boxes[0]->cells, copy->w * copy->h * sizeof(uint32_t));
        }
        // Copy metadata if present
        if (boxes[0]->meta) {
            box_ensure_meta(copy);
            if (copy->meta) {
                memcpy(copy->meta, boxes[0]->meta, copy->w * copy->h * sizeof(uint8_t));
            }
        }
        // Copy style if present
        if (boxes[0]->style) {
            box_ensure_style(copy);
            if (copy->style) {
                memcpy(copy->style, boxes[0]->style, copy->w * copy->h * sizeof(uint16_t));
            }
        }
        return copy;
    }

    // Check if we should use baseline alignment
    // ALIGN_TOP means top-alignment (no baseline), used for tables
    // Otherwise use baseline alignment if at least one box has:
    //   - a baseline not at the bottom row, OR
    //   - height > 1 (multi-line boxes need baseline alignment)
    bool use_baseline_align = false;
    int trigger_box = -1; // Which box triggered baseline alignment
    const char *trigger_reason = NULL;
    if (v_align != ALIGN_TOP) {
        for (int i = 0; i < n; i++) {
            // Trigger baseline alignment for any multi-line box or meaningful baseline
            if (boxes[i]->h > 1) {
                use_baseline_align = true;
                trigger_box = i;
                trigger_reason = "h > 1";
                break;
            }
            if (boxes[i]->baseline < boxes[i]->h - 1) {
                use_baseline_align = true;
                trigger_box = i;
                trigger_reason = "baseline < h-1";
                break;
            }
        }
    }

    // Log baseline alignment decision
    if (diag_is_enabled(DIAG_MERGE)) {
        diag_log(DIAG_MERGE, 0, "hbox_merge: %d boxes, v_align=%s", n,
                 v_align == ALIGN_TOP ? "TOP" : "BASELINE");
        for (int i = 0; i < n && i < 8; i++) { // Limit to first 8 boxes
            diag_log(DIAG_MERGE, 1, "box[%d]: %dx%d baseline=%d", i, boxes[i]->w, boxes[i]->h,
                     boxes[i]->baseline);
        }
        if (n > 8) {
            diag_log(DIAG_MERGE, 1, "... and %d more boxes", n - 8);
        }
        if (use_baseline_align) {
            diag_result(DIAG_MERGE, 0, "baseline alignment ON (box[%d] %s)", trigger_box,
                        trigger_reason);
        } else {
            diag_result(DIAG_MERGE, 0, "baseline alignment OFF (top-align)");
        }
    }

    // Calculate total width and alignment
    int total_width = 0;
    int max_height = 0;
    int max_ascent = 0;  // max distance above baseline
    int max_descent = 0; // max distance below baseline

    for (int i = 0; i < n; i++) {
        total_width += boxes[i]->w;
        if (boxes[i]->h > max_height) max_height = boxes[i]->h;
        if (use_baseline_align) {
            // Ascent = baseline (how many rows above baseline row 0)
            int ascent = boxes[i]->baseline;
            // Descent = remaining rows below baseline
            int descent = boxes[i]->h - 1 - boxes[i]->baseline;
            if (ascent > max_ascent) max_ascent = ascent;
            if (descent > max_descent) max_descent = descent;
        }
    }

    // Result height: use baseline calc if baseline aligning, else max height
    int result_height = use_baseline_align ? (max_ascent + 1 + max_descent) : max_height;

    // Create result box with appropriate baseline
    Box *result = malloc(sizeof(Box));
    int result_baseline = use_baseline_align ? max_ascent : (result_height - 1);
    *result = make_box(total_width, result_height, result_baseline);
    if (!result->cells) return result; // Allocation failed

    // Log result dimensions
    if (diag_is_enabled(DIAG_MERGE)) {
        if (use_baseline_align) {
            diag_log(DIAG_MERGE, 1, "ascent=%d descent=%d", max_ascent, max_descent);
        }
        diag_result(DIAG_MERGE, 0, "result: %dx%d baseline=%d", total_width, result_height,
                    result_baseline);
    }

    // Copy children horizontally, aligned at baseline
    int x_offset = 0;
    for (int i = 0; i < n; i++) {
        Box *child = boxes[i];
        if (!child->cells) {
            x_offset += child->w;
            continue; // Skip boxes with no cells
        }

        // Calculate y offset based on alignment mode
        int y_offset;
        if (use_baseline_align) {
            // Baseline alignment: child's baseline aligns with result's baseline
            y_offset = max_ascent - child->baseline;
        } else {
            // Top alignment (default for tables, text content)
            y_offset = 0;
        }

        // Copy cells and metadata with baseline alignment
        for (int cy = 0; cy < child->h; cy++) {
            int ry = cy + y_offset; // Result row
            if (ry < 0 || ry >= result_height) continue;

            for (int x = 0; x < child->w; x++) {
                int rx = x_offset + x;
                if (rx < total_width) {
                    result->cells[ry * total_width + rx] = child->cells[cy * child->w + x];
                    // Copy metadata if present
                    if (child->meta) {
                        uint8_t m = child->meta[cy * child->w + x];
                        if (m != CELL_META_NONE) {
                            box_set_meta(result, rx, ry, m);
                        }
                    }
                    // Copy style if present
                    if (child->style) {
                        uint16_t s = child->style[cy * child->w + x];
                        if (s) box_set_cell_style(result, rx, ry, s);
                    }
                }
            }
        }

        x_offset += child->w;
    }

    return result;
}

// ============================================================================
// Vertical Merge
// ============================================================================

// Merge boxes vertically (stacked)
Box *vbox_merge(Box **boxes, int n, int width) {
    if (n == 0) {
        // Return empty box with specified width
        Box *empty = malloc(sizeof(Box));
        *empty = make_box(width > 0 ? width : 1, 1, 0);
        if (empty->cells) {
            for (int i = 0; i < empty->w; i++) {
                empty->cells[i] = (uint32_t)' ';
            }
        }
        return empty;
    }

    if (n == 1) {
        // Return copy (including metadata and style)
        Box *copy = calloc(1, sizeof(Box));
        copy->w = boxes[0]->w;
        copy->h = boxes[0]->h;
        copy->baseline = boxes[0]->baseline;
        copy->cells = malloc(copy->w * copy->h * sizeof(uint32_t));
        if (copy->cells && boxes[0]->cells) {
            memcpy(copy->cells, boxes[0]->cells, copy->w * copy->h * sizeof(uint32_t));
        }
        // Copy metadata if present
        if (boxes[0]->meta) {
            box_ensure_meta(copy);
            if (copy->meta) {
                memcpy(copy->meta, boxes[0]->meta, copy->w * copy->h * sizeof(uint8_t));
            }
        }
        // Copy style if present
        if (boxes[0]->style) {
            box_ensure_style(copy);
            if (copy->style) {
                memcpy(copy->style, boxes[0]->style, copy->w * copy->h * sizeof(uint16_t));
            }
        }
        return copy;
    }

    // Calculate total height
    int total_height = 0;
    for (int i = 0; i < n; i++) {
        total_height += boxes[i]->h;
    }

    // Create result box
    Box *result = malloc(sizeof(Box));
    *result = make_box(width, total_height, 0);
    if (!result->cells) return result; // Allocation failed

    // Copy children vertically
    int y_offset = 0;
    for (int i = 0; i < n; i++) {
        Box *child = boxes[i];
        if (!child->cells) {
            y_offset += child->h;
            continue; // Skip boxes with no cells
        }

        // Copy cells and metadata (left-aligned)
        for (int y = 0; y < child->h; y++) {
            int ry = y_offset + y;
            if (ry >= total_height) continue;

            // Copy child content
            for (int x = 0; x < child->w && x < width; x++) {
                result->cells[ry * width + x] = child->cells[y * child->w + x];
                // Copy metadata if present
                if (child->meta) {
                    uint8_t m = child->meta[y * child->w + x];
                    if (m != CELL_META_NONE) {
                        box_set_meta(result, x, ry, m);
                    }
                }
                // Copy style if present
                if (child->style) {
                    uint16_t s = child->style[y * child->w + x];
                    if (s) box_set_cell_style(result, x, ry, s);
                }
            }

            // Pad rest of row with spaces
            for (int x = child->w; x < width; x++) {
                result->cells[ry * width + x] = (uint32_t)' ';
            }
        }

        y_offset += child->h;
    }

    return result;
}

// ============================================================================
// Offset-aware Vertical Merge (for \vskip support)
// ============================================================================

// Merge boxes vertically with y-offset adjustments
// y_offsets[i] is the relative y adjustment BEFORE placing box i (can be negative)
// This allows overlapping content when y_offsets[i] < 0
Box *vbox_merge_with_skips(Box **boxes, int *y_offsets, int n, int width) {
    if (n == 0) {
        Box *empty = malloc(sizeof(Box));
        *empty = make_box(width > 0 ? width : 1, 1, 0);
        if (empty->cells) {
            for (int i = 0; i < empty->w; i++) {
                empty->cells[i] = (uint32_t)' ';
            }
        }
        return empty;
    }

    // First pass: calculate actual positions and total height
    // (accounting for negative offsets that might reduce height)
    int *actual_y = malloc(n * sizeof(int));
    int current_y = 0;
    int min_y = 0;
    int max_y = 0;

    for (int i = 0; i < n; i++) {
        // Apply offset adjustment before this box
        if (y_offsets) {
            current_y += y_offsets[i];
        }
        // Clamp to prevent going above start
        if (current_y < 0) current_y = 0;

        actual_y[i] = current_y;

        if (current_y < min_y) min_y = current_y;
        int box_bottom = current_y + boxes[i]->h;
        if (box_bottom > max_y) max_y = box_bottom;

        // Move to next position (after this box)
        current_y += boxes[i]->h;
    }

    // Total height is max_y - min_y (min_y should be 0 after clamping)
    int total_height = max_y > 0 ? max_y : 1;

    // Create result box
    Box *result = malloc(sizeof(Box));
    *result = make_box(width, total_height, 0);
    if (!result->cells) {
        free(actual_y);
        return result;
    }

    // Second pass: copy boxes at their computed positions
    // Later boxes overwrite earlier ones in overlapping regions
    for (int i = 0; i < n; i++) {
        Box *child = boxes[i];
        if (!child->cells) continue;

        int y_pos = actual_y[i];

        for (int y = 0; y < child->h; y++) {
            int dest_y = y_pos + y;
            if (dest_y < 0 || dest_y >= total_height) continue;

            for (int x = 0; x < child->w && x < width; x++) {
                uint32_t cell = child->cells[y * child->w + x];
                // Only overwrite non-space characters (transparent spaces)
                // This allows overlaying text without erasing background
                if (cell != ' ' && cell != 0) {
                    result->cells[dest_y * width + x] = cell;
                }
                // Copy metadata if present
                if (child->meta) {
                    uint8_t m = child->meta[y * child->w + x];
                    if (m != CELL_META_NONE) {
                        box_set_meta(result, x, dest_y, m);
                    }
                }
                // Copy style if present
                if (child->style) {
                    uint16_t s = child->style[y * child->w + x];
                    if (s) box_set_cell_style(result, x, dest_y, s);
                }
            }
        }
    }

    free(actual_y);
    return result;
}

// ============================================================================
// Offset-aware Horizontal Merge (for \hskip support)
// ============================================================================

// Merge boxes horizontally with x-offset adjustments
// x_offsets[i] is the relative x adjustment BEFORE placing box i (can be negative)
Box *hbox_merge_with_skips(Box **boxes, int *x_offsets, int n) {
    if (n == 0) {
        Box *empty = malloc(sizeof(Box));
        *empty = make_box(1, 1, 0);
        return empty;
    }

    // First pass: calculate actual positions and dimensions
    int *actual_x = malloc(n * sizeof(int));
    int current_x = 0;
    int max_x = 0;
    int max_height = 0;

    for (int i = 0; i < n; i++) {
        // Apply offset adjustment before this box
        if (x_offsets) {
            current_x += x_offsets[i];
        }
        // Clamp to prevent going before start
        if (current_x < 0) current_x = 0;

        actual_x[i] = current_x;

        int box_right = current_x + boxes[i]->w;
        if (box_right > max_x) max_x = box_right;
        if (boxes[i]->h > max_height) max_height = boxes[i]->h;

        // Move to next position (after this box)
        current_x += boxes[i]->w;
    }

    int total_width = max_x > 0 ? max_x : 1;

    // Create result box
    Box *result = malloc(sizeof(Box));
    *result = make_box(total_width, max_height, max_height - 1);
    if (!result->cells) {
        free(actual_x);
        return result;
    }

    // Second pass: copy boxes at their computed positions
    for (int i = 0; i < n; i++) {
        Box *child = boxes[i];
        if (!child->cells) continue;

        int x_pos = actual_x[i];

        for (int y = 0; y < child->h && y < max_height; y++) {
            for (int x = 0; x < child->w; x++) {
                int dest_x = x_pos + x;
                if (dest_x < 0 || dest_x >= total_width) continue;

                uint32_t cell = child->cells[y * child->w + x];
                // Only overwrite non-space characters (transparent spaces)
                if (cell != ' ' && cell != 0) {
                    result->cells[y * total_width + dest_x] = cell;
                }
                // Copy metadata if present
                if (child->meta) {
                    uint8_t m = child->meta[y * child->w + x];
                    if (m != CELL_META_NONE) {
                        box_set_meta(result, dest_x, y, m);
                    }
                }
                // Copy style if present
                if (child->style) {
                    uint16_t s = child->style[y * child->w + x];
                    if (s) box_set_cell_style(result, dest_x, y, s);
                }
            }
        }
    }

    free(actual_x);
    return result;
}

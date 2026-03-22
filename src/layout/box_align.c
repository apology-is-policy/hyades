// box_align.c - Box alignment operations for Hyades layout system
//
// Functions for aligning boxes within containers (horizontal and vertical).

#include "layout_internal.h"
#include "layout_types.h"

#include <stdlib.h>
#include <string.h>

// ============================================================================
// Horizontal Alignment
// ============================================================================

// Apply horizontal alignment to a box within a container
// Returns the original box if no alignment needed, otherwise a new aligned box
Box *apply_h_alignment(Box *content, int container_width, Alignment align) {
    if (!content || !content->cells || content->w >= container_width) {
        return content; // Already fits (no padding needed)
    }

    // Per-row alignment: when row_flags are set, flagged rows get left-aligned
    // while other rows get normal centering. Used for \intertext in \aligned.
    if (align == ALIGN_CENTER && content->row_flags) {
        // Compute equation-only width (excluding intertext rows) for centering.
        // Intertext rows may be wider than equations and must not affect centering.
        int eq_only_w = 0;
        for (int y = 0; y < content->h; y++) {
            if (content->row_flags[y] & BOX_ROW_FLAG_FULL_LEFT) continue;
            for (int x = content->w - 1; x >= 0; x--) {
                uint32_t cell = content->cells[y * content->w + x];
                if (cell != 0 && cell != ' ') {
                    if (x + 1 > eq_only_w) eq_only_w = x + 1;
                    break;
                }
            }
        }
        if (eq_only_w == 0) eq_only_w = content->w;

        int center_pad = (container_width - eq_only_w) / 2;
        if (center_pad < 0) center_pad = 0;

        // Also handle tag_width if present
        int tw = content->tag_width;
        int eq_w = (tw > 0 && tw < eq_only_w) ? eq_only_w - tw : eq_only_w;
        int tag_center_pad = (tw > 0) ? (container_width - eq_w) / 2 : center_pad;
        if (tag_center_pad < 0) tag_center_pad = 0;
        int tag_start = (tw > 0) ? container_width - tw : 0;
        if (tw > 0 && tag_start < tag_center_pad + eq_w) tag_start = tag_center_pad + eq_w;

        Box *result = malloc(sizeof(Box));
        *result = make_box(container_width, content->h, content->baseline);
        result->row_flags = calloc(result->h, 1);
        memcpy(result->row_flags, content->row_flags,
               (size_t)(content->h < result->h ? content->h : result->h));

        for (int y = 0; y < content->h; y++) {
            int lp = (content->row_flags[y] & BOX_ROW_FLAG_FULL_LEFT)
                         ? 0
                         : (tw > 0 ? tag_center_pad : center_pad);
            int copy_w =
                (tw > 0 && !(content->row_flags[y] & BOX_ROW_FLAG_FULL_LEFT)) ? eq_w : content->w;
            for (int x = 0; x < copy_w && x < content->w; x++) {
                int dst_idx = y * container_width + lp + x;
                int src_idx = y * content->w + x;
                if (dst_idx < container_width * content->h) {
                    result->cells[dst_idx] = content->cells[src_idx];
                    if (content->meta) {
                        uint8_t m = content->meta[src_idx];
                        if (m != CELL_META_NONE) box_set_meta(result, lp + x, y, m);
                    }
                    if (content->style) {
                        uint16_t s = content->style[src_idx];
                        if (s) box_set_cell_style(result, lp + x, y, s);
                    }
                }
            }
            // Copy tag part for non-intertext rows
            if (tw > 0 && !(content->row_flags[y] & BOX_ROW_FLAG_FULL_LEFT)) {
                for (int x = 0; x < tw; x++) {
                    int dst_idx = y * container_width + tag_start + x;
                    int src_idx = y * content->w + eq_w + x;
                    if (src_idx < content->w * content->h) {
                        result->cells[dst_idx] = content->cells[src_idx];
                        if (content->meta) {
                            uint8_t m = content->meta[src_idx];
                            if (m != CELL_META_NONE) box_set_meta(result, tag_start + x, y, m);
                        }
                        if (content->style) {
                            uint16_t s = content->style[src_idx];
                            if (s) box_set_cell_style(result, tag_start + x, y, s);
                        }
                    }
                }
            }
        }
        return result;
    }

    // Tag-aware centering: when content has a tag (tag_width > 0) and we're centering,
    // center the equation portion and right-justify the tag
    int tw = content->tag_width;
    if (align == ALIGN_CENTER && tw > 0 && tw < content->w) {
        int eq_w = content->w - tw; // equation width (including gap before tag)
        int left_pad = (container_width - eq_w) / 2;
        if (left_pad < 0) left_pad = 0;
        int tag_start = container_width - tw; // right-justify tag
        // Ensure no overlap
        if (tag_start < left_pad + eq_w) tag_start = left_pad + eq_w;

        Box *result = malloc(sizeof(Box));
        *result = make_box(container_width, content->h, content->baseline);
        if (!result->cells) return result;

        for (int y = 0; y < content->h; y++) {
            // Copy equation part at left_pad
            for (int x = 0; x < eq_w && x < content->w; x++) {
                int dst_idx = y * container_width + (left_pad + x);
                int src_idx = y * content->w + x;
                result->cells[dst_idx] = content->cells[src_idx];
                if (content->meta) {
                    uint8_t m = content->meta[src_idx];
                    if (m != CELL_META_NONE) box_set_meta(result, left_pad + x, y, m);
                }
                if (content->style) {
                    uint16_t s = content->style[src_idx];
                    if (s) box_set_cell_style(result, left_pad + x, y, s);
                }
            }
            // Copy tag part at tag_start
            for (int x = 0; x < tw; x++) {
                int dst_idx = y * container_width + (tag_start + x);
                int src_idx = y * content->w + (eq_w + x);
                result->cells[dst_idx] = content->cells[src_idx];
                if (content->meta) {
                    uint8_t m = content->meta[src_idx];
                    if (m != CELL_META_NONE) box_set_meta(result, tag_start + x, y, m);
                }
                if (content->style) {
                    uint16_t s = content->style[src_idx];
                    if (s) box_set_cell_style(result, tag_start + x, y, s);
                }
            }
        }
        return result;
    }

    int padding = container_width - content->w;
    int left_pad = 0;

    switch (align) {
    case ALIGN_LEFT:
        left_pad = 0; // Content on left, padding on right
        break;
    case ALIGN_CENTER: left_pad = padding / 2; break;
    case ALIGN_RIGHT: left_pad = padding; break;
    case ALIGN_JUSTIFY:
        // TODO: Justify requires distributing space within the content
        // For now, treat as left-aligned
        left_pad = 0;
        break;
    default: left_pad = 0;
    }

    int right_pad = padding - left_pad;

    // Create new box with padding
    Box *result = malloc(sizeof(Box));
    *result = make_box(container_width, content->h, content->baseline);
    if (!result->cells) return result; // Allocation failed

    // Copy content with padding, including metadata
    for (int y = 0; y < content->h; y++) {
        // Left padding
        for (int x = 0; x < left_pad; x++) {
            result->cells[y * container_width + x] = (uint32_t)' ';
        }

        // Content (cells and metadata)
        for (int x = 0; x < content->w; x++) {
            int dst_idx = y * container_width + (left_pad + x);
            int src_idx = y * content->w + x;
            result->cells[dst_idx] = content->cells[src_idx];
            // Copy metadata if present
            if (content->meta) {
                uint8_t m = content->meta[src_idx];
                if (m != CELL_META_NONE) {
                    box_set_meta(result, left_pad + x, y, m);
                }
            }
            // Copy style if present
            if (content->style) {
                uint16_t s = content->style[src_idx];
                if (s) box_set_cell_style(result, left_pad + x, y, s);
            }
        }

        // Right padding
        for (int x = 0; x < right_pad; x++) {
            result->cells[y * container_width + (left_pad + content->w + x)] = (uint32_t)' ';
        }
    }

    return result;
}

// ============================================================================
// Vertical Alignment
// ============================================================================

// Apply vertical alignment to a box within a container
// Returns the original box if no alignment needed, otherwise a new aligned box
Box *apply_v_alignment(Box *content, int container_height, Alignment align) {
    if (!content || !content->cells || content->h >= container_height || align == ALIGN_TOP) {
        return content; // Already fits or top-aligned
    }

    int padding = container_height - content->h;
    int top_pad = 0;

    switch (align) {
    case ALIGN_MIDDLE: top_pad = padding / 2; break;
    case ALIGN_BOTTOM: top_pad = padding; break;
    default: top_pad = 0;
    }

    int bottom_pad = padding - top_pad;

    // Create new box with padding
    Box *result = malloc(sizeof(Box));
    *result = make_box(content->w, container_height, content->baseline + top_pad);
    if (!result->cells) return result; // Allocation failed

    // Top padding rows
    for (int y = 0; y < top_pad; y++) {
        for (int x = 0; x < content->w; x++) {
            result->cells[y * content->w + x] = (uint32_t)' ';
        }
    }

    // Content rows (cells and metadata)
    for (int y = 0; y < content->h; y++) {
        for (int x = 0; x < content->w; x++) {
            int dst_idx = (top_pad + y) * content->w + x;
            int src_idx = y * content->w + x;
            result->cells[dst_idx] = content->cells[src_idx];
            // Copy metadata if present
            if (content->meta) {
                uint8_t m = content->meta[src_idx];
                if (m != CELL_META_NONE) {
                    box_set_meta(result, x, top_pad + y, m);
                }
            }
            // Copy style if present
            if (content->style) {
                uint16_t s = content->style[src_idx];
                if (s) box_set_cell_style(result, x, top_pad + y, s);
            }
        }
    }

    // Bottom padding rows
    for (int y = 0; y < bottom_pad; y++) {
        for (int x = 0; x < content->w; x++) {
            result->cells[(top_pad + content->h + y) * content->w + x] = (uint32_t)' ';
        }
    }

    return result;
}

// ============================================================================
// Combined Alignment
// ============================================================================

// Apply both horizontal and vertical alignment
Box *apply_alignment(Box *content, int container_width, int container_height, Alignment h_align,
                     Alignment v_align) {
    if (!content) return NULL;

    Box *result = content;

    // Apply horizontal alignment (including ALIGN_LEFT for HBOX children!)
    if (h_align != ALIGN_TOP) { // ALIGN_TOP is for vertical only
        Box *h_aligned = apply_h_alignment(result, container_width, h_align);
        if (h_aligned != result && result != content) {
            // Free intermediate result
            box_free(result);
            free(result);
        }
        result = h_aligned;
    }

    // Apply vertical alignment
    if (v_align != ALIGN_TOP && v_align != ALIGN_LEFT) {
        Box *v_aligned = apply_v_alignment(result, container_height, v_align);
        if (v_aligned != result && result != content) {
            // Free intermediate result
            box_free(result);
            free(result);
        }
        result = v_aligned;
    }

    return result;
}

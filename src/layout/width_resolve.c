// width_resolve.c - Width inheritance resolution for Hyades layout system
//
// Resolves width inheritance throughout the BoxLayout tree.
// Children with width=-1 inherit from their parent, with special
// handling for hbox children to distribute remaining space.

#include "layout_internal.h"
#include "layout_types.h"

#include <stdio.h>

// ============================================================================
// Width Resolution
// ============================================================================

void box_layout_resolve_widths(BoxLayout *layout, int parent_width) {
    if (!layout) return;

    // First, resolve this layout's own width
    if (layout->width == -1 || layout->width == WIDTH_AUTO) {
        layout->computed_width = parent_width;
    } else {
        layout->computed_width = layout->width;
    }

    // For HBOX: smart distribution of remaining width to auto children
    if (layout->type == BOX_TYPE_HBOX && layout->n_children > 0) {
        int fixed_total = 0;
        int auto_count = 0;

        // Pass 1: sum fixed widths, count auto children
        // Skip intrinsic-width children (they render to natural size)
        for (int i = 0; i < layout->n_children; i++) {
            BoxLayout *child = layout->children[i];
            if (child->width > 0) {
                fixed_total += child->width;
            } else if (child->width == WIDTH_INTRINSIC) {
                // Intrinsic width - don't count towards distribution
                // These will render to natural size
            } else {
                // width == -1 or -2 means auto
                auto_count++;
            }
        }

        // Calculate width for auto children
        int remaining = layout->computed_width - fixed_total;
        if (remaining < 0) remaining = 0;

        int auto_width = 0;
        int auto_remainder = 0;
        if (auto_count > 0) {
            auto_width = remaining / auto_count;
            auto_remainder = remaining % auto_count; // Distribute extra pixels to first children
        }

        // Pass 2: assign computed widths and recurse
        int auto_index = 0;
        for (int i = 0; i < layout->n_children; i++) {
            BoxLayout *child = layout->children[i];

            if (child->width > 0) {
                // Fixed width child
                child->computed_width = child->width;
            } else if (child->width == WIDTH_INTRINSIC) {
                // Intrinsic width - don't set computed_width, let rendering decide
                child->computed_width = 0; // Signal to render naturally
            } else {
                // Auto width child - gets share of remaining space
                // Distribute remainder to first auto children for pixel-perfect fit
                int extra = (auto_index < auto_remainder) ? 1 : 0;
                child->computed_width = auto_width + extra;
                auto_index++;
            }

            // Recurse into child (use computed_width or parent_width for intrinsic)
            int child_parent_width =
                child->computed_width > 0 ? child->computed_width : parent_width;
            box_layout_resolve_widths(child, child_parent_width);
        }

        return; // Already handled children, don't fall through
    }

    // For VBOX and other types: children inherit parent width (existing behavior)
    for (int i = 0; i < layout->n_children; i++) {
        box_layout_resolve_widths(layout->children[i], layout->computed_width);
    }
}

// ============================================================================
// Validation
// ============================================================================

bool box_layout_validate(BoxLayout *layout, char *error_msg, int error_size) {
    if (!layout) return true;

    // For HBOX, check that children widths don't exceed parent
    if (layout->type == BOX_TYPE_HBOX) {
        int total_width = 0;
        for (int i = 0; i < layout->n_children; i++) {
            if (layout->children[i]->computed_width > 0) {
                total_width += layout->children[i]->computed_width;
            }
        }

        if (total_width > layout->computed_width) {
            snprintf(error_msg, error_size,
                     "hbox children width sum (%d) exceeds parent width (%d)", total_width,
                     layout->computed_width);
            return false;
        }
    }

    // Recursively validate children
    for (int i = 0; i < layout->n_children; i++) {
        if (!box_layout_validate(layout->children[i], error_msg, error_size)) {
            return false;
        }
    }

    return true;
}

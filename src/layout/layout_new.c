// layout_new.c - BoxLayout constructors and lifecycle management
//
// Functions for creating, destroying, and modifying BoxLayout nodes.

#include "layout_internal.h"
#include "layout_types.h"

#include <stdlib.h>
#include <string.h>

// ============================================================================
// Constructor
// ============================================================================

BoxLayout *box_layout_new(BoxLayoutType type, int width) {
    BoxLayout *layout = calloc(1, sizeof(BoxLayout));
    if (!layout) return NULL;

    layout->type = type;
    layout->width = width;
    layout->computed_width = width;
    layout->h_align = ALIGN_LEFT;
    layout->v_align = ALIGN_TOP;

    // Children (for HBOX/VBOX)
    layout->children = NULL;
    layout->n_children = 0;
    layout->children_capacity = 0;

    // Content (for CONTENT type)
    layout->content = NULL;
    layout->preformatted = false; // Default: apply compositor

    // Math source (for DISPLAY_MATH)
    layout->math_src = NULL;

    // Command (for COMMAND type)
    layout->command_name = NULL;
    layout->command_args = NULL;
    layout->n_command_args = 0;

    // Rule parameters (for VRULE/HRULE)
    layout->rule_width = 0;
    layout->rule_height = 0;
    layout->rule_start = NULL;
    layout->rule_fill = NULL;
    layout->rule_center = NULL;
    layout->rule_end = NULL;

    // Rendering cache
    layout->rendered = NULL;

    return layout;
}

// ============================================================================
// Destructor
// ============================================================================

void box_layout_free(BoxLayout *layout) {
    if (!layout) return;

    // Free children recursively
    for (int i = 0; i < layout->n_children; i++) {
        box_layout_free(layout->children[i]);
    }
    free(layout->children);

    // Free content strings
    free(layout->content);
    free(layout->math_src);
    free(layout->command_name);

    // Free command args
    for (int i = 0; i < layout->n_command_args; i++) {
        free(layout->command_args[i]);
    }
    free(layout->command_args);

    // Free rule strings
    free(layout->rule_start);
    free(layout->rule_fill);
    free(layout->rule_center);
    free(layout->rule_end);

    // Free line routine name
    free(layout->routine_name);

    // Free line insert ID
    free(layout->lineinsert_id);

    // Free ANSI codes
    free(layout->ansi_codes);

    // Free rendered cache
    if (layout->rendered) {
        box_free(layout->rendered);
        free(layout->rendered);
    }

    free(layout);
}

// ============================================================================
// Child Management
// ============================================================================

void box_layout_add_child(BoxLayout *parent, BoxLayout *child) {
    if (!parent || !child) return;

    // Grow children array if needed
    if (parent->n_children >= parent->children_capacity) {
        int new_capacity = parent->children_capacity == 0 ? 4 : parent->children_capacity * 2;
        BoxLayout **new_children = realloc(parent->children, new_capacity * sizeof(BoxLayout *));
        if (!new_children) return; // OOM
        parent->children = new_children;
        parent->children_capacity = new_capacity;
    }

    parent->children[parent->n_children++] = child;
}

// ============================================================================
// Content Setters
// ============================================================================

void box_layout_set_content(BoxLayout *layout, const char *content) {
    if (!layout) return;
    free(layout->content);
    layout->content = content ? strdup(content) : NULL;
}

void box_layout_set_math(BoxLayout *layout, const char *math_src) {
    if (!layout || layout->type != BOX_TYPE_DISPLAY_MATH) return;

    free(layout->math_src);
    layout->math_src = math_src ? strdup(math_src) : NULL;
}

void box_layout_set_command(BoxLayout *layout, const char *name, char **args, int n_args) {
    if (!layout || layout->type != BOX_TYPE_COMMAND) return;

    // Free old command
    free(layout->command_name);
    for (int i = 0; i < layout->n_command_args; i++) {
        free(layout->command_args[i]);
    }
    free(layout->command_args);

    // Set new command
    layout->command_name = name ? strdup(name) : NULL;
    layout->command_args = NULL;
    layout->n_command_args = n_args;

    if (n_args > 0 && args) {
        layout->command_args = malloc(n_args * sizeof(char *));
        if (layout->command_args) {
            for (int i = 0; i < n_args; i++) {
                layout->command_args[i] = args[i] ? strdup(args[i]) : NULL;
            }
        } else {
            layout->n_command_args = 0; // OOM
        }
    }
}

// layout.h - Public API for Hyades box layout system
//
// The box layout system provides document structure through nested
// horizontal (hbox) and vertical (vbox) containers. This is the primary
// interface for document rendering.
//
// Usage:
//   BoxLayout *doc = parse_document_as_vbox(input, width, &err);
//   Box *rendered = box_layout_render(doc, &options, &err);
//   char *output = box_to_string(rendered);
//   // ... use output ...
//   free(output);
//   box_free(rendered);
//   free(rendered);
//   box_layout_free(doc);

#ifndef LAYOUT_H
#define LAYOUT_H

#include "compositor/compositor.h" // For CompOptions
#include "layout_types.h"
#include "utils/error.h" // For ParseError

// ============================================================================
// BoxLayout Construction
// ============================================================================

// Create a new BoxLayout node
// type: BOX_TYPE_HBOX, BOX_TYPE_VBOX, etc.
// width: desired width (-1 for auto/inherit)
BoxLayout *box_layout_new(BoxLayoutType type, int width);

// Free a BoxLayout and all its children
void box_layout_free(BoxLayout *layout);

// Add a child to an hbox or vbox
void box_layout_add_child(BoxLayout *parent, BoxLayout *child);

// Set text content (for BOX_TYPE_CONTENT)
void box_layout_set_content(BoxLayout *layout, const char *content);

// Set math source (for BOX_TYPE_DISPLAY_MATH)
void box_layout_set_math(BoxLayout *layout, const char *math_src);

// Set command (for BOX_TYPE_COMMAND)
void box_layout_set_command(BoxLayout *layout, const char *name, char **args, int n_args);

// ============================================================================
// Rule Construction
// ============================================================================

// Create horizontal rule with custom characters
// width: fixed width in columns
// left, fill, right: UTF-8 strings for ends and middle
BoxLayout *box_layout_hrule_custom(int width, const char *left, const char *fill,
                                   const char *right);

// Create vertical rule with custom characters
// width: character width, height: fixed height in lines
// center: optional center character (NULL = use fill), for curly brace waist
BoxLayout *box_layout_vrule_custom(int width, int height, const char *top, const char *fill,
                                   const char *center, const char *bottom);

// Create auto-sizing rules (height/width determined at render time)
BoxLayout *box_layout_hrule_auto(int width, const char *left, const char *fill, const char *right);
BoxLayout *box_layout_vrule_auto(int width, int height, const char *top, const char *fill,
                                 const char *center, const char *bottom);

// ============================================================================
// Width Resolution
// ============================================================================

// Resolve width inheritance throughout the layout tree
// Call after constructing the tree but before rendering
void box_layout_resolve_widths(BoxLayout *layout, int parent_width);

// Validate layout structure (e.g., hbox children don't exceed width)
bool box_layout_validate(BoxLayout *layout, char *error_msg, int error_size);

// ============================================================================
// Rendering
// ============================================================================

// Render a BoxLayout tree to a Box
// opt: rendering options (may be modified by commands in the document)
// err: error output (may be NULL)
// Returns newly allocated Box, or NULL on error
Box *box_layout_render(BoxLayout *layout, CompOptions *opt, ParseError *err);

// ============================================================================
// Box Operations
// ============================================================================

// Convert a Box to a UTF-8 string (with newlines)
// Returns newly allocated string, caller must free
char *box_to_string(Box *box);

// Encode a single cell value to UTF-8, handling combining mark encoded cells.
// Returns number of bytes written (up to 8 for base + combining mark).
// Buffer must have room for at least 8 bytes.
size_t encode_cell_utf8(uint32_t cell, char *out);

// Merge boxes horizontally (side by side)
// v_align: ALIGN_TOP for top-alignment (tables), other values for baseline alignment (math)
Box *hbox_merge(Box **boxes, int n, Alignment v_align);

// Merge boxes vertically (stacked)
Box *vbox_merge(Box **boxes, int n, int width);

// Apply alignment to a box within a container
Box *apply_alignment(Box *content, int container_width, int container_height, Alignment h_align,
                     Alignment v_align);

// Measure actual content span (leftmost to rightmost non-space content)
// Used for measuring natural width of intrinsic boxes
int measure_content_span(Box *box);

// ============================================================================
// Lineroutine Support
// ============================================================================

// Get text content of a line by marker ID (e.g., "__lr_0__")
// Returns NULL if not found, caller must free the returned string
// Used by string primitives to inspect lineroutine content
char *line_registry_get_text(const char *id);

#endif // LAYOUT_H

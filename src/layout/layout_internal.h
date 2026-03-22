// layout_internal.h - Internal declarations for Hyades layout module
//
// This header is for functions shared between .c files in the layout module
// but not exposed in the public API.

#ifndef LAYOUT_INTERNAL_H
#define LAYOUT_INTERNAL_H

#include "layout_types.h"
#include "math/ast.h" // For Box type

// ============================================================================
// BoxLayout Constructors (layout_new.c)
// ============================================================================

BoxLayout *box_layout_new(BoxLayoutType type, int width);
void box_layout_free(BoxLayout *layout);
void box_layout_add_child(BoxLayout *parent, BoxLayout *child);
void box_layout_set_content(BoxLayout *layout, const char *content);
void box_layout_set_math(BoxLayout *layout, const char *math_src);
void box_layout_set_command(BoxLayout *layout, const char *name, char **args, int n_args);

// ============================================================================
// Box Primitive Operations (box.c)
// ============================================================================

// Convert a Box to a UTF-8 string with newlines
// Returns newly allocated string, caller must free
char *box_to_string(Box *box);

// Convert a UTF-8 string to a Box
// Returns newly allocated Box, caller must free
Box *string_to_box(const char *str);

// Convert a UTF-8 string to a Box with parallel metadata array
// meta: array of metadata flags (one per codepoint, not per byte)
// meta_len: number of entries in meta array
// Returns newly allocated Box, caller must free
Box *string_to_box_with_meta(const char *str, const uint8_t *meta, int meta_len);

// ============================================================================
// Alignment Operations (box_align.c)
// ============================================================================

// Apply horizontal alignment to a box within a container
// Returns original box if no change needed, otherwise new box
Box *apply_h_alignment(Box *content, int container_width, Alignment align);

// Apply vertical alignment to a box within a container
// Returns original box if no change needed, otherwise new box
Box *apply_v_alignment(Box *content, int container_height, Alignment align);

// Apply both horizontal and vertical alignment
Box *apply_alignment(Box *content, int container_width, int container_height, Alignment h_align,
                     Alignment v_align);

// ============================================================================
// Box Merging Operations (box_merge.c)
// ============================================================================

// Merge boxes horizontally (side by side)
// boxes: array of Box pointers
// n: number of boxes
// v_align: ALIGN_TOP for top-alignment (tables), other values for baseline alignment (math)
// Returns newly allocated merged Box, caller must free
Box *hbox_merge(Box **boxes, int n, Alignment v_align);

// Merge boxes vertically (stacked)
// boxes: array of Box pointers
// n: number of boxes
// width: container width (for padding shorter lines)
// Returns newly allocated merged Box, caller must free
Box *vbox_merge(Box **boxes, int n, int width);

// Merge boxes vertically with y-offset adjustments (for \vskip)
// y_offsets[i] is applied before placing box i (can be negative for overlap)
Box *vbox_merge_with_skips(Box **boxes, int *y_offsets, int n, int width);

// Merge boxes horizontally with x-offset adjustments (for \hskip)
// x_offsets[i] is applied before placing box i (can be negative for overlap)
Box *hbox_merge_with_skips(Box **boxes, int *x_offsets, int n);

// ============================================================================
// Width Resolution (width_resolve.c)
// ============================================================================

// Resolve width inheritance throughout the layout tree
// Called after parsing to compute actual widths
void box_layout_resolve_widths(BoxLayout *layout, int parent_width);

// ============================================================================
// Validation (layout_validate.c)
// ============================================================================

// Validate layout structure
// Returns true if valid, false otherwise
// On error, writes message to error_msg
bool box_layout_validate(BoxLayout *layout, char *error_msg, int error_size);

// ============================================================================
// Rule Rendering (rules.c)
// ============================================================================

// Create horizontal rule with custom characters
// left, fill, right can be multi-char UTF-8 strings
BoxLayout *box_layout_hrule_custom(int width, const char *left, const char *fill,
                                   const char *right);

// Create vertical rule with custom characters
// center is optional (NULL = use fill for all middle rows)
BoxLayout *box_layout_vrule_custom(int width, int height, const char *top, const char *fill,
                                   const char *center, const char *bottom);

// Create vrule with potentially auto height
// height = RULE_SIZE_AUTO for automatic height based on hbox siblings
// center is optional (NULL = use fill for all middle rows)
BoxLayout *box_layout_vrule_auto(int width, int height, const char *top, const char *fill,
                                 const char *center, const char *bottom);

// Create hrule with potentially auto width
// width = RULE_SIZE_AUTO for automatic width based on vbox parent
BoxLayout *box_layout_hrule_auto(int width, const char *left, const char *fill, const char *right);

// ============================================================================
// Skip Operations (box_layout_helpers.c)
// ============================================================================

// Create vertical skip (positive = down, negative = overlap upward)
BoxLayout *box_layout_vskip(int lines);

// Create horizontal skip (positive = right, negative = overlap leftward)
BoxLayout *box_layout_hskip(int columns);

// ============================================================================
// State Reset (layout_render.c)
// ============================================================================

// Reset line registry state (for WASM re-renders)
void line_registry_reset(void);

#endif // LAYOUT_INTERNAL_H

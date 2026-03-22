// layout_types.h - Type definitions for Hyades box layout system
//
// This file contains the core type definitions used throughout the layout system.
// It should be included by any code that works with BoxLayout or Box types.

#ifndef LAYOUT_TYPES_H
#define LAYOUT_TYPES_H

#include <stdbool.h>
#include <stdint.h>

// Forward declaration for ast.h dependency
// (Box is defined in ast.h but we need it here)
#include "math/ast.h"

// ============================================================================
// Special Values
// ============================================================================

// Special value for auto-sizing rules
#define RULE_SIZE_AUTO -1

// Width value meanings:
//   -1 = not specified (inherit from parent)
//   -2 = explicitly "auto" (same as -1, but explicit in syntax)
//   -3 = intrinsic (render to natural size, don't apply width distribution)
//   >0 = fixed width in columns
#define WIDTH_NOT_SPECIFIED -1
#define WIDTH_AUTO -2
#define WIDTH_INTRINSIC -3

// ============================================================================
// BoxLayout Type
// ============================================================================

typedef enum {
    BOX_TYPE_HBOX,            // Horizontal arrangement of children
    BOX_TYPE_VBOX,            // Vertical arrangement of children
    BOX_TYPE_CONTENT,         // Text content (inline math $...$ allowed)
    BOX_TYPE_DISPLAY_MATH,    // Display math $$...$$
    BOX_TYPE_COMMAND,         // Document command (\setunicode, etc.)
    BOX_TYPE_INTERSECT_RULES, // Container that auto-fixes rule intersections
    BOX_TYPE_VRULE,           // Vertical rule (deferred rendering for auto height)
    BOX_TYPE_HRULE,           // Horizontal rule (deferred rendering for auto width)
    BOX_TYPE_LINE_BREAK,      // Line break (\\) - no vertical space, resets spacing chain
    BOX_TYPE_LINEROUTINE,     // Line routine - post-process rendered lines with a macro
    BOX_TYPE_LINEINSERT,      // Line insert - placeholder for pre-rendered line content
    BOX_TYPE_VSKIP,           // Vertical skip - adjust y_offset (can be negative for overlap)
    BOX_TYPE_HSKIP,           // Horizontal skip - adjust x_offset (can be negative for overlap)
    BOX_TYPE_ANSI             // ANSI escape sequence (zero-width, emitted at render time)
} BoxLayoutType;

// ============================================================================
// Alignment
// ============================================================================

typedef enum {
    // Horizontal alignments
    ALIGN_LEFT,
    ALIGN_CENTER,
    ALIGN_RIGHT,
    ALIGN_JUSTIFY,

    // Vertical alignments
    ALIGN_TOP,
    ALIGN_MIDDLE,
    ALIGN_BOTTOM
} Alignment;

// ============================================================================
// BoxLayout Structure
// ============================================================================

typedef struct BoxLayout {
    BoxLayoutType type;

    // Dimensions
    int width;          // Specified width (-1 = inherit from parent)
    int computed_width; // Resolved width after inheritance

    // Alignment
    Alignment h_align; // Horizontal alignment
    Alignment v_align; // Vertical alignment

    // For HBOX/VBOX - children
    struct BoxLayout **children;
    int n_children;
    int children_capacity;

    // For CONTENT - text to compose (may contain inline $...$)
    char *content;     // Text content
    bool preformatted; // If true, skip compositor (for rules, etc.)

    // For DISPLAY_MATH - math source
    char *math_src; // Math source code (content of $$...$$)

    // For COMMAND - command to execute
    char *command_name;  // Command name (e.g., "setunicode")
    char **command_args; // Command arguments
    int n_command_args;  // Number of arguments

    // For VRULE/HRULE - rule parameters (for deferred rendering)
    int rule_width;    // Width of rule (for vrule: char width, for hrule: may be auto)
    int rule_height;   // Height of rule (for vrule: may be auto, for hrule: always 1)
    char *rule_start;  // Start cap (top for vrule, left for hrule)
    char *rule_fill;   // Fill character
    char *rule_center; // Center character (vrule only, for curly brace waist)
    char *rule_end;    // End cap (bottom for vrule, right for hrule)

    // For LINEROUTINE - line routine parameters
    char *routine_name; // Name of macro to apply to each line

    // For LINEINSERT - pre-rendered line content reference
    char *lineinsert_id; // ID to look up pre-rendered line Box

    // For VSKIP/HSKIP - skip amount (can be negative for overlap)
    int skip_amount; // Lines (vskip) or columns (hskip) to skip

    // For ANSI - escape code string (zero-width, applied to adjacent content)
    char *ansi_codes; // ANSI codes (e.g., "31" for red, "0" for reset)

    // Inherited ANSI style (for rules unwrapped from \term_color wrapping)
    // Encoding: (bg_sgr << 8) | fg_sgr, 0 = none
    uint16_t inherit_style;

    // Background fill style for \child[bg=color] — applied to ALL cells after rendering
    // Encoding: (bg_sgr << 8), fg bits always 0. 0 = no fill.
    uint16_t bg_fill;

    // Rendering cache
    Box *rendered; // Cached rendered box (NULL = not rendered yet)
} BoxLayout;

#endif // LAYOUT_TYPES_H

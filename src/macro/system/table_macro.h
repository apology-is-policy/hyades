// table_macro.h - Table macro system for Hyades
// Expands \table{...} syntax into box layout primitives

#ifndef TABLE_MACRO_H
#define TABLE_MACRO_H

#include "../../document/calc.h" // For CalcContext
#include <stdbool.h>

// ============================================================================
// Table Macro Expansion
// ============================================================================
//
// The table macro transforms:
//
//   \table[width:60][frame:double]{
//       \row[frame:{b:double}]{
//           \col[width:20][align:c]{Header A}
//           \col[align:c]{Header B}
//       }
//       \row{
//           \col{Data 1}
//           \col{Data 2}
//       }
//   }
//
// Into box layout primitives:
//
//   \intersect_rules{
//   \begin[60]{vbox}
//     \child{\hrule[auto]{}{═}{}}
//     \child{\begin{hbox}
//       \child[1]{\vrule[auto]{}{║}{}}
//       \child[20][center]{Header A}
//       \child[1]{\vrule[auto]{}{║}{}}
//       \child[center]{Header B}
//       \child[1]{\vrule[auto]{}{║}{}}
//     \end{hbox}}
//     \child{\hrule[auto]{}{═}{}}
//     ... etc
//   \end{vbox}
//   }
//
// ============================================================================

// Frame styles
typedef enum {
    FRAME_UNSET = -1, // Not specified (edge retains inherited value)
    FRAME_NONE,       // Explicitly no frame
    FRAME_DOTTED,     // ┄ ┆
    FRAME_SINGLE,     // ─ │
    FRAME_ROUNDED,    // ─ │ (same lines, but rounded corners ╭╮╰╯ at table border)
    FRAME_DOUBLE,     // ═ ║
    FRAME_BOLD,       // ━ ┃
} FrameStyle;

// Alignment
typedef enum { TABLE_ALIGN_LEFT, TABLE_ALIGN_CENTER, TABLE_ALIGN_RIGHT } TableAlign;

typedef enum { TABLE_VALIGN_TOP, TABLE_VALIGN_MIDDLE, TABLE_VALIGN_BOTTOM } TableVAlign;

// Padding specification
typedef struct {
    int top;
    int bottom;
    int left;
    int right;
} Padding;

// Frame specification (per-edge)
typedef struct {
    FrameStyle top;
    FrameStyle bottom;
    FrameStyle left;
    FrameStyle right;
} FrameSpec;

// Column properties (persists down the column)
typedef struct {
    int width; // -1 = auto
    TableAlign align;
    TableVAlign valign;
    FrameSpec frame;
    Padding pad;
    bool has_width;
    bool has_align;
    bool has_valign;
    bool has_frame;
    bool has_pad;
} ColProps;

// Cell in a table
typedef struct {
    char *content;  // Cell content (Hyades script)
    ColProps props; // Cell-level properties
    int col_span;   // Column span (default 1)
    int row_span;   // Row span (default 1, not implemented yet)
} TableCell;

// Row in a table
typedef struct {
    TableCell **cells;
    int n_cells;
    int capacity;

    // Row-level properties
    int height; // -1 = auto
    FrameSpec frame;
    TableAlign align;
    TableVAlign valign;
    Padding pad;
    bool has_height;
    bool has_frame;
    bool has_align;
    bool has_valign;
    bool has_pad;
} TableRow;

// Complete table
typedef struct {
    TableRow **rows;
    int n_rows;
    int capacity;

    // Table-level properties
    int width; // -1 = auto (use parent width)
    FrameSpec frame;
    TableAlign align;
    TableVAlign valign;
    Padding pad;
    bool has_width;
    bool has_frame;
    bool has_align;
    bool has_valign;
    bool has_pad;

    // Outer border override (takes priority over frame at table edges)
    FrameSpec border;
    bool has_border;

    // Computed values
    int n_cols;          // Max columns across all rows
    ColProps *col_props; // Column property inheritance array

    // Row-level inheritance defaults (accumulate from previous rows)
    FrameSpec row_frame;
    TableAlign row_align;
    TableVAlign row_valign;
    Padding row_pad;
    bool has_row_frame;
    bool has_row_align;
    bool has_row_valign;
    bool has_row_pad;
} Table;

// ============================================================================
// Parsing API
// ============================================================================

// Parse a \table{...} macro and return the Table structure
// Returns NULL on parse error
// *end_pos is updated to point past the closing }
// calc_ctx may be NULL if calc expansion is not needed
Table *table_parse(const char *input, int *end_pos, CalcContext *calc_ctx, char *error_msg,
                   int error_size);

// Free a parsed table
void table_free(Table *table);

// ============================================================================
// Expansion API
// ============================================================================

// Expand a Table into Hyades box layout script
// Returns newly allocated string, caller must free
// Returns NULL on error
char *table_expand(const Table *table, char *error_msg, int error_size);

// ============================================================================
// Convenience API
// ============================================================================

// Parse and expand in one step
// Returns newly allocated Hyades script string, caller must free
// Returns NULL on error
// calc_ctx may be NULL if calc expansion is not needed
char *table_macro_expand(const char *input, int *end_pos, CalcContext *calc_ctx, char *error_msg,
                         int error_size);

// Check if input starts with \table
bool is_table_macro(const char *input);

// ============================================================================
// Frame Character Helpers
// ============================================================================

// Get the horizontal line character for a frame style
const char *frame_hline_char(FrameStyle style);

// Get the vertical line character for a frame style
const char *frame_vline_char(FrameStyle style);

#endif // TABLE_MACRO_H
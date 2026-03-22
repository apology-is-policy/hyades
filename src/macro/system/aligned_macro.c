// aligned_macro.c - Aligned equations macro for Hyades
// Expands \aligned{...} and \cases{...} into measure/layout code
//
// KNOWN LIMITATION: Multi-line math expressions (fractions, roots, superscripts)
// do not align at the baseline because hbox processes lines sequentially rather
// than treating each child as a complete box. Single-line expressions work correctly.

#include "aligned_macro.h"
#include "diagnostics/diagnostics.h"
#include "utils/util.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Data Structures
// ============================================================================

typedef struct {
    char *content; // Cell content (without $ delimiters)
} AlignedCell;

typedef struct {
    AlignedCell *cells;
    int n_cells;
    int capacity;
    bool is_intertext;
    char *intertext_content;
} AlignedRow;

typedef struct {
    AlignedRow *rows;
    int n_rows;
    int capacity;
    int n_cols; // Max columns across all rows
} AlignedGrid;

// Options for aligned/cases rendering
typedef struct {
    int space; // Spacing around alignment point (default 1 = 1 char each side)
    int vpad;  // Vertical padding between rows (default 1 = 1 blank line)
} AlignedOptions;

// ============================================================================
// Memory Management
// ============================================================================

static void cell_free(AlignedCell *cell) {
    if (cell) {
        free(cell->content);
    }
}

// Check if content starts with an operator that should have space after it
static bool is_leading_operator(const char *content) {
    if (!content || !*content) return false;
    // Common alignment operators
    return (*content == '=' || *content == '<' || *content == '>' || *content == '+' ||
            *content == '-' ||
            (content[0] == '\\' && content[1] == 'l' && content[2] == 'e') || // \le, \leq
            (content[0] == '\\' && content[1] == 'g' && content[2] == 'e') || // \ge, \geq
            (content[0] == '\\' && content[1] == 'n' && content[2] == 'e') || // \ne, \neq
            (content[0] == '\\' && content[1] == 'a' && content[2] == 'p'));  // \approx
}

static void row_init(AlignedRow *row) {
    row->cells = NULL;
    row->n_cells = 0;
    row->capacity = 0;
    row->is_intertext = false;
    row->intertext_content = NULL;
}

static void row_add_cell(AlignedRow *row, const char *content, size_t len) {
    if (row->n_cells >= row->capacity) {
        row->capacity = row->capacity == 0 ? 4 : row->capacity * 2;
        row->cells = realloc(row->cells, row->capacity * sizeof(AlignedCell));
    }

    // Trim leading/trailing whitespace from content
    const char *start = content;
    const char *end = content + len;

    while (start < end && (*start == ' ' || *start == '\t' || *start == '\n')) {
        start++;
    }
    while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\n')) {
        end--;
    }

    size_t trimmed_len = end - start;
    char *cell_content = malloc(trimmed_len + 1);
    if (trimmed_len > 0) {
        memcpy(cell_content, start, trimmed_len);
    }
    cell_content[trimmed_len] = '\0';

    row->cells[row->n_cells].content = cell_content;
    row->n_cells++;
}

static void row_free(AlignedRow *row) {
    if (row) {
        for (int i = 0; i < row->n_cells; i++) {
            cell_free(&row->cells[i]);
        }
        free(row->cells);
        free(row->intertext_content);
    }
}

static AlignedGrid *grid_new(void) {
    AlignedGrid *grid = malloc(sizeof(AlignedGrid));
    grid->rows = NULL;
    grid->n_rows = 0;
    grid->capacity = 0;
    grid->n_cols = 0;
    return grid;
}

static AlignedRow *grid_add_row(AlignedGrid *grid) {
    if (grid->n_rows >= grid->capacity) {
        grid->capacity = grid->capacity == 0 ? 4 : grid->capacity * 2;
        grid->rows = realloc(grid->rows, grid->capacity * sizeof(AlignedRow));
    }

    AlignedRow *row = &grid->rows[grid->n_rows++];
    row_init(row);
    return row;
}

static void grid_free(AlignedGrid *grid) {
    if (grid) {
        for (int i = 0; i < grid->n_rows; i++) {
            row_free(&grid->rows[i]);
        }
        free(grid->rows);
        free(grid);
    }
}

// ============================================================================
// String Buffer
// ============================================================================

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} StrBuf;

static void strbuf_init(StrBuf *sb) {
    sb->capacity = 256;
    sb->data = malloc(sb->capacity);
    sb->data[0] = '\0';
    sb->len = 0;
}

static void strbuf_append(StrBuf *sb, const char *str) {
    size_t add_len = strlen(str);
    if (sb->len + add_len + 1 > sb->capacity) {
        while (sb->len + add_len + 1 > sb->capacity) {
            sb->capacity *= 2;
        }
        sb->data = realloc(sb->data, sb->capacity);
    }
    memcpy(sb->data + sb->len, str, add_len + 1);
    sb->len += add_len;
}

static void strbuf_append_len(StrBuf *sb, const char *str, size_t add_len) {
    if (sb->len + add_len + 1 > sb->capacity) {
        while (sb->len + add_len + 1 > sb->capacity) {
            sb->capacity *= 2;
        }
        sb->data = realloc(sb->data, sb->capacity);
    }
    memcpy(sb->data + sb->len, str, add_len);
    sb->len += add_len;
    sb->data[sb->len] = '\0';
}

static void strbuf_appendf(StrBuf *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (sb->len + needed + 1 > sb->capacity) {
        while (sb->len + needed + 1 > sb->capacity) {
            sb->capacity *= 2;
        }
        sb->data = realloc(sb->data, sb->capacity);
    }

    vsnprintf(sb->data + sb->len, needed + 1, fmt, args);
    sb->len += needed;

    va_end(args);
}

// ============================================================================
// Parsing
// ============================================================================

// Skip whitespace
static void skip_ws(const char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') {
        (*p)++;
    }
}

// Parse braced content {...}, handling nested braces
static char *parse_braced_content(const char **p) {
    skip_ws(p);
    if (**p != '{') return NULL;
    (*p)++;

    const char *start = *p;
    int depth = 1;

    while (**p && depth > 0) {
        if (**p == '{')
            depth++;
        else if (**p == '}')
            depth--;
        if (depth > 0) (*p)++;
    }

    size_t len = *p - start;
    char *content = malloc(len + 1);
    memcpy(content, start, len);
    content[len] = '\0';

    if (**p == '}') (*p)++;

    return content;
}

// Parse optional parameters [space][vpad] or [space,vpad]
static AlignedOptions parse_aligned_options(const char **p) {
    AlignedOptions opts = {.space = 1, .vpad = 1}; // defaults

    skip_ws(p);
    if (**p != '[') return opts;

    (*p)++; // skip '['

    // Parse first number (space)
    int val = 0;
    bool has_val = false;
    while (**p >= '0' && **p <= '9') {
        val = val * 10 + (**p - '0');
        has_val = true;
        (*p)++;
    }
    if (has_val) opts.space = val;

    // Check for comma or second bracket
    skip_ws(p);
    if (**p == ',') {
        (*p)++;
        skip_ws(p);
        // Parse second number (vpad)
        val = 0;
        has_val = false;
        while (**p >= '0' && **p <= '9') {
            val = val * 10 + (**p - '0');
            has_val = true;
            (*p)++;
        }
        if (has_val) opts.vpad = val;
    }

    skip_ws(p);
    if (**p == ']') (*p)++;

    return opts;
}

// Check if a completed row is an \intertext{} row
static void check_intertext(AlignedRow *row) {
    if (row->n_cells != 1) return;
    const char *ct = row->cells[0].content;
    while (*ct == ' ' || *ct == '\t') ct++;
    if (strncmp(ct, "\\intertext{", 11) != 0) return;
    const char *bp = ct + 10;
    int depth = 0;
    const char *start = NULL;
    while (*bp) {
        if (*bp == '{') {
            if (depth == 0) start = bp + 1;
            depth++;
        } else if (*bp == '}') {
            depth--;
            if (depth == 0) break;
        }
        bp++;
    }
    if (start && depth == 0) {
        row->is_intertext = true;
        row->intertext_content = strndup(start, bp - start);
    }
}

// Parse the aligned content into a grid
// Splits by \\ for rows and & for columns
static AlignedGrid *parse_aligned_content(const char *content, char *error_msg, int error_size) {
    AlignedGrid *grid = grid_new();

    const char *p = content;
    const char *row_start = p;
    const char *cell_start = p;

    AlignedRow *current_row = grid_add_row(grid);

    while (*p) {
        // Check for \\ (row break)
        if (p[0] == '\\' && p[1] == '\\') {
            // End current cell
            row_add_cell(current_row, cell_start, p - cell_start);

            // Check for \intertext before updating max columns
            check_intertext(current_row);

            // Update max columns (skip intertext rows)
            if (!current_row->is_intertext && current_row->n_cells > grid->n_cols) {
                grid->n_cols = current_row->n_cells;
            }

            // Start new row
            p += 2;
            current_row = grid_add_row(grid);
            cell_start = p;
            row_start = p;
            continue;
        }

        // Check for & (column separator)
        if (*p == '&') {
            // End current cell
            row_add_cell(current_row, cell_start, p - cell_start);

            p++;
            cell_start = p;
            continue;
        }

        // Skip over braced content (don't split inside braces)
        if (*p == '{') {
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '{')
                    depth++;
                else if (*p == '}')
                    depth--;
                p++;
            }
            continue;
        }

        // Detect \intertext{...} — acts as implicit row separator
        if (strncmp(p, "\\intertext{", 11) == 0) {
            // Finalize current row with content before \intertext
            // Check if there's meaningful content (not just whitespace)
            bool has_content = false;
            if (current_row->n_cells > 0) {
                has_content = true;
            } else {
                const char *chk = cell_start;
                while (chk < p && (*chk == ' ' || *chk == '\t' || *chk == '\n')) chk++;
                has_content = (chk < p);
            }
            if (has_content) {
                row_add_cell(current_row, cell_start, p - cell_start);
                if (!current_row->is_intertext && current_row->n_cells > grid->n_cols)
                    grid->n_cols = current_row->n_cells;
            } else {
                // Empty row before intertext — remove it
                row_free(current_row);
                grid->n_rows--;
            }
            // Extract braced content of \intertext{...}
            const char *bp = p + 10; // points to '{'
            int depth = 0;
            const char *it_start = NULL;
            while (*bp) {
                if (*bp == '{') {
                    if (depth == 0) it_start = bp + 1;
                    depth++;
                } else if (*bp == '}') {
                    depth--;
                    if (depth == 0) break;
                }
                bp++;
            }
            // Create intertext row
            AlignedRow *it_row = grid_add_row(grid);
            if (it_start && depth == 0) {
                it_row->is_intertext = true;
                it_row->intertext_content = strndup(it_start, bp - it_start);
                row_add_cell(it_row, "", 0); // placeholder cell
            }
            p = (*bp == '}') ? bp + 1 : bp;
            // Start new row for content after \intertext
            current_row = grid_add_row(grid);
            cell_start = p;
            row_start = p;
            continue;
        }

        // Handle escape sequences - skip \& and similar
        if (*p == '\\' && p[1] && p[1] != '\\') {
            p += 2;
            // Skip command name
            while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
            continue;
        }

        p++;
    }

    // Add final cell if there's content
    if (p > cell_start || current_row->n_cells > 0) {
        row_add_cell(current_row, cell_start, p - cell_start);
    }

    // Check final row for intertext
    check_intertext(current_row);

    // Update max columns for last row (skip intertext)
    if (!current_row->is_intertext && current_row->n_cells > grid->n_cols) {
        grid->n_cols = current_row->n_cells;
    }

    // Remove empty trailing row if present
    if (grid->n_rows > 0) {
        AlignedRow *last = &grid->rows[grid->n_rows - 1];
        if (last->n_cells == 0 || (last->n_cells == 1 && last->cells[0].content[0] == '\0')) {
            row_free(last);
            grid->n_rows--;
        }
    }

    if (grid->n_rows == 0 || grid->n_cols == 0) {
        snprintf(error_msg, error_size, "Empty aligned content");
        grid_free(grid);
        return NULL;
    }

    return grid;
}

// ============================================================================
// Code Generation
// ============================================================================

// Generate a unique prefix for this expansion to avoid register collisions
static int g_aligned_counter = 0;

static char *expand_grid(const AlignedGrid *grid, AlignedFlags flags, AlignedOptions opts,
                         char *error_msg, int error_size) {
    StrBuf sb;
    strbuf_init(&sb);

    // Unique prefix for this expansion
    int prefix = g_aligned_counter++;

    // Log aligned/cases expansion
    if (diag_is_enabled(DIAG_SYSTEM)) {
        const char *type = (flags & ALIGNED_BRACE_LEFT) ? "\\cases" : "\\aligned";
        diag_log(DIAG_SYSTEM, 0, "%s: %d rows x %d cols, prefix=%d", type, grid->n_rows,
                 grid->n_cols, prefix);

        // Log flags
        if (flags & ALIGNED_BRACE_LEFT) {
            if (grid->n_rows == 1) {
                diag_log(DIAG_SYSTEM, 1, "single-row: using \\vrule with \\{ brace");
            } else {
                diag_log(DIAG_SYSTEM, 1, "multi-row: using \\vrule with Unicode braces (⎧⎪⎨⎩)");
            }
        }
        if (flags & ALIGNED_BRACE_RIGHT) {
            diag_log(DIAG_SYSTEM, 1, "right brace enabled");
        }

        // Log first few cells for debugging
        for (int r = 0; r < grid->n_rows && r < 3; r++) {
            AlignedRow *row = &grid->rows[r];
            for (int c = 0; c < row->n_cells && c < 4; c++) {
                diag_log(DIAG_SYSTEM, 1, "cell[%d][%d]: \"%s\"", r, c,
                         row->cells[c].content ? row->cells[c].content : "");
            }
        }
        if (grid->n_rows > 3) {
            diag_log(DIAG_SYSTEM, 1, "... and %d more rows", grid->n_rows - 3);
        }
    }

    // Step 1: Generate measure commands for each cell
    for (int r = 0; r < grid->n_rows; r++) {
        AlignedRow *row = &grid->rows[r];
        if (row->is_intertext) continue;
        for (int c = 0; c < row->n_cells; c++) {
            const char *content = row->cells[c].content;
            // Register names: _aXX_RR_CC for content, _wXX_RR_CC for width, _hXX_RR_CC for height

            // For all columns after the first (after &), insert thin spaces after leading operators
            // This creates symmetric spacing like "a = 1" instead of "a =1"
            bool no_dollar = (flags & ALIGNED_NO_DOLLAR) != 0;
            const char *dleft = no_dollar ? "" : "$";
            const char *dright = no_dollar ? "" : "$";

            if (c > 0 && opts.space > 0 && is_leading_operator(content)) {
                // Find end of operator (single char or command like \leq)
                const char *op_end = content;
                if (*op_end == '\\') {
                    op_end++;
                    while (*op_end && isalpha((unsigned char)*op_end)) op_end++;
                } else {
                    op_end++;
                }
                // Build content with thin spaces after operator
                strbuf_appendf(&sb, "\\measure<_a%d_%d_%d,_w%d_%d_%d,_h%d_%d_%d>{%s", prefix, r, c,
                               prefix, r, c, prefix, r, c, dleft);
                // Append operator
                strbuf_append_len(&sb, content, op_end - content);
                // Append thin spaces (\ in math mode)
                for (int s = 0; s < opts.space; s++) {
                    strbuf_append(&sb, "\\ ");
                }
                // Append rest of content
                strbuf_append(&sb, op_end);
                strbuf_appendf(&sb, "%s}\n", dright);
            } else {
                strbuf_appendf(&sb, "\\measure<_a%d_%d_%d,_w%d_%d_%d,_h%d_%d_%d>{%s%s%s}\n", prefix,
                               r, c, prefix, r, c, prefix, r, c, dleft, content, dright);
            }
        }
    }

    // Step 2: Generate max-width calculations for each column
    for (int c = 0; c < grid->n_cols; c++) {
        // Start with 0 or first row's width
        bool first = true;
        for (int r = 0; r < grid->n_rows; r++) {
            AlignedRow *row = &grid->rows[r];
            if (row->is_intertext) continue;
            if (c >= row->n_cells) continue;

            if (first) {
                strbuf_appendf(&sb, "\\let<_mw%d_%d>{\\valueof<_w%d_%d_%d>}\n", prefix, c, prefix,
                               r, c);
                first = false;
            } else {
                // Update max: if new > current, use new
                strbuf_appendf(&sb,
                               "\\let<_mw%d_%d>{\\if{\\gt{\\valueof<_w%d_%d_%d>,\\valueof<_mw%d_%d>"
                               "}}{\\valueof<_w%d_%d_%d>}\\else{\\valueof<_mw%d_%d>}}\n",
                               prefix, c, prefix, r, c, prefix, c, prefix, r, c, prefix, c);
            }
        }
    }

    // Step 3: Calculate total height for stretchy braces (if needed)
    if (flags & (ALIGNED_BRACE_LEFT | ALIGNED_BRACE_RIGHT)) {
        // Sum all row heights and track max per row, plus vpad between rows
        strbuf_appendf(&sb, "\\let<_th%d>{0}\n", prefix);
        for (int r = 0; r < grid->n_rows; r++) {
            AlignedRow *row = &grid->rows[r];
            if (row->is_intertext) continue;
            // Find max height in this row
            strbuf_appendf(&sb, "\\let<_rh%d_%d>{0}\n", prefix, r);
            for (int c = 0; c < row->n_cells; c++) {
                strbuf_appendf(&sb,
                               "\\let<_rh%d_%d>{\\if{\\gt{\\valueof<_h%d_%d_%d>,\\valueof<_rh%d_%d>"
                               "}}{\\valueof<_h%d_%d_%d>}\\else{\\valueof<_rh%d_%d>}}\n",
                               prefix, r, prefix, r, c, prefix, r, prefix, r, c, prefix, r);
            }
            // Add to total
            strbuf_appendf(&sb, "\\let<_th%d>{\\add{\\valueof<_th%d>,\\valueof<_rh%d_%d>}}\n",
                           prefix, prefix, prefix, r);
            // Add vpad between rows (not after last row)
            if (r < grid->n_rows - 1 && opts.vpad > 0) {
                strbuf_appendf(&sb, "\\let<_th%d>{\\add{\\valueof<_th%d>,%d}}\n", prefix, prefix,
                               opts.vpad);
            }
        }
    }

    // Step 4: Generate the layout
    // If braces needed, wrap in hbox with brace + aligned content
    if (flags & ALIGNED_BRACE_LEFT) {
        strbuf_append(&sb, "\\begin{hbox}\n");
        // Use a vrule with Unicode brace characters
        // For 2+ rows: ⎧ top, ⎪ fill, ⎨ center (waist), ⎩ bottom
        // For 1 row: use vrule with height 1 to render single { character
        if (grid->n_rows == 1) {
            strbuf_append(&sb, "\\child[1]{\\vrule[1][1]{\\{}{}{}{\\}}}\n");
        } else {
            strbuf_appendf(&sb, "\\child[1]{\\vrule[1][\\valueof<_th%d>]{⎧}{⎪}{⎨}{⎩}}\n", prefix);
        }
        // Add 1-char spacing between brace and content
        strbuf_append(&sb, "\\child[1]{}\n");
        strbuf_append(&sb, "\\child{");
    }

    // Begin vbox for the rows
    strbuf_append(&sb, "\\begin{vbox}\n");

    for (int r = 0; r < grid->n_rows; r++) {
        AlignedRow *row = &grid->rows[r];

        if (row->is_intertext) {
            strbuf_appendf(&sb, "\\child{\xEE\x80\x80%s}\n", row->intertext_content);
            // Add vertical padding between rows (not after the last row)
            if (r < grid->n_rows - 1 && opts.vpad > 0) {
                strbuf_appendf(&sb, "\\xvskip{%d}\n", opts.vpad);
            }
            continue;
        }

        strbuf_append(&sb, "\\child{\\begin[middle]{hbox}\n");

        for (int c = 0; c < row->n_cells; c++) {
            // Alignment: even columns (0, 2, 4...) right-aligned, odd columns (1, 3, 5...) left-aligned
            const char *align = (c % 2 == 0) ? "right" : "left";

            // Add spacing between column pairs (between & separators)
            // For multiple alignment points (col 2, 4, ...), add extra separation
            if (c > 0 && c % 2 == 0 && opts.space > 0) {
                // Larger gap between column pairs (e.g., between "= b" and "= c")
                strbuf_appendf(&sb, "  \\child[%d]{}\n", opts.space * 2);
            }

            // Space before odd columns removed - we only add space after even columns
            // to avoid doubling up

            strbuf_appendf(&sb, "  \\child[\\valueof<_mw%d_%d>][%s]{\\recall<_a%d_%d_%d>}\n",
                           prefix, c, align, prefix, r, c);

            // Add space after the right-aligned content (after the variable, before &)
            if (c % 2 == 0 && opts.space > 0) {
                strbuf_appendf(&sb, "  \\child[%d]{}\n", opts.space);
            }
        }

        // Pad with empty cells if this row has fewer columns
        for (int c = row->n_cells; c < grid->n_cols; c++) {
            const char *align = (c % 2 == 0) ? "right" : "left";

            // Same spacing logic for padding cells
            if (c > 0 && c % 2 == 0 && opts.space > 0) {
                strbuf_appendf(&sb, "  \\child[%d]{}\n", opts.space * 2);
            }
            // Space before odd columns removed - we only add space after even columns

            strbuf_appendf(&sb, "  \\child[\\valueof<_mw%d_%d>][%s]{}\n", prefix, c, align);

            if (c % 2 == 0 && opts.space > 0) {
                strbuf_appendf(&sb, "  \\child[%d]{}\n", opts.space);
            }
        }

        strbuf_append(&sb, "\\end{hbox}}\n");

        // Add vertical padding between rows (not after the last row)
        // Use \xvskip inside box layouts (not \vskip which is for document level)
        if (r < grid->n_rows - 1 && opts.vpad > 0) {
            strbuf_appendf(&sb, "\\xvskip{%d}\n", opts.vpad);
        }
    }

    strbuf_append(&sb, "\\end{vbox}\n");

    if (flags & ALIGNED_BRACE_LEFT) {
        strbuf_append(&sb, "}\n\\end{hbox}\n");
    }

    if (flags & ALIGNED_BRACE_RIGHT) {
        // Similar wrapping for right brace (TODO)
    }

    return sb.data;
}

// ============================================================================
// Public API
// ============================================================================

bool is_aligned_macro(const char *input) {
    if (!input) return false;
    while (*input == ' ' || *input == '\t' || *input == '\n') input++;
    return strncmp(input, "\\aligned", 8) == 0;
}

bool is_cases_macro(const char *input) {
    if (!input) return false;
    while (*input == ' ' || *input == '\t' || *input == '\n') input++;
    return strncmp(input, "\\cases", 6) == 0;
}

static char *aligned_expand_internal(const char *input, int *end_pos, const char *cmd_name,
                                     int cmd_len, AlignedFlags flags, char *error_msg,
                                     int error_size) {
    const char *p = input;
    skip_ws(&p);

    if (strncmp(p, cmd_name, cmd_len) != 0) {
        snprintf(error_msg, error_size, "Expected %s", cmd_name);
        return NULL;
    }
    p += cmd_len;

    // Parse optional parameters [space,vpad]
    AlignedOptions opts = parse_aligned_options(&p);

    // Parse content
    char *content = parse_braced_content(&p);
    if (!content) {
        snprintf(error_msg, error_size, "Expected {content} after %s", cmd_name);
        return NULL;
    }

    if (end_pos) *end_pos = (int)(p - input);

    // Parse into grid
    AlignedGrid *grid = parse_aligned_content(content, error_msg, error_size);
    free(content);

    if (!grid) return NULL;

    // Generate output
    char *result = expand_grid(grid, flags, opts, error_msg, error_size);
    grid_free(grid);

    return result;
}

char *aligned_macro_expand(const char *input, int *end_pos, char *error_msg, int error_size) {
    return aligned_expand_internal(input, end_pos, "\\aligned", 8, ALIGNED_NONE, error_msg,
                                   error_size);
}

char *cases_macro_expand(const char *input, int *end_pos, char *error_msg, int error_size) {
    return aligned_expand_internal(input, end_pos, "\\cases", 6, ALIGNED_BRACE_LEFT, error_msg,
                                   error_size);
}

// Raw versions for math-mode embedding (no $...$ wrapping)
char *aligned_macro_expand_raw(const char *input, int *end_pos, char *error_msg, int error_size) {
    return aligned_expand_internal(input, end_pos, "\\aligned", 8, ALIGNED_NO_DOLLAR, error_msg,
                                   error_size);
}

char *cases_macro_expand_raw(const char *input, int *end_pos, char *error_msg, int error_size) {
    return aligned_expand_internal(input, end_pos, "\\cases", 6,
                                   ALIGNED_BRACE_LEFT | ALIGNED_NO_DOLLAR, error_msg, error_size);
}

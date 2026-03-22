// table_macro.c - Table macro system for Hyades
// Expands \table{...} syntax into box layout primitives

#include "table_macro.h"
#include "diagnostics/diagnostics.h" // For diagnostics
#include "layout/layout_types.h"     // For WIDTH_INTRINSIC
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Non-breaking space (U+00A0) - survives text processing unlike regular space
#define NBSP "\xC2\xA0"

// ============================================================================
// Frame Character Mapping
// ============================================================================

const char *frame_hline_char(FrameStyle style) {
    switch (style) {
    case FRAME_NONE: return " ";
    case FRAME_DOTTED: return "┄";
    case FRAME_SINGLE: return "─";
    case FRAME_ROUNDED: return "─";
    case FRAME_DOUBLE: return "═";
    case FRAME_BOLD: return "━";
    default: return "─";
    }
}

const char *frame_vline_char(FrameStyle style) {
    switch (style) {
    case FRAME_NONE: return " ";
    case FRAME_DOTTED: return "┆";
    case FRAME_SINGLE: return "│";
    case FRAME_ROUNDED: return "│";
    case FRAME_DOUBLE: return "║";
    case FRAME_BOLD: return "┃";
    default: return "│";
    }
}

// ============================================================================
// Default Values
// ============================================================================

static FrameSpec default_frame(void) {
    return (FrameSpec){
        .top = FRAME_SINGLE, .bottom = FRAME_SINGLE, .left = FRAME_SINGLE, .right = FRAME_SINGLE};
}

static Padding default_padding(void) {
    return (Padding){.top = 0, .bottom = 0, .left = 0, .right = 0};
}

static ColProps default_col_props(void) {
    return (ColProps){.width = -1,
                      .align = TABLE_ALIGN_LEFT,
                      .valign = TABLE_VALIGN_TOP,
                      .frame = default_frame(),
                      .pad = default_padding(),
                      .has_width = false,
                      .has_align = false,
                      .has_valign = false,
                      .has_frame = false,
                      .has_pad = false};
}

// ============================================================================
// Memory Management
// ============================================================================

static TableCell *cell_new(void) {
    TableCell *cell = malloc(sizeof(TableCell));
    cell->content = NULL;
    cell->props = default_col_props();
    cell->col_span = 1;
    cell->row_span = 1;
    return cell;
}

static void cell_free(TableCell *cell) {
    if (!cell) return;
    free(cell->content);
    free(cell);
}

static TableRow *row_new(void) {
    TableRow *row = malloc(sizeof(TableRow));
    row->cells = NULL;
    row->n_cells = 0;
    row->capacity = 0;
    row->height = -1;
    row->frame = default_frame();
    row->align = TABLE_ALIGN_LEFT;
    row->valign = TABLE_VALIGN_TOP;
    row->pad = default_padding();
    row->has_height = false;
    row->has_frame = false;
    row->has_align = false;
    row->has_valign = false;
    row->has_pad = false;
    return row;
}

static void row_add_cell(TableRow *row, TableCell *cell) {
    if (row->n_cells >= row->capacity) {
        row->capacity = row->capacity == 0 ? 4 : row->capacity * 2;
        row->cells = realloc(row->cells, row->capacity * sizeof(TableCell *));
    }
    row->cells[row->n_cells++] = cell;
}

static void row_free(TableRow *row) {
    if (!row) return;
    for (int i = 0; i < row->n_cells; i++) {
        cell_free(row->cells[i]);
    }
    free(row->cells);
    free(row);
}

static Table *table_new(void) {
    Table *table = malloc(sizeof(Table));
    table->rows = NULL;
    table->n_rows = 0;
    table->capacity = 0;
    table->width = -1;
    table->frame = default_frame();
    table->align = TABLE_ALIGN_LEFT;
    table->valign = TABLE_VALIGN_TOP;
    table->pad = default_padding();
    table->has_width = false;
    table->has_frame = true;
    table->has_align = true;
    table->has_valign = true;
    table->has_pad = true;
    table->border = (FrameSpec){FRAME_UNSET, FRAME_UNSET, FRAME_UNSET, FRAME_UNSET};
    table->has_border = false;
    table->n_cols = 0;
    table->col_props = NULL;
    table->row_frame = (FrameSpec){FRAME_UNSET, FRAME_UNSET, FRAME_UNSET, FRAME_UNSET};
    table->row_align = TABLE_ALIGN_LEFT;
    table->row_valign = TABLE_VALIGN_TOP;
    table->row_pad = default_padding();
    table->has_row_frame = false;
    table->has_row_align = false;
    table->has_row_valign = false;
    table->has_row_pad = false;
    return table;
}

static void table_add_row(Table *table, TableRow *row) {
    if (table->n_rows >= table->capacity) {
        table->capacity = table->capacity == 0 ? 4 : table->capacity * 2;
        table->rows = realloc(table->rows, table->capacity * sizeof(TableRow *));
    }
    table->rows[table->n_rows++] = row;

    // Update max columns
    if (row->n_cells > table->n_cols) {
        table->n_cols = row->n_cells;
    }
}

void table_free(Table *table) {
    if (!table) return;
    for (int i = 0; i < table->n_rows; i++) {
        row_free(table->rows[i]);
    }
    free(table->rows);
    free(table->col_props);
    free(table);
}

// ============================================================================
// Parsing Helpers
// ============================================================================

// Skip whitespace (including newlines)
static void skip_ws(const char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') {
        (*p)++;
    }
}

// Skip whitespace (not newlines)
static void skip_ws_inline(const char **p) {
    while (**p == ' ' || **p == '\t') {
        (*p)++;
    }
}

// Parse an identifier (letters, digits, underscore, hyphen)
static char *parse_identifier(const char **p) {
    skip_ws_inline(p);
    const char *start = *p;

    while (isalnum((unsigned char)**p) || **p == '_' || **p == '-') {
        (*p)++;
    }

    if (*p == start) return NULL;

    size_t len = *p - start;
    char *id = malloc(len + 1);
    memcpy(id, start, len);
    id[len] = '\0';
    return id;
}

// Parse a number
static int parse_number(const char **p, bool *success) {
    skip_ws_inline(p);
    *success = false;

    if (!isdigit((unsigned char)**p)) return 0;

    int value = 0;
    while (isdigit((unsigned char)**p)) {
        value = value * 10 + (**p - '0');
        (*p)++;
    }

    *success = true;
    return value;
}

// Parse a value that may contain calc commands
// Extracts the value string, expands through calc, then parses as integer
static int parse_calc_value(const char **p, CalcContext *calc_ctx, bool *success) {
    skip_ws_inline(p);
    *success = false;

    // First check if it's a simple number
    if (isdigit((unsigned char)**p)) {
        return parse_number(p, success);
    }

    // Check for "auto" keyword
    if (strncmp(*p, "auto", 4) == 0 && !isalnum((unsigned char)(*p)[4])) {
        *p += 4;
        *success = true;
        return WIDTH_INTRINSIC; // -3 = measure natural width
    }

    // Check if it starts with a backslash (calc command)
    if (**p != '\\') return 0;

    // Find the end of the calc expression
    // It could be \valueof<name>, \add{a,b}, etc.
    const char *start = *p;

    // Skip the backslash and command name
    (*p)++;
    while (isalpha((unsigned char)**p)) (*p)++;

    // Handle angle brackets for \valueof<name>, \inc<name>, etc.
    if (**p == '<') {
        (*p)++;
        while (**p && **p != '>') (*p)++;
        if (**p == '>') (*p)++;
    }

    // Handle braces for \add{a,b}, etc.
    if (**p == '{') {
        int depth = 1;
        (*p)++;
        while (**p && depth > 0) {
            if (**p == '{')
                depth++;
            else if (**p == '}')
                depth--;
            (*p)++;
        }
    }

    size_t len = *p - start;
    char *expr = malloc(len + 1);
    memcpy(expr, start, len);
    expr[len] = '\0';

    // Expand through calc if context available
    if (calc_ctx) {
        int end_pos = 0;
        char error_msg[256] = {0};
        char *expanded = calc_try_expand(expr, &end_pos, calc_ctx, error_msg, sizeof(error_msg));
        if (expanded && end_pos > 0) {
            int value = atoi(expanded);
            free(expanded);
            free(expr);
            *success = true;
            return value;
        }
    }

    free(expr);
    return 0;
}

// Parse a frame style name
static FrameStyle parse_frame_style(const char *name) {
    if (!name) return FRAME_SINGLE;
    if (strcmp(name, "none") == 0) return FRAME_NONE;
    if (strcmp(name, "dotted") == 0 || strcmp(name, "dot") == 0) return FRAME_DOTTED;
    if (strcmp(name, "single") == 0) return FRAME_SINGLE;
    if (strcmp(name, "rounded") == 0 || strcmp(name, "round") == 0) return FRAME_ROUNDED;
    if (strcmp(name, "double") == 0) return FRAME_DOUBLE;
    if (strcmp(name, "bold") == 0) return FRAME_BOLD;
    return FRAME_SINGLE;
}

// Parse alignment
static TableAlign parse_align(const char *name) {
    if (!name) return TABLE_ALIGN_LEFT;
    if (strcmp(name, "l") == 0 || strcmp(name, "left") == 0) return TABLE_ALIGN_LEFT;
    if (strcmp(name, "c") == 0 || strcmp(name, "center") == 0) return TABLE_ALIGN_CENTER;
    if (strcmp(name, "r") == 0 || strcmp(name, "right") == 0) return TABLE_ALIGN_RIGHT;
    return TABLE_ALIGN_LEFT;
}

// Parse vertical alignment
static TableVAlign parse_valign(const char *name) {
    if (!name) return TABLE_VALIGN_TOP;
    if (strcmp(name, "t") == 0 || strcmp(name, "top") == 0) return TABLE_VALIGN_TOP;
    if (strcmp(name, "m") == 0 || strcmp(name, "middle") == 0) return TABLE_VALIGN_MIDDLE;
    if (strcmp(name, "b") == 0 || strcmp(name, "bottom") == 0) return TABLE_VALIGN_BOTTOM;
    return TABLE_VALIGN_TOP;
}

// Parse a compound value: {key:val, key:val, ...}
// Used for frame:{t:double, b:single} and pad:{t:1, b:2}
static bool parse_compound(const char **p,
                           void (*handler)(const char *key, const char *val, int num_val,
                                           void *ctx),
                           void *ctx) {
    skip_ws_inline(p);
    if (**p != '{') return false;
    (*p)++; // Skip '{'

    while (**p && **p != '}') {
        skip_ws(p);
        if (**p == '}') break;

        // Parse key
        char *key = parse_identifier(p);
        if (!key) {
            // Skip to next comma or closing brace
            while (**p && **p != ',' && **p != '}') (*p)++;
            if (**p == ',') (*p)++;
            continue;
        }

        skip_ws_inline(p);
        if (**p != ':') {
            free(key);
            while (**p && **p != ',' && **p != '}') (*p)++;
            if (**p == ',') (*p)++;
            continue;
        }
        (*p)++; // Skip ':'

        skip_ws_inline(p);

        // Parse value - could be identifier or number
        bool is_num;
        int num_val = parse_number(p, &is_num);
        char *val = NULL;

        if (!is_num) {
            val = parse_identifier(p);
        }

        // Call handler
        handler(key, val, is_num ? num_val : -1, ctx);

        free(key);
        free(val);

        skip_ws(p);
        if (**p == ',') (*p)++;
    }

    if (**p == '}') (*p)++;
    return true;
}

// Handler for frame compound values
typedef struct {
    FrameSpec *frame;
} FrameParseCtx;

static void frame_handler(const char *key, const char *val, int num_val, void *ctx) {
    (void)num_val; // Unused
    FrameParseCtx *fctx = ctx;
    FrameStyle style = parse_frame_style(val);

    if (strcmp(key, "t") == 0 || strcmp(key, "top") == 0) {
        fctx->frame->top = style;
    } else if (strcmp(key, "b") == 0 || strcmp(key, "bottom") == 0) {
        fctx->frame->bottom = style;
    } else if (strcmp(key, "l") == 0 || strcmp(key, "left") == 0) {
        fctx->frame->left = style;
    } else if (strcmp(key, "r") == 0 || strcmp(key, "right") == 0) {
        fctx->frame->right = style;
    }
}

// Handler for padding compound values
typedef struct {
    Padding *pad;
} PadParseCtx;

static void pad_handler(const char *key, const char *val, int num_val, void *ctx) {
    (void)val; // Unused
    PadParseCtx *pctx = ctx;
    int value = num_val >= 0 ? num_val : 0;

    if (strcmp(key, "t") == 0 || strcmp(key, "top") == 0) {
        pctx->pad->top = value;
    } else if (strcmp(key, "b") == 0 || strcmp(key, "bottom") == 0) {
        pctx->pad->bottom = value;
    } else if (strcmp(key, "l") == 0 || strcmp(key, "left") == 0) {
        pctx->pad->left = value;
    } else if (strcmp(key, "r") == 0 || strcmp(key, "right") == 0) {
        pctx->pad->right = value;
    }
}

// Parse a single option: key:value or key:{compound}
static bool parse_option(const char **p, const char *key, CalcContext *calc_ctx, int *width,
                         TableAlign *align, TableVAlign *valign, FrameSpec *frame, Padding *pad,
                         int *height, bool *has_width, bool *has_align, bool *has_valign,
                         bool *has_frame, bool *has_pad, bool *has_height, bool *is_reset,
                         bool *has_span, int *span, FrameSpec *border, bool *has_border) {

    if (strcmp(key, "width") == 0 || strcmp(key, "w") == 0) {
        skip_ws_inline(p);
        if (**p == ':') {
            (*p)++;
            bool success;
            *width = parse_calc_value(p, calc_ctx, &success);
            if (success && has_width) *has_width = true;
            return success;
        }
    } else if (strcmp(key, "height") == 0 || strcmp(key, "h") == 0) {
        skip_ws_inline(p);
        if (**p == ':') {
            (*p)++;
            bool success;
            *height = parse_calc_value(p, calc_ctx, &success);
            if (success && has_height) *has_height = true;
            return success;
        }
    } else if (strcmp(key, "align") == 0 || strcmp(key, "a") == 0) {
        skip_ws_inline(p);
        if (**p == ':') {
            (*p)++;
            char *val = parse_identifier(p);
            if (val) {
                *align = parse_align(val);
                if (has_align) *has_align = true;
                free(val);
                return true;
            }
        }
    } else if (strcmp(key, "valign") == 0 || strcmp(key, "va") == 0) {
        skip_ws_inline(p);
        if (**p == ':') {
            (*p)++;
            char *val = parse_identifier(p);
            if (val) {
                *valign = parse_valign(val);
                if (has_valign) *has_valign = true;
                free(val);
                return true;
            }
        }
    } else if (strcmp(key, "frame") == 0 || strcmp(key, "f") == 0) {
        skip_ws_inline(p);
        if (**p == ':') {
            (*p)++;
            skip_ws_inline(p);

            if (**p == '{') {
                // Compound: frame:{t:double, b:single}
                // Initialize to UNSET so only specified edges get values
                frame->top = frame->bottom = frame->left = frame->right = FRAME_UNSET;
                FrameParseCtx ctx = {.frame = frame};
                if (parse_compound(p, frame_handler, &ctx)) {
                    if (has_frame) *has_frame = true;
                    return true;
                }
            } else {
                // Simple: frame:double (applies to all edges)
                char *val = parse_identifier(p);
                if (val) {
                    FrameStyle style = parse_frame_style(val);
                    frame->top = frame->bottom = frame->left = frame->right = style;
                    if (has_frame) *has_frame = true;
                    free(val);
                    return true;
                }
            }
        }
    } else if (strcmp(key, "pad") == 0 || strcmp(key, "p") == 0) {
        skip_ws_inline(p);
        if (**p == ':') {
            (*p)++;
            skip_ws_inline(p);

            if (**p == '{') {
                // Compound: pad:{t:1, b:2}
                PadParseCtx ctx = {.pad = pad};
                if (parse_compound(p, pad_handler, &ctx)) {
                    if (has_pad) *has_pad = true;
                    return true;
                }
            } else {
                // Simple: pad:1 (applies to all edges)
                bool success;
                int value = parse_calc_value(p, calc_ctx, &success);
                if (success) {
                    pad->top = pad->bottom = pad->left = pad->right = value;
                    if (has_pad) *has_pad = true;
                    return true;
                }
            }
        }
    } else if (strcmp(key, "reset") == 0) {
        // Flag option, no value
        if (is_reset) *is_reset = true;
        return true;
    } else if (strcmp(key, "span") == 0) {
        skip_ws_inline(p);
        if (**p == ':') {
            (*p)++;
            bool success;
            *span = parse_calc_value(p, calc_ctx, &success);
            if (success && has_span) *has_span = true;
            return success;
        }
    } else if (strcmp(key, "border") == 0) {
        if (!border) return false; // Only valid on \table
        skip_ws_inline(p);
        if (**p == ':') {
            (*p)++;
            skip_ws_inline(p);

            if (**p == '{') {
                // Compound: border:{t:double, b:single}
                border->top = border->bottom = border->left = border->right = FRAME_UNSET;
                FrameParseCtx ctx = {.frame = border};
                if (parse_compound(p, frame_handler, &ctx)) {
                    if (has_border) *has_border = true;
                    return true;
                }
            } else {
                // Simple: border:double (applies to all edges)
                char *val = parse_identifier(p);
                if (val) {
                    FrameStyle style = parse_frame_style(val);
                    border->top = border->bottom = border->left = border->right = style;
                    if (has_border) *has_border = true;
                    free(val);
                    return true;
                }
            }
        }
    }

    return false;
}

// Parse options in [...] brackets
// Can have multiple: [width:20][align:c][frame:double]
static void parse_options(const char **p, CalcContext *calc_ctx, int *width, TableAlign *align,
                          TableVAlign *valign, FrameSpec *frame, Padding *pad, int *height,
                          bool *has_width, bool *has_align, bool *has_valign, bool *has_frame,
                          bool *has_pad, bool *has_height, bool *is_reset, bool *has_span,
                          int *span, FrameSpec *border, bool *has_border) {

    while (1) {
        skip_ws_inline(p);
        if (**p != '[') break;
        (*p)++; // Skip '['

        // Parse options inside this bracket
        while (**p && **p != ']') {
            skip_ws(p);
            if (**p == ']') break;

            char *key = parse_identifier(p);
            if (!key) {
                // Skip to comma or closing bracket
                while (**p && **p != ',' && **p != ']') (*p)++;
                if (**p == ',') (*p)++;
                continue;
            }

            parse_option(p, key, calc_ctx, width, align, valign, frame, pad, height, has_width,
                         has_align, has_valign, has_frame, has_pad, has_height, is_reset, has_span,
                         span, border, has_border);

            free(key);

            skip_ws(p);
            if (**p == ',') (*p)++;
        }

        if (**p == ']') (*p)++;
    }
}

// Parse braced content {...}
// Handles nested braces correctly
static char *parse_braced_content(const char **p) {
    skip_ws(p);
    if (**p != '{') return NULL;
    (*p)++; // Skip '{'

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

// ============================================================================
// Table Parsing
// ============================================================================

// Parse a \col[options]{content}
static TableCell *parse_col(const char **p, CalcContext *calc_ctx, char *error_msg,
                            int error_size) {
    skip_ws(p);

    if (strncmp(*p, "\\col", 4) != 0) {
        snprintf(error_msg, error_size, "Expected \\col");
        return NULL;
    }
    *p += 4;

    TableCell *cell = cell_new();

    // Parse options
    int span = 1;
    bool has_span = false;
    bool is_reset = false;

    parse_options(p, calc_ctx, &cell->props.width, &cell->props.align, &cell->props.valign,
                  &cell->props.frame, &cell->props.pad, NULL, &cell->props.has_width,
                  &cell->props.has_align, &cell->props.has_valign, &cell->props.has_frame,
                  &cell->props.has_pad, NULL, &is_reset, &has_span, &span, NULL, NULL);

    if (has_span) cell->col_span = span;

    // Handle reset flag - clear all "has_" flags to trigger reset during expansion
    if (is_reset) {
        cell->props = default_col_props();
        // Mark as explicit reset
        cell->props.has_width = true;
        cell->props.has_align = true;
        cell->props.has_valign = true;
        cell->props.has_frame = true;
        cell->props.has_pad = true;
    }

    // Parse content
    cell->content = parse_braced_content(p);
    if (!cell->content) {
        snprintf(error_msg, error_size, "Expected {content} after \\col");
        cell_free(cell);
        return NULL;
    }

    return cell;
}

// Parse a \row[options]{ \col{...} \col{...} ... }
static TableRow *parse_row(const char **p, CalcContext *calc_ctx, char *error_msg, int error_size) {
    skip_ws(p);

    if (strncmp(*p, "\\row", 4) != 0) {
        snprintf(error_msg, error_size, "Expected \\row");
        return NULL;
    }
    *p += 4;

    TableRow *row = row_new();

    // Parse options
    parse_options(p, calc_ctx, NULL, &row->align, &row->valign, &row->frame, &row->pad,
                  &row->height, NULL, &row->has_align, &row->has_valign, &row->has_frame,
                  &row->has_pad, &row->has_height, NULL, NULL, NULL, NULL, NULL);

    // Parse row content: { ... }
    skip_ws(p);
    if (**p != '{') {
        snprintf(error_msg, error_size, "Expected { after \\row options");
        row_free(row);
        return NULL;
    }
    (*p)++; // Skip '{'

    // Parse cells until closing }
    int depth = 1;
    while (**p && depth > 0) {
        skip_ws(p);

        if (**p == '{') {
            depth++;
            (*p)++;
        } else if (**p == '}') {
            depth--;
            if (depth > 0) (*p)++;
        } else if (strncmp(*p, "\\col", 4) == 0) {
            TableCell *cell = parse_col(p, calc_ctx, error_msg, error_size);
            if (!cell) {
                row_free(row);
                return NULL;
            }
            row_add_cell(row, cell);
        } else {
            // Skip unknown content
            (*p)++;
        }
    }

    if (**p == '}') (*p)++;

    return row;
}

// Parse \table[options]{ \row{...} \row{...} ... }
Table *table_parse(const char *input, int *end_pos, CalcContext *calc_ctx, char *error_msg,
                   int error_size) {
    const char *p = input;

    skip_ws(&p);

    if (strncmp(p, "\\table", 6) != 0) {
        snprintf(error_msg, error_size, "Expected \\table");
        return NULL;
    }
    p += 6;

    Table *table = table_new();

    // Parse options
    int dummy_height = 0;
    parse_options(&p, calc_ctx, &table->width, &table->align, &table->valign, &table->frame,
                  &table->pad, &dummy_height, &table->has_width, &table->has_align,
                  &table->has_valign, &table->has_frame, &table->has_pad, NULL, NULL, NULL, NULL,
                  &table->border, &table->has_border);

    // Parse table content: { ... }
    skip_ws(&p);
    if (*p != '{') {
        snprintf(error_msg, error_size, "Expected { after \\table options");
        table_free(table);
        return NULL;
    }
    p++; // Skip '{'

    // Parse rows until closing }
    int depth = 1;
    while (*p && depth > 0) {
        skip_ws(&p);

        if (*p == '{') {
            depth++;
            p++;
        } else if (*p == '}') {
            depth--;
            if (depth > 0) p++;
        } else if (strncmp(p, "\\row", 4) == 0) {
            TableRow *row = parse_row(&p, calc_ctx, error_msg, error_size);
            if (!row) {
                table_free(table);
                return NULL;
            }
            table_add_row(table, row);
        } else {
            // Skip unknown content
            p++;
        }
    }

    if (*p == '}') p++;

    if (end_pos) *end_pos = (int)(p - input);

    // Initialize column properties array
    table->col_props = malloc(table->n_cols * sizeof(ColProps));
    for (int i = 0; i < table->n_cols; i++) {
        table->col_props[i] = default_col_props();
        // Frame starts UNSET so column inheritance only carries explicitly set edges
        table->col_props[i].frame = (FrameSpec){FRAME_UNSET, FRAME_UNSET, FRAME_UNSET, FRAME_UNSET};
    }

    return table;
}

bool is_table_macro(const char *input) {
    if (!input) return false;
    while (*input == ' ' || *input == '\t' || *input == '\n') input++;
    return strncmp(input, "\\table", 6) == 0;
}

// ============================================================================
// Table Expansion
// ============================================================================

// Dynamic string buffer for building output
typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} StringBuf;

static void strbuf_init(StringBuf *sb) {
    sb->capacity = 256;
    sb->data = malloc(sb->capacity);
    sb->data[0] = '\0';
    sb->len = 0;
}

static void strbuf_free(StringBuf *sb) {
    free(sb->data);
}

static void strbuf_append(StringBuf *sb, const char *str) {
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

static void strbuf_appendf(StringBuf *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Calculate needed size
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    // Ensure capacity
    if (sb->len + needed + 1 > sb->capacity) {
        while (sb->len + needed + 1 > sb->capacity) {
            sb->capacity *= 2;
        }
        sb->data = realloc(sb->data, sb->capacity);
    }

    // Append
    vsnprintf(sb->data + sb->len, needed + 1, fmt, args);
    sb->len += needed;

    va_end(args);
}

// Get alignment string for box layout
static const char *align_to_string(TableAlign align) {
    switch (align) {
    case TABLE_ALIGN_LEFT: return "left";
    case TABLE_ALIGN_CENTER: return "center";
    case TABLE_ALIGN_RIGHT: return "right";
    default: return "left";
    }
}

// Resolve effective frame style between adjacent cells
// Higher strength wins: double > bold > single > none.
// FRAME_UNSET is treated as transparent (use the other side's value).
static FrameStyle resolve_frame(FrameStyle a, FrameStyle b) {
    if (a == FRAME_UNSET) return b;
    if (b == FRAME_UNSET) return a;
    int strength_a = (a == FRAME_DOUBLE)                         ? 4
                     : (a == FRAME_BOLD)                         ? 3
                     : (a == FRAME_SINGLE || a == FRAME_ROUNDED) ? 2
                     : (a == FRAME_DOTTED)                       ? 1
                                                                 : 0;
    int strength_b = (b == FRAME_DOUBLE)                         ? 4
                     : (b == FRAME_BOLD)                         ? 3
                     : (b == FRAME_SINGLE || b == FRAME_ROUNDED) ? 2
                     : (b == FRAME_DOTTED)                       ? 1
                                                                 : 0;
    return (strength_a >= strength_b) ? a : b;
}

// Merge frame overlay onto base, skipping FRAME_UNSET edges
static void frame_merge(FrameSpec *base, const FrameSpec *overlay) {
    if (overlay->top != FRAME_UNSET) base->top = overlay->top;
    if (overlay->bottom != FRAME_UNSET) base->bottom = overlay->bottom;
    if (overlay->left != FRAME_UNSET) base->left = overlay->left;
    if (overlay->right != FRAME_UNSET) base->right = overlay->right;
}

// Compute effective properties for a cell, considering inheritance
static ColProps compute_cell_props(const Table *table, const TableRow *row, const TableCell *cell,
                                   int col_idx) {
    ColProps result = default_col_props();

    // Layer 1: Table defaults
    if (table->has_frame) frame_merge(&result.frame, &table->frame);
    if (table->has_pad) result.pad = table->pad;
    if (table->has_align) result.align = table->align;
    if (table->has_valign) result.valign = table->valign;

    // Layer 2: Column inheritance (from previous rows)
    if (col_idx < table->n_cols) {
        ColProps *col = &table->col_props[col_idx];
        if (col->has_width) result.width = col->width;
        if (col->has_align) result.align = col->align;
        if (col->has_valign) result.valign = col->valign;
        if (col->has_frame) frame_merge(&result.frame, &col->frame);
        if (col->has_pad) result.pad = col->pad;
    }

    // Layer 3a: Row defaults (inherited from previous rows)
    if (table->has_row_align) result.align = table->row_align;
    if (table->has_row_valign) result.valign = table->row_valign;
    if (table->has_row_pad) result.pad = table->row_pad;
    if (table->has_row_frame) frame_merge(&result.frame, &table->row_frame);

    // Layer 3b: Row explicit properties (override inherited)
    if (row->has_align) result.align = row->align;
    if (row->has_valign) result.valign = row->valign;
    if (row->has_pad) result.pad = row->pad;
    if (row->has_frame) frame_merge(&result.frame, &row->frame);

    // Layer 4: Cell properties (highest priority)
    if (cell->props.has_width) result.width = cell->props.width;
    if (cell->props.has_align) result.align = cell->props.align;
    if (cell->props.has_valign) result.valign = cell->props.valign;
    if (cell->props.has_frame) frame_merge(&result.frame, &cell->props.frame);
    if (cell->props.has_pad) result.pad = cell->props.pad;

    return result;
}

// Update column inheritance from this cell
static void update_col_inheritance(Table *table, const TableCell *cell, int col_idx) {
    if (col_idx >= table->n_cols) return;

    ColProps *col = &table->col_props[col_idx];

    if (cell->props.has_width) {
        col->width = cell->props.width;
        col->has_width = true;
    }
    if (cell->props.has_align) {
        col->align = cell->props.align;
        col->has_align = true;
    }
    if (cell->props.has_valign) {
        col->valign = cell->props.valign;
        col->has_valign = true;
    }
    if (cell->props.has_frame) {
        frame_merge(&col->frame, &cell->props.frame);
        col->has_frame = true;
    }
    if (cell->props.has_pad) {
        col->pad = cell->props.pad;
        col->has_pad = true;
    }
}

// Update row inheritance defaults from this row's explicit properties
static void update_row_inheritance(Table *table, const TableRow *row) {
    if (row->has_align) {
        table->row_align = row->align;
        table->has_row_align = true;
    }
    if (row->has_valign) {
        table->row_valign = row->valign;
        table->has_row_valign = true;
    }
    if (row->has_frame) {
        frame_merge(&table->row_frame, &row->frame);
        table->has_row_frame = true;
    }
    if (row->has_pad) {
        table->row_pad = row->pad;
        table->has_row_pad = true;
    }
}

// Resolve a table-level outer edge. Border takes priority over frame.
static FrameStyle table_edge(const Table *table, FrameStyle frame_edge, FrameStyle border_edge,
                             FrameStyle dflt) {
    if (table->has_border && border_edge != FRAME_UNSET) return border_edge;
    if (frame_edge != FRAME_UNSET) return frame_edge;
    return dflt;
}

// Get hrule style for a column between two adjacent rows.
// above=NULL means top table edge, below=NULL means bottom table edge.
static FrameStyle col_hrule_style(const Table *table, const TableRow *above, const TableRow *below,
                                  int col_idx) {
    FrameStyle bottom_edge, top_edge;

    if (!above) {
        // Top table edge: table's frame is sole authority for outer border
        return table_edge(table, table->frame.top, table->border.top, FRAME_SINGLE);
    }
    if (!below) {
        // Bottom table edge: table's frame is sole authority for outer border
        return table_edge(table, table->frame.bottom, table->border.bottom, FRAME_SINGLE);
    }

    // Between rows: resolve above cell's bottom with below cell's top
    if (col_idx < above->n_cells) {
        ColProps p = compute_cell_props(table, above, above->cells[col_idx], col_idx);
        bottom_edge = p.frame.bottom;
    } else {
        bottom_edge = table_edge(table, table->frame.bottom, table->border.bottom, FRAME_NONE);
    }

    if (col_idx < below->n_cells) {
        ColProps p = compute_cell_props(table, below, below->cells[col_idx], col_idx);
        top_edge = p.frame.top;
    } else {
        top_edge = table_edge(table, table->frame.top, table->border.top, FRAME_NONE);
    }

    return resolve_frame(bottom_edge, top_edge);
}

// Emit an hrule between two rows (or at table edge).
// ref_row is the row used for column widths (non-NULL).
static void emit_hrule_row(StringBuf *sb, const Table *table, const TableRow *above,
                           const TableRow *below, const TableRow *ref_row, const char *start_cap,
                           const char *end_cap) {
    int n_cols = table->n_cols;
    if (n_cols == 0) return;

    // Compute per-column hrule styles
    FrameStyle styles[n_cols];
    bool any = false;
    bool all_same = true;
    for (int i = 0; i < n_cols; i++) {
        styles[i] = col_hrule_style(table, above, below, i);
        if (styles[i] != FRAME_NONE) any = true;
        if (i > 0 && styles[i] != styles[0]) all_same = false;
    }

    if (!any) return;

    // If all columns have the same style, emit a single hrule (clean, simple)
    if (all_same) {
        strbuf_appendf(sb, "  \\child{\\hrule[auto]{%s}{%s}{%s}}\n", start_cap ? start_cap : "",
                       frame_hline_char(styles[0]), end_cap ? end_cap : "");
        return;
    }

    // Per-column hrule: emit hbox mirroring cell row structure
    strbuf_append(sb, "  \\child{\\begin[top]{hbox}\n");
    for (int col_idx = 0; col_idx < n_cols; col_idx++) {
        // Left vrule placeholder — mirrors the vrule in cell rows
        bool is_first = (col_idx == 0);
        bool has_left_vrule = false;

        if (is_first) {
            has_left_vrule = table_edge(table, table->frame.left, table->border.left,
                                        FRAME_SINGLE) != FRAME_NONE;
        } else {
            // Check if a vrule exists between prev and current column
            // Use ref_row cell props, same logic as cell row vrule emission
            ColProps curr =
                (col_idx < ref_row->n_cells)
                    ? compute_cell_props(table, ref_row, ref_row->cells[col_idx], col_idx)
                    : default_col_props();
            ColProps prev =
                (col_idx - 1 < ref_row->n_cells)
                    ? compute_cell_props(table, ref_row, ref_row->cells[col_idx - 1], col_idx - 1)
                    : default_col_props();
            FrameStyle vrule = resolve_frame(prev.frame.right, curr.frame.left);
            has_left_vrule = (vrule != FRAME_NONE);
        }

        if (has_left_vrule) {
            // 1-wide placeholder; junction fixup will replace with intersection char
            if (is_first && start_cap) {
                strbuf_appendf(sb, "    \\child[1]{%s}\n", start_cap);
            } else {
                strbuf_append(sb, "    \\child[1]{}\n");
            }
        }

        // Hrule segment for this column
        ColProps cprops = (col_idx < ref_row->n_cells)
                              ? compute_cell_props(table, ref_row, ref_row->cells[col_idx], col_idx)
                              : default_col_props();
        if (styles[col_idx] != FRAME_NONE) {
            if (cprops.width > 0) {
                strbuf_appendf(sb, "    \\child[%d]{\\hrule[auto]{}{%s}{}}\n", cprops.width,
                               frame_hline_char(styles[col_idx]));
            } else {
                strbuf_appendf(sb, "    \\child{\\hrule[auto]{}{%s}{}}\n",
                               frame_hline_char(styles[col_idx]));
            }
        } else {
            // No hrule for this column — emit empty spacer with same width
            if (cprops.width > 0) {
                strbuf_appendf(sb, "    \\child[%d]{}\n", cprops.width);
            } else {
                strbuf_append(sb, "    \\child{}\n");
            }
        }

        // Right vrule placeholder for last column
        if (col_idx == n_cols - 1) {
            bool has_right = table_edge(table, table->frame.right, table->border.right,
                                        FRAME_SINGLE) != FRAME_NONE;
            if (has_right) {
                if (end_cap) {
                    strbuf_appendf(sb, "    \\child[1]{%s}\n", end_cap);
                } else {
                    strbuf_append(sb, "    \\child[1]{}\n");
                }
            }
        }
    }
    strbuf_append(sb, "  \\end{hbox}}\n");
}

// Check if the resolved border style is FRAME_ROUNDED for a given edge
static bool border_is_rounded(const Table *table, FrameStyle frame_edge, FrameStyle border_edge) {
    if (table->has_border && border_edge != FRAME_UNSET) return border_edge == FRAME_ROUNDED;
    return frame_edge == FRAME_ROUNDED;
}

char *table_expand(const Table *table, char *error_msg, int error_size) {
    if (!table || table->n_rows == 0) {
        snprintf(error_msg, error_size, "Empty table");
        return NULL;
    }

    StringBuf sb;
    strbuf_init(&sb);

    // Determine table width
    // WIDTH_INTRINSIC (-3): measure natural content width
    // -1: inherit from parent
    // >0: explicit width
    int table_width = table->width;

    if (diag_is_enabled(DIAG_SYSTEM)) {
        diag_log(DIAG_SYSTEM, 0, "table_expand: width=%d %s, %d rows, %d cols", table_width,
                 table_width == WIDTH_INTRINSIC ? "(intrinsic)"
                 : table_width > 0              ? "(fixed)"
                                                : "(inherit)",
                 table->n_rows, table->n_cols);
    }

    // Start with intersect_rules wrapper
    strbuf_append(&sb, "\\intersect_rules{\n");

    // Begin vbox
    if (table_width > 0) {
        strbuf_appendf(&sb, "\\begin[%d]{vbox}\n", table_width);
    } else if (table_width == WIDTH_INTRINSIC) {
        // Use intrinsic width - the layout system will measure content
        strbuf_append(&sb, "\\begin[intrinsic]{vbox}\n");
    } else {
        // Inherit width (-1 or other negative values)
        strbuf_append(&sb, "\\begin{vbox}\n");
    }

    // Process each row
    for (int row_idx = 0; row_idx < table->n_rows; row_idx++) {
        TableRow *row = table->rows[row_idx];
        bool is_first_row = (row_idx == 0);
        bool is_last_row = (row_idx == table->n_rows - 1);

        // Emit hrule above this row
        if (is_first_row) {
            // Top table edge: check for rounded corners
            bool top_rounded = border_is_rounded(table, table->frame.top, table->border.top);
            bool left_rounded = border_is_rounded(table, table->frame.left, table->border.left);
            bool right_rounded = border_is_rounded(table, table->frame.right, table->border.right);
            emit_hrule_row(&sb, table, NULL, row, row, (top_rounded && left_rounded) ? "╭" : NULL,
                           (top_rounded && right_rounded) ? "╮" : NULL);
        } else {
            TableRow *prev_row = table->rows[row_idx - 1];
            emit_hrule_row(&sb, table, prev_row, row, prev_row, NULL, NULL);
        }

        // Begin hbox for this row with top alignment (no baseline alignment)
        strbuf_append(&sb, "  \\child{\\begin[top]{hbox}\n");

        // Process each cell in the row
        for (int col_idx = 0; col_idx < row->n_cells; col_idx++) {
            TableCell *cell = row->cells[col_idx];
            bool is_first_col = (col_idx == 0);
            bool is_last_col = (col_idx == row->n_cells - 1);

            // Compute effective properties
            ColProps props = compute_cell_props(table, row, cell, col_idx);

            // Update column inheritance for future rows
            update_col_inheritance((Table *)table, cell, col_idx);

            // Determine left vrule style
            FrameStyle left_vrule = FRAME_NONE;
            if (is_first_col) {
                // First column: use table's left frame
                left_vrule = table_edge(table, table->frame.left, table->border.left, FRAME_SINGLE);
            } else {
                // Between columns: resolve previous cell's right with this cell's left
                TableCell *prev_cell = row->cells[col_idx - 1];
                ColProps prev_props = compute_cell_props(table, row, prev_cell, col_idx - 1);
                left_vrule = resolve_frame(prev_props.frame.right, props.frame.left);
            }

            // Emit left vrule
            if (left_vrule != FRAME_NONE) {
                strbuf_appendf(&sb, "    \\child[1]{\\vrule[auto]{}{%s}{}}\n",
                               frame_vline_char(left_vrule));
            }

            // Emit cell content
            strbuf_append(&sb, "    \\child");

            // Width
            if (props.width > 0) {
                strbuf_appendf(&sb, "[%d]", props.width);
            }

            // Alignment
            strbuf_appendf(&sb, "[%s]", align_to_string(props.align));

            // Vertical alignment (only emit when non-default)
            if (props.valign == TABLE_VALIGN_MIDDLE) {
                strbuf_append(&sb, "[middle]");
            } else if (props.valign == TABLE_VALIGN_BOTTOM) {
                strbuf_append(&sb, "[bottom]");
            }

            // Content with padding
            strbuf_append(&sb, "{");

            // Add top padding using \vskip (bypasses text processing)
            if (props.pad.top > 0) {
                strbuf_appendf(&sb, "\\vskip{%d}", props.pad.top);
            }

            // Add horizontal padding using hbox with spacer children
            // This ensures padding applies to ALL lines, not just the first
            if (props.pad.left > 0 || props.pad.right > 0) {
                strbuf_append(&sb, "\\begin{hbox}");
                if (props.pad.left > 0) {
                    strbuf_appendf(&sb, "\\child[%d]{}", props.pad.left);
                }
                strbuf_appendf(&sb, "\\child[%s]{", align_to_string(props.align));
                if (cell->content) strbuf_append(&sb, cell->content);
                strbuf_append(&sb, "}");
                if (props.pad.right > 0) {
                    strbuf_appendf(&sb, "\\child[%d]{}", props.pad.right);
                }
                strbuf_append(&sb, "\\end{hbox}");
            } else {
                if (cell->content) strbuf_append(&sb, cell->content);
            }

            // Add bottom padding using \vskip (bypasses text processing)
            if (props.pad.bottom > 0) {
                strbuf_appendf(&sb, "\\vskip{%d}", props.pad.bottom);
            }

            strbuf_append(&sb, "}\n");

            // Emit right vrule for last column
            if (is_last_col) {
                // Use table's right frame for outer border (consistent with left border logic)
                FrameStyle right_vrule =
                    table_edge(table, table->frame.right, table->border.right, FRAME_SINGLE);
                if (right_vrule != FRAME_NONE) {
                    strbuf_appendf(&sb, "    \\child[1]{\\vrule[auto]{}{%s}{}}\n",
                                   frame_vline_char(right_vrule));
                }
            }
        }

        // End hbox
        strbuf_append(&sb, "  \\end{hbox}}\n");

        // Update row-level inheritance for future rows
        update_row_inheritance((Table *)table, row);

        // Emit bottom hrule for last row
        if (is_last_row) {
            bool bot_rounded = border_is_rounded(table, table->frame.bottom, table->border.bottom);
            bool left_rounded = border_is_rounded(table, table->frame.left, table->border.left);
            bool right_rounded = border_is_rounded(table, table->frame.right, table->border.right);
            emit_hrule_row(&sb, table, row, NULL, row, (bot_rounded && left_rounded) ? "╰" : NULL,
                           (bot_rounded && right_rounded) ? "╯" : NULL);
        }
    }

    // End vbox
    strbuf_append(&sb, "\\end{vbox}\n");

    // End intersect_rules
    strbuf_append(&sb, "}\n");

    // Return the result (caller must free)
    char *result = sb.data; // Transfer ownership
    return result;
}

char *table_macro_expand(const char *input, int *end_pos, CalcContext *calc_ctx, char *error_msg,
                         int error_size) {
    Table *table = table_parse(input, end_pos, calc_ctx, error_msg, error_size);
    if (!table) return NULL;

    char *result = table_expand(table, error_msg, error_size);
    table_free(table);

    return result;
}
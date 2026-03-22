// parser_constructs.c - Sqrt, matrices, and other constructs
// Verbatim from original parser.c

#include "parser_internal.h"

Ast *parse_sqrt(Parser *p) {
    // at \sqrt
    next(p); // consume \sqrt

    Ast *index = NULL;
    // optional [ ... ] (or \[ ... \])
    if (peek_ch(p, '[') || is_cmd(&p->look, "\\[")) {
        // consume open
        next(p);
        if (p->in_matrix) p->matrix_depth++;
        index = parse_expr(p);
        if (!index) return NULL;
        // accept ] or \]
        if (!(peek_ch(p, ']') || is_cmd(&p->look, "\\]"))) {
            return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0,
                           "expected ']' or \\] to close sqrt index");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth--;
    }

    // radicand: group or one token
    Ast *rad = NULL;
    if (peek_ch(p, '{'))
        rad = parse_group(p);
    else
        rad = parse_primary(p);
    if (!rad) return NULL;

    return ast_sqrt(index, rad);
}

Ast *empty_cell_ast(void) {
    return ast_symbol(" ");
}

Ast *parse_matrix_core(Parser *p, int *out_rows, int *out_cols) {
    // we are at '{'
    next(p); // consume '{'

    int saved_in_matrix = p->in_matrix;
    p->in_matrix = 1;

    // Simple stretchy array of cells
    int cap = 16, ncell = 0;
    Ast **cells = (Ast **)malloc((size_t)cap * sizeof(Ast *));

    int rows = 0;
    int cols_in_first = -1;
    int cols_this_row = 0;

#define PUSH_CELL(A)                                                                               \
    do {                                                                                           \
        if (ncell >= cap) {                                                                        \
            cap *= 2;                                                                              \
            cells = (Ast **)realloc(cells, (size_t)cap * sizeof(Ast *));                           \
        }                                                                                          \
        cells[ncell++] = (A);                                                                      \
        cols_this_row++;                                                                           \
    } while (0)

#define END_ROW()                                                                                  \
    do {                                                                                           \
        if (cols_in_first < 0)                                                                     \
            cols_in_first = cols_this_row;                                                         \
        else if (cols_this_row != cols_in_first) {                                                 \
            char buf[128];                                                                         \
            sprintf(buf, "matrix rows must have the same number of columns (got %d, expected %d)", \
                    cols_this_row, cols_in_first);                                                 \
            return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, buf);                                   \
        }                                                                                          \
        rows++;                                                                                    \
        cols_this_row = 0;                                                                         \
    } while (0)

    for (;;) {
        // End of entire matrix?
        if (peek_ch(p, '}')) {
            if (cols_this_row > 0) END_ROW();
            break;
        }

        // Parse one row.
        // 'expecting_cell' means we're at the start of the next cell since the last separator (or row start)
        int expecting_cell = 1;

        for (;;) {
            // Row end?
            if (tok_is_rowbreak(&p->look) || (p->look.kind == TOK_SYM && p->look.text[0] == ';') ||
                peek_ch(p, '}')) {
                // If the row ends while we're expecting a cell (i.e., just after a separator or at row start),
                // that trailing/leading cell is empty.
                if (expecting_cell) PUSH_CELL(empty_cell_ast());
                if (tok_is_rowbreak(&p->look) ||
                    (p->look.kind == TOK_SYM && p->look.text[0] == ';'))
                    next(p); // consume rowbreak
                END_ROW();
                // If '}' we will finish at top of outer loop
                break;
            }

            // Column separator?
            if (p->look.kind == TOK_SYM && p->look.text[0] == '&') {
                // Separator without content before it => emit an empty cell.
                if (expecting_cell) PUSH_CELL(empty_cell_ast());
                next(p);            // consume '&'
                expecting_cell = 1; // new cell starts now
                continue;
            }

            // Otherwise: parse a real cell expression (parse_expr must NOT consume '&' or rowbreak)
            {
                Ast *entry = parse_expr(p);
                if (!entry) return NULL;
                PUSH_CELL(entry);
                expecting_cell = 0; // we just produced a cell; next must be '&' or row end
            }

            // Loop continues; the top checks for '&', rowbreak, or '}'
        }
    }

    if (!peek_ch(p, '}')) {
        return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected '}' to close matrix");
    }
    next(p); // consume '}'

    p->in_matrix = saved_in_matrix; // <— restore

    // Empty matrix -> 1×1 space
    if (rows == 0) {
        rows = 1;
        cols_in_first = 1;
    }

    *out_rows = rows;
    *out_cols = cols_in_first;

    // Pad if needed (shouldn’t be needed, but be safe)
    const int need = rows * cols_in_first;
    while (ncell < need) {
        if (ncell >= cap) {
            cap *= 2;
            cells = (Ast **)realloc(cells, (size_t)cap * sizeof(Ast *));
        }
        cells[ncell++] = empty_cell_ast();
    }

#undef PUSH_CELL
#undef END_ROW
    return ast_matrix(rows, cols_in_first, cells, 1); // padding=1 for regular matrices
}

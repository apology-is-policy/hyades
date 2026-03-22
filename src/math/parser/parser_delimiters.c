// parser_delimiters.c - Parentheses and delimiter parsing
// Verbatim from original parser.c

#include "parser_internal.h"

int size_from_height(int h, int baseline) {
    // If nothing extends below baseline and total height is <= 2,
    // the extra row is just an accent — use size 1
    int below = h - 1 - baseline;
    if (below <= 0 && h <= 2) return 1;
    if (h <= 1) return 1;
    if (h <= 3) return 3;
    // Round up to next odd number >= h
    int sz = h;
    if (sz % 2 == 0) sz++;
    return sz;
}

// Helper: parse a delimiter from current token (for \left, \right, \middle)
// Returns -1 on error (no valid delimiter found), 0 on success.
// On success, consumes the delimiter token and sets *out.
static int parse_delim(Parser *p, ParenType *out) {
    if (peek_ch(p, '.')) {
        next(p);
        *out = PAREN_NONE;
        return 0;
    }
    if (peek_ch(p, '(') || tok_is_close_round(&p->look)) {
        next(p);
        *out = PAREN_ROUND;
        return 0;
    }
    if (peek_ch(p, '[') || tok_is_close_square(&p->look)) {
        next(p);
        *out = PAREN_SQUARE;
        return 0;
    }
    if (is_cmd(&p->look, "\\lbrace") || is_cmd(&p->look, "\\{") || is_cmd(&p->look, "\\rbrace") ||
        is_cmd(&p->look, "\\}")) {
        next(p);
        *out = PAREN_CURLY;
        return 0;
    }
    if (tok_is_open_vbar(&p->look) || tok_is_close_vbar(&p->look)) {
        next(p);
        *out = PAREN_VBAR;
        return 0;
    }
    if (tok_is_open_dvbar(&p->look) || tok_is_close_dvbar(&p->look) || is_cmd(&p->look, "\\|")) {
        next(p);
        *out = PAREN_DVBAR;
        return 0;
    }
    if (tok_is_open_floor(&p->look) || tok_is_close_floor(&p->look)) {
        next(p);
        *out = PAREN_FLOOR;
        return 0;
    }
    if (tok_is_open_ceil(&p->look) || tok_is_close_ceil(&p->look)) {
        next(p);
        *out = PAREN_CEIL;
        return 0;
    }
    if (tok_is_open_angle(&p->look) || tok_is_close_angle(&p->look)) {
        next(p);
        *out = PAREN_ANGLE;
        return 0;
    }
    return -1; // no valid delimiter
}

Ast *parse_left_right(Parser *p) {
    // We are at \left
    next(p);

    // Opening delimiter
    ParenType pt;
    if (parse_delim(p, &pt) < 0) {
        return err_ret_here(p, PARSE_ERR_MATH_SYNTAX,
                            "\\left must be followed by (, [, ., \\lbrace/\\{, '|'/\\lvert/\\vert, "
                            "\\Vert, \\lfloor, \\lceil, or \\langle");
    }

    // ---- IMPORTANT: suspend sized-paren boundary while parsing the body ----
    int saved_in = p->in_sized;
    ParenType saved_pt = p->bound_pt;
    int saved_size = p->bound_size;
    p->in_sized = 0; // disable 'at_sized_boundary' during \left ... \right body
    if (p->in_matrix) p->matrix_depth++;

// Parse body, handling \middle delimiters
#define MAX_MIDDLE 16
    Ast *segments[MAX_MIDDLE + 1];
    ParenType mid_types[MAX_MIDDLE];
    int n_middles = 0;

    segments[0] = parse_expr(p);
    if (!segments[0]) return NULL;

    while (is_cmd(&p->look, "\\middle") && n_middles < MAX_MIDDLE) {
        next(p); // consume \middle
        ParenType mpt;
        if (parse_delim(p, &mpt) < 0) {
            return err_ret_here(p, PARSE_ERR_MATH_SYNTAX,
                                "\\middle must be followed by a delimiter");
        }
        mid_types[n_middles] = mpt;
        n_middles++;
        segments[n_middles] = parse_expr(p);
        if (!segments[n_middles]) return NULL;
    }

    // restore boundary context
    p->in_sized = saved_in;
    p->bound_pt = saved_pt;
    p->bound_size = saved_size;
    if (p->in_matrix) p->matrix_depth--;
    // -----------------------------------------------------------------------

    // Expect \right
    if (!is_cmd(&p->look, "\\right")) {
        return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected \\right to match \\left");
    }
    next(p); // consume \right

    // Closing delimiter
    ParenType close_pt;
    if (parse_delim(p, &close_pt) < 0) {
        return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "\\right must be followed by a delimiter");
    }

    // Build the full body by concatenating segments with middle delimiters
    // First, combine all segments to measure full height
    Ast *body = segments[0];
    for (int i = 0; i < n_middles; i++) {
        // Insert a placeholder symbol for the middle delimiter that we'll size later
        // We use a temporary marker — after measuring, we'll rebuild with sized delimiters
        body = ast_binop('\0', body, segments[i + 1]);
    }

    // Auto-size from rendered child height (baseline-aware)
    Box measure = render_ast(body);
    int sz = size_from_height(measure.h, measure.baseline);
    box_free(&measure);

    // If we have middle delimiters, rebuild with properly sized delimiter AST nodes
    if (n_middles > 0) {
        // Free the quick-concatenated body — we'll rebuild properly
        // But we can't free it because it shares child pointers with segments[].
        // Instead, rebuild from scratch using the segments + sized middle delimiters.

        // Start with left_delim + segments[0]
        Ast *result = segments[0];
        for (int i = 0; i < n_middles; i++) {
            // Create a sized middle delimiter (both sides visible, content empty)
            ParenType mpt = mid_types[i];
            Ast *mid_delim = ast_paren_asym(ast_symbol(""), mpt, PAREN_NONE, sz);
            result = ast_binop('\0', result, mid_delim);
            result = ast_binop('\0', result, segments[i + 1]);
        }

        // Wrap in outer delimiters
        if (pt != close_pt) {
            return ast_paren_asym(result, pt, close_pt, sz);
        }
        return ast_paren(result, pt, sz);
    }

    // No middle delimiters — original behavior
    if (pt != close_pt) {
        return ast_paren_asym(body, pt, close_pt, sz);
    }
    return ast_paren(body, pt, sz);
}

Ast *parse_group(Parser *p) {
    if (!peek_ch(p, '{')) {
        return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected '{'");
    }
    next(p); // '{'
    if (p->in_matrix) p->matrix_depth++;

    // NEW: Check for empty group
    if (peek_ch(p, '}')) {
        next(p); // '}'
        if (p->in_matrix) p->matrix_depth--;
        // Return an empty group (could also be a special empty symbol)
        return ast_group(ast_symbol(""));
    }

    Ast *e = parse_expr(p);
    if (!e) return NULL;
    if (!peek_ch(p, '}')) {
        char buf[64];
        sprintf(buf, "expected '}', got '%s'", p->look.text);
        return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, buf);
    }
    next(p); // '}'
    if (p->in_matrix) p->matrix_depth--;
    return ast_group(e);
}

Ast *parse_paren_basic(Parser *p, ParenType pt) {
    if (pt == PAREN_ROUND) {
        if (!(peek_ch(p, '(') || is_cmd(&p->look, "\\("))) {
            return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected '(' or \\(");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth++;
        Ast *e = parse_expr(p);
        if (!e) return NULL;
        if (!(tok_is_close_round(&p->look))) {
            return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected ')' or \\)");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth--;
        return ast_paren(e, PAREN_ROUND, 1);
    } else if (pt == PAREN_SQUARE) {
        if (!(peek_ch(p, '[') || is_cmd(&p->look, "\\["))) {
            return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected '[' or \\[");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth++;
        Ast *e = parse_expr(p);
        if (!e) return NULL;
        if (!(tok_is_close_square(&p->look))) {
            return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected ']' or \\]");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth--;
        return ast_paren(e, PAREN_SQUARE, 1);
    } else {
        if (!(is_cmd(&p->look, "\\lbrace") || is_cmd(&p->look, "\\{"))) {
            return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected \\lbrace or \\{");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth++;
        Ast *e = parse_expr(p);
        if (!e) return NULL;
        if (!(is_cmd(&p->look, "\\rbrace") || is_cmd(&p->look, "\\}"))) {
            return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, "expected \\rbrace or \\}");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth--;
        return ast_paren(e, PAREN_CURLY, 1);
    }
}

Ast *parse_paren_sized(Parser *p, int size) {
    ParenType pt;

    if (tok_is_open_round(&p->look)) {
        next(p);
        pt = PAREN_ROUND;
    } else if (tok_is_open_square(&p->look)) {
        next(p);
        pt = PAREN_SQUARE;
    } else if (tok_is_open_curly(&p->look)) {
        next(p);
        pt = PAREN_CURLY;
    } else if (tok_is_open_vbar(&p->look)) {
        next(p);
        pt = PAREN_VBAR;
    } else if (tok_is_open_dvbar(&p->look)) {
        next(p);
        pt = PAREN_DVBAR;
    } else if (tok_is_open_floor(&p->look)) {
        next(p);
        pt = PAREN_FLOOR;
    } else if (tok_is_open_ceil(&p->look)) {
        next(p);
        pt = PAREN_CEIL;
    } else if (tok_is_open_angle(&p->look)) {
        next(p);
        pt = PAREN_ANGLE;
    } else {
        return err_ret_here(p, PARSE_ERR_MATH_SYNTAX,
                            "sized paren requires one of '(', '[', \\lbrace/\\{, "
                            "\\lvert/\\Vert/\\lfloor/\\lceil/\\langle after the size macro");
    }

    // push boundary
    int prev_in = p->in_sized;
    ParenType prev_pt = p->bound_pt;
    int prev_size = p->bound_size;
    p->in_sized = prev_in + 1;
    p->bound_pt = pt;
    p->bound_size = size;

    if (p->in_matrix) p->matrix_depth++;
    Ast *e = parse_expr(p);
    if (!e) return NULL;

    // pop boundary
    p->in_sized = prev_in;
    p->bound_pt = prev_pt;
    p->bound_size = prev_size;

    // If we hit end-of-input inside a sized paren, give a clear error
    if (p->look.kind == TOK_EOF) {
        if (p->in_matrix) p->matrix_depth--;
        ast_free(e);
        return err_ret_here(p, PARSE_ERR_MATH_SYNTAX,
                            "unclosed sized delimiter (e.g. \\big( without matching \\big))");
    }

    // Optional closing size macro (must match opening size)
    if (is_size_macro_tok(&p->look)) {
        int close_sz = macro_size(p->look.text);
        if (close_sz != size) {
            char buf[64];
            sprintf(buf, "closing size macro must match opening (got %s)", p->look.text);
            return err_ret_here(p, PARSE_ERR_MATH_SYNTAX, buf);
        }
        next(p); // consume \big/\Big/\bigg/\Bigg
    }

    // Matching closing delimiter for the chosen pt
    if (!tok_is_close_for_pt(&p->look, pt)) {
        return err_ret_here(p, PARSE_ERR_MATH_SYNTAX,
                            "expected matching closing delimiter for sized group");
    }
    next(p);
    if (p->in_matrix) p->matrix_depth--;
    return ast_paren(e, pt, size);
}

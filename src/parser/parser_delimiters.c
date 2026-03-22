// parser_delimiters.c - Parentheses and delimiter parsing
// Verbatim from original parser.c

#include "parser_internal.h"

int size_from_height(int h) {
    if (h <= 1) return 1;
    if (h <= 3) return 3;
    // Round up to next odd number >= h
    int sz = h;
    if (sz % 2 == 0) sz++;
    return sz;
}

Ast *parse_left_right(Parser *p) {
    // We are at \left
    next(p);

    // Opening delimiter
    ParenType pt = PAREN_ROUND;
    if (peek_ch(p, '(')) {
        next(p);
        pt = PAREN_ROUND;
    } else if (peek_ch(p, '[')) {
        next(p);
        pt = PAREN_SQUARE;
    } else if (is_cmd(&p->look, "\\lbrace") || is_cmd(&p->look, "\\{")) {
        next(p);
        pt = PAREN_CURLY;
    } else if (tok_is_open_vbar(&p->look)) {
        next(p);
        pt = PAREN_VBAR;
    } // '|' or \lvert/\vert
    else if (tok_is_open_dvbar(&p->look)) {
        next(p);
        pt = PAREN_DVBAR;
    } // \Vert
    else if (tok_is_open_floor(&p->look)) {
        next(p);
        pt = PAREN_FLOOR;
    } // \lfloor
    else if (tok_is_open_ceil(&p->look)) {
        next(p);
        pt = PAREN_CEIL;
    } // \lceil
    else {
        return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0,
                       "\\left must be followed by (, [, \\lbrace/\\{, '|'/\\lvert/\\vert, \\Vert, "
                       "\\lfloor, or \\lceil");
    }

    // ---- IMPORTANT: suspend sized-paren boundary while parsing the body ----
    int saved_in = p->in_sized;
    ParenType saved_pt = p->bound_pt;
    int saved_size = p->bound_size;
    p->in_sized = 0; // disable 'at_sized_boundary' during \left ... \right body
    if (p->in_matrix) p->matrix_depth++;

    Ast *body = parse_expr(p);
    if (!body) return NULL;

    // restore boundary context
    p->in_sized = saved_in;
    p->bound_pt = saved_pt;
    p->bound_size = saved_size;
    if (p->in_matrix) p->matrix_depth--;
    // -----------------------------------------------------------------------

    // Expect \right
    if (!is_cmd(&p->look, "\\right")) {
        return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected \\right to match \\left");
    }
    next(p); // consume \right

    // Closing delimiter must match type
    if (!tok_is_close_for_pt(&p->look, pt)) {
        return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0,
                       "\\right closing delimiter does not match \\left opener");
    }
    next(p); // consume the closer

    // Auto-size from rendered child height
    Box measure = render_ast(body);
    int sz = size_from_height(measure.h);
    box_free(&measure);

    return ast_paren(body, pt, sz);
}

Ast *parse_group(Parser *p) {
    if (!peek_ch(p, '{')) {
        return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected '{'");
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
        return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, buf);
    }
    next(p); // '}'
    if (p->in_matrix) p->matrix_depth--;
    return ast_group(e);
}

Ast *parse_paren_basic(Parser *p, ParenType pt) {
    if (pt == PAREN_ROUND) {
        if (!(peek_ch(p, '(') || is_cmd(&p->look, "\\("))) {
            return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected '(' or \\(");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth++;
        Ast *e = parse_expr(p);
        if (!e) return NULL;
        if (!(tok_is_close_round(&p->look))) {
            return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected ')' or \\)");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth--;
        return ast_paren(e, PAREN_ROUND, 1);
    } else if (pt == PAREN_SQUARE) {
        if (!(peek_ch(p, '[') || is_cmd(&p->look, "\\["))) {
            return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected '[' or \\[");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth++;
        Ast *e = parse_expr(p);
        if (!e) return NULL;
        if (!(tok_is_close_square(&p->look))) {
            return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected ']' or \\]");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth--;
        return ast_paren(e, PAREN_SQUARE, 1);
    } else {
        if (!(is_cmd(&p->look, "\\lbrace") || is_cmd(&p->look, "\\{"))) {
            return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected \\lbrace or \\{");
        }
        next(p);
        if (p->in_matrix) p->matrix_depth++;
        Ast *e = parse_expr(p);
        if (!e) return NULL;
        if (!(is_cmd(&p->look, "\\rbrace") || is_cmd(&p->look, "\\}"))) {
            return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, "expected \\rbrace or \\}");
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
    } else {
        return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0,
                       "sized paren requires one of '(', '[', \\lbrace/\\{, or "
                       "\\lvert/\\Vert/\\lfloor/\\lceil after the size macro");
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

    // Optional closing size macro (must match opening size)
    if (is_size_macro_tok(&p->look)) {
        int close_sz = macro_size(p->look.text);
        if (close_sz != size) {
            char buf[64];
            sprintf(buf, "closing size macro must match opening (got %s)", p->look.text);
            return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0, buf);
        }
        next(p); // consume \big/\Big/\bigg/\Bigg
    }

    // Matching closing delimiter for the chosen pt
    if (!tok_is_close_for_pt(&p->look, pt)) {
        return err_ret(p, PARSE_ERR_MATH_SYNTAX, 0, 0,
                       "expected matching closing delimiter for sized group");
    }
    next(p);
    if (p->in_matrix) p->matrix_depth--;
    return ast_paren(e, pt, size);
}

// parser_operators.c - Sum, product, integral, limit operators
// Verbatim from original parser.c

#include "parser_internal.h"

int size_from_macro(const char *cmd) {
    // Recognize size macros across families
    // Returns 1..3 for big, Big, bigg/Bigg; 0 if not a size macro token.
    if (!cmd) return 0;
    if (!strcmp(cmd, "\\sum") || !strcmp(cmd, "\\prod") || !strcmp(cmd, "\\int")) return 1;
    if (!strcmp(cmd, "\\Sum") || !strcmp(cmd, "\\Prod") || !strcmp(cmd, "\\Int")) return 2;
    if (!strcmp(cmd, "\\SUM") || !strcmp(cmd, "\\PROD") || !strcmp(cmd, "\\INT")) return 3;

    // Multiple integrals
    if (!strcmp(cmd, "\\iint") || !strcmp(cmd, "\\iiint") || !strcmp(cmd, "\\oint") ||
        !strcmp(cmd, "\\oiint"))
        return 1;
    if (!strcmp(cmd, "\\Iint") || !strcmp(cmd, "\\Iiint") || !strcmp(cmd, "\\Oint") ||
        !strcmp(cmd, "\\Oiint"))
        return 2;
    if (!strcmp(cmd, "\\IINT") || !strcmp(cmd, "\\IIINT") || !strcmp(cmd, "\\OINT") ||
        !strcmp(cmd, "\\OIINT"))
        return 3;

    // coproduct
    if (!strcmp(cmd, "\\coprod")) return 1;
    if (!strcmp(cmd, "\\Coprod")) return 2;
    if (!strcmp(cmd, "\\COPROD")) return 3;

    // bigcup/bigcap families
    if (!strcmp(cmd, "\\bigcup") || !strcmp(cmd, "\\bigcap")) return 1;
    if (!strcmp(cmd, "\\Bigcup") || !strcmp(cmd, "\\Bigcap")) return 2;
    if (!strcmp(cmd, "\\biggcup") || !strcmp(cmd, "\\biggcap")) return 3;
    if (!strcmp(cmd, "\\Biggcup") || !strcmp(cmd, "\\Biggcap")) return 3;

    // quantifiers: fixed-size 1
    if (!strcmp(cmd, "\\forall") || !strcmp(cmd, "\\exists")) return 1;

    // bigcup/bigcap families (duplicated - keep for safety)
    if (!strcmp(cmd, "\\bigcup") || !strcmp(cmd, "\\bigcap")) return 1;
    if (!strcmp(cmd, "\\Bigcup") || !strcmp(cmd, "\\Bigcap") || !strcmp(cmd, "\\BigCup") ||
        !strcmp(cmd, "\\BigCap"))
        return 2;
    if (!strcmp(cmd, "\\biggcup") || !strcmp(cmd, "\\biggcap")) return 3;
    if (!strcmp(cmd, "\\Biggcup") || !strcmp(cmd, "\\Biggcap")) return 3;

    return 0;
}

Ast *parse_fraction(Parser *p) {
    // at \frac
    next(p); // consume \frac

    Ast *num = NULL;
    Ast *den = NULL;

    // numerator: group or single primary (one token)
    if (peek_ch(p, '{'))
        num = parse_group(p);
    else
        num = parse_primary(p);
    if (!num) return NULL;

    // denominator: group or single primary
    if (peek_ch(p, '{'))
        den = parse_group(p);
    else
        den = parse_primary(p);
    if (!den) return NULL;

    return ast_fraction(num, den);
}

int manual_size_from_cmd(const char *cmd) {
    if (strcmp(cmd, "\\sum") == 0) return 1;
    if (strcmp(cmd, "\\Sum") == 0) return 2;
    if (strcmp(cmd, "\\SUM") == 0) return 3;
    return 1;
}
Ast *parse_sum(Parser *p) {
    int size = manual_size_from_cmd(p->look.text);
    next(p); // consume \sum/\Sum/\SUM

    Ast *lower = NULL, *upper = NULL;
    if (peek_ch(p, '_')) {
        next(p);
        if (peek_ch(p, '{'))
            lower = parse_group(p);
        else if (p->look.kind == TOK_IDENT || p->look.kind == TOK_SYM) {
            lower = ast_symbol(p->look.text);
            next(p);
        } else
            lower = parse_primary(p);
        if (!lower) return NULL;
    }
    if (peek_ch(p, '^')) {
        next(p);
        if (peek_ch(p, '{'))
            upper = parse_group(p);
        else if (p->look.kind == TOK_IDENT || p->look.kind == TOK_SYM) {
            upper = ast_symbol(p->look.text);
            next(p);
        } else
            upper = parse_primary(p);
        if (!upper) return NULL;
    }

    Ast *body = parse_expr(p);
    if (!body) return NULL;
    return ast_sum(lower, upper, body, size);
}

Ast *parse_limop(Parser *p, AstKind kind, int size_macro_just_consumed) {
    // We enter with look == the operator token; consume it.
    const char *tok = p->look.text;
    int size = size_from_macro(tok);
    if (size_macro_just_consumed) size = size_macro_just_consumed;
    next(p);

    Ast *lower = NULL, *upper = NULL;

    // NEW: Loop to handle _ and ^ in ANY order
    while (peek_ch(p, '_') || peek_ch(p, '^')) {
        if (peek_ch(p, '_')) {
            next(p);
            if (peek_ch(p, '{'))
                lower = parse_group(p);
            else if (p->look.kind == TOK_IDENT || p->look.kind == TOK_SYM) {
                lower = ast_symbol(p->look.text);
                next(p);
            } else
                lower = parse_primary(p);
            if (!lower) return NULL;
        } else if (peek_ch(p, '^')) {
            next(p);
            if (peek_ch(p, '{'))
                upper = parse_group(p);
            else if (p->look.kind == TOK_IDENT || p->look.kind == TOK_SYM) {
                upper = ast_symbol(p->look.text);
                next(p);
            } else
                upper = parse_primary(p);
            if (!upper) return NULL;
        }
    }

    Ast *body = parse_expr(p);
    if (!body) return NULL;

    switch (kind) {
    case AST_SUM: return ast_sum(lower, upper, body, size);
    case AST_PROD: return ast_prod(lower, upper, body, size);
    case AST_INT: return ast_int(lower, upper, body, size);
    case AST_IINT: return ast_iint(lower, upper, body, size);
    case AST_IIINT: return ast_iiint(lower, upper, body, size);
    case AST_OINT: return ast_oint(lower, upper, body, size);
    case AST_OIINT: return ast_oiint(lower, upper, body, size);
    case AST_BIGCUP: return ast_bigcup(lower, upper, body, size);
    case AST_BIGCAP: return ast_bigcap(lower, upper, body, size);
    case AST_COPROD: return ast_coprod(lower, upper, body, size);
    case AST_FORALL: return ast_forall(lower, upper, body, size);
    case AST_EXISTS: return ast_exists(lower, upper, body, size);
    default: return body; // shouldn't happen
    }
}

Ast *parse_limfunc(Parser *p, const char *name) {
    // We enter with look == the operator token; consume it.
    next(p);

    Ast *lower = NULL, *upper = NULL;

    // Handle _ and ^ in any order
    while (peek_ch(p, '_') || peek_ch(p, '^')) {
        if (peek_ch(p, '_')) {
            next(p);
            if (peek_ch(p, '{'))
                lower = parse_group(p);
            else if (p->look.kind == TOK_IDENT || p->look.kind == TOK_SYM) {
                lower = ast_symbol(p->look.text);
                next(p);
            } else
                lower = parse_primary(p);
            if (!lower) return NULL;
        } else if (peek_ch(p, '^')) {
            next(p);
            if (peek_ch(p, '{'))
                upper = parse_group(p);
            else if (p->look.kind == TOK_IDENT || p->look.kind == TOK_SYM) {
                upper = ast_symbol(p->look.text);
                next(p);
            } else
                upper = parse_primary(p);
            if (!upper) return NULL;
        }
    }

    // Don't consume body if at a context boundary (e.g., inside a group argument)
    if (peek_ch(p, '}') || p->look.kind == TOK_EOF || is_cmd(&p->look, "\\right") ||
        is_cmd(&p->look, "\\end") || is_cmd(&p->look, "\\middle")) {
        return ast_limfunc(name, lower, upper, NULL);
    }
    Ast *body = parse_expr(p);
    if (!body) return NULL;

    return ast_limfunc(name, lower, upper, body);
}

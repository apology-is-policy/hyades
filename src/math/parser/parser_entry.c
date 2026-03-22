// parser_entry.c - Entry point: parse_math()
// Verbatim from original parser.c

#include "parser_internal.h"

Ast *parse_math(const char *src, ParseError *err) {
    if (err) {
        err->code = PARSE_OK;
        err->row = 0;
        err->col = 0;
        err->message[0] = '\0';
    }

    Parser p = {
        .lx = lex_make(src),
        .in_sized = 0,
        .bound_pt = PAREN_ROUND,
        .bound_size = 1,
        .in_matrix = 0,
        .matrix_depth = 0,
        .err = err,
    };
    next(&p);
    Ast *e = parse_expr(&p);

    if (!e) {
        // error already filled by err_ret
        return NULL;
    }

    return e; // (ignore trailing)
}
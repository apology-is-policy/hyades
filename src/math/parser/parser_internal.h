// parser_internal.h - Internal types and declarations for math parser
//
// This header is shared by all files in the parser/ module.

#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer/lexer.h"
#include "math/ast.h"
#include "math/renderer/symbols.h"
#include "parser.h"

// ============================================================================
// Parser State
// ============================================================================

typedef struct Parser {
    Lexer lx;
    Token look;

    // sized-paren boundary context (supports nesting)
    int in_sized;       // depth > 0 means we're inside a sized paren body
    ParenType bound_pt; // current innermost sized paren type
    int bound_size;     // size (3/5/7) for the innermost sized paren
    int in_matrix;
    int matrix_depth;
    int in_absval; // depth > 0 means we're inside absolute value |...|

    ParseError *err;
} Parser;

// ============================================================================
// Parser Core (parser_core.c)
// ============================================================================

Ast *err_ret(Parser *p, ParseErrorCode code, int row, int col, const char *fmt, ...);

// Convenience macro: auto-populate position from the current lookahead token
#define err_ret_here(p, code, fmt, ...)                                                            \
    err_ret((p), (code), (p)->look.line, (p)->look.col, (fmt), ##__VA_ARGS__)

void next(Parser *p);
int peek_ch(Parser *p, char ch);
int is_cmd(const Token *t, const char *name);

// ============================================================================
// Token Classification (parser_tokens.c)
// ============================================================================

// Size macros
int macro_size(const char *s);
int is_size_macro_tok(const Token *t);
int is_left_size_macro(const Token *t);
int is_right_size_macro(const Token *t);

// Round parens
int tok_is_open_round(const Token *t);
int tok_is_close_round(const Token *t);

// Square brackets
int tok_is_open_square(const Token *t);
int tok_is_close_square(const Token *t);

// Curly braces
int tok_is_open_curly(const Token *t);
int tok_is_close_curly(const Token *t);

// Relations
int tok_is_relation(const Token *t);

// Close matching
int tok_is_close_for_pt(const Token *t, ParenType pt);

// Implicit multiplication
int starts_primary_tok(const Token *t);

// Sized paren boundaries
int peek_is_matching_close(Parser *p);
int at_sized_boundary(Parser *p);

// Bar delimiters
int tok_is_sym_bar(const Token *t);
int tok_is_open_vbar(const Token *t);
int tok_is_close_vbar(const Token *t);
int tok_is_open_dvbar(const Token *t);
int tok_is_close_dvbar(const Token *t);

// Floor/ceil
int tok_is_open_floor(const Token *t);
int tok_is_close_floor(const Token *t);
int tok_is_open_ceil(const Token *t);
int tok_is_close_ceil(const Token *t);

// Angle brackets
int tok_is_open_angle(const Token *t);
int tok_is_close_angle(const Token *t);

// Row breaks
int tok_is_rowbreak(const Token *t);

// ============================================================================
// Delimiters (parser_delimiters.c)
// ============================================================================

int size_from_height(int h, int baseline);
Ast *parse_left_right(Parser *p);
Ast *parse_group(Parser *p);
Ast *parse_paren_basic(Parser *p, ParenType pt);
Ast *parse_paren_sized(Parser *p, int size);

// ============================================================================
// Operators (parser_operators.c)
// ============================================================================

int size_from_macro(const char *cmd);
int manual_size_from_cmd(const char *cmd);
Ast *parse_fraction(Parser *p);
Ast *parse_sum(Parser *p);
Ast *parse_limop(Parser *p, AstKind kind, int size_macro_just_consumed);
Ast *parse_limfunc(Parser *p, const char *name);

// ============================================================================
// Constructs (parser_constructs.c)
// ============================================================================

Ast *parse_sqrt(Parser *p);
Ast *empty_cell_ast(void);
Ast *parse_matrix_core(Parser *p, int *out_rows, int *out_cols);

// ============================================================================
// Grammar (parser_grammar.c)
// ============================================================================

Ast *parse_primary(Parser *p);
Ast *parse_factor(Parser *p);
Ast *parse_term(Parser *p);
Ast *parse_expr(Parser *p);

// ============================================================================
// Entry (parser_entry.c)
// ============================================================================

// parse_math is declared in parser.h (public API)

#endif // PARSER_INTERNAL_H

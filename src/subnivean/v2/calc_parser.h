// calc_parser.h - Parser for Hyades Computational Expressions
//
// Parses Hyades calc expressions into an AST.

#ifndef CALC_PARSER_H
#define CALC_PARSER_H

#include "calc_ast.h"

// ============================================================================
// Parser State
// ============================================================================

typedef struct {
    const char *input; // Full input string
    const char *p;     // Current position
    int line;
    int col;

    // Error handling
    char error_msg[256];
    bool had_error;
} CalcParser;

// ============================================================================
// Parser API
// ============================================================================

// Initialize parser with input string
void calc_parser_init(CalcParser *parser, const char *input);

// Parse a complete expression (may be a sequence)
AstNode *calc_parse(CalcParser *parser);

// Parse a single expression
AstNode *calc_parse_expr(CalcParser *parser);

// Get error message (if had_error is true)
const char *calc_parser_error(CalcParser *parser);

#endif // CALC_PARSER_H

// parser.h - Subnivean parser interface
//
// Parses token stream into AST.

#ifndef SUBNIVEAN_PARSER_H
#define SUBNIVEAN_PARSER_H

#include "ast.h"
#include "lexer.h"
#include <stdbool.h>

// Parser state
typedef struct {
    Lexer lexer;
    Token current;  // Current token
    Token previous; // Previous token (for error context)

    // Error handling
    bool had_error;
    bool panic_mode; // In panic mode, skip tokens until sync point
    char error_msg[512];

    // Statistics (for debugging)
    int nodes_created;
} Parser;

// ============================================================================
// Parser Lifecycle
// ============================================================================

// Initialize parser with source string
void parser_init(Parser *p, const char *source);

// Free any resources held by parser (does not free AST)
void parser_free(Parser *p);

// ============================================================================
// Parsing
// ============================================================================

// Parse entire source into a block of statements
// Returns AST_BLOCK node, or NULL on error
// Caller owns the returned AST
AstNode *parser_parse(Parser *p);

// Parse a single statement
// Returns statement node, or NULL on error
AstNode *parser_parse_statement(Parser *p);

// Parse a single expression
// Returns expression node, or NULL on error
AstNode *parser_parse_expression(Parser *p);

// ============================================================================
// Error Handling
// ============================================================================

// Check if parser encountered any errors
bool parser_had_error(Parser *p);

// Get error message (may be multi-line for multiple errors)
const char *parser_error_msg(Parser *p);

// Get number of nodes created (for debugging/stats)
int parser_nodes_created(Parser *p);

// ============================================================================
// Grammar (for reference)
// ============================================================================
//
// program      → statement* EOF
//
// statement    → let_stmt
//              | assign_stmt
//              | inc_dec_stmt
//              | if_stmt
//              | loop_stmt
//              | enumerate_stmt
//              | lambda_def
//              | return_stmt
//              | exit_when_stmt
//              | expr_stmt
//              | text_splice
//
// let_stmt     → "\let" "<" NAME ">" "{" expression "}"
//              | "\let" "<" NAME "[]>" "{" array_init "}"
//
// assign_stmt  → "\assign" "<" NAME ">" "{" expression "}"
//
// inc_dec_stmt → ("\inc" | "\dec") "<" NAME ">"
//
// if_stmt      → "\if" "{" expression "}" "{" block "}" ("\else" "{" block "}")?
//
// loop_stmt    → "\begin" "{loop}" block "\end" "{loop}"
//
// enumerate_stmt → "\begin" "<" NAME ">" "[" NAME "," NAME "]" "{enumerate}"
//                  block "\end" "{enumerate}"
//
// lambda_def   → "\lambda" "<" NAME ">" ("[" param_list "]")? "#"? "{" block "}"
//
// return_stmt  → "\return" "{" expression "}"
//
// exit_when_stmt → "\exit_when" "{" expression "}"
//
// expr_stmt    → expression (when expression has side effects)
//
// text_splice  → TEXT | UNKNOWN_CMD ...
//
// expression   → or_expr
//
// or_expr      → and_expr | "\or" "{" expression "," expression "}"
//
// and_expr     → equality | "\and" "{" expression "," expression "}"
//
// equality     → comparison | ("\eq" | "\ne") "{" expression "," expression "}"
//
// comparison   → term | ("\lt" | "\gt" | "\le" | "\ge") "{" expression "," expression "}"
//
// term         → factor | ("\add" | "\sub") "{" expression "," expression "}"
//
// factor       → unary | ("\mul" | "\div" | "\mod") "{" expression "," expression "}"
//
// unary        → "\not" "{" expression "}" | primary
//
// primary      → INT
//              | "\valueof" "<" NAME ">" ("[" expression "]")?
//              | "\recall" "<" NAME ">" ("[" arg_list "]")?
//              | "\len" "<" NAME ">"
//              | "\ref" "<" NAME ">"
//              | "\pop" "<" NAME ">"
//              | "\peek" "<" NAME ">"
//              | "{" expression "}"
//
// param_list   → NAME ("," NAME)*
// arg_list     → expression ("," expression)*
// array_init   → "[" (expression ("," expression)*)? "]"
// block        → statement*
//
// ============================================================================

#endif // SUBNIVEAN_PARSER_H

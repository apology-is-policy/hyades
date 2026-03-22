#pragma once
#include <stdbool.h>

typedef enum {
    TOK_EOF = 0,
    TOK_IDENT, // identifiers and \commands (stored in text)
    TOK_SYM    // one-character symbols in text[0]
} TokKind;

typedef struct {
    TokKind kind;
    char text[64];
    int line;     // 1-based line number where token starts
    int col;      // 1-based column where token starts
    int end_line; // 1-based line number where token ends
    int end_col;  // 1-based column where token ends
} Token;

typedef struct {
    const char *src;
    int pos;       // Byte offset in source
    int line;      // Current 1-based line number
    int col;       // Current 1-based column number
    int last_line; // Line at start of current token
    int last_col;  // Column at start of current token
} Lexer;

Lexer lex_make(const char *src);
Token lex_next(Lexer *lx);

// Get current position in lexer (useful for error reporting)
void lex_get_position(const Lexer *lx, int *line, int *col);
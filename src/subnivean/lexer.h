// lexer.h - Subnivean lexer interface
//
// Tokenizes Hyades computational expressions.

#ifndef SUBNIVEAN_LEXER_H
#define SUBNIVEAN_LEXER_H

#include "token.h"
#include <stdbool.h>

// Lexer state
typedef struct {
    const char *source;  // Full source string (not owned)
    const char *start;   // Start of current lexeme
    const char *current; // Current position

    int line;       // Current line (1-based)
    int col;        // Current column (1-based)
    int start_line; // Line at start of current lexeme
    int start_col;  // Column at start of current lexeme

    // Peeked token (for lookahead)
    Token peeked;
    bool has_peeked;

    // Error state
    bool had_error;
    char error_msg[256];
} Lexer;

// ============================================================================
// Lexer Lifecycle
// ============================================================================

// Initialize lexer with source string
void lexer_init(Lexer *lex, const char *source);

// Reset lexer to beginning
void lexer_reset(Lexer *lex);

// ============================================================================
// Token Retrieval
// ============================================================================

// Get the next token (advances lexer)
Token lexer_next(Lexer *lex);

// Peek at the next token (does not advance)
Token lexer_peek(Lexer *lex);

// Check if next token is of given type (does not advance)
bool lexer_check(Lexer *lex, TokenType type);

// Advance if next token matches, return true; else return false
bool lexer_match(Lexer *lex, TokenType type);

// Expect a specific token type; emit error if not found
Token lexer_expect(Lexer *lex, TokenType expected, const char *context);

// ============================================================================
// Position Tracking
// ============================================================================

// Get current source position
const char *lexer_current_pos(Lexer *lex);

// Get current line number
int lexer_current_line(Lexer *lex);

// Get current column number
int lexer_current_col(Lexer *lex);

// ============================================================================
// Error Handling
// ============================================================================

// Check if lexer has encountered an error
bool lexer_had_error(Lexer *lex);

// Get error message (if any)
const char *lexer_error_msg(Lexer *lex);

// ============================================================================
// Command Recognition
// ============================================================================

// Check if a string is a recognized computational command
// Returns token type, or TOK_UNKNOWN_CMD if not recognized
TokenType lexer_lookup_command(const char *name, int length);

// Table of recognized commands (for iteration/debugging)
typedef struct {
    const char *name;
    int length;
    TokenType type;
} CommandEntry;

extern const CommandEntry command_table[];
extern const int command_table_size;

#endif // SUBNIVEAN_LEXER_H

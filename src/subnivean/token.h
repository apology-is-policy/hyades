// token.h - Subnivean lexer token definitions
//
// Tokens for the Hyades computational language subset.

#ifndef SUBNIVEAN_TOKEN_H
#define SUBNIVEAN_TOKEN_H

#include <stdint.h>

typedef enum {
    // ========================================================================
    // Literals
    // ========================================================================
    TOK_INT,    // Integer literal: 42, -17, 0
    TOK_STRING, // String/text content (non-command)

    // ========================================================================
    // Delimiters
    // ========================================================================
    TOK_LBRACE,   // {
    TOK_RBRACE,   // }
    TOK_LBRACKET, // [
    TOK_RBRACKET, // ]
    TOK_LANGLE,   // <
    TOK_RANGLE,   // >
    TOK_COMMA,    // ,
    TOK_HASH,     // # (for compact lambda bodies #{...})
    TOK_PERCENT,  // % (line continuation in lambda bodies)

    // ========================================================================
    // Arithmetic Commands
    // ========================================================================
    TOK_CMD_ADD, // \add
    TOK_CMD_SUB, // \sub
    TOK_CMD_MUL, // \mul
    TOK_CMD_DIV, // \div
    TOK_CMD_MOD, // \mod

    // ========================================================================
    // Comparison Commands
    // ========================================================================
    TOK_CMD_EQ, // \eq
    TOK_CMD_NE, // \ne
    TOK_CMD_LT, // \lt
    TOK_CMD_GT, // \gt
    TOK_CMD_LE, // \le
    TOK_CMD_GE, // \ge

    // ========================================================================
    // Logic Commands
    // ========================================================================
    TOK_CMD_AND, // \and
    TOK_CMD_OR,  // \or
    TOK_CMD_NOT, // \not

    // ========================================================================
    // Variable Commands
    // ========================================================================
    TOK_CMD_LET,     // \let
    TOK_CMD_VALUEOF, // \valueof
    TOK_CMD_INC,     // \inc
    TOK_CMD_DEC,     // \dec
    TOK_CMD_ASSIGN,  // \assign
    TOK_CMD_REF,     // \ref

    // ========================================================================
    // Control Flow Commands
    // ========================================================================
    TOK_CMD_IF,        // \if
    TOK_CMD_ELSE,      // \else
    TOK_CMD_BEGIN,     // \begin
    TOK_CMD_END,       // \end
    TOK_CMD_EXIT_WHEN, // \exit_when

    // ========================================================================
    // Function Commands
    // ========================================================================
    TOK_CMD_LAMBDA, // \lambda
    TOK_CMD_RECALL, // \recall
    TOK_CMD_RETURN, // \return

    // ========================================================================
    // Array Commands
    // ========================================================================
    TOK_CMD_LEN,        // \len
    TOK_CMD_PUSH,       // \push
    TOK_CMD_POP,        // \pop
    TOK_CMD_PEEK,       // \peek
    TOK_CMD_SETELEMENT, // \setelement
    TOK_CMD_ENQUEUE,    // \enqueue
    TOK_CMD_DEQUEUE,    // \dequeue

    // ========================================================================
    // Environment Names (after \begin{ or \end{)
    // ========================================================================
    TOK_ENV_LOOP,      // loop
    TOK_ENV_ENUMERATE, // enumerate

    // ========================================================================
    // Special Tokens
    // ========================================================================
    TOK_BACKSLASH,   // \ followed by unrecognized command
    TOK_UNKNOWN_CMD, // Unrecognized \command (name captured in lexeme)
    TOK_TEXT,        // Arbitrary text (pass to text interpreter)
    TOK_NEWLINE,     // Explicit newline (for line continuation tracking)
    TOK_EOF,         // End of input
    TOK_ERROR,       // Lexer error

    TOK_COUNT // Number of token types
} TokenType;

// Token structure
typedef struct {
    TokenType type;

    // Source location
    const char *start; // Pointer into source string
    int length;        // Length of lexeme
    int line;          // 1-based line number
    int col;           // 1-based column number

    // Parsed values (type-dependent)
    union {
        int32_t int_value; // For TOK_INT
        struct {
            const char *name; // For TOK_UNKNOWN_CMD: command name (not owned)
            int name_len;
        } cmd;
    };
} Token;

// Token type names for debugging
extern const char *token_type_names[TOK_COUNT];

// Token utilities
const char *token_type_name(TokenType type);
void token_print(const Token *tok);

// Create a simple token
Token token_make(TokenType type, const char *start, int length, int line, int col);

// Create an integer token
Token token_make_int(const char *start, int length, int line, int col, int32_t value);

// Create an error token
Token token_make_error(const char *message, int line, int col);

#endif // SUBNIVEAN_TOKEN_H

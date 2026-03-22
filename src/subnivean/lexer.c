// lexer.c - Subnivean lexer implementation
//
// Tokenizes Hyades computational expressions.

#include "lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// Command Table
// ============================================================================

// Sorted by length (descending) then alphabetically for efficient lookup
const CommandEntry command_table[] = {
    // 11 characters
    {"setelement", 10, TOK_CMD_SETELEMENT},

    // 9 characters
    {"exit_when", 9, TOK_CMD_EXIT_WHEN},

    // 7 characters
    {"dequeue", 7, TOK_CMD_DEQUEUE},
    {"enqueue", 7, TOK_CMD_ENQUEUE},
    {"valueof", 7, TOK_CMD_VALUEOF},

    // 6 characters
    {"assign", 6, TOK_CMD_ASSIGN},
    {"lambda", 6, TOK_CMD_LAMBDA},
    {"recall", 6, TOK_CMD_RECALL},
    {"return", 6, TOK_CMD_RETURN},

    // 5 characters
    {"begin", 5, TOK_CMD_BEGIN},

    // 4 characters
    {"else", 4, TOK_CMD_ELSE},
    {"peek", 4, TOK_CMD_PEEK},
    {"push", 4, TOK_CMD_PUSH},

    // 3 characters
    {"add", 3, TOK_CMD_ADD},
    {"and", 3, TOK_CMD_AND},
    {"dec", 3, TOK_CMD_DEC},
    {"div", 3, TOK_CMD_DIV},
    {"end", 3, TOK_CMD_END},
    {"inc", 3, TOK_CMD_INC},
    {"len", 3, TOK_CMD_LEN},
    {"let", 3, TOK_CMD_LET},
    {"mod", 3, TOK_CMD_MOD},
    {"mul", 3, TOK_CMD_MUL},
    {"not", 3, TOK_CMD_NOT},
    {"pop", 3, TOK_CMD_POP},
    {"ref", 3, TOK_CMD_REF},
    {"sub", 3, TOK_CMD_SUB},

    // 2 characters
    {"eq", 2, TOK_CMD_EQ},
    {"ge", 2, TOK_CMD_GE},
    {"gt", 2, TOK_CMD_GT},
    {"if", 2, TOK_CMD_IF},
    {"le", 2, TOK_CMD_LE},
    {"lt", 2, TOK_CMD_LT},
    {"ne", 2, TOK_CMD_NE},
    {"or", 2, TOK_CMD_OR},
};

const int command_table_size = sizeof(command_table) / sizeof(command_table[0]);

// ============================================================================
// Token Type Names
// ============================================================================

const char *token_type_names[TOK_COUNT] = {
    [TOK_INT] = "INT",
    [TOK_STRING] = "STRING",
    [TOK_LBRACE] = "LBRACE",
    [TOK_RBRACE] = "RBRACE",
    [TOK_LBRACKET] = "LBRACKET",
    [TOK_RBRACKET] = "RBRACKET",
    [TOK_LANGLE] = "LANGLE",
    [TOK_RANGLE] = "RANGLE",
    [TOK_COMMA] = "COMMA",
    [TOK_HASH] = "HASH",
    [TOK_PERCENT] = "PERCENT",
    [TOK_CMD_ADD] = "CMD_ADD",
    [TOK_CMD_SUB] = "CMD_SUB",
    [TOK_CMD_MUL] = "CMD_MUL",
    [TOK_CMD_DIV] = "CMD_DIV",
    [TOK_CMD_MOD] = "CMD_MOD",
    [TOK_CMD_EQ] = "CMD_EQ",
    [TOK_CMD_NE] = "CMD_NE",
    [TOK_CMD_LT] = "CMD_LT",
    [TOK_CMD_GT] = "CMD_GT",
    [TOK_CMD_LE] = "CMD_LE",
    [TOK_CMD_GE] = "CMD_GE",
    [TOK_CMD_AND] = "CMD_AND",
    [TOK_CMD_OR] = "CMD_OR",
    [TOK_CMD_NOT] = "CMD_NOT",
    [TOK_CMD_LET] = "CMD_LET",
    [TOK_CMD_VALUEOF] = "CMD_VALUEOF",
    [TOK_CMD_INC] = "CMD_INC",
    [TOK_CMD_DEC] = "CMD_DEC",
    [TOK_CMD_ASSIGN] = "CMD_ASSIGN",
    [TOK_CMD_REF] = "CMD_REF",
    [TOK_CMD_IF] = "CMD_IF",
    [TOK_CMD_ELSE] = "CMD_ELSE",
    [TOK_CMD_BEGIN] = "CMD_BEGIN",
    [TOK_CMD_END] = "CMD_END",
    [TOK_CMD_EXIT_WHEN] = "CMD_EXIT_WHEN",
    [TOK_CMD_LAMBDA] = "CMD_LAMBDA",
    [TOK_CMD_RECALL] = "CMD_RECALL",
    [TOK_CMD_RETURN] = "CMD_RETURN",
    [TOK_CMD_LEN] = "CMD_LEN",
    [TOK_CMD_PUSH] = "CMD_PUSH",
    [TOK_CMD_POP] = "CMD_POP",
    [TOK_CMD_PEEK] = "CMD_PEEK",
    [TOK_CMD_SETELEMENT] = "CMD_SETELEMENT",
    [TOK_CMD_ENQUEUE] = "CMD_ENQUEUE",
    [TOK_CMD_DEQUEUE] = "CMD_DEQUEUE",
    [TOK_ENV_LOOP] = "ENV_LOOP",
    [TOK_ENV_ENUMERATE] = "ENV_ENUMERATE",
    [TOK_BACKSLASH] = "BACKSLASH",
    [TOK_UNKNOWN_CMD] = "UNKNOWN_CMD",
    [TOK_TEXT] = "TEXT",
    [TOK_NEWLINE] = "NEWLINE",
    [TOK_EOF] = "EOF",
    [TOK_ERROR] = "ERROR",
};

// ============================================================================
// Token Utilities
// ============================================================================

const char *token_type_name(TokenType type) {
    if (type >= 0 && type < TOK_COUNT) {
        return token_type_names[type];
    }
    return "UNKNOWN";
}

void token_print(const Token *tok) {
    fprintf(stderr, "[%d:%d] %s", tok->line, tok->col, token_type_name(tok->type));
    if (tok->type == TOK_INT) {
        fprintf(stderr, "(%d)", tok->int_value);
    } else if (tok->length > 0 && tok->length < 50) {
        fprintf(stderr, "(\"%.*s\")", tok->length, tok->start);
    }
    fprintf(stderr, "\n");
}

Token token_make(TokenType type, const char *start, int length, int line, int col) {
    Token tok = {0};
    tok.type = type;
    tok.start = start;
    tok.length = length;
    tok.line = line;
    tok.col = col;
    return tok;
}

Token token_make_int(const char *start, int length, int line, int col, int32_t value) {
    Token tok = token_make(TOK_INT, start, length, line, col);
    tok.int_value = value;
    return tok;
}

Token token_make_error(const char *message, int line, int col) {
    Token tok = {0};
    tok.type = TOK_ERROR;
    tok.start = message;
    tok.length = (int)strlen(message);
    tok.line = line;
    tok.col = col;
    return tok;
}

// ============================================================================
// Lexer Implementation
// ============================================================================

void lexer_init(Lexer *lex, const char *source) {
    lex->source = source;
    lex->start = source;
    lex->current = source;
    lex->line = 1;
    lex->col = 1;
    lex->start_line = 1;
    lex->start_col = 1;
    lex->has_peeked = false;
    lex->had_error = false;
    lex->error_msg[0] = '\0';
}

void lexer_reset(Lexer *lex) {
    lex->start = lex->source;
    lex->current = lex->source;
    lex->line = 1;
    lex->col = 1;
    lex->has_peeked = false;
}

// Internal: Check if at end of input
static bool is_at_end(Lexer *lex) {
    return *lex->current == '\0';
}

// Internal: Advance and return current character
static char advance(Lexer *lex) {
    char c = *lex->current++;
    if (c == '\n') {
        lex->line++;
        lex->col = 1;
    } else {
        lex->col++;
    }
    return c;
}

// Internal: Peek at current character without advancing
static char peek(Lexer *lex) {
    return *lex->current;
}

// Internal: Peek at next character
static char peek_next(Lexer *lex) {
    if (is_at_end(lex)) return '\0';
    return lex->current[1];
}

// Internal: Match current character and advance if match
static bool match(Lexer *lex, char expected) {
    if (is_at_end(lex)) return false;
    if (*lex->current != expected) return false;
    advance(lex);
    return true;
}

// Internal: Create token from current position
static Token make_token(Lexer *lex, TokenType type) {
    return token_make(type, lex->start, (int)(lex->current - lex->start), lex->start_line,
                      lex->start_col);
}

// Internal: Create error token
static Token error_token(Lexer *lex, const char *message) {
    lex->had_error = true;
    snprintf(lex->error_msg, sizeof(lex->error_msg), "%s", message);
    return token_make_error(message, lex->start_line, lex->start_col);
}

// Internal: Skip whitespace (but not newlines in some contexts)
static void skip_whitespace(Lexer *lex) {
    for (;;) {
        char c = peek(lex);
        switch (c) {
        case ' ':
        case '\t':
        case '\r': advance(lex); break;
        case '\n':
            // Newlines are significant in some contexts
            // For now, skip them as whitespace
            advance(lex);
            break;
        default: return;
        }
    }
}

// Internal: Scan an integer literal
static Token scan_integer(Lexer *lex) {
    // Handle negative sign
    bool negative = false;
    if (peek(lex) == '-') {
        negative = true;
        advance(lex);
    }

    // Scan digits
    while (isdigit((unsigned char)peek(lex))) {
        advance(lex);
    }

    // Parse value
    int32_t value = 0;
    const char *p = lex->start;
    if (*p == '-') p++;
    while (p < lex->current) {
        value = value * 10 + (*p - '0');
        p++;
    }
    if (negative) value = -value;

    Token tok = make_token(lex, TOK_INT);
    tok.int_value = value;
    return tok;
}

// Internal: Lookup command in table
TokenType lexer_lookup_command(const char *name, int length) {
    for (int i = 0; i < command_table_size; i++) {
        if (command_table[i].length == length &&
            strncmp(command_table[i].name, name, length) == 0) {
            return command_table[i].type;
        }
    }
    return TOK_UNKNOWN_CMD;
}

// Internal: Scan a backslash command
static Token scan_command(Lexer *lex) {
    // Skip the backslash
    advance(lex);

    // Check for escaped characters
    char c = peek(lex);
    if (c == '{' || c == '}' || c == '%' || c == '\\' || c == '#' || c == '_' || c == '&') {
        advance(lex);
        return make_token(lex, TOK_TEXT);
    }

    // Scan command name (alphanumeric + underscore)
    const char *name_start = lex->current;
    while (isalnum((unsigned char)peek(lex)) || peek(lex) == '_') {
        advance(lex);
    }

    int name_len = (int)(lex->current - name_start);
    if (name_len == 0) {
        // Bare backslash or unrecognized
        return make_token(lex, TOK_BACKSLASH);
    }

    // Lookup command
    TokenType type = lexer_lookup_command(name_start, name_len);

    // For UNKNOWN_CMD, store the name
    Token tok = make_token(lex, type);
    if (type == TOK_UNKNOWN_CMD) {
        tok.cmd.name = name_start;
        tok.cmd.name_len = name_len;
    }

    return tok;
}

// Internal: Scan text until a command or delimiter
static Token scan_text(Lexer *lex) {
    while (!is_at_end(lex)) {
        char c = peek(lex);

        // Stop at delimiters or commands
        if (c == '\\' || c == '{' || c == '}' || c == '[' || c == ']' || c == '<' || c == '>' ||
            c == ',' || c == '#' || c == '%') {
            break;
        }

        advance(lex);
    }

    if (lex->current == lex->start) {
        // No text consumed - shouldn't happen
        return error_token(lex, "Internal lexer error");
    }

    return make_token(lex, TOK_TEXT);
}

// Main scanning function
static Token scan_token(Lexer *lex) {
    skip_whitespace(lex);

    lex->start = lex->current;
    lex->start_line = lex->line;
    lex->start_col = lex->col;

    if (is_at_end(lex)) {
        return make_token(lex, TOK_EOF);
    }

    char c = peek(lex);

    // Single-character tokens
    switch (c) {
    case '{': advance(lex); return make_token(lex, TOK_LBRACE);
    case '}': advance(lex); return make_token(lex, TOK_RBRACE);
    case '[': advance(lex); return make_token(lex, TOK_LBRACKET);
    case ']': advance(lex); return make_token(lex, TOK_RBRACKET);
    case '<': advance(lex); return make_token(lex, TOK_LANGLE);
    case '>': advance(lex); return make_token(lex, TOK_RANGLE);
    case ',': advance(lex); return make_token(lex, TOK_COMMA);
    case '#': advance(lex); return make_token(lex, TOK_HASH);
    case '%': advance(lex); return make_token(lex, TOK_PERCENT);
    }

    // Backslash commands
    if (c == '\\') {
        return scan_command(lex);
    }

    // Numbers (including negative)
    if (isdigit((unsigned char)c) || (c == '-' && isdigit((unsigned char)peek_next(lex)))) {
        return scan_integer(lex);
    }

    // Text (anything else)
    return scan_text(lex);
}

Token lexer_next(Lexer *lex) {
    if (lex->has_peeked) {
        lex->has_peeked = false;
        return lex->peeked;
    }
    return scan_token(lex);
}

Token lexer_peek(Lexer *lex) {
    if (!lex->has_peeked) {
        lex->peeked = scan_token(lex);
        lex->has_peeked = true;
    }
    return lex->peeked;
}

bool lexer_check(Lexer *lex, TokenType type) {
    return lexer_peek(lex).type == type;
}

bool lexer_match(Lexer *lex, TokenType type) {
    if (lexer_check(lex, type)) {
        lexer_next(lex);
        return true;
    }
    return false;
}

Token lexer_expect(Lexer *lex, TokenType expected, const char *context) {
    Token tok = lexer_next(lex);
    if (tok.type != expected) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Expected %s in %s, got %s", token_type_name(expected), context,
                 token_type_name(tok.type));
        return error_token(lex, msg);
    }
    return tok;
}

const char *lexer_current_pos(Lexer *lex) {
    return lex->current;
}

int lexer_current_line(Lexer *lex) {
    return lex->line;
}

int lexer_current_col(Lexer *lex) {
    return lex->col;
}

bool lexer_had_error(Lexer *lex) {
    return lex->had_error;
}

const char *lexer_error_msg(Lexer *lex) {
    return lex->error_msg;
}

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

static bool isident_start(int c) {
    return isalpha(c) || c == '\\';
}
static bool isident(int c) {
    return isalnum(c);
}

Lexer lex_make(const char *src) {
    Lexer lx = {.src = src, .pos = 0, .line = 1, .col = 1, .last_line = 1, .last_col = 1};
    return lx;
}

void lex_get_position(const Lexer *lx, int *line, int *col) {
    if (line) *line = lx->line;
    if (col) *col = lx->col;
}

static int peek(Lexer *lx) {
    return lx->src[lx->pos];
}

static int get(Lexer *lx) {
    int c = lx->src[lx->pos];
    if (c) {
        lx->pos++;
        if (c == '\n') {
            lx->line++;
            lx->col = 1;
        } else {
            lx->col++;
        }
    }
    return c;
}

static void skip_ws(Lexer *lx) {
    for (;;) {
        int c = peek(lx);

        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            get(lx);
            continue;
        }

        // Skip comments: % to end of line
        if (c == '%') {
            // Consume the % and everything until newline
            while (peek(lx) && peek(lx) != '\n') {
                get(lx);
            }
            // Also consume the newline (TeX behavior)
            if (peek(lx) == '\n') {
                get(lx);
            }
            continue; // Check for more whitespace/comments
        }

        break;
    }
}

Token lex_next(Lexer *lx) {
    skip_ws(lx);

    // Save position before consuming token
    lx->last_line = lx->line;
    lx->last_col = lx->col;

    Token t = {.kind = TOK_EOF,
               .text = "",
               .line = lx->line,
               .col = lx->col,
               .end_line = lx->line,
               .end_col = lx->col};

    int c = peek(lx);
    if (c == 0) {
        t.kind = TOK_EOF;
        return t;
    }

    // Single-character symbol tokens (math punctuation/operators)
    if (strchr("{}^_()+-=*/[]|", c)) {
        t.kind = TOK_SYM;
        t.text[0] = (char)c;
        t.text[1] = 0;
        get(lx);
        t.end_line = lx->line;
        t.end_col = lx->col;
        return t;
    }

    // Dollar sign ('$'): kept as a symbol for the document scanner (though we don't lex it for math normally)
    if (c == '$') {
        t.kind = TOK_SYM;
        t.text[0] = '$';
        t.text[1] = 0;
        get(lx);
        t.end_line = lx->line;
        t.end_col = lx->col;
        return t;
    }

    // Numbers: sequence of digits, optionally with decimal point
    if (isdigit(c)) {
        int i = 0;
        // Read integer part
        while (isdigit(peek(lx)) && i < (int)sizeof(t.text) - 1) {
            t.text[i++] = (char)get(lx);
        }
        // Check for decimal point followed by digits
        if (peek(lx) == '.') {
            // Peek ahead to see if there's a digit after the dot
            // (to distinguish 3.14 from 3. at end of sentence)
            int saved_pos = lx->pos;
            int saved_line = lx->line;
            int saved_col = lx->col;
            get(lx); // consume '.'
            if (isdigit(peek(lx))) {
                // It's a decimal number
                t.text[i++] = '.';
                while (isdigit(peek(lx)) && i < (int)sizeof(t.text) - 1) {
                    t.text[i++] = (char)get(lx);
                }
            } else {
                // Just a trailing dot, put it back
                lx->pos = saved_pos;
                lx->line = saved_line;
                lx->col = saved_col;
            }
        }
        t.text[i] = 0;
        t.kind = TOK_IDENT;
        t.end_line = lx->line;
        t.end_col = lx->col;
        return t;
    }

    // Identifiers / TeX control sequences
    if (c == '\\') {
        int i = 0;
        t.text[i++] = (char)get(lx); // backslash

        int c2 = peek(lx);
        if (isalpha(c2)) {
            // Control word: \frac, \sum, \big, \Bigg, \lbrace, \rbrace, ...
            while (isalpha(peek(lx)) && i < (int)sizeof(t.text) - 1) {
                t.text[i++] = (char)get(lx);
            }
            t.text[i] = 0;
            t.kind = TOK_IDENT;
            t.end_line = lx->line;
            t.end_col = lx->col;
            return t;
        } else if (c2 != 0) {
            // Control symbol: \{ \} \( \) \[ \] \_ \^ etc. Take exactly one char after backslash.
            t.text[i++] = (char)get(lx); // consume that single char
            t.text[i] = 0;
            t.kind = TOK_IDENT;
            t.end_line = lx->line;
            t.end_col = lx->col;
            return t;
        } else {
            // Lone backslash at end → treat as identifier '\'
            t.text[i] = 0;
            t.kind = TOK_IDENT;
            t.end_line = lx->line;
            t.end_col = lx->col;
            return t;
        }
    }

    // Alphabetic identifiers (variables like x, abc) — read a-zA-Z0-9*
    if (isalpha(c)) {
        int i = 0;
        while (isalnum(peek(lx)) && i < (int)sizeof(t.text) - 1) {
            t.text[i++] = (char)get(lx);
        }
        t.text[i] = 0;
        t.kind = TOK_IDENT;
        t.end_line = lx->line;
        t.end_col = lx->col;
        return t;
    }

    // Fallback unknown character as a symbol (handles multi-byte UTF-8)
    t.kind = TOK_SYM;
    {
        unsigned char lead = (unsigned char)get(lx);
        int i = 0;
        t.text[i++] = (char)lead;
        // Determine UTF-8 continuation bytes from lead byte
        int extra = 0;
        if (lead >= 0xC0 && lead < 0xE0)
            extra = 1;
        else if (lead >= 0xE0 && lead < 0xF0)
            extra = 2;
        else if (lead >= 0xF0 && lead < 0xF8)
            extra = 3;
        for (int j = 0; j < extra && i < (int)sizeof(t.text) - 1; j++) {
            unsigned char cont = (unsigned char)peek(lx);
            if ((cont & 0xC0) != 0x80) break; // not a continuation byte
            t.text[i++] = (char)get(lx);
        }
        t.text[i] = 0;
    }
    t.end_line = lx->line;
    t.end_col = lx->col;
    return t;
}

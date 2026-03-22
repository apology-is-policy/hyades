// External scanner for Cassilda/Hyades tree-sitter grammar
// Handles verbatim content where we need to match until the same delimiter

#include "tree_sitter/parser.h"
#include <string.h>

enum TokenType {
    VERBATIM_DELIMITED, // Matches: delimiter + content + delimiter (e.g., |content|)
};

void *tree_sitter_cassilda_external_scanner_create(void) {
    return NULL; // No state needed
}

void tree_sitter_cassilda_external_scanner_destroy(void *payload) {
    // Nothing to free
}

unsigned tree_sitter_cassilda_external_scanner_serialize(void *payload, char *buffer) {
    return 0; // No state to serialize
}

void tree_sitter_cassilda_external_scanner_deserialize(void *payload, const char *buffer,
                                                       unsigned length) {
    // No state to deserialize
}

// Check if a character is a valid verbatim delimiter
// Exclude characters that are part of the grammar syntax
static bool is_valid_delimiter(int32_t c) {
    // Common verbatim delimiters: | ! @ # + = ~ ` ' " and digits
    // Exclude: letters, whitespace, and core grammar syntax chars
    if (c >= 'a' && c <= 'z') return false;
    if (c >= 'A' && c <= 'Z') return false;
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') return false;
    // Exclude grammar syntax characters that would be ambiguous
    if (c == '{' || c == '}') return false; // Brace groups
    if (c == '[' || c == ']') return false; // Optional args
    if (c == '<' || c == '>') return false; // Angle groups
    if (c == '$') return false;             // Math mode
    if (c == '%') return false;             // Comments
    if (c == '\\') return false;            // Commands
    if (c == 0) return false;               // EOF
    // Allow: digits (0-9), @ # as delimiters (common in verbatim)
    return true;
}

bool tree_sitter_cassilda_external_scanner_scan(void *payload, TSLexer *lexer,
                                                const bool *valid_symbols) {
    (void)payload;

    if (!valid_symbols[VERBATIM_DELIMITED]) {
        return false;
    }

    // We're positioned right after \verb
    // The delimiter should be the next character (no whitespace allowed in standard \verb)
    if (lexer->eof(lexer)) {
        return false;
    }

    // First character is the delimiter - must be a valid delimiter char
    int32_t delimiter = lexer->lookahead;
    if (!is_valid_delimiter(delimiter)) {
        return false; // Not a valid delimiter, don't match
    }

    lexer->advance(lexer, false); // Include delimiter in the token
    lexer->mark_end(lexer);       // Mark after opening delimiter

    // Now consume until we find the closing delimiter
    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == delimiter) {
            // Found closing delimiter - include it in the token
            lexer->advance(lexer, false);
            lexer->mark_end(lexer);
            lexer->result_symbol = VERBATIM_DELIMITED;
            return true;
        }
        // Allow newlines in verbatim content
        lexer->advance(lexer, false);
    }

    // EOF without closing delimiter - don't match (would cause errors)
    return false;
}

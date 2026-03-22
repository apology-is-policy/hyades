// hyades_parse_api.h - Unified parse API for LSP integration
//
// This API provides a single entry point for parsing Hyades documents with
// full error reporting, symbol tracking, and source mapping for LSP features.

#pragma once

#include <stdbool.h>

#include "../document/source_map.h"
#include "../document/symbol_table.h"
#include "../layout/layout_types.h"
#include "../utils/error.h"
#include "../utils/parse_options.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Parse Result
// ============================================================================

typedef struct {
    // Parse tree (the rendered document layout)
    BoxLayout *layout;

    // Errors collected during parsing
    ParseErrorList *errors;

    // Symbol definitions (for go-to-definition, autocomplete)
    LspSymbolTable *symbols;

    // Source mapping (transformed position → original position)
    SourceMap *source_map;

    // Parse options used
    HyadesParseOptions options;

    // Parse statistics
    struct {
        int total_lines;      // Lines in source
        int total_chars;      // Characters in source
        int parse_time_ms;    // Time to parse (milliseconds)
        int transform_passes; // Number of expansion passes
    } stats;

    // Internal state (for incremental re-parsing)
    void *_internal;
} HyadesParseResult;

// ============================================================================
// Main Parse API
// ============================================================================

// Parse a document with default options (strict, all features)
HyadesParseResult *hyades_parse(const char *source);

// Parse with custom options
HyadesParseResult *hyades_parse_with_options(const char *source, const HyadesParseOptions *opts);

// Parse for LSP (optimized for IDE features)
HyadesParseResult *hyades_parse_for_lsp(const char *source);

// Free a parse result
void hyades_parse_result_free(HyadesParseResult *result);

// ============================================================================
// Error Access
// ============================================================================

// Check if parsing produced any errors
bool hyades_has_errors(const HyadesParseResult *result);

// Get number of errors
int hyades_error_count(const HyadesParseResult *result);

// Get number of warnings
int hyades_warning_count(const HyadesParseResult *result);

// Get error at index (0-based)
const ParseError *hyades_error_at(const HyadesParseResult *result, int index);

// Get all errors as JSON string (caller must free)
char *hyades_errors_to_json(const HyadesParseResult *result);

// ============================================================================
// Symbol Access (for LSP features)
// ============================================================================

// Get number of symbols
int hyades_symbol_count(const HyadesParseResult *result);

// Get symbol at index
const Symbol *hyades_symbol_at(const HyadesParseResult *result, int index);

// Find symbol by name
const Symbol *hyades_symbol_find(const HyadesParseResult *result, const char *name);

// Find symbol at position (for go-to-definition)
const Symbol *hyades_symbol_at_position(const HyadesParseResult *result, int line, int col);

// Find definition for symbol at position
const Symbol *hyades_definition_at_position(const HyadesParseResult *result, int line, int col);

// Get all symbols as JSON string (caller must free)
char *hyades_symbols_to_json(const HyadesParseResult *result);

// ============================================================================
// Completion Support
// ============================================================================

// Completion item for autocomplete
typedef struct {
    const char *label;         // Display text
    const char *insert_text;   // Text to insert
    const char *detail;        // Type/kind info
    const char *documentation; // Full documentation
    int kind;                  // LSP CompletionItemKind
    bool is_snippet;           // Uses snippet syntax
} HyadesCompletion;

// Get completions at position
// Returns number of completions, fills out_completions array
int hyades_get_completions(const HyadesParseResult *result, int line, int col,
                           HyadesCompletion *out_completions, int max_completions);

// Get completions as JSON string (caller must free)
char *hyades_completions_to_json(const HyadesParseResult *result, int line, int col);

// ============================================================================
// Position Mapping
// ============================================================================

// Map a position in transformed text back to original source
bool hyades_map_position(const HyadesParseResult *result, int trans_line, int trans_col,
                         int *orig_line, int *orig_col);

// Map a position from original source to transformed text
bool hyades_map_position_reverse(const HyadesParseResult *result, int orig_line, int orig_col,
                                 int *trans_line, int *trans_col);

// ============================================================================
// Hover Information
// ============================================================================

// Hover result for showing info at position
typedef struct {
    const char *contents; // Markdown content to display
    int start_line;       // Highlight range start
    int start_col;
    int end_line;
    int end_col;
} HyadesHoverResult;

// Get hover information at position
// Returns NULL if no hover info available
HyadesHoverResult *hyades_get_hover(const HyadesParseResult *result, int line, int col);

// Free hover result
void hyades_hover_free(HyadesHoverResult *hover);

// Get hover as JSON string (caller must free)
char *hyades_hover_to_json(const HyadesParseResult *result, int line, int col);

// ============================================================================
// References
// ============================================================================

// Find all references to symbol at position
int hyades_find_references(const HyadesParseResult *result, int line, int col,
                           const SymbolReference **out_refs);

// Get references as JSON string (caller must free)
char *hyades_references_to_json(const HyadesParseResult *result, int line, int col);

// ============================================================================
// Document Symbols (for outline)
// ============================================================================

// Get document symbols as JSON string (for VSCode outline)
char *hyades_document_symbols_to_json(const HyadesParseResult *result);

// ============================================================================
// Semantic Tokens (for syntax highlighting)
// ============================================================================

// Semantic token types (must match LSP legend)
typedef enum {
    SEM_TOK_NAMESPACE = 0, // STD:: prefix
    SEM_TOK_TYPE = 1,
    SEM_TOK_CLASS = 2,
    SEM_TOK_ENUM = 3,
    SEM_TOK_INTERFACE = 4,
    SEM_TOK_STRUCT = 5,
    SEM_TOK_TYPE_PARAM = 6,
    SEM_TOK_PARAMETER = 7, // Macro parameters ${name}
    SEM_TOK_VARIABLE = 8,  // References, counters
    SEM_TOK_PROPERTY = 9,
    SEM_TOK_ENUM_MEMBER = 10,
    SEM_TOK_EVENT = 11,
    SEM_TOK_FUNCTION = 12, // Built-in commands (\frac, \sum)
    SEM_TOK_METHOD = 13,
    SEM_TOK_MACRO = 14,   // User-defined macros
    SEM_TOK_KEYWORD = 15, // @label, @end, \begin, \end
    SEM_TOK_MODIFIER = 16,
    SEM_TOK_COMMENT = 17, // % comments
    SEM_TOK_STRING = 18,  // Verbatim content
    SEM_TOK_NUMBER = 19,  // Numeric literals
    SEM_TOK_REGEXP = 20,
    SEM_TOK_OPERATOR = 21, // Math operators
} SemanticTokenType;

// Token modifiers (bit flags)
typedef enum {
    SEM_MOD_DECLARATION = 1 << 0,
    SEM_MOD_DEFINITION = 1 << 1,
    SEM_MOD_READONLY = 1 << 2,
    SEM_MOD_STATIC = 1 << 3,
    SEM_MOD_DEPRECATED = 1 << 4,
    SEM_MOD_ABSTRACT = 1 << 5,
    SEM_MOD_ASYNC = 1 << 6,
    SEM_MOD_MODIFICATION = 1 << 7,
    SEM_MOD_DOCUMENTATION = 1 << 8,
    SEM_MOD_DEFAULT_LIBRARY = 1 << 9,
} SemanticTokenModifier;

// Get semantic tokens as JSON with delta-encoded data array
// Returns: { "data": [deltaLine, deltaChar, length, type, modifiers, ...] }
char *hyades_semantic_tokens_to_json(const HyadesParseResult *result, const char *source);

// Get semantic tokens as raw int array (no JSON overhead)
// Returns count of ints (multiple of 5). Caller must free *out_data.
int hyades_semantic_tokens_to_raw(const HyadesParseResult *result, const char *source,
                                  int **out_data);

// ============================================================================
// Validation
// ============================================================================

// Run additional validation passes on an existing parse result
// Useful for running slow validations on-demand
void hyades_validate(HyadesParseResult *result);

// Validate specific aspects
void hyades_validate_references(HyadesParseResult *result);
void hyades_validate_delimiters(HyadesParseResult *result);
void hyades_validate_arity(HyadesParseResult *result);

#ifdef __cplusplus
}
#endif

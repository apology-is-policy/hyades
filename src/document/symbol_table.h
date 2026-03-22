// symbol_table.h - Track macro, lambda, and other definitions for LSP
#pragma once

#ifndef SYMBOL_TABLE_H_INCLUDED
#define SYMBOL_TABLE_H_INCLUDED

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Symbol Types
// ============================================================================

typedef enum {
    SYMKIND_MACRO,      // User-defined macro (\macro<\name...>)
    SYMKIND_LAMBDA,     // Lambda function (\lambda<name>)
    SYMKIND_COUNTER,    // Counter (\let<name>{n})
    SYMKIND_ARRAY,      // Array (\let<name[]>)
    SYMKIND_VALUE,      // Stored value (\assign<name>)
    SYMKIND_LABEL,      // Cassilda label (@label name)
    SYMKIND_ENVIRONMENT // Environment definition
} SymbolKind;

// ============================================================================
// Symbol Definition
// ============================================================================

typedef struct {
    char *name; // Symbol name (without backslash for macros)
    SymbolKind kind;

    // Definition location
    int def_line;     // 1-based line of definition
    int def_col;      // 1-based column of definition
    int def_end_line; // End of definition
    int def_end_col;

    // Definition metadata
    char *signature;    // Parameter signature (e.g., "[x, y]" for lambda)
    char *doc_comment;  // Documentation comment (if preceded by %%)
    char *body_preview; // First ~100 chars of body

    // For macros
    int n_required_args; // Number of required arguments
    int n_optional_args; // Number of optional arguments
    char **param_names;  // Parameter names (for hover/completion)
    int n_params;

    // For tracking references
    int n_references; // Count of references to this symbol
} Symbol;

// ============================================================================
// Symbol Reference
// ============================================================================

typedef struct {
    char *name; // Symbol name
    int line;   // 1-based line of reference
    int col;    // 1-based column of reference
    int end_line;
    int end_col;
    bool is_definition; // true if this is the definition site
} SymbolReference;

// ============================================================================
// Symbol Table
// ============================================================================

typedef struct LspSymbolTable {
    Symbol *symbols;
    int n_symbols;
    int capacity;

    SymbolReference *references;
    int n_references;
    int ref_capacity;
} LspSymbolTable;

// ============================================================================
// Lifecycle
// ============================================================================

// Create a new symbol table
LspSymbolTable *lsp_symbol_table_new(void);

// Free a symbol table
void lsp_symbol_table_free(LspSymbolTable *st);

// ============================================================================
// Adding Symbols
// ============================================================================

// Add a symbol definition
// Returns the added symbol, or NULL on failure
Symbol *lsp_symbol_table_add(LspSymbolTable *st, const char *name, SymbolKind kind, int line,
                             int col, int end_line, int end_col);

// Set symbol signature (e.g., parameter list)
void symbol_set_signature(Symbol *sym, const char *signature);

// Set documentation comment
void symbol_set_doc(Symbol *sym, const char *doc);

// Set body preview
void symbol_set_body_preview(Symbol *sym, const char *body);

// Add a parameter name to a macro/lambda
void symbol_add_param(Symbol *sym, const char *param_name);

// ============================================================================
// Adding References
// ============================================================================

// Add a reference to a symbol
void lsp_symbol_table_add_reference(LspSymbolTable *st, const char *name, int line, int col,
                                    int end_line, int end_col, bool is_definition);

// ============================================================================
// Querying
// ============================================================================

// Find a symbol by name
Symbol *lsp_symbol_table_find(LspSymbolTable *st, const char *name);

// Find symbol at a given position (for go-to-definition)
Symbol *lsp_symbol_table_at_position(LspSymbolTable *st, int line, int col);

// Get all symbols (for autocomplete)
int lsp_symbol_table_count(const LspSymbolTable *st);
const Symbol *lsp_symbol_table_get(const LspSymbolTable *st, int index);

// Get symbols of a specific kind
int lsp_symbol_table_count_kind(const LspSymbolTable *st, SymbolKind kind);

// Get all references to a symbol
int lsp_symbol_table_find_references(const LspSymbolTable *st, const char *name,
                                     const SymbolReference **out_refs);

// Find reference at position (for go-to-definition from reference)
const SymbolReference *lsp_symbol_table_reference_at_position(const LspSymbolTable *st, int line,
                                                              int col);

// ============================================================================
// Utility
// ============================================================================

// Get symbol kind name as string
const char *symbol_kind_name(SymbolKind kind);

// Get LSP symbol kind code
int symbol_kind_to_lsp(SymbolKind kind);

// Check if a name is a valid symbol name
bool symbol_is_valid_name(const char *name);

// Get completion label for a symbol (name + signature)
const char *symbol_completion_label(const Symbol *sym);

// Get detail string for a symbol (kind + type info)
const char *symbol_detail(const Symbol *sym);

#ifdef __cplusplus
}
#endif

#endif // SYMBOL_TABLE_H_INCLUDED

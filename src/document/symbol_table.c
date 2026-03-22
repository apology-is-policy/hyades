// symbol_table.c - Implementation of symbol table for LSP
#include "symbol_table.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 64
#define INITIAL_REF_CAPACITY 256

// ============================================================================
// Lifecycle
// ============================================================================

LspSymbolTable *lsp_symbol_table_new(void) {
    LspSymbolTable *st = calloc(1, sizeof(LspSymbolTable));
    if (!st) return NULL;

    st->symbols = calloc(INITIAL_CAPACITY, sizeof(Symbol));
    if (!st->symbols) {
        free(st);
        return NULL;
    }

    st->references = calloc(INITIAL_REF_CAPACITY, sizeof(SymbolReference));
    if (!st->references) {
        free(st->symbols);
        free(st);
        return NULL;
    }

    st->n_symbols = 0;
    st->capacity = INITIAL_CAPACITY;
    st->n_references = 0;
    st->ref_capacity = INITIAL_REF_CAPACITY;

    return st;
}

static void free_symbol(Symbol *sym) {
    if (!sym) return;
    free(sym->name);
    free(sym->signature);
    free(sym->doc_comment);
    free(sym->body_preview);
    for (int i = 0; i < sym->n_params; i++) {
        free(sym->param_names[i]);
    }
    free(sym->param_names);
}

static void free_reference(SymbolReference *ref) {
    if (!ref) return;
    free(ref->name);
}

void lsp_symbol_table_free(LspSymbolTable *st) {
    if (!st) return;

    for (int i = 0; i < st->n_symbols; i++) {
        free_symbol(&st->symbols[i]);
    }
    free(st->symbols);

    for (int i = 0; i < st->n_references; i++) {
        free_reference(&st->references[i]);
    }
    free(st->references);

    free(st);
}

// ============================================================================
// Adding Symbols
// ============================================================================

static int ensure_symbol_capacity(LspSymbolTable *st) {
    if (st->n_symbols < st->capacity) return 1;

    int old_cap = st->capacity;
    int new_cap = st->capacity * 2;
    Symbol *new_symbols = realloc(st->symbols, new_cap * sizeof(Symbol));
    if (!new_symbols) return 0;

    // Zero the new portion
    memset(new_symbols + old_cap, 0, (new_cap - old_cap) * sizeof(Symbol));

    st->symbols = new_symbols;
    st->capacity = new_cap;
    return 1;
}

Symbol *lsp_symbol_table_add(LspSymbolTable *st, const char *name, SymbolKind kind, int line,
                             int col, int end_line, int end_col) {
    if (!st || !name || !ensure_symbol_capacity(st)) return NULL;

    Symbol *sym = &st->symbols[st->n_symbols++];
    memset(sym, 0, sizeof(Symbol));

    sym->name = strdup(name);
    sym->kind = kind;
    sym->def_line = line;
    sym->def_col = col;
    sym->def_end_line = end_line > 0 ? end_line : line;
    sym->def_end_col = end_col > 0 ? end_col : col;

    // Note: We don't add a reference for definitions here.
    // References are for tracking USAGES of symbols, not definitions.
    // Hover at definitions works via lsp_symbol_table_at_position.

    return sym;
}

void symbol_set_signature(Symbol *sym, const char *signature) {
    if (!sym) return;
    free(sym->signature);
    sym->signature = signature ? strdup(signature) : NULL;
}

void symbol_set_doc(Symbol *sym, const char *doc) {
    if (!sym) return;
    free(sym->doc_comment);
    sym->doc_comment = doc ? strdup(doc) : NULL;
}

void symbol_set_body_preview(Symbol *sym, const char *body) {
    if (!sym || !body) return;
    free(sym->body_preview);

    // Limit to ~100 chars
    int len = (int)strlen(body);
    if (len > 100) len = 100;

    sym->body_preview = malloc(len + 4);
    if (!sym->body_preview) return;

    strncpy(sym->body_preview, body, len);
    if (strlen(body) > 100) {
        strcpy(sym->body_preview + len, "...");
    } else {
        sym->body_preview[len] = '\0';
    }
}

void symbol_add_param(Symbol *sym, const char *param_name) {
    if (!sym || !param_name) return;

    // Reallocate param_names array
    char **new_params = realloc(sym->param_names, (sym->n_params + 1) * sizeof(char *));
    if (!new_params) return;

    sym->param_names = new_params;
    sym->param_names[sym->n_params++] = strdup(param_name);
}

// ============================================================================
// Adding References
// ============================================================================

static int ensure_ref_capacity(LspSymbolTable *st) {
    if (st->n_references < st->ref_capacity) return 1;

    int old_cap = st->ref_capacity;
    int new_cap = st->ref_capacity * 2;
    SymbolReference *new_refs = realloc(st->references, new_cap * sizeof(SymbolReference));
    if (!new_refs) return 0;

    // Zero the new portion
    memset(new_refs + old_cap, 0, (new_cap - old_cap) * sizeof(SymbolReference));

    st->references = new_refs;
    st->ref_capacity = new_cap;
    return 1;
}

void lsp_symbol_table_add_reference(LspSymbolTable *st, const char *name, int line, int col,
                                    int end_line, int end_col, bool is_definition) {
    if (!st || !name || !ensure_ref_capacity(st)) return;

    SymbolReference *ref = &st->references[st->n_references++];
    memset(ref, 0, sizeof(SymbolReference));
    ref->name = strdup(name);
    ref->line = line;
    ref->col = col;
    ref->end_line = end_line > 0 ? end_line : line;
    ref->end_col = end_col > 0 ? end_col : col;
    ref->is_definition = is_definition;

    // Update reference count on symbol
    if (!is_definition) {
        Symbol *sym = lsp_symbol_table_find(st, name);
        if (sym) sym->n_references++;
    }
}

// ============================================================================
// Querying
// ============================================================================

Symbol *lsp_symbol_table_find(LspSymbolTable *st, const char *name) {
    if (!st || !name) return NULL;

    for (int i = 0; i < st->n_symbols; i++) {
        if (strcmp(st->symbols[i].name, name) == 0) {
            return &st->symbols[i];
        }
    }
    return NULL;
}

Symbol *lsp_symbol_table_at_position(LspSymbolTable *st, int line, int col) {
    if (!st) return NULL;

    for (int i = 0; i < st->n_symbols; i++) {
        Symbol *sym = &st->symbols[i];

        // Skip stdlib/built-in symbols with default positions (0:0 or 1:1)
        // These have no meaningful position in the user's file
        if ((sym->def_line == 0 && sym->def_col == 0) ||
            (sym->def_line == 1 && sym->def_col == 1 && sym->def_end_line == 1 &&
             sym->def_end_col == 1)) {
            continue;
        }

        // Check if position is within symbol definition
        if (line < sym->def_line || line > sym->def_end_line) continue;

        if (line == sym->def_line && col < sym->def_col) continue;
        if (line == sym->def_end_line && col > sym->def_end_col) continue;

        return sym;
    }
    return NULL;
}

int lsp_symbol_table_count(const LspSymbolTable *st) {
    return st ? st->n_symbols : 0;
}

const Symbol *lsp_symbol_table_get(const LspSymbolTable *st, int index) {
    if (!st || index < 0 || index >= st->n_symbols) return NULL;
    return &st->symbols[index];
}

int lsp_symbol_table_count_kind(const LspSymbolTable *st, SymbolKind kind) {
    if (!st) return 0;

    int count = 0;
    for (int i = 0; i < st->n_symbols; i++) {
        if (st->symbols[i].kind == kind) count++;
    }
    return count;
}

int lsp_symbol_table_find_references(const LspSymbolTable *st, const char *name,
                                     const SymbolReference **out_refs) {
    if (!st || !name) {
        if (out_refs) *out_refs = NULL;
        return 0;
    }

    // Count matching references
    int count = 0;
    for (int i = 0; i < st->n_references; i++) {
        if (strcmp(st->references[i].name, name) == 0) count++;
    }

    if (out_refs && count > 0) {
        // Return pointer to first match (caller iterates)
        for (int i = 0; i < st->n_references; i++) {
            if (strcmp(st->references[i].name, name) == 0) {
                *out_refs = &st->references[i];
                break;
            }
        }
    }

    return count;
}

const SymbolReference *lsp_symbol_table_reference_at_position(const LspSymbolTable *st, int line,
                                                              int col) {
    if (!st) return NULL;

    for (int i = 0; i < st->n_references; i++) {
        SymbolReference *ref = &st->references[i];

        // Skip references with default positions (0:0 or 1:1)
        if ((ref->line == 0 && ref->col == 0) ||
            (ref->line == 1 && ref->col == 1 && ref->end_line == 1 && ref->end_col == 1)) {
            continue;
        }

        if (line < ref->line || line > ref->end_line) continue;
        if (line == ref->line && col < ref->col) continue;
        if (line == ref->end_line && col > ref->end_col) continue;

        return ref;
    }
    return NULL;
}

// ============================================================================
// Utility
// ============================================================================

const char *symbol_kind_name(SymbolKind kind) {
    switch (kind) {
    case SYMKIND_MACRO: return "macro";
    case SYMKIND_LAMBDA: return "lambda";
    case SYMKIND_COUNTER: return "counter";
    case SYMKIND_ARRAY: return "array";
    case SYMKIND_VALUE: return "value";
    case SYMKIND_LABEL: return "label";
    case SYMKIND_ENVIRONMENT: return "environment";
    default: return "unknown";
    }
}

// LSP SymbolKind enum values
int symbol_kind_to_lsp(SymbolKind kind) {
    switch (kind) {
    case SYMKIND_MACRO: return 12;      // Function
    case SYMKIND_LAMBDA: return 12;     // Function
    case SYMKIND_COUNTER: return 13;    // Variable
    case SYMKIND_ARRAY: return 18;      // Array
    case SYMKIND_VALUE: return 13;      // Variable
    case SYMKIND_LABEL: return 14;      // Constant
    case SYMKIND_ENVIRONMENT: return 5; // Class
    default: return 13;                 // Variable
    }
}

bool symbol_is_valid_name(const char *name) {
    if (!name || !name[0]) return false;

    // First char must be letter or backslash
    if (!isalpha(name[0]) && name[0] != '\\') return false;

    // Rest must be alphanumeric
    for (int i = 1; name[i]; i++) {
        if (!isalnum(name[i]) && name[i] != '_') return false;
    }

    return true;
}

const char *symbol_completion_label(const Symbol *sym) {
    static char buffer[256];

    if (!sym) return "";

    if (sym->signature) {
        snprintf(buffer, sizeof(buffer), "%s%s", sym->name, sym->signature);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", sym->name);
    }

    return buffer;
}

const char *symbol_detail(const Symbol *sym) {
    static char buffer[256];

    if (!sym) return "";

    const char *kind = symbol_kind_name(sym->kind);

    if (sym->n_required_args > 0 || sym->n_optional_args > 0) {
        snprintf(buffer, sizeof(buffer), "%s (%d required, %d optional args)", kind,
                 sym->n_required_args, sym->n_optional_args);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", kind);
    }

    return buffer;
}

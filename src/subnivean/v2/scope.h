// scope.h - Subnivean 2.0 Scope/Environment System
//
// Scopes are first-class values that can be captured by closures.
// This enables lexical scoping with capture-by-reference semantics.
//
// Key design: Scopes are reference-counted. When a closure captures a scope,
// it increments the refcount. The scope stays alive as long as any closure
// references it. This solves the "closure in loop" problem elegantly.

#ifndef SUBNIVEAN_SCOPE_H
#define SUBNIVEAN_SCOPE_H

#include "value.h"

// ============================================================================
// Scope Structure
// ============================================================================

// Binding in a scope
typedef struct Binding {
    Symbol *name;         // Interned name (not owned)
    Value value;          // The bound value
    struct Binding *next; // Hash chain
} Binding;

// Scope - a lexical environment
struct Scope {
    Scope *parent; // Enclosing scope (NULL for global)

    // Hash table of bindings
    Binding **buckets;
    size_t n_buckets;
    size_t n_bindings;

    // Reference counting
    int refcount;

    // Debug info
    uint32_t id;     // Unique ID for debugging
    const char *tag; // Optional tag ("global", "loop-iter", etc.)
};

// ============================================================================
// Scope Lifecycle
// ============================================================================

// Create a new scope with given parent
Scope *sn_scope_new(Scope *parent);

// Create global scope (no parent, larger hash table)
Scope *sn_scope_new_global(void);

// Increment reference count
void sn_scope_incref(Scope *s);

// Decrement reference count (frees if zero)
void sn_scope_decref(Scope *s);

// ============================================================================
// Binding Operations
// ============================================================================

// Bind a name in the current scope (does NOT search chain)
// If name already bound in this scope, updates it.
void sn_scope_bind(Scope *s, Symbol *name, Value v);

// Lookup a name in the scope chain
// Returns true if found, sets *out to the value
// Returns false if not found
bool sn_scope_lookup(Scope *s, Symbol *name, Value *out);

// Lookup only in this scope (no chain traversal)
bool sn_scope_lookup_here(Scope *s, Symbol *name, Value *out);

// Update an existing binding in the scope chain
// Returns true if found and updated, false if not found
bool sn_scope_set(Scope *s, Symbol *name, Value v);

// Check if a name is bound in this scope (no chain)
bool sn_scope_has(Scope *s, Symbol *name);

// ============================================================================
// Dynamic Name Operations (for Hyades \lambda<f\valueof<i>>)
// ============================================================================

// Bind with a string name (converted to symbol)
void sn_scope_bind_dynamic(Scope *s, SymbolTable *st, String *name, Value v);

// Lookup with a string name
bool sn_scope_lookup_dynamic(Scope *s, SymbolTable *st, String *name, Value *out);

// Set with a string name
bool sn_scope_set_dynamic(Scope *s, SymbolTable *st, String *name, Value v);

// ============================================================================
// Value Integration
// ============================================================================

// Now we can define value_scope
static inline Value value_scope(Scope *sc) {
    sn_scope_incref(sc);
    return (Value){.kind = VAL_SCOPE, .as_scope = sc};
}

// ============================================================================
// Debug
// ============================================================================

// Print scope contents (for debugging)
void sn_scope_print(Scope *s, int indent);

// Get scope depth (distance to global)
int sn_scope_depth(Scope *s);

#endif // SUBNIVEAN_SCOPE_H

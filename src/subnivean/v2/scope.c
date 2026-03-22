// scope.c - Subnivean 2.0 Scope Implementation

#include "scope.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Scope ID Counter
// ============================================================================

static uint32_t next_scope_id = 0;

// ============================================================================
// Binding Hash Table Helpers
// ============================================================================

static size_t binding_hash(Symbol *name, size_t n_buckets) {
    return name->hash % n_buckets;
}

static void scope_grow(Scope *s) {
    size_t old_n_buckets = s->n_buckets;
    Binding **old_buckets = s->buckets;

    s->n_buckets *= 2;
    s->buckets = calloc(s->n_buckets, sizeof(Binding *));

    // Rehash all bindings
    for (size_t i = 0; i < old_n_buckets; i++) {
        Binding *b = old_buckets[i];
        while (b) {
            Binding *next = b->next;
            size_t idx = binding_hash(b->name, s->n_buckets);
            b->next = s->buckets[idx];
            s->buckets[idx] = b;
            b = next;
        }
    }

    free(old_buckets);
}

// ============================================================================
// Scope Lifecycle
// ============================================================================

Scope *sn_scope_new(Scope *parent) {
    Scope *s = malloc(sizeof(Scope));
    s->parent = parent;
    if (parent) {
        sn_scope_incref(parent);
    }

    s->n_buckets = 16;
    s->buckets = calloc(s->n_buckets, sizeof(Binding *));
    s->n_bindings = 0;

    s->refcount = 1;
    s->id = next_scope_id++;
    s->tag = NULL;

    return s;
}

Scope *sn_scope_new_global(void) {
    Scope *s = malloc(sizeof(Scope));
    s->parent = NULL;

    s->n_buckets = 256; // Larger for global scope
    s->buckets = calloc(s->n_buckets, sizeof(Binding *));
    s->n_bindings = 0;

    s->refcount = 1;
    s->id = next_scope_id++;
    s->tag = "global";

    return s;
}

void sn_scope_incref(Scope *s) {
    if (s) s->refcount++;
}

void sn_scope_decref(Scope *s) {
    if (!s) return;
    s->refcount--;

    if (s->refcount <= 0) {
        // Free all bindings
        for (size_t i = 0; i < s->n_buckets; i++) {
            Binding *b = s->buckets[i];
            while (b) {
                Binding *next = b->next;
                value_free(b->value);
                free(b);
                b = next;
            }
        }
        free(s->buckets);

        // Decref parent
        if (s->parent) {
            sn_scope_decref(s->parent);
        }

        free(s);
    }
}

// ============================================================================
// Binding Operations
// ============================================================================

void sn_scope_bind(Scope *s, Symbol *name, Value v) {
    size_t idx = binding_hash(name, s->n_buckets);

    // Check if already bound in this scope
    for (Binding *b = s->buckets[idx]; b; b = b->next) {
        if (b->name == name) { // Pointer equality for symbols
            value_free(b->value);
            b->value = v;
            return;
        }
    }

    // Create new binding
    Binding *b = malloc(sizeof(Binding));
    b->name = name;
    b->value = v;
    b->next = s->buckets[idx];
    s->buckets[idx] = b;
    s->n_bindings++;

    // Grow if too full
    if (s->n_bindings > s->n_buckets * 2) {
        scope_grow(s);
    }
}

bool sn_scope_lookup(Scope *s, Symbol *name, Value *out) {
    for (Scope *cur = s; cur != NULL; cur = cur->parent) {
        size_t idx = binding_hash(name, cur->n_buckets);
        for (Binding *b = cur->buckets[idx]; b; b = b->next) {
            if (b->name == name) {
                *out = value_copy(b->value);
                return true;
            }
        }
    }
    return false;
}

bool sn_scope_lookup_here(Scope *s, Symbol *name, Value *out) {
    size_t idx = binding_hash(name, s->n_buckets);
    for (Binding *b = s->buckets[idx]; b; b = b->next) {
        if (b->name == name) {
            *out = value_copy(b->value);
            return true;
        }
    }
    return false;
}

bool sn_scope_set(Scope *s, Symbol *name, Value v) {
    for (Scope *cur = s; cur != NULL; cur = cur->parent) {
        size_t idx = binding_hash(name, cur->n_buckets);
        for (Binding *b = cur->buckets[idx]; b; b = b->next) {
            if (b->name == name) {
                value_free(b->value);
                b->value = v;
                return true;
            }
        }
    }
    return false;
}

bool sn_scope_has(Scope *s, Symbol *name) {
    size_t idx = binding_hash(name, s->n_buckets);
    for (Binding *b = s->buckets[idx]; b; b = b->next) {
        if (b->name == name) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Dynamic Name Operations
// ============================================================================

void sn_scope_bind_dynamic(Scope *s, SymbolTable *st, String *name, Value v) {
    Symbol *sym = symbol_intern(st, name->data, name->len);
    sn_scope_bind(s, sym, v);
}

bool sn_scope_lookup_dynamic(Scope *s, SymbolTable *st, String *name, Value *out) {
    Symbol *sym = symbol_intern(st, name->data, name->len);
    return sn_scope_lookup(s, sym, out);
}

bool sn_scope_set_dynamic(Scope *s, SymbolTable *st, String *name, Value v) {
    Symbol *sym = symbol_intern(st, name->data, name->len);
    return sn_scope_set(s, sym, v);
}

// ============================================================================
// Debug
// ============================================================================

void sn_scope_print(Scope *s, int indent) {
    for (int i = 0; i < indent; i++) fprintf(stderr, "  ");
    fprintf(stderr, "Scope #%u", s->id);
    if (s->tag) fprintf(stderr, " (%s)", s->tag);
    fprintf(stderr, " [refcount=%d, %zu bindings]\n", s->refcount, s->n_bindings);

    for (size_t i = 0; i < s->n_buckets; i++) {
        for (Binding *b = s->buckets[i]; b; b = b->next) {
            for (int j = 0; j < indent + 1; j++) fprintf(stderr, "  ");
            fprintf(stderr, "%s = ", b->name->name);
            value_print(b->value);
            fprintf(stderr, "\n");
        }
    }

    if (s->parent) {
        for (int i = 0; i < indent; i++) fprintf(stderr, "  ");
        fprintf(stderr, "Parent:\n");
        sn_scope_print(s->parent, indent + 1);
    }
}

int sn_scope_depth(Scope *s) {
    int depth = 0;
    while (s->parent) {
        depth++;
        s = s->parent;
    }
    return depth;
}

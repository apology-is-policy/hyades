// value.c - Subnivean 2.0 Value Implementation

#include "value.h"
#include "function.h"
#include "scope.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// String Implementation
// ============================================================================

String *string_new(const char *data, size_t len) {
    String *s = malloc(sizeof(String));
    s->data = malloc(len + 1);
    memcpy(s->data, data, len);
    s->data[len] = '\0';
    s->len = len;
    s->refcount = 1;
    return s;
}

String *string_from_cstr(const char *cstr) {
    return string_new(cstr, strlen(cstr));
}

void string_incref(String *s) {
    if (s) s->refcount++;
}

void string_decref(String *s) {
    if (!s) return;
    s->refcount--;
    if (s->refcount <= 0) {
        free(s->data);
        free(s);
    }
}

String *string_concat(String *a, String *b) {
    size_t len = a->len + b->len;
    String *s = malloc(sizeof(String));
    s->data = malloc(len + 1);
    memcpy(s->data, a->data, a->len);
    memcpy(s->data + a->len, b->data, b->len);
    s->data[len] = '\0';
    s->len = len;
    s->refcount = 1;
    return s;
}

bool string_eq(String *a, String *b) {
    if (a == b) return true;
    if (a->len != b->len) return false;
    return memcmp(a->data, b->data, a->len) == 0;
}

// ============================================================================
// Symbol Table Implementation
// ============================================================================

static uint32_t hash_string(const char *key, size_t len) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

void symbol_table_init(SymbolTable *st) {
    st->n_buckets = 256;
    st->buckets = calloc(st->n_buckets, sizeof(Symbol *));
    st->count = 0;
}

void symbol_table_free(SymbolTable *st) {
    for (size_t i = 0; i < st->n_buckets; i++) {
        Symbol *sym = st->buckets[i];
        while (sym) {
            Symbol *next = sym->next;
            free((void *)sym->name);
            free(sym);
            sym = next;
        }
    }
    free(st->buckets);
}

Symbol *symbol_intern(SymbolTable *st, const char *name, size_t len) {
    uint32_t hash = hash_string(name, len);
    size_t idx = hash % st->n_buckets;

    // Check if already interned
    for (Symbol *sym = st->buckets[idx]; sym; sym = sym->next) {
        if (sym->hash == hash && sym->len == len && memcmp(sym->name, name, len) == 0) {
            return sym;
        }
    }

    // Create new symbol
    Symbol *sym = malloc(sizeof(Symbol));
    char *name_copy = malloc(len + 1);
    memcpy(name_copy, name, len);
    name_copy[len] = '\0';

    sym->name = name_copy;
    sym->len = len;
    sym->hash = hash;
    sym->next = st->buckets[idx];
    st->buckets[idx] = sym;
    st->count++;

    // TODO: Rehash if too full

    return sym;
}

Symbol *symbol_intern_cstr(SymbolTable *st, const char *name) {
    return symbol_intern(st, name, strlen(name));
}

// ============================================================================
// Cell Implementation
// ============================================================================

Cell *cell_new(Value v) {
    Cell *c = malloc(sizeof(Cell));
    c->value = v;
    c->refcount = 1;
    return c;
}

void cell_incref(Cell *c) {
    if (c) c->refcount++;
}

void cell_decref(Cell *c) {
    if (!c) return;
    c->refcount--;
    if (c->refcount <= 0) {
        value_free(c->value);
        free(c);
    }
}

Value cell_get(Cell *c) {
    return value_copy(c->value);
}

void cell_set(Cell *c, Value v) {
    value_free(c->value);
    c->value = v;
}

// ============================================================================
// Array Implementation
// ============================================================================

Array *array_new(size_t initial_cap) {
    Array *arr = malloc(sizeof(Array));
    arr->cap = initial_cap > 0 ? initial_cap : 8;
    arr->data = calloc(arr->cap, sizeof(Value));
    arr->len = 0;
    arr->refcount = 1;
    return arr;
}

void array_incref(Array *arr) {
    if (arr) arr->refcount++;
}

void array_decref(Array *arr) {
    if (!arr) return;
    arr->refcount--;
    if (arr->refcount <= 0) {
        for (size_t i = 0; i < arr->len; i++) {
            value_free(arr->data[i]);
        }
        free(arr->data);
        free(arr);
    }
}

static void array_grow(Array *arr) {
    arr->cap *= 2;
    arr->data = realloc(arr->data, arr->cap * sizeof(Value));
}

void array_push(Array *arr, Value v) {
    if (arr->len >= arr->cap) {
        array_grow(arr);
    }
    arr->data[arr->len++] = v;
}

Value array_pop(Array *arr) {
    if (arr->len == 0) {
        return value_nil();
    }
    return arr->data[--arr->len];
}

Value array_get(Array *arr, size_t idx) {
    if (idx >= arr->len) {
        return value_nil();
    }
    return value_copy(arr->data[idx]);
}

void array_set(Array *arr, size_t idx, Value v) {
    // Grow if needed
    while (idx >= arr->cap) {
        array_grow(arr);
    }
    // Fill gaps with nil
    while (arr->len <= idx) {
        arr->data[arr->len++] = value_nil();
    }
    value_free(arr->data[idx]);
    arr->data[idx] = v;
}

size_t array_len(Array *arr) {
    return arr->len;
}

// ============================================================================
// Value Operations
// ============================================================================

Value value_copy(Value v) {
    switch (v.kind) {
    case VAL_STRING: string_incref(v.as_string); break;
    case VAL_CELL: cell_incref(v.as_cell); break;
    case VAL_ARRAY: array_incref(v.as_array); break;
    case VAL_CLOSURE: closure_incref(v.as_closure); break;
    case VAL_SCOPE: sn_scope_incref(v.as_scope); break;
    default: break;
    }
    return v;
}

void value_free(Value v) {
    switch (v.kind) {
    case VAL_STRING: string_decref(v.as_string); break;
    case VAL_CELL: cell_decref(v.as_cell); break;
    case VAL_ARRAY: array_decref(v.as_array); break;
    case VAL_CLOSURE: closure_decref(v.as_closure); break;
    case VAL_SCOPE: sn_scope_decref(v.as_scope); break;
    default: break;
    }
}

bool value_truthy(Value v) {
    switch (v.kind) {
    case VAL_NIL: return false;
    case VAL_INT: return v.as_int != 0;
    case VAL_STRING: return v.as_string->len > 0;
    case VAL_ARRAY: return v.as_array->len > 0;
    default: return true;
    }
}

int64_t value_to_int(Value v) {
    switch (v.kind) {
    case VAL_NIL: return 0;
    case VAL_INT: return v.as_int;
    case VAL_STRING: return atoll(v.as_string->data);
    case VAL_CELL: return value_to_int(v.as_cell->value);
    case VAL_ARRAY: return (int64_t)v.as_array->len;
    default: return 0;
    }
}

String *value_to_string(Value v, SymbolTable *symbols) {
    char buf[64];
    (void)symbols; // May be used for symbol lookup later

    switch (v.kind) {
    case VAL_NIL: return string_from_cstr("");
    case VAL_INT:
        snprintf(buf, sizeof(buf), "%lld", (long long)v.as_int);
        return string_from_cstr(buf);
    case VAL_STRING: string_incref(v.as_string); return v.as_string;
    case VAL_SYMBOL: return string_new(v.as_symbol->name, v.as_symbol->len);
    case VAL_CELL: return value_to_string(v.as_cell->value, symbols);
    case VAL_ARRAY:
        snprintf(buf, sizeof(buf), "[array:%zu]", v.as_array->len);
        return string_from_cstr(buf);
    case VAL_CLOSURE:
        snprintf(buf, sizeof(buf), "<closure:%s>",
                 v.as_closure->func->name ? v.as_closure->func->name : "anon");
        return string_from_cstr(buf);
    case VAL_SCOPE:
        snprintf(buf, sizeof(buf), "<scope:%u>", v.as_scope->id);
        return string_from_cstr(buf);
    default: return string_from_cstr("<?>");
    }
}

bool value_eq(Value a, Value b) {
    if (a.kind != b.kind) {
        // Special case: compare integers and cells containing integers
        if (a.kind == VAL_CELL) return value_eq(a.as_cell->value, b);
        if (b.kind == VAL_CELL) return value_eq(a, b.as_cell->value);
        return false;
    }

    switch (a.kind) {
    case VAL_NIL: return true;
    case VAL_INT: return a.as_int == b.as_int;
    case VAL_STRING: return string_eq(a.as_string, b.as_string);
    case VAL_SYMBOL: return a.as_symbol == b.as_symbol; // Pointer equality
    case VAL_CELL: return a.as_cell == b.as_cell;       // Identity
    case VAL_ARRAY: return a.as_array == b.as_array;    // Identity
    case VAL_CLOSURE: return a.as_closure == b.as_closure;
    case VAL_SCOPE: return a.as_scope == b.as_scope;
    default: return false;
    }
}

void value_print(Value v) {
    switch (v.kind) {
    case VAL_NIL: fprintf(stderr, "nil"); break;
    case VAL_INT: fprintf(stderr, "%lld", (long long)v.as_int); break;
    case VAL_STRING: fprintf(stderr, "\"%.*s\"", (int)v.as_string->len, v.as_string->data); break;
    case VAL_SYMBOL: fprintf(stderr, "#%s", v.as_symbol->name); break;
    case VAL_CELL:
        fprintf(stderr, "Cell{");
        value_print(v.as_cell->value);
        fprintf(stderr, "}");
        break;
    case VAL_ARRAY: fprintf(stderr, "[array:%zu]", v.as_array->len); break;
    case VAL_CLOSURE:
        fprintf(stderr, "<closure:%s>",
                v.as_closure->func->name ? v.as_closure->func->name : "anon");
        break;
    case VAL_SCOPE: fprintf(stderr, "<scope:%u>", v.as_scope->id); break;
    default: fprintf(stderr, "<?>"); break;
    }
}

const char *value_kind_name(ValueKind kind) {
    switch (kind) {
    case VAL_NIL: return "nil";
    case VAL_INT: return "int";
    case VAL_STRING: return "string";
    case VAL_SYMBOL: return "symbol";
    case VAL_CELL: return "cell";
    case VAL_ARRAY: return "array";
    case VAL_MAP: return "map";
    case VAL_CLOSURE: return "closure";
    case VAL_SCOPE: return "scope";
    default: return "unknown";
    }
}

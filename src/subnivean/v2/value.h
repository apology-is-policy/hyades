// value.h - Subnivean 2.0 Value System
//
// Generic tagged value type supporting multiple languages.
// Key innovation: Cell type for mutable bindings (enables capture-by-reference).

#ifndef SUBNIVEAN_VALUE_H
#define SUBNIVEAN_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct Value Value;
typedef struct Cell Cell;
typedef struct Array Array;
typedef struct Map Map;
typedef struct String String;
typedef struct Symbol Symbol;
typedef struct Closure Closure;
typedef struct Scope Scope;
typedef struct Function Function;

// ============================================================================
// Value Kind
// ============================================================================

typedef enum {
    VAL_NIL,     // Null/none/undefined
    VAL_INT,     // 64-bit signed integer
    VAL_STRING,  // Heap-allocated string
    VAL_SYMBOL,  // Interned string (fast equality)
    VAL_CELL,    // Mutable box (for Hyades counters, Python nonlocal)
    VAL_ARRAY,   // Dynamic array
    VAL_MAP,     // Key-value store (future)
    VAL_CLOSURE, // Function + captured scope
    VAL_SCOPE,   // First-class scope reference
} ValueKind;

// ============================================================================
// Value Structure
// ============================================================================

struct Value {
    ValueKind kind;
    union {
        int64_t as_int;
        String *as_string;
        Symbol *as_symbol;
        Cell *as_cell;
        Array *as_array;
        Map *as_map;
        Closure *as_closure;
        Scope *as_scope;
    };
};

// ============================================================================
// String (Heap-allocated, refcounted)
// ============================================================================

struct String {
    char *data;
    size_t len;
    int refcount;
};

String *string_new(const char *data, size_t len);
String *string_from_cstr(const char *cstr);
void string_incref(String *s);
void string_decref(String *s);
String *string_concat(String *a, String *b);
bool string_eq(String *a, String *b);

// ============================================================================
// Symbol (Interned string for fast comparison)
// ============================================================================

// Symbols are interned - same string always yields same pointer.
// Comparison is pointer equality, not string comparison.

typedef struct SymbolTable SymbolTable;

struct SymbolTable {
    Symbol **buckets;
    size_t n_buckets;
    size_t count;
};

struct Symbol {
    const char *name; // Interned, do not free
    size_t len;
    uint32_t hash;
    Symbol *next; // Hash chain
};

void symbol_table_init(SymbolTable *st);
void symbol_table_free(SymbolTable *st);
Symbol *symbol_intern(SymbolTable *st, const char *name, size_t len);
Symbol *symbol_intern_cstr(SymbolTable *st, const char *name);

// ============================================================================
// Cell (Mutable box - key for capture-by-reference)
// ============================================================================
//
// A Cell wraps a Value, allowing mutation through a reference.
// When a closure captures a Cell, mutations are visible to the closure.
//
// Hyades usage:
//   \let<x>{5}    -> Bind 'x' to Cell{Int 5}
//   \valueof<x>   -> Lookup 'x', get Cell, read value
//   \inc<x>       -> Lookup 'x', get Cell, mutate in place
//
// This enables:
//   \let<x>{1}
//   \lambda<get>{\valueof<x>}
//   \inc<x>
//   \recall<get>  -> 2 (sees mutation!)

struct Cell {
    Value value;
    int refcount;
};

Cell *cell_new(Value v);
void cell_incref(Cell *c);
void cell_decref(Cell *c);
Value cell_get(Cell *c);
void cell_set(Cell *c, Value v);

// ============================================================================
// Array (Dynamic, refcounted)
// ============================================================================

struct Array {
    Value *data;
    size_t len;
    size_t cap;
    int refcount;
};

Array *array_new(size_t initial_cap);
void array_incref(Array *arr);
void array_decref(Array *arr);
void array_push(Array *arr, Value v);
Value array_pop(Array *arr);
Value array_get(Array *arr, size_t idx);
void array_set(Array *arr, size_t idx, Value v);
size_t array_len(Array *arr);

// ============================================================================
// Value Constructors
// ============================================================================

static inline Value value_nil(void) {
    return (Value){.kind = VAL_NIL};
}

static inline Value value_int(int64_t n) {
    return (Value){.kind = VAL_INT, .as_int = n};
}

static inline Value value_string(String *s) {
    string_incref(s);
    return (Value){.kind = VAL_STRING, .as_string = s};
}

static inline Value value_symbol(Symbol *sym) {
    return (Value){.kind = VAL_SYMBOL, .as_symbol = sym};
}

static inline Value value_cell(Cell *c) {
    cell_incref(c);
    return (Value){.kind = VAL_CELL, .as_cell = c};
}

static inline Value value_array(Array *arr) {
    array_incref(arr);
    return (Value){.kind = VAL_ARRAY, .as_array = arr};
}

static inline Value value_closure(Closure *cl); // Defined after Closure
static inline Value value_scope(Scope *sc);     // Defined after Scope

// ============================================================================
// Value Operations
// ============================================================================

// Copy a value (increments refcount for heap objects)
Value value_copy(Value v);

// Release a value (decrements refcount for heap objects)
void value_free(Value v);

// Check truthiness (for conditionals)
bool value_truthy(Value v);

// Convert to integer (for arithmetic)
int64_t value_to_int(Value v);

// Convert to string (for output/concatenation)
String *value_to_string(Value v, SymbolTable *symbols);

// Equality test
bool value_eq(Value a, Value b);

// Debug print
void value_print(Value v);

// Kind name for error messages
const char *value_kind_name(ValueKind kind);

#endif // SUBNIVEAN_VALUE_H

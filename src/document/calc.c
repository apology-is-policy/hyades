// calc.c - Computational features for Hyades
//
// Implements arithmetic, comparisons, counters, and conditionals.

#include "calc.h"
#include "compositor/compositor.h"
#include "diagnostics/diagnostics.h"
#include "document.h"
#include "document/document.h" // For macro_is_continuous_mode()
#include "document/symbol_table.h"
#include "layout/layout.h"
#include "macro/user/macro.h"
#include "math/ast.h"
#include "math/parser/parser.h"
#include "math/renderer/render_opts.h"
#include "math/renderer/symbols.h"
#include "utils/terminal_input.h" // For keyboard/mouse input in continuous mode
#include "utils/util.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define STDOUT_FILENO 1
#define write(fd, buf, len) _write(fd, buf, (unsigned int)(len))
#else
#include <unistd.h>
#endif

// Subnivean VM integration for computational lambdas
#ifdef SUBNIVEAN_ENABLED
// Forward declarations to avoid header conflicts (calc.c has its own Array type)
typedef struct VM VM;

// Persistent array store for zero-copy arrays between interpreter and Subnivean
int sn_store_create_array(int64_t *elements, int n_elements);
int64_t sn_store_array_get(int addr, int idx);
bool sn_store_array_set(int addr, int idx, int64_t value);
int sn_store_array_len(int addr);

// Persistent string array store
int sn_store_create_string_array(const char **elements, int n_elements);
const char *sn_store_string_array_get(int addr, int idx);
int sn_store_string_array_len(int addr);
bool sn_store_is_string_array(int addr);

// Persistent map store for Robin Hood hash maps
int sn_store_create_map(void);
int64_t sn_store_map_get(int addr, int64_t key);
bool sn_store_map_set(int addr, int64_t key, int64_t value);
bool sn_store_map_has(int addr, int64_t key);
bool sn_store_map_del(int addr, int64_t key);
int sn_store_map_len(int addr);
int sn_store_map_keys(int addr); // Returns addr of new array containing keys
bool sn_store_is_map(int addr);

typedef struct SubniveanFunction SubniveanFunction; // Renamed to avoid conflict

// Subnivean API functions (implemented in subnivean/v2/)
VM *subnivean_vm_new(void);
void subnivean_vm_free(VM *vm);
SubniveanFunction *subnivean_compile(VM *vm, const char *name, char **params, int n_params,
                                     const char *source, char *error_msg, int error_size);
char *subnivean_execute(VM *vm, SubniveanFunction *func, const char **args, int n_args);
void subnivean_function_decref(SubniveanFunction *func);
char *subnivean_disassemble(SubniveanFunction *func);
SubniveanFunction *subnivean_assemble(VM *vm, const char *source, char *error_msg, int error_size);
void subnivean_set_array_lookup(VM *vm, int (*lookup)(void *, const char *, char ***, int *),
                                void *ctx);
void subnivean_set_array_set(VM *vm, int (*set_fn)(void *, const char *, int, const char *),
                             void *ctx);
void subnivean_set_lambda_compile(VM *vm, SubniveanFunction *(*compile)(void *, const char *),
                                  void *ctx);
#endif

// ============================================================================
// Defensive Limits (prevent hangs from memory corruption)
// ============================================================================

// Maximum characters to scan in string parsing loops.
// If we scan more than this, something is wrong (corruption or missing terminator).
#define MAX_SCAN_CHARS (500 * 1024 * 1024) // 500MB - effectively unlimited

// ============================================================================
// Forward Declarations
// ============================================================================

// Lambda creation with computational flag
Lambda *lambda_new_ex(char **params, int n_params, const char *body, bool is_computational);

// ============================================================================
// Safe Memory Allocation Helpers
// ============================================================================

// Safe realloc that preserves the original pointer on failure.
// Returns NULL on failure but does NOT free the original pointer.
// Caller should check return value and handle failure appropriately.
static void *safe_realloc(void *ptr, size_t new_size) {
    if (new_size == 0) {
        return ptr; // Don't shrink to zero
    }
    void *new_ptr = realloc(ptr, new_size);
    if (!new_ptr) {
// realloc failed - original ptr is still valid
// Log warning in debug builds
#ifndef NDEBUG
        fprintf(stderr, "calc.c: realloc failed for size %zu\n", new_size);
#endif
        return NULL;
    }
// Check if allocation is in suspicious low-address region (static data)
// In WASM, addresses < 0x10000 are typically static data
#ifdef __EMSCRIPTEN__
    if ((uintptr_t)new_ptr < 0x10000) {
        fprintf(stderr, "WARNING: realloc returned low address %p (size=%zu)\n", new_ptr, new_size);
    }
#endif
    return new_ptr;
}

// Checked malloc that warns about low addresses
static void *checked_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
#ifndef NDEBUG
        fprintf(stderr, "calc.c: malloc failed for size %zu\n", size);
#endif
        return NULL;
    }
#ifdef __EMSCRIPTEN__
    if ((uintptr_t)ptr < 0x10000) {
        fprintf(stderr, "WARNING: malloc returned low address %p (size=%zu)\n", ptr, size);
    }
#endif
    return ptr;
}

// ============================================================================
// String Escape Helpers
// ============================================================================

// Escape braces in string output to prevent re-parsing issues.
// Replaces { with \{ and } with \}.
// Caller must free the returned string.
static char *escape_braces(const char *str) {
    if (!str) return NULL;

    // Count braces to determine output size
    size_t len = strlen(str);
    size_t n_braces = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '{' || str[i] == '}') n_braces++;
    }

    if (n_braces == 0) return strdup(str);

    // Allocate output (each brace becomes 2 chars: \{ or \})
    char *out = malloc(len + n_braces + 1);
    if (!out) return strdup(str);

    char *p = out;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '{' || str[i] == '}') {
            *p++ = '\\';
        }
        *p++ = str[i];
    }
    *p = '\0';

    return out;
}

// ============================================================================
// Counter Registry Implementation
// ============================================================================

typedef struct Counter {
    char *name;
    int value;
} Counter;

struct CounterRegistry {
    Counter *counters;
    int n_counters;
    int capacity;
};

CounterRegistry *counter_registry_new(void) {
    CounterRegistry *reg = calloc(1, sizeof(CounterRegistry));
    if (!reg) return NULL;
    reg->capacity = 16;
    reg->counters = calloc(reg->capacity, sizeof(Counter));
    if (!reg->counters) {
        free(reg);
        return NULL;
    }
    return reg;
}

void counter_registry_free(CounterRegistry *reg) {
    if (!reg) return;
    for (int i = 0; i < reg->n_counters; i++) {
        free(reg->counters[i].name);
    }
    free(reg->counters);
    free(reg);
}

static Counter *find_counter(CounterRegistry *reg, const char *name) {
    for (int i = 0; i < reg->n_counters; i++) {
        if (strcmp(reg->counters[i].name, name) == 0) {
            return &reg->counters[i];
        }
    }
    return NULL;
}

void counter_set(CounterRegistry *reg, const char *name, int value) {
    Counter *c = find_counter(reg, name);
    if (c) {
        c->value = value;
        return;
    }

    // Create new counter
    if (reg->n_counters >= reg->capacity) {
        reg->capacity *= 2;
        reg->counters = realloc(reg->counters, reg->capacity * sizeof(Counter));
    }

    reg->counters[reg->n_counters].name = strdup(name);
    reg->counters[reg->n_counters].value = value;
    reg->n_counters++;
}

int counter_get(CounterRegistry *reg, const char *name) {
    Counter *c = find_counter(reg, name);
    return c ? c->value : 0;
}

int counter_inc(CounterRegistry *reg, const char *name) {
    Counter *c = find_counter(reg, name);
    if (c) {
        return ++c->value;
    }
    // Create with value 1
    counter_set(reg, name, 1);
    return 1;
}

int counter_dec(CounterRegistry *reg, const char *name) {
    Counter *c = find_counter(reg, name);
    if (c) {
        return --c->value;
    }
    // Create with value -1
    counter_set(reg, name, -1);
    return -1;
}

// ============================================================================
// Content Registry Implementation (for \measure and \recall)
// ============================================================================

typedef struct StoredContent {
    char *name;
    char *content;
    int width;
    int height;
} StoredContent;

struct ContentRegistry {
    StoredContent *items;
    int n_items;
    int capacity;
};

ContentRegistry *content_registry_new(void) {
    ContentRegistry *reg = calloc(1, sizeof(ContentRegistry));
    if (!reg) return NULL;
    reg->capacity = 16;
    reg->items = calloc(reg->capacity, sizeof(StoredContent));
    if (!reg->items) {
        free(reg);
        return NULL;
    }
    return reg;
}

void content_registry_free(ContentRegistry *reg) {
    if (!reg) return;
    for (int i = 0; i < reg->n_items; i++) {
        free(reg->items[i].name);
        free(reg->items[i].content);
    }
    free(reg->items);
    free(reg);
}

static StoredContent *find_content(ContentRegistry *reg, const char *name) {
    for (int i = 0; i < reg->n_items; i++) {
        if (strcmp(reg->items[i].name, name) == 0) {
            return &reg->items[i];
        }
    }
    return NULL;
}

void content_store(ContentRegistry *reg, const char *name, const char *content, int width,
                   int height) {
    StoredContent *item = find_content(reg, name);
    if (item) {
        free(item->content);
        item->content = strdup(content);
        item->width = width;
        item->height = height;
        return;
    }

    // Create new entry
    if (reg->n_items >= reg->capacity) {
        reg->capacity *= 2;
        reg->items = realloc(reg->items, reg->capacity * sizeof(StoredContent));
    }

    reg->items[reg->n_items].name = strdup(name);
    reg->items[reg->n_items].content = strdup(content);
    reg->items[reg->n_items].width = width;
    reg->items[reg->n_items].height = height;
    reg->n_items++;
}

const char *content_get(ContentRegistry *reg, const char *name) {
    StoredContent *item = find_content(reg, name);
    return item ? item->content : NULL;
}

int content_get_width(ContentRegistry *reg, const char *name) {
    StoredContent *item = find_content(reg, name);
    return item ? item->width : 0;
}

int content_get_height(ContentRegistry *reg, const char *name) {
    StoredContent *item = find_content(reg, name);
    return item ? item->height : 0;
}

// ============================================================================
// Array Registry Implementation (for \let<name[]> and \assign<name[]>)
// ============================================================================

typedef struct Array {
    char *name;
    char **elements; // Array of strings (integers stored as their string repr)
    int n_elements;
    int capacity;
    bool is_counter_array; // true = integer array (\let), false = content array (\assign)
} Array;

typedef struct ArrayRegistry {
    Array *arrays;
    int n_arrays;
    int capacity;
} ArrayRegistry;

ArrayRegistry *array_registry_new(void) {
    ArrayRegistry *reg = calloc(1, sizeof(ArrayRegistry));
    if (!reg) return NULL;
    reg->capacity = 8;
    reg->arrays = calloc(reg->capacity, sizeof(Array));
    if (!reg->arrays) {
        free(reg);
        return NULL;
    }
    return reg;
}

void array_registry_free(ArrayRegistry *reg) {
    if (!reg) return;
    for (int i = 0; i < reg->n_arrays; i++) {
        free(reg->arrays[i].name);
        for (int j = 0; j < reg->arrays[i].n_elements; j++) {
            free(reg->arrays[i].elements[j]);
        }
        free(reg->arrays[i].elements);
    }
    free(reg->arrays);
    free(reg);
}

// Forward declaration for find_array (needed by public API)
static Array *find_array(ArrayRegistry *reg, const char *name);

int array_registry_len(ArrayRegistry *reg, const char *name) {
    Array *arr = find_array(reg, name);
    return arr ? arr->n_elements : 0;
}

const char *array_registry_get(ArrayRegistry *reg, const char *name, int index) {
    Array *arr = find_array(reg, name);
    if (!arr || index < 0 || index >= arr->n_elements) return NULL;
    return arr->elements[index];
}

static Array *find_array(ArrayRegistry *reg, const char *name) {
    if (!reg) return NULL;
    for (int i = 0; i < reg->n_arrays; i++) {
        if (strcmp(reg->arrays[i].name, name) == 0) {
            return &reg->arrays[i];
        }
    }
    return NULL;
}

static Array *create_array(ArrayRegistry *reg, const char *name, bool is_counter) {
    if (reg->n_arrays >= reg->capacity) {
        reg->capacity *= 2;
        reg->arrays = realloc(reg->arrays, reg->capacity * sizeof(Array));
    }

    Array *arr = &reg->arrays[reg->n_arrays++];
    arr->name = strdup(name);
    arr->elements = NULL;
    arr->n_elements = 0;
    arr->capacity = 0;
    arr->is_counter_array = is_counter;
    return arr;
}

static void array_push(Array *arr, const char *value) {
    if (arr->n_elements >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->elements = realloc(arr->elements, arr->capacity * sizeof(char *));
    }
    arr->elements[arr->n_elements++] = strdup(value);
}

static const char *array_get(Array *arr, int index) {
    if (!arr || index < 0 || index >= arr->n_elements) return NULL;
    return arr->elements[index];
}

static int array_len(Array *arr) {
    return arr ? arr->n_elements : 0;
}

static char *array_pop(Array *arr) {
    if (!arr || arr->n_elements == 0) return NULL;
    char *value = arr->elements[--arr->n_elements];
    return value; // Caller takes ownership
}

static const char *array_peek(Array *arr) {
    if (!arr || arr->n_elements == 0) return NULL;
    return arr->elements[arr->n_elements - 1];
}

static char *array_dequeue(Array *arr) {
    if (!arr || arr->n_elements == 0) return NULL;
    char *value = arr->elements[0];
    // Shift all elements down
    for (int i = 1; i < arr->n_elements; i++) {
        arr->elements[i - 1] = arr->elements[i];
    }
    arr->n_elements--;
    return value; // Caller takes ownership
}

static bool array_set(Array *arr, int index, const char *value) {
    if (!arr || index < 0 || index >= arr->n_elements) return false;
    free(arr->elements[index]);
    arr->elements[index] = strdup(value);
    return true;
}

// ============================================================================
// Type Registry Implementation (maps hygienized names to their registry type)
// ============================================================================

typedef struct TypeEntry {
    char *name;
    char type; // 'v' = value, 'l' = lambda, 'c' = counter, 'a' = array
} TypeEntry;

typedef struct TypeRegistry {
    TypeEntry *entries;
    int n_entries;
    int capacity;
} TypeRegistry;

TypeRegistry *type_registry_new(void) {
    TypeRegistry *reg = calloc(1, sizeof(TypeRegistry));
    if (!reg) return NULL;
    reg->capacity = 32;
    reg->entries = calloc(reg->capacity, sizeof(TypeEntry));
    if (!reg->entries) {
        free(reg);
        return NULL;
    }
    return reg;
}

void type_registry_free(TypeRegistry *reg) {
    if (!reg) return;
    for (int i = 0; i < reg->n_entries; i++) {
        free(reg->entries[i].name);
    }
    free(reg->entries);
    free(reg);
}

static TypeEntry *find_type_entry(TypeRegistry *reg, const char *name) {
    if (!reg || !name) return NULL;
    for (int i = 0; i < reg->n_entries; i++) {
        if (strcmp(reg->entries[i].name, name) == 0) {
            return &reg->entries[i];
        }
    }
    return NULL;
}

void type_registry_set(TypeRegistry *reg, const char *name, char type) {
    if (!reg || !name) return;

    // Check if already exists (update type)
    TypeEntry *entry = find_type_entry(reg, name);
    if (entry) {
        entry->type = type;
        return;
    }

    // Add new entry
    if (reg->n_entries >= reg->capacity) {
        reg->capacity *= 2;
        reg->entries = realloc(reg->entries, reg->capacity * sizeof(TypeEntry));
    }

    reg->entries[reg->n_entries].name = strdup(name);
    reg->entries[reg->n_entries].type = type;
    reg->n_entries++;
}

char type_registry_get(TypeRegistry *reg, const char *name) {
    TypeEntry *entry = find_type_entry(reg, name);
    return entry ? entry->type : 0; // 0 means not found
}

// ============================================================================
// Lambda Implementation (for \lambda<name>[params]{body})
// ============================================================================

struct Lambda {
    char **param_names;    // Array of parameter names, NULL for thunks
    int param_count;       // Number of parameters (0 for thunks)
    char *body;            // The unevaluated expression
    Scope *captured_scope; // Reference to defining scope (refcounted)
    bool is_computational; // True for #{} lambdas - must use \return{}, no rendering

#ifdef SUBNIVEAN_ENABLED
    // Subnivean compile cache (lazy compilation on first call)
    SubniveanFunction *compiled; // Compiled bytecode, NULL until first call
    bool compile_attempted;      // True if we tried to compile (even if failed)
#endif
};

typedef struct LambdaEntry {
    char *name;
    Lambda *lambda;
} LambdaEntry;

struct LambdaRegistry {
    LambdaEntry *entries;
    int n_entries;
    int capacity;
};

LambdaRegistry *lambda_registry_new(void) {
    LambdaRegistry *reg = calloc(1, sizeof(LambdaRegistry));
    if (!reg) return NULL;
    reg->capacity = 8;
    reg->entries = calloc(reg->capacity, sizeof(LambdaEntry));
    if (!reg->entries) {
        free(reg);
        return NULL;
    }
    return reg;
}

void lambda_registry_free(LambdaRegistry *reg) {
    if (!reg) return;
    for (int i = 0; i < reg->n_entries; i++) {
        free(reg->entries[i].name);
        lambda_free(reg->entries[i].lambda);
    }
    free(reg->entries);
    free(reg);
}

Lambda *lambda_new(char **params, int n_params, const char *body) {
    return lambda_new_ex(params, n_params, body, false);
}

Lambda *lambda_new_ex(char **params, int n_params, const char *body, bool is_computational) {
    Lambda *l = calloc(1, sizeof(Lambda));
    if (!l) return NULL;

    if (n_params > 0 && params) {
        l->param_names = calloc(n_params, sizeof(char *));
        if (!l->param_names) {
            free(l);
            return NULL;
        }
        for (int i = 0; i < n_params; i++) {
            l->param_names[i] = strdup(params[i]);
        }
        l->param_count = n_params;
    } else {
        l->param_names = NULL;
        l->param_count = 0;
    }

    l->body = strdup(body);
    l->captured_scope = NULL; // Set when stored in a scope
    l->is_computational = is_computational;
    return l;
}

bool lambda_is_computational(Lambda *l) {
    return l ? l->is_computational : false;
}

void lambda_free(Lambda *l) {
    if (!l) return;
    if (l->param_names) {
        for (int i = 0; i < l->param_count; i++) {
            free(l->param_names[i]);
        }
        free(l->param_names);
    }
    free(l->body);
    // Decrement captured scope's refcount
    if (l->captured_scope) {
        scope_decref(l->captured_scope);
    }
#ifdef SUBNIVEAN_ENABLED
    // Free compiled bytecode if present
    if (l->compiled) {
        subnivean_function_decref(l->compiled);
    }
#endif
    free(l);
}

int lambda_param_count(Lambda *l) {
    return l ? l->param_count : 0;
}

const char *lambda_get_body(Lambda *l) {
    return l ? l->body : NULL;
}

const char *lambda_get_param(Lambda *l, int index) {
    if (!l || index < 0 || index >= l->param_count) return NULL;
    return l->param_names[index];
}

static LambdaEntry *find_lambda_entry(LambdaRegistry *reg, const char *name) {
    if (!reg) return NULL;
    for (int i = 0; i < reg->n_entries; i++) {
        if (strcmp(reg->entries[i].name, name) == 0) {
            return &reg->entries[i];
        }
    }
    return NULL;
}

void lambda_store(LambdaRegistry *reg, const char *name, Lambda *lambda) {
    if (!reg || !name || !lambda) return;

    // Check if already exists (replace)
    LambdaEntry *entry = find_lambda_entry(reg, name);
    if (entry) {
        lambda_free(entry->lambda);
        entry->lambda = lambda;
        return;
    }

    // Add new entry
    if (reg->n_entries >= reg->capacity) {
        reg->capacity *= 2;
        reg->entries = realloc(reg->entries, reg->capacity * sizeof(LambdaEntry));
    }

    reg->entries[reg->n_entries].name = strdup(name);
    reg->entries[reg->n_entries].lambda = lambda;
    reg->n_entries++;
}

Lambda *lambda_get(LambdaRegistry *reg, const char *name) {
    LambdaEntry *entry = find_lambda_entry(reg, name);
    return entry ? entry->lambda : NULL;
}

// ============================================================================
// Scope Implementation (lexical scoping with reference counting)
// ============================================================================

// Scope-local bindings
typedef struct ScopeValue {
    char *name;
    char *content;
} ScopeValue;

typedef struct ScopeCounter {
    char *name;
    int value;
} ScopeCounter;

typedef struct ScopeLambda {
    char *name;
    Lambda *lambda;
} ScopeLambda;

typedef struct ScopeArray {
    char *name;
    char **elements;
    int n_elements;
    int capacity;
    bool is_counter_array;
} ScopeArray;

struct Scope {
    int refcount;
    Scope *parent;

    // Local bindings
    ScopeValue *values;
    int n_values;
    int values_cap;

    ScopeCounter *counters;
    int n_counters;
    int counters_cap;

    ScopeLambda *lambdas;
    int n_lambdas;
    int lambdas_cap;

    ScopeArray *arrays;
    int n_arrays;
    int arrays_cap;
};

// Scope ID counter for diagnostic tracking
static int g_scope_id_counter = 0;

Scope *scope_new(Scope *parent) {
    Scope *s = calloc(1, sizeof(Scope));
    if (!s) return NULL;

    s->refcount = 1;
    s->parent = parent;
    if (parent) {
        scope_incref(parent);
    }

    s->values_cap = 8;
    s->values = calloc(s->values_cap, sizeof(ScopeValue));

    s->counters_cap = 8;
    s->counters = calloc(s->counters_cap, sizeof(ScopeCounter));

    s->lambdas_cap = 4;
    s->lambdas = calloc(s->lambdas_cap, sizeof(ScopeLambda));

    s->arrays_cap = 4;
    s->arrays = calloc(s->arrays_cap, sizeof(ScopeArray));

    // Log scope creation
    int scope_id = ++g_scope_id_counter;
    if (diag_is_enabled(DIAG_CALC)) {
        if (parent) {
            diag_log(DIAG_CALC, 1, "scope #%d created (parent exists, refcount=1)", scope_id);
        } else {
            diag_log(DIAG_CALC, 1, "scope #%d created (global, refcount=1)", scope_id);
        }
    }

    return s;
}

void scope_incref(Scope *s) {
    if (s) s->refcount++;
}

void scope_decref(Scope *s) {
    if (!s) return;
    int old_refcount = s->refcount;
    s->refcount--;
    if (s->refcount <= 0) {
        if (diag_is_enabled(DIAG_CALC)) {
            diag_log(DIAG_CALC, 1,
                     "scope freed (had %d values, %d counters, %d lambdas, %d arrays)", s->n_values,
                     s->n_counters, s->n_lambdas, s->n_arrays);
        }

        // Free local values
        for (int i = 0; i < s->n_values; i++) {
            free(s->values[i].name);
            free(s->values[i].content);
        }
        free(s->values);

        // Free local counters
        for (int i = 0; i < s->n_counters; i++) {
            free(s->counters[i].name);
        }
        free(s->counters);

        // Free local lambdas - break circular references first
        // A lambda may capture this same scope, creating a cycle that prevents cleanup
        for (int i = 0; i < s->n_lambdas; i++) {
            free(s->lambdas[i].name);
            Lambda *l = s->lambdas[i].lambda;
            if (l && l->captured_scope == s) {
                // Break the cycle: don't decref ourselves via lambda_free
                l->captured_scope = NULL;
            }
            lambda_free(l);
        }
        free(s->lambdas);

        // Free local arrays
        for (int i = 0; i < s->n_arrays; i++) {
            free(s->arrays[i].name);
            for (int j = 0; j < s->arrays[i].n_elements; j++) {
                free(s->arrays[i].elements[j]);
            }
            free(s->arrays[i].elements);
        }
        free(s->arrays);

        // Decrement parent's refcount
        if (s->parent) {
            scope_decref(s->parent);
        }

        free(s);
    } else if (diag_is_enabled(DIAG_CALC)) {
        diag_log(DIAG_CALC, 2, "scope decref %d -> %d (kept alive)", old_refcount, s->refcount);
    }
}

Scope *scope_get_parent(Scope *s) {
    return s ? s->parent : NULL;
}

// Local lookup helpers (don't walk chain)
static ScopeValue *scope_find_local_value(Scope *s, const char *name) {
    for (int i = 0; i < s->n_values; i++) {
        if (strcmp(s->values[i].name, name) == 0) {
            return &s->values[i];
        }
    }
    return NULL;
}

static ScopeCounter *scope_find_local_counter(Scope *s, const char *name) {
    for (int i = 0; i < s->n_counters; i++) {
        if (strcmp(s->counters[i].name, name) == 0) {
            return &s->counters[i];
        }
    }
    return NULL;
}

static ScopeLambda *scope_find_local_lambda(Scope *s, const char *name) {
    for (int i = 0; i < s->n_lambdas; i++) {
        if (strcmp(s->lambdas[i].name, name) == 0) {
            return &s->lambdas[i];
        }
    }
    return NULL;
}

void scope_bind_value(Scope *s, const char *name, const char *content) {
    if (!s || !name) return;

    // First check if variable exists anywhere in scope chain
    // If so, update it in place (don't shadow)
    Scope *scope = s;
    while (scope) {
        ScopeValue *v = scope_find_local_value(scope, name);
        if (v) {
            if (diag_is_enabled(DIAG_CALC)) {
                diag_log(DIAG_CALC, 2, "rebind value <%s> = '%.40s%s'", name,
                         content ? content : "", (content && strlen(content) > 40) ? "..." : "");
            }
            free(v->content);
            v->content = strdup(content ? content : "");
            return;
        }
        scope = scope->parent;
    }

    // Not found anywhere - create new binding in current scope
    if (s->n_values >= s->values_cap) {
        s->values_cap *= 2;
        s->values = realloc(s->values, s->values_cap * sizeof(ScopeValue));
    }
    s->values[s->n_values].name = strdup(name);
    s->values[s->n_values].content = strdup(content ? content : "");
    s->n_values++;

    if (diag_is_enabled(DIAG_CALC)) {
        diag_log(DIAG_CALC, 2, "bind value <%s> = '%.40s%s'", name, content ? content : "",
                 (content && strlen(content) > 40) ? "..." : "");
    }
}

void scope_bind_lambda(Scope *s, const char *name, Lambda *lambda) {
    if (!s || !name || !lambda) return;

    // Set captured scope
    lambda->captured_scope = s;
    scope_incref(s);

    // Check if exists locally
    ScopeLambda *l = scope_find_local_lambda(s, name);
    if (l) {
        if (diag_is_enabled(DIAG_CALC)) {
            diag_log(DIAG_CALC, 2, "rebind lambda <%s> (captures current scope)", name);
        }
        lambda_free(l->lambda);
        l->lambda = lambda;
        return;
    }

    // Add new
    if (s->n_lambdas >= s->lambdas_cap) {
        s->lambdas_cap *= 2;
        s->lambdas = realloc(s->lambdas, s->lambdas_cap * sizeof(ScopeLambda));
    }
    s->lambdas[s->n_lambdas].name = strdup(name);
    s->lambdas[s->n_lambdas].lambda = lambda;
    s->n_lambdas++;

    if (diag_is_enabled(DIAG_CALC)) {
        diag_log(DIAG_CALC, 2, "bind lambda <%s> (captures current scope)", name);
    }
}

void scope_bind_lambda_with_capture(Scope *store_in, Scope *capture, const char *name,
                                    Lambda *lambda) {
    if (!store_in || !capture || !name || !lambda) return;

    // Set captured scope to the capture scope (not the storage scope)
    lambda->captured_scope = capture;
    scope_incref(capture);

    // Store the lambda in store_in scope
    ScopeLambda *l = scope_find_local_lambda(store_in, name);
    if (l) {
        if (diag_is_enabled(DIAG_CALC)) {
            diag_log(DIAG_CALC, 2, "rebind lambda <%s> (stored in outer, captures iteration)",
                     name);
        }
        lambda_free(l->lambda);
        l->lambda = lambda;
        return;
    }

    // Add new
    if (store_in->n_lambdas >= store_in->lambdas_cap) {
        store_in->lambdas_cap *= 2;
        store_in->lambdas = realloc(store_in->lambdas, store_in->lambdas_cap * sizeof(ScopeLambda));
    }
    store_in->lambdas[store_in->n_lambdas].name = strdup(name);
    store_in->lambdas[store_in->n_lambdas].lambda = lambda;
    store_in->n_lambdas++;

    if (diag_is_enabled(DIAG_CALC)) {
        diag_log(DIAG_CALC, 2, "bind lambda <%s> (stored in outer, captures iteration)", name);
    }
}

void scope_bind_counter(Scope *s, const char *name, int value) {
    if (!s || !name) return;

    // Check if exists locally
    ScopeCounter *c = scope_find_local_counter(s, name);
    if (c) {
        if (diag_is_enabled(DIAG_CALC)) {
            diag_log(DIAG_CALC, 2, "rebind counter <%s> = %d", name, value);
        }
        c->value = value;
        return;
    }

    // Add new
    if (s->n_counters >= s->counters_cap) {
        s->counters_cap *= 2;
        s->counters = realloc(s->counters, s->counters_cap * sizeof(ScopeCounter));
    }
    s->counters[s->n_counters].name = strdup(name);
    s->counters[s->n_counters].value = value;
    s->n_counters++;

    if (diag_is_enabled(DIAG_CALC)) {
        diag_log(DIAG_CALC, 2, "bind counter <%s> = %d", name, value);
    }
}

const char *scope_lookup_value(Scope *s, const char *name) {
    int depth = 0;
    Scope *start = s;
    while (s) {
        ScopeValue *v = scope_find_local_value(s, name);
        if (v) {
            if (diag_is_enabled(DIAG_CALC) && depth > 0) {
                diag_log(DIAG_CALC, 2, "lookup value <%s>: found at depth %d", name, depth);
            }
            return v->content;
        }
        s = s->parent;
        depth++;
    }
    if (diag_is_enabled(DIAG_CALC) && start) {
        diag_log(DIAG_CALC, 2, "lookup value <%s>: not found (searched %d scopes)", name, depth);
    }
    return NULL;
}

Lambda *scope_lookup_lambda(Scope *s, const char *name) {
    int depth = 0;
    Scope *start = s;
    while (s) {
        ScopeLambda *l = scope_find_local_lambda(s, name);
        if (l) {
            if (diag_is_enabled(DIAG_CALC) && depth > 0) {
                diag_log(DIAG_CALC, 2, "lookup lambda <%s>: found at depth %d", name, depth);
            }
            return l->lambda;
        }
        s = s->parent;
        depth++;
    }
    if (diag_is_enabled(DIAG_CALC) && start) {
        diag_log(DIAG_CALC, 2, "lookup lambda <%s>: not found (searched %d scopes)", name, depth);
    }
    return NULL;
}

int scope_lookup_counter(Scope *s, const char *name, bool *found) {
    int depth = 0;
    Scope *start = s;
    while (s) {
        ScopeCounter *c = scope_find_local_counter(s, name);
        if (c) {
            if (found) *found = true;
            if (diag_is_enabled(DIAG_CALC) && depth > 0) {
                diag_log(DIAG_CALC, 2, "lookup counter <%s>: found %d at depth %d", name, c->value,
                         depth);
            }
            return c->value;
        }
        s = s->parent;
        depth++;
    }
    if (found) *found = false;
    if (diag_is_enabled(DIAG_CALC) && start) {
        diag_log(DIAG_CALC, 2, "lookup counter <%s>: not found (searched %d scopes)", name, depth);
    }
    return 0;
}

// Find or create counter in scope chain (for mutation)
static ScopeCounter *scope_find_or_create_counter(Scope *s, const char *name) {
    // First, try to find in chain
    Scope *search = s;
    while (search) {
        ScopeCounter *c = scope_find_local_counter(search, name);
        if (c) return c;
        search = search->parent;
    }

    // Not found, create in current scope
    if (s->n_counters >= s->counters_cap) {
        s->counters_cap *= 2;
        s->counters = realloc(s->counters, s->counters_cap * sizeof(ScopeCounter));
    }
    s->counters[s->n_counters].name = strdup(name);
    s->counters[s->n_counters].value = 0;
    return &s->counters[s->n_counters++];
}

void scope_set_counter(Scope *s, const char *name, int value) {
    if (!s || !name) return;
    ScopeCounter *c = scope_find_or_create_counter(s, name);
    if (c) c->value = value;
}

int scope_inc_counter(Scope *s, const char *name) {
    if (!s || !name) return 0;
    ScopeCounter *c = scope_find_or_create_counter(s, name);
    if (c) return ++c->value;
    return 0;
}

int scope_dec_counter(Scope *s, const char *name) {
    if (!s || !name) return 0;
    ScopeCounter *c = scope_find_or_create_counter(s, name);
    if (c) return --c->value;
    return 0;
}

// ============================================================================
// Scoped Array Functions
// ============================================================================

// Find array in local scope only
static ScopeArray *scope_find_local_array(Scope *s, const char *name) {
    if (!s || !name) return NULL;
    for (int i = 0; i < s->n_arrays; i++) {
        if (strcmp(s->arrays[i].name, name) == 0) {
            return &s->arrays[i];
        }
    }
    return NULL;
}

// Lookup array in scope chain
ScopeArray *scope_lookup_array(Scope *s, const char *name) {
    Scope *search = s;
    int depth = 0;
    while (search) {
        ScopeArray *arr = scope_find_local_array(search, name);
        if (arr) {
            if (diag_is_enabled(DIAG_CALC)) {
                diag_log(DIAG_CALC, 2, "lookup array <%s>: found at depth %d", name, depth);
            }
            return arr;
        }
        search = search->parent;
        depth++;
    }
    if (diag_is_enabled(DIAG_CALC) && s) {
        diag_log(DIAG_CALC, 2, "lookup array <%s>: not found (searched %d scopes)", name, depth);
    }
    return NULL;
}

// Check if a name should be global (ALL_CAPS convention)
// Names like _RANGE_RESULT, _FILTER_RESULT go to global registry
// Names like _i, _v, arr go to current scope
static bool is_global_name(const char *name) {
    if (!name || !*name) return false;

    const char *p = name;
    // Skip leading underscore
    if (*p == '_') p++;

    // Must have at least one character after underscore
    if (!*p) return false;

    // Check if remaining chars are uppercase letters, digits, or underscores
    // Must have at least one uppercase letter
    bool has_upper = false;
    while (*p) {
        if (*p >= 'A' && *p <= 'Z') {
            has_upper = true;
        } else if (*p >= 'a' && *p <= 'z') {
            // Has lowercase - not a global name
            return false;
        } else if (*p != '_' && !(*p >= '0' && *p <= '9')) {
            // Not alphanumeric or underscore
            return false;
        }
        p++;
    }
    return has_upper;
}

// Create or get array in current scope
ScopeArray *scope_create_array(Scope *s, const char *name, bool is_counter) {
    if (!s || !name) return NULL;

    // Check if it already exists in this scope
    ScopeArray *existing = scope_find_local_array(s, name);
    if (existing) {
        // Clear existing elements
        for (int i = 0; i < existing->n_elements; i++) {
            free(existing->elements[i]);
        }
        existing->n_elements = 0;
        existing->is_counter_array = is_counter;
        return existing;
    }

    // Create new array in this scope
    if (s->n_arrays >= s->arrays_cap) {
        s->arrays_cap *= 2;
        s->arrays = realloc(s->arrays, s->arrays_cap * sizeof(ScopeArray));
    }

    ScopeArray *arr = &s->arrays[s->n_arrays++];
    arr->name = strdup(name);
    arr->elements = NULL;
    arr->n_elements = 0;
    arr->capacity = 0;
    arr->is_counter_array = is_counter;

    if (diag_is_enabled(DIAG_CALC)) {
        diag_log(DIAG_CALC, 1, "created scoped array <%s>", name);
    }

    return arr;
}

// Push element to scoped array
static void scope_array_push(ScopeArray *arr, const char *value) {
    if (!arr) return;
    if (arr->n_elements >= arr->capacity) {
        arr->capacity = arr->capacity ? arr->capacity * 2 : 8;
        arr->elements = realloc(arr->elements, arr->capacity * sizeof(char *));
    }
    arr->elements[arr->n_elements++] = strdup(value);
}

// Set element in scoped array
static bool scope_array_set(ScopeArray *arr, int index, const char *value) {
    if (!arr || index < 0 || index >= arr->n_elements) return false;
    free(arr->elements[index]);
    arr->elements[index] = strdup(value);
    return true;
}

// Pop element from scoped array (removes and returns last element)
static char *scope_array_pop(ScopeArray *arr) {
    if (!arr || arr->n_elements == 0) return NULL;
    char *value = arr->elements[--arr->n_elements];
    return value; // Caller takes ownership
}

// Peek at last element of scoped array (without removing)
static const char *scope_array_peek(ScopeArray *arr) {
    if (!arr || arr->n_elements == 0) return NULL;
    return arr->elements[arr->n_elements - 1];
}

// Dequeue element from scoped array (removes and returns first element)
static char *scope_array_dequeue(ScopeArray *arr) {
    if (!arr || arr->n_elements == 0) return NULL;
    char *value = arr->elements[0];
    // Shift all elements down
    for (int i = 1; i < arr->n_elements; i++) {
        arr->elements[i - 1] = arr->elements[i];
    }
    arr->n_elements--;
    return value; // Caller takes ownership
}

// Unified array lookup: check scopes first, then global registry
// Returns ScopeArray* (which is layout-compatible with Array*)
static ScopeArray *unified_find_array(Scope *scope, ArrayRegistry *global, const char *name) {
    // First check scope chain
    if (scope) {
        ScopeArray *arr = scope_lookup_array(scope, name);
        if (arr) return arr;
    }
    // Fall back to global registry (cast is safe since structures are identical)
    if (global) {
        Array *arr = find_array(global, name);
        if (arr) return (ScopeArray *)arr;
    }
    return NULL;
}

// Unified array length: check scopes first, then global
static int unified_array_len(Scope *scope, ArrayRegistry *global, const char *name) {
    ScopeArray *arr = unified_find_array(scope, global, name);
    return arr ? arr->n_elements : 0;
}

// Unified array get: check scopes first, then global
static const char *unified_array_get(Scope *scope, ArrayRegistry *global, const char *name,
                                     int index) {
    ScopeArray *arr = unified_find_array(scope, global, name);
    if (!arr || index < 0 || index >= arr->n_elements) return NULL;
    return arr->elements[index];
}

// ============================================================================
// Content Measurement Helpers
// ============================================================================

// Measure actual content width by scanning the rendered box for non-space chars
static int measure_actual_width(Box *box) {
    if (!box || !box->cells) return 1;
    int max_width = 0;
    for (int y = 0; y < box->h; y++) {
        // Find rightmost non-space character on this line
        int line_width = 0;
        for (int x = 0; x < box->w; x++) {
            uint32_t c = box->cells[y * box->w + x];
            // Check for actual content (not space, null, or NBSP)
            if (c != ' ' && c != 0 && c != 0xA0) {
                line_width = x + 1;
            }
        }
        if (line_width > max_width) {
            max_width = line_width;
        }
    }
    return max_width > 0 ? max_width : 1;
}

// Measure content span - from leftmost to rightmost non-space character
// This properly handles centered content like sum symbols where different
// rows have different starting positions
int measure_content_span(Box *box) {
    if (!box || !box->cells) return 1;
    int min_left = box->w; // Start with max
    int max_right = 0;

    for (int y = 0; y < box->h; y++) {
        int line_left = -1;
        int line_right = 0;
        for (int x = 0; x < box->w; x++) {
            uint32_t c = box->cells[y * box->w + x];
            if (c != ' ' && c != 0 && c != 0xA0) {
                if (line_left < 0) line_left = x;
                line_right = x + 1;
            }
        }
        if (line_left >= 0) {
            if (line_left < min_left) min_left = line_left;
            if (line_right > max_right) max_right = line_right;
        }
    }

    // Return the span from leftmost to rightmost content
    return (max_right > min_left) ? (max_right - min_left) : 1;
}

// Check if content is pure math (starts and ends with $ or $$)
static bool is_pure_math(const char *content, bool *is_display) {
    while (*content == ' ' || *content == '\t' || *content == '\n') content++;
    if (content[0] == '$' && content[1] == '$') {
        *is_display = true;
        // Find closing $$
        const char *end = strstr(content + 2, "$$");
        if (end) {
            end += 2;
            while (*end == ' ' || *end == '\t' || *end == '\n') end++;
            return *end == '\0';
        }
    } else if (content[0] == '$') {
        *is_display = false;
        // Find closing $ (not $$)
        const char *p = content + 1;
        while (*p) {
            if (*p == '$' && *(p - 1) != '\\') {
                p++;
                while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                return *p == '\0';
            }
            p++;
        }
    }
    return false;
}

// Extract math content from delimiters
static char *extract_math(const char *content, bool is_display) {
    while (*content == ' ' || *content == '\t' || *content == '\n') content++;
    int start = is_display ? 2 : 1;
    const char *end = is_display ? strstr(content + 2, "$$") : NULL;
    if (!is_display) {
        const char *p = content + 1;
        while (*p && !(*p == '$' && *(p - 1) != '\\')) p++;
        end = p;
    }
    if (!end) return NULL;
    size_t len = end - (content + start);
    char *math = malloc(len + 1);
    memcpy(math, content + start, len);
    math[len] = '\0';
    return math;
}

// Measure content dimensions by actually rendering it
// This properly handles math, unicode, and other complex content
// If user_macros is provided, content is pre-expanded using those macros
static void measure_content(const char *content, MacroRegistry *user_macros, int *out_width,
                            int *out_height) {
    *out_width = 0;
    *out_height = 1;

    if (!content || !*content) return;

    // Log the content being measured
    diag_log(DIAG_CALC, 0, "measure_content: input='%.60s%s'", content,
             strlen(content) > 60 ? "..." : "");

    // Pre-expand content using user macros if available
    // This ensures macros like \cmd are expanded before we try to measure
    char *expanded_content = NULL;
    if (user_macros && user_macros->n_macros > 0) {
        diag_log(DIAG_CALC, 1, "pre-expanding with %d user macros", user_macros->n_macros);
        char expand_err[256] = {0};
        expanded_content = macro_expand_all(content, user_macros, expand_err, sizeof(expand_err));
        if (expanded_content && strcmp(content, expanded_content) != 0) {
            diag_result(DIAG_CALC, 1, "expanded to '%.60s%s'", expanded_content,
                        strlen(expanded_content) > 60 ? "..." : "");
        }
    } else {
        diag_log(DIAG_CALC, 1, "no user macros available for pre-expansion");
    }
    const char *to_measure = expanded_content ? expanded_content : content;

    // Save current render settings and enable unicode for accurate measurement
    // (Macro expansion happens before \setunicode is processed, so we need
    // to explicitly enable unicode here to get correct widths for math)
    int saved_unicode = get_unicode_mode();
    int saved_cursive = get_math_cursive_mode();
    RenderMode saved_render_mode = get_render_mode();
    set_unicode_mode(1);
    set_math_cursive_mode(1);
    set_render_mode(MODE_UNICODE);

    // Check if content is pure math - render directly for accurate measurement
    bool is_display = false;
    if (is_pure_math(to_measure, &is_display)) {
        diag_log(DIAG_CALC, 1, "detected pure math (%s mode)", is_display ? "display" : "inline");
        char *math_src = extract_math(to_measure, is_display);
        if (math_src) {
            ParseError err = {0};
            Ast *ast = parse_math(math_src, &err);
            free(math_src);
            if (ast) {
                Box box = render_ast(ast);
                ast_free(ast);
                // Use full box width for measurement to ensure alignment doesn't clip
                // centered content (like sum symbols)
                *out_width = box.w;
                *out_height = box.h;
                diag_result(DIAG_CALC, 0, "measured math: width=%d, height=%d", *out_width,
                            *out_height);
                free(box.cells);
                if (box.meta) free(box.meta);
                // Restore settings
                set_unicode_mode(saved_unicode);
                set_math_cursive_mode(saved_cursive);
                set_render_mode(saved_render_mode);
                if (expanded_content) free(expanded_content);
                return;
            }
        }
    }

    // Protect any \verb blocks in the content before parsing
    // This ensures verb content from macro expansion gets properly handled
    char *protected_content = protect_verbatim(to_measure);
    const char *parse_input = protected_content ? protected_content : to_measure;

    // Parse the content through the document parser
    // Use a large width to avoid artificial wrapping during measurement
    ParseError err = {0};
    BoxLayout *layout = parse_document_as_vbox(parse_input, 1000, &err);
    if (!layout) {
        // Fallback: count characters
        *out_width = (int)strlen(to_measure);
        // Restore settings
        set_unicode_mode(saved_unicode);
        set_math_cursive_mode(saved_cursive);
        if (protected_content) free(protected_content);
        if (expanded_content) free(expanded_content);
        return;
    }

    // Render the layout to get actual dimensions
    // Use a large width to avoid line wrapping during measurement
    CompOptions measure_opt = {0};
    measure_opt.width = 1000;
    measure_opt.measuring_mode = true; // Skip alignment/centering during measurement
    measure_opt.hyphenate = false;     // Disable hyphenation for measurement
    Box *box = box_layout_render(layout, &measure_opt, &err);
    box_layout_free(layout);

    if (!box) {
        *out_width = (int)strlen(to_measure);
        // Restore settings
        set_unicode_mode(saved_unicode);
        set_math_cursive_mode(saved_cursive);
        if (protected_content) free(protected_content);
        if (expanded_content) free(expanded_content);
        return;
    }

    // Get actual content width (from leftmost to rightmost content)
    // Use content_span instead of actual_width to handle centered/aligned content correctly
    *out_width = measure_content_span(box);
    *out_height = box->h;

    diag_result(DIAG_CALC, 0, "measured: width=%d, height=%d", *out_width, *out_height);

    // Free the box
    free(box->cells);
    if (box->meta) free(box->meta);
    free(box);

    // Restore settings
    set_unicode_mode(saved_unicode);
    set_math_cursive_mode(saved_cursive);
    set_render_mode(saved_render_mode);
    if (protected_content) free(protected_content);
    if (expanded_content) free(expanded_content);
}

// ============================================================================
// Parsing Helpers
// ============================================================================

// Skip whitespace, return new position
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

// Strip leading/trailing whitespace (including newlines) from a string
// Returns newly allocated trimmed string (caller frees)
static char *strip_whitespace(const char *s) {
    if (!s) return NULL;

    // Skip leading whitespace
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;

    if (*s == '\0') {
        // All whitespace - return empty string
        char *result = malloc(1);
        result[0] = '\0';
        return result;
    }

    // Find end of string
    const char *end = s + strlen(s) - 1;

    // Skip trailing whitespace
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;

    // Copy trimmed content
    size_t len = end - s + 1;
    char *result = malloc(len + 1);
    memcpy(result, s, len);
    result[len] = '\0';
    return result;
}

// Parse angle-bracketed name: <name>
// Returns name (caller frees) or NULL if not found
static char *parse_angle_name(const char *p, int *consumed) {
    if (*p != '<') return NULL;
    p++;

    const char *start = p;
    while (*p && *p != '>' && *p != '\n') p++;
    if (*p != '>') return NULL;

    size_t len = p - start;
    if (len == 0) return NULL;

    char *name = malloc(len + 1);
    memcpy(name, start, len);
    name[len] = '\0';

    *consumed = (int)(p - start) + 2; // +2 for < and >
    return name;
}

// Parse angle-bracketed content with nested bracket support: <content>
// This handles cases like \len<\recall<arr>> where content contains nested brackets
// Returns content (caller frees) or NULL if not found
static char *parse_angle_content_nested(const char *p, int *consumed) {
    if (*p != '<') return NULL;
    p++;

    const char *start = p;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '<')
            depth++;
        else if (*p == '>')
            depth--;
        if (depth > 0) p++;
    }
    if (*p != '>' || depth != 0) return NULL;

    size_t len = p - start;
    if (len == 0) return NULL;

    char *content = malloc(len + 1);
    memcpy(content, start, len);
    content[len] = '\0';

    *consumed = (int)(p - start) + 2; // +2 for < and >
    return content;
}

// Process a lambda body in "compact mode" (from #{...})
// Strips: leading whitespace per line, trailing whitespace per line, newlines
// Result is a single line with all content concatenated
static char *process_compact_body(const char *body) {
    if (!body) return strdup("");

    size_t capacity = strlen(body) + 1;
    char *result = malloc(capacity);
    if (!result) {
// Memory allocation failed - return empty string to avoid crash
#ifdef __EMSCRIPTEN__
        fprintf(stderr, "MALLOC FAILED in process_compact_body! capacity=%zu\n", capacity);
#endif
        return strdup("");
    }
    size_t result_len = 0;

    const char *p = body;
    while (*p) {
        // Skip leading whitespace of this line
        while (*p == ' ' || *p == '\t') p++;

        // Find end of line (or end of string)
        const char *line_start = p;
        while (*p && *p != '\n' && *p != '\r') p++;
        const char *line_end = p;

        // Trim trailing whitespace from line
        while (line_end > line_start && (line_end[-1] == ' ' || line_end[-1] == '\t')) line_end--;

        // Copy non-empty line content
        if (line_end > line_start) {
            size_t len = line_end - line_start;
            if (result_len + len + 1 > capacity) {
                capacity = (result_len + len + 1) * 2;
                char *new_result = realloc(result, capacity);
                if (!new_result) {
                    // Memory allocation failed - return what we have
                    result[result_len] = '\0';
                    return result;
                }
                result = new_result;
            }
            memcpy(result + result_len, line_start, len);
            result_len += len;
        }

        // Skip newline characters
        while (*p == '\n' || *p == '\r') p++;
    }

    result[result_len] = '\0';
    return result;
}

// Process a lambda body to handle % comment markers (like LaTeX)
// % at end of line consumes the rest of the line INCLUDING the newline
static char *process_comment_markers(const char *body) {
    if (!body) return strdup("");

    size_t capacity = strlen(body) + 1;
    char *result = malloc(capacity);
    if (!result) {
// Memory allocation failed - return empty string to avoid crash
#ifdef __EMSCRIPTEN__
        fprintf(stderr, "MALLOC FAILED in process_comment_markers! capacity=%zu\n", capacity);
#endif
        return strdup("");
    }
    size_t result_len = 0;

    const char *p = body;
    while (*p) {
        if (*p == '%') {
            // Skip % and everything until end of line (including newline)
            p++;
            while (*p && *p != '\n' && *p != '\r') p++;
            // Skip the newline itself
            if (*p == '\r') p++;
            if (*p == '\n') p++;
        } else {
            result[result_len++] = *p++;
        }
    }

    result[result_len] = '\0';
    return result;
}

// Parse single braced content: {content} - no comma splitting
// Returns content (caller frees), or NULL if not a braced arg
static char *parse_brace_content(const char *p, int *consumed) {
    if (*p != '{') return NULL;
    p++;

    const char *start = p;
    int depth = 1;
    size_t chars_scanned = 0;
    while (*p && depth > 0) {
        // Defensive: prevent infinite loop from memory corruption
        if (++chars_scanned > MAX_SCAN_CHARS) {
            fprintf(stderr, "parse_brace_content: scan limit exceeded (corruption?)\n");
            *consumed = 0;
            return NULL;
        }
        // Handle \verb blocks - skip over content without counting braces
        if (strncmp(p, "\\verb", 5) == 0 && p[5] != '\0' && !isalpha((unsigned char)p[5])) {
            p += 5; // Skip \verb
            char delim = *p;
            if (!delim) break; // Defensive: null delimiter
            p++;               // Skip delimiter
            // Skip until closing delimiter
            while (*p && *p != delim && chars_scanned++ < MAX_SCAN_CHARS) p++;
            if (*p == delim) p++; // Skip closing delimiter
            continue;
        }
        if (*p == '{')
            depth++;
        else if (*p == '}')
            depth--;
        if (depth > 0) p++;
    }
    if (depth != 0) return NULL;

    size_t len = p - start;
    char *content = malloc(len + 1);
    if (!content) {
#ifdef __EMSCRIPTEN__
        fprintf(stderr, "MALLOC FAILED in parse_brace_content! len=%zu\n", len);
#endif
        *consumed = 0;
        return NULL;
    }
    memcpy(content, start, len);
    content[len] = '\0';

    *consumed = (int)(p - start) + 2; // +2 for { and }
    return content;
}

// Parse array initializer: [elem1, elem2, ...]
// Returns array of strings and count, or NULL if empty/error
// For content arrays, elements can contain complex content
static char **parse_array_initializer(const char *p, int *n_elements, bool is_counter_array) {
    *n_elements = 0;

    // Skip whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n') p++;

    // Empty initializer is valid
    if (*p == '\0') return NULL;

    // Must start with [
    if (*p != '[') return NULL;
    p++;

    // Allocate array
    int capacity = 8;
    char **elements = malloc(capacity * sizeof(char *));
    int count = 0;

    while (*p && *p != ']') {
        // Skip whitespace
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (*p == ']') break;

        // Parse one element
        const char *elem_start = p;
        int depth = 0;

        // Find end of element (comma or ] at depth 0)
        while (*p && !(*p == ',' && depth == 0) && !(*p == ']' && depth == 0)) {
            if (*p == '{' || *p == '[' || *p == '(')
                depth++;
            else if (*p == '}' || *p == ']' || *p == ')')
                depth--;
            // Handle \verb blocks
            if (strncmp(p, "\\verb", 5) == 0 && p[5] != '\0' && !isalpha((unsigned char)p[5])) {
                p += 5;
                char delim = *p++;
                while (*p && *p != delim) p++;
                if (*p == delim) p++;
                continue;
            }
            p++;
        }

        // Extract element, trim whitespace
        const char *elem_end = p;
        while (elem_end > elem_start &&
               (elem_end[-1] == ' ' || elem_end[-1] == '\t' || elem_end[-1] == '\n'))
            elem_end--;
        while (elem_start < elem_end &&
               (*elem_start == ' ' || *elem_start == '\t' || *elem_start == '\n'))
            elem_start++;

        if (elem_end > elem_start) {
            // Grow array if needed
            if (count >= capacity) {
                capacity *= 2;
                elements = realloc(elements, capacity * sizeof(char *));
            }

            size_t len = elem_end - elem_start;
            char *elem = malloc(len + 1);
            memcpy(elem, elem_start, len);
            elem[len] = '\0';
            elements[count++] = elem;
        }

        // Skip comma
        if (*p == ',') p++;
    }

    *n_elements = count;
    return elements;
}

// Parse braced argument with comma-separated values: {a,b}
// Returns array of values (caller frees each + array), sets *n_args
// Returns NULL if not a braced arg
static char **parse_brace_args(const char *p, int *consumed, int *n_args) {
    *n_args = 0;
    if (*p != '{') return NULL;
    p++;

    const char *start = p;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '{')
            depth++;
        else if (*p == '}')
            depth--;
        if (depth > 0) p++;
    }
    if (depth != 0) return NULL;

    size_t content_len = p - start;
    char *content = malloc(content_len + 1);
    memcpy(content, start, content_len);
    content[content_len] = '\0';

    *consumed = (int)(p - start) + 2; // +2 for { and }

    // Split by commas (respecting nested braces)
    char **args = malloc(16 * sizeof(char *));
    int capacity = 16;
    int count = 0;

    const char *arg_start = content;
    const char *q = content;
    depth = 0;
    int bracket_depth = 0;

    while (*q) {
        if (*q == '{')
            depth++;
        else if (*q == '}')
            depth--;
        else if (*q == '[')
            bracket_depth++;
        else if (*q == ']')
            bracket_depth--;
        else if (*q == ',' && depth == 0 && bracket_depth == 0) {
            // Extract raw argument
            size_t arg_len = q - arg_start;
            char *raw = malloc(arg_len + 1);
            memcpy(raw, arg_start, arg_len);
            raw[arg_len] = '\0';
            // Strip whitespace (including newlines)
            args[count] = strip_whitespace(raw);
            free(raw);
            count++;
            if (count >= capacity) {
                capacity *= 2;
                args = realloc(args, capacity * sizeof(char *));
            }
            arg_start = q + 1;
        }
        q++;
    }

    // Last argument - also strip whitespace
    size_t arg_len = q - arg_start;
    char *raw = malloc(arg_len + 1);
    memcpy(raw, arg_start, arg_len);
    raw[arg_len] = '\0';
    args[count] = strip_whitespace(raw);
    free(raw);
    count++;

    free(content);
    *n_args = count;
    return args;
}

// Forward declare so we can use it recursively
static char *expand_calc_recursive(const char *s, CalcContext *ctx);
char *calc_try_expand_dollar(const char *p, int *end_pos, CalcContext *ctx);

// Parse a string as an integer
static int parse_int(const char *s) {
    // Skip whitespace (including newlines)
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;

    // Handle negative
    int sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    }

    int val = 0;
    while (isdigit((unsigned char)*s)) {
        val = val * 10 + (*s - '0');
        s++;
    }
    return sign * val;
}

// Expand all nested calc commands in a string, then parse as integer
static int eval_int_with_ctx(const char *s, CalcContext *ctx) {
    if (!s || !*s) return 0;

    // First recursively expand any calc commands
    char *expanded = expand_calc_recursive(s, ctx);
    int result = parse_int(expanded);
    free(expanded);
    return result;
}

// Recursively expand all calc commands in a string
static char *expand_calc_recursive(const char *s, CalcContext *ctx) {
    if (!s) return strdup("");
    if (!ctx) return strdup(s);

    // Check if there are any calc commands
    if (!calc_has_commands(s)) {
        return strdup(s);
    }

    // Expand in a loop until no more changes
    char *current = strdup(s);
    if (!current) {
#ifdef __EMSCRIPTEN__
        fprintf(stderr, "STRDUP FAILED in expand_calc_recursive!\n");
#endif
        return strdup("");
    }
    for (int iter = 0; iter < 20; iter++) { // limit iterations
        // Use dynamic buffer since \recall can return content much larger than input
        size_t result_capacity = strlen(current) * 2 + 256;
        size_t result_len = 0;
        char *result = malloc(result_capacity);
        if (!result) {
// Memory allocation failed - return what we have
#ifdef __EMSCRIPTEN__
            fprintf(stderr, "MALLOC FAILED in expand_calc_recursive! capacity=%zu\n",
                    result_capacity);
#endif
            return current;
        }
        const char *p = current;
        bool changed = false;
        size_t chars_scanned = 0;

        while (*p) {
            // Defensive: prevent infinite loop from memory corruption
            if (++chars_scanned > MAX_SCAN_CHARS) {
                fprintf(stderr, "expand_calc_recursive: scan limit exceeded (corruption?)\n");
                result[result_len] = '\0';
                free(current);
                return result;
            }
            if (*p == '\\') {
                int end_pos = 0;
                char error_msg[256] = {0};
                char *expanded = calc_try_expand(p, &end_pos, ctx, error_msg, sizeof(error_msg));
                if (expanded && end_pos > 0) {
                    size_t exp_len = strlen(expanded);
                    // Grow buffer if needed
                    while (result_len + exp_len + 1 > result_capacity) {
                        size_t new_capacity = result_capacity * 2;
                        char *new_result = safe_realloc(result, new_capacity);
                        if (!new_result) {
                            // Memory allocation failed - return what we have
                            free(expanded);
                            result[result_len] = '\0';
                            free(current);
                            return result;
                        }
                        result = new_result;
                        result_capacity = new_capacity;
                    }
                    memcpy(result + result_len, expanded, exp_len);
                    result_len += exp_len;
                    free(expanded);
                    p += end_pos;
                    changed = true;

                    // Check if \exit_when triggered immediate exit
                    if (ctx->exit_loop_requested) {
                        result[result_len] = '\0';
                        free(current);
                        return result;
                    }

                    continue;
                }
            }
            // Handle ${name} - CL-style variable access
            if (*p == '$' && *(p + 1) == '{') {
                int end_pos = 0;
                char *expanded = calc_try_expand_dollar(p, &end_pos, ctx);
                if (expanded && end_pos > 0) {
                    size_t exp_len = strlen(expanded);
                    // Grow buffer if needed
                    while (result_len + exp_len + 1 > result_capacity) {
                        size_t new_capacity = result_capacity * 2;
                        char *new_result = safe_realloc(result, new_capacity);
                        if (!new_result) {
                            free(expanded);
                            result[result_len] = '\0';
                            free(current);
                            return result;
                        }
                        result = new_result;
                        result_capacity = new_capacity;
                    }
                    memcpy(result + result_len, expanded, exp_len);
                    result_len += exp_len;
                    free(expanded);
                    p += end_pos;
                    changed = true;
                    continue;
                }
            }
            // Grow buffer if needed (for single char)
            if (result_len + 2 > result_capacity) {
                size_t new_capacity = result_capacity * 2;
                char *new_result = safe_realloc(result, new_capacity);
                if (!new_result) {
                    // Memory allocation failed - return what we have
                    result[result_len] = '\0';
                    free(current);
                    return result;
                }
                result = new_result;
                result_capacity = new_capacity;
            }
            result[result_len++] = *p++;
        }
        result[result_len] = '\0';

        free(current);
        current = result;

        if (!changed) break;
    }

    return current;
}

// ============================================================================
// Public Expansion API
// ============================================================================

// Initialize a CalcContext
void calc_context_init(CalcContext *ctx) {
    if (!ctx) return;
    // Legacy registries (kept for backwards compatibility)
    ctx->counters = counter_registry_new();
    ctx->contents = content_registry_new();
    ctx->arrays = array_registry_new();
    ctx->types = type_registry_new();

    // New scope-based system
    ctx->global_scope = scope_new(NULL);
    ctx->current_scope = ctx->global_scope;
    ctx->lambda_storage_scope = NULL; // Only set during iteration contexts

    ctx->user_macros = NULL;
    ctx->width = 80;
    ctx->in_math_mode = false;
    ctx->is_display_math = false;
    ctx->linebreaker = NULL;

    // Loop control
    ctx->exit_loop_requested = false;

    // Rvalue tracking for move semantics
    ctx->rvalue_counter = 0;
    ctx->active_rvalues = NULL;
    ctx->n_rvalues = 0;
    ctx->rvalues_cap = 0;

    // Return value handling for computational lambdas
    ctx->return_value = NULL;
    ctx->return_requested = false;
    ctx->in_computational = false;

    // LSP integration
    ctx->symbols = NULL;
    ctx->current_line = 1;
    ctx->current_col = 1;

    // Subnivean VM (lazy initialization on first computational lambda call)
    ctx->subnivean_vm = NULL;

    // Terminal settings
    ctx->clear_bg_code = -1; // -1 means use terminal default
}

// Helper: Break all lambda->scope cycles in a scope tree
// This must be called before freeing to handle circular references
static void scope_break_lambda_cycles(Scope *s) {
    if (!s) return;

    // Break cycles for all lambdas in this scope
    for (int i = 0; i < s->n_lambdas; i++) {
        Lambda *l = s->lambdas[i].lambda;
        if (l && l->captured_scope) {
            // Decref the captured scope (break the reference)
            scope_decref(l->captured_scope);
            l->captured_scope = NULL;
        }
    }
}

// Free resources in a CalcContext
void calc_context_free(CalcContext *ctx) {
    if (!ctx) return;
    // Legacy registries
    if (ctx->counters) counter_registry_free(ctx->counters);
    if (ctx->contents) content_registry_free(ctx->contents);
    if (ctx->arrays) array_registry_free(ctx->arrays);
    if (ctx->types) type_registry_free(ctx->types);

    // Break lambda cycles before freeing scope chain
    // Lambdas capture scopes, creating circular references that prevent cleanup
    if (ctx->global_scope) {
        scope_break_lambda_cycles(ctx->global_scope);
    }

    // Now scope can be freed (refcount should reach 0)
    if (ctx->global_scope) {
        scope_decref(ctx->global_scope);
    }

    // Rvalue tracking
    if (ctx->active_rvalues) {
        for (int i = 0; i < ctx->n_rvalues; i++) {
            free(ctx->active_rvalues[i]);
        }
        free(ctx->active_rvalues);
    }

    // Return value
    if (ctx->return_value) {
        free(ctx->return_value);
    }

    // Don't free user_macros - it's typically shared
    ctx->counters = NULL;
    ctx->contents = NULL;
    ctx->arrays = NULL;
    ctx->types = NULL;
    ctx->global_scope = NULL;
    ctx->current_scope = NULL;
    ctx->user_macros = NULL;
    ctx->active_rvalues = NULL;
    ctx->return_value = NULL;

#ifdef SUBNIVEAN_ENABLED
    // Free Subnivean VM
    if (ctx->subnivean_vm) {
        subnivean_vm_free((VM *)ctx->subnivean_vm);
        ctx->subnivean_vm = NULL;
    }
#endif
}

// ============================================================================
// Rvalue Management (for move semantics in computational lambdas)
// ============================================================================

// Generate a unique rvalue name (_RV_N) - ALL_CAPS to avoid hygienization
static char *rvalue_generate_name(CalcContext *ctx) {
    char buf[32];
    snprintf(buf, sizeof(buf), "_RV_%d", ctx->rvalue_counter++);
    return strdup(buf);
}

// Check if a name is an rvalue (starts with _RV_)
static bool is_rvalue_name(const char *name) {
    return name && strncmp(name, "_RV_", 4) == 0;
}

// Register an rvalue for potential cleanup
static void rvalue_register(CalcContext *ctx, const char *name) {
    if (!ctx || !name) return;

    // Grow array if needed
    if (ctx->n_rvalues >= ctx->rvalues_cap) {
        ctx->rvalues_cap = ctx->rvalues_cap ? ctx->rvalues_cap * 2 : 8;
        ctx->active_rvalues = realloc(ctx->active_rvalues, ctx->rvalues_cap * sizeof(char *));
    }

    ctx->active_rvalues[ctx->n_rvalues++] = strdup(name);

    if (diag_is_enabled(DIAG_CALC)) {
        diag_log(DIAG_CALC, 1, "rvalue registered: %s (total: %d)", name, ctx->n_rvalues);
    }
}

// Consume an rvalue (remove from cleanup list - it was moved/used)
static void rvalue_consume(CalcContext *ctx, const char *name) {
    if (!ctx || !name) return;

    for (int i = 0; i < ctx->n_rvalues; i++) {
        if (strcmp(ctx->active_rvalues[i], name) == 0) {
            free(ctx->active_rvalues[i]);
            // Shift remaining elements
            for (int j = i; j < ctx->n_rvalues - 1; j++) {
                ctx->active_rvalues[j] = ctx->active_rvalues[j + 1];
            }
            ctx->n_rvalues--;

            if (diag_is_enabled(DIAG_CALC)) {
                diag_log(DIAG_CALC, 1, "rvalue consumed: %s (remaining: %d)", name, ctx->n_rvalues);
            }
            return;
        }
    }
}

// Clean up all active rvalues (called at expression boundaries)
static void rvalue_cleanup_all(CalcContext *ctx) {
    if (!ctx || ctx->n_rvalues == 0) return;

    if (diag_is_enabled(DIAG_CALC)) {
        diag_log(DIAG_CALC, 1, "cleaning up %d unconsumed rvalues", ctx->n_rvalues);
    }

    for (int i = 0; i < ctx->n_rvalues; i++) {
        const char *name = ctx->active_rvalues[i];
        // Remove the array from global registry
        // TODO: Implement array removal
        if (diag_is_enabled(DIAG_CALC)) {
            diag_log(DIAG_CALC, 2, "  cleanup rvalue: %s", name);
        }
        free(ctx->active_rvalues[i]);
    }
    ctx->n_rvalues = 0;
}

#ifdef SUBNIVEAN_ENABLED
// Callback for Subnivean to look up arrays from interpreter context
static int subnivean_array_lookup_callback(void *ctx_ptr, const char *name, char ***elements,
                                           int *n_elements) {
    CalcContext *ctx = (CalcContext *)ctx_ptr;

    if (diag_is_enabled(DIAG_SUBNIVEAN)) {
        diag_log(DIAG_SUBNIVEAN, 1, "External array lookup: '%s'", name);
    }

    // Try with the name as-is
    ScopeArray *arr = unified_find_array(ctx->current_scope, ctx->arrays, name);

    // Try with _m0_ prefix if not found and name doesn't already have it
    char *hyg_name = NULL;
    if (!arr && name[0] != '_') {
        hyg_name = malloc(strlen(name) + 5);
        sprintf(hyg_name, "_m0_%s", name);
        arr = unified_find_array(ctx->current_scope, ctx->arrays, hyg_name);
        if (arr && diag_is_enabled(DIAG_SUBNIVEAN)) {
            diag_log(DIAG_SUBNIVEAN, 1, "  Found with hygiene prefix: %s", hyg_name);
        }
        free(hyg_name);
    }

    if (!arr) {
        if (diag_is_enabled(DIAG_SUBNIVEAN)) {
            diag_log(DIAG_SUBNIVEAN, 1, "  NOT FOUND");
        }
        return -1; // Not found
    }

    if (diag_is_enabled(DIAG_SUBNIVEAN)) {
        diag_log(DIAG_SUBNIVEAN, 1, "  Found, %d elements", arr->n_elements);
    }

    // Copy elements as strings
    *n_elements = arr->n_elements;
    *elements = malloc(arr->n_elements * sizeof(char *));
    for (int i = 0; i < arr->n_elements; i++) {
        (*elements)[i] = strdup(arr->elements[i]);
    }

    return 0; // Success
}

// Callback for Subnivean to set elements in external arrays
static int subnivean_array_set_callback(void *ctx_ptr, const char *name, int index,
                                        const char *value) {
    CalcContext *ctx = (CalcContext *)ctx_ptr;

    // Try with the name as-is
    ScopeArray *arr = unified_find_array(ctx->current_scope, ctx->arrays, name);

    // Try with _m0_ prefix if not found
    if (!arr && name[0] != '_') {
        char *hyg_name = malloc(strlen(name) + 5);
        sprintf(hyg_name, "_m0_%s", name);
        arr = unified_find_array(ctx->current_scope, ctx->arrays, hyg_name);
        free(hyg_name);
    }

    if (!arr) {
        return -1; // Array not found
    }

    if (index < 0 || index >= arr->n_elements) {
        return -1; // Index out of bounds
    }

    // Free old value and set new one
    if (arr->elements[index]) {
        free(arr->elements[index]);
    }
    arr->elements[index] = strdup(value);

    return 0; // Success
}

// Callback for Subnivean to compile external lambdas (for calling other lambdas)
static SubniveanFunction *subnivean_lambda_compile_callback(void *ctx_ptr, const char *name) {
    CalcContext *ctx = (CalcContext *)ctx_ptr;

    // Look up the lambda
    Lambda *lambda = scope_lookup_lambda(ctx->current_scope, name);

    // Try with _m0_ prefix if not found
    char *hyg_name = NULL;
    if (!lambda && name[0] != '_') {
        hyg_name = malloc(strlen(name) + 5);
        sprintf(hyg_name, "_m0_%s", name);
        lambda = scope_lookup_lambda(ctx->current_scope, hyg_name);
        free(hyg_name);
    }

    if (!lambda) {
        return NULL; // Lambda not found
    }

    // Only compile computational lambdas
    if (!lambda->is_computational) {
        return NULL; // Non-computational, let interpreter handle it
    }

    // Lazy compile if not yet compiled
    if (!lambda->compile_attempted) {
        lambda->compile_attempted = true;
        char error_msg[256] = {0};
        lambda->compiled =
            subnivean_compile((VM *)ctx->subnivean_vm, name, lambda->param_names,
                              lambda->param_count, lambda->body, error_msg, sizeof(error_msg));
        if (!lambda->compiled && diag_is_enabled(DIAG_SUBNIVEAN)) {
            diag_log(DIAG_SUBNIVEAN, 1, "External lambda '%s' compile failed: %s", name, error_msg);
        }
    }

    return lambda->compiled;
}

// Try to execute a computational lambda via Subnivean VM
// Returns result string on success, NULL if should fall back to interpreter
static char *try_subnivean_execute(CalcContext *ctx, Lambda *lambda, char **args, int n_args) {
    // Lazy init VM on first use
    if (!ctx->subnivean_vm) {
        ctx->subnivean_vm = subnivean_vm_new();
        if (!ctx->subnivean_vm) {
            return NULL; // VM creation failed, fall back
        }
    }

    VM *vm = (VM *)ctx->subnivean_vm;

    // Set the external callbacks so Subnivean can access interpreter resources
    subnivean_set_array_lookup(vm, subnivean_array_lookup_callback, ctx);
    subnivean_set_array_set(vm, subnivean_array_set_callback, ctx);
    subnivean_set_lambda_compile(vm, subnivean_lambda_compile_callback, ctx);

    // Lazy compile on first call
    if (!lambda->compile_attempted) {
        lambda->compile_attempted = true;
        char error_msg[256] = {0};
        lambda->compiled = subnivean_compile(vm,
                                             "lambda", // Anonymous name
                                             lambda->param_names, lambda->param_count, lambda->body,
                                             error_msg, sizeof(error_msg));
        if (!lambda->compiled) {
            // Always print compilation errors - computational lambdas MUST compile
            fprintf(stderr, "Subnivean compile failed: %s\n", error_msg);
        }
    }

    if (!lambda->compiled) {
        return NULL; // Compilation failed
    }

    // Execute via VM
    char *result = subnivean_execute(vm, lambda->compiled, (const char **)args, n_args);
    if (result && diag_is_enabled(DIAG_SUBNIVEAN)) {
        diag_log(DIAG_SUBNIVEAN, 0, "Subnivean executed lambda -> '%s'", result);
    }
    return result;
}
#endif

// Expand all calc commands in a string
char *expand_calc(const char *input, CalcContext *ctx) {
    return expand_calc_recursive(input, ctx);
}

// ============================================================================
// Command Detection
// ============================================================================

bool calc_has_commands(const char *input) {
    return strstr(input, "\\add") != NULL || strstr(input, "\\sub") != NULL ||
           strstr(input, "\\mul") != NULL || strstr(input, "\\div") != NULL ||
           strstr(input, "\\mod") != NULL || strstr(input, "\\eq") != NULL ||
           strstr(input, "\\ne") != NULL || strstr(input, "\\gt") != NULL ||
           strstr(input, "\\lt") != NULL || strstr(input, "\\ge") != NULL ||
           strstr(input, "\\le") != NULL || strstr(input, "\\max") != NULL ||
           strstr(input, "\\min") != NULL ||
           // Logical operators
           strstr(input, "\\and{") != NULL || strstr(input, "\\or{") != NULL ||
           strstr(input, "\\not{") != NULL || strstr(input, "\\let<") != NULL ||
           strstr(input, "\\inc<") != NULL || strstr(input, "\\dec<") != NULL ||
           strstr(input, "\\valueof<") != NULL || strstr(input, "\\width") != NULL ||
           strstr(input, "\\mathmode") != NULL || strstr(input, "\\displaymath") != NULL ||
           strstr(input, "\\unicode") != NULL || strstr(input, "\\getlinebreaker") != NULL ||
           strstr(input, "\\if{") != NULL || strstr(input, "\\measure<") != NULL ||
           strstr(input, "\\measureref<") != NULL || strstr(input, "\\recall<") != NULL ||
           strstr(input, "\\assign<") != NULL || strstr(input, "\\lambda<") != NULL ||
           // String primitives
           strstr(input, "\\streq{") != NULL || strstr(input, "\\startswith{") != NULL ||
           strstr(input, "\\endswith{") != NULL || strstr(input, "\\strlen{") != NULL ||
           strstr(input, "\\trim{") != NULL || strstr(input, "\\stripbraces{") != NULL ||
           strstr(input, "\\concat{") != NULL || strstr(input, "\\substr{") != NULL ||
           strstr(input, "\\indexof{") != NULL || strstr(input, "\\contains{") != NULL ||
           // Array operations
           strstr(input, "\\len<") != NULL || strstr(input, "\\push<") != NULL ||
           strstr(input, "\\pop<") != NULL || strstr(input, "\\peek<") != NULL ||
           strstr(input, "\\enqueue<") != NULL || strstr(input, "\\dequeue<") != NULL ||
           strstr(input, "\\setelement<") != NULL || strstr(input, "\\split<") != NULL ||
           // Loop and enumerate control
           strstr(input, "\\exit_when{") != NULL || strstr(input, "\\begin{loop}") != NULL ||
           strstr(input, "\\begin<") != NULL ||
           // Return from computational lambda
           strstr(input, "\\return{") != NULL ||
           // Diagnostics
           strstr(input, "\\diag_decompile<") != NULL ||
           // Terminal input (continuous mode)
           strstr(input, "\\haskey") != NULL || strstr(input, "\\getkey") != NULL ||
           strstr(input, "\\getmouseX") != NULL || strstr(input, "\\getmouseY") != NULL ||
           strstr(input, "\\getmousebutton") != NULL || strstr(input, "\\gettime") != NULL ||
           strstr(input, "\\time") != NULL || strstr(input, "\\rand{") != NULL ||
           strstr(input, "\\srand{") != NULL ||
           // Inline Subnivean assembly
           strstr(input, "\\sn{") != NULL ||
           // Persistent array store (Subnivean)
           strstr(input, "\\sn_array{") != NULL || strstr(input, "\\sn_setelement{") != NULL ||
           strstr(input, "\\sn_len{") != NULL ||
           // Map operations
           strstr(input, "\\map_get<") != NULL || strstr(input, "\\map_set<") != NULL ||
           strstr(input, "\\map_has<") != NULL || strstr(input, "\\map_del<") != NULL ||
           strstr(input, "\\map_len<") != NULL || strstr(input, "\\map_keys<") != NULL ||
           // Continuous mode control
           strstr(input, "\\wait{") != NULL || strstr(input, "\\exit") != NULL ||
           // ANSI escape codes and cursor control
           strstr(input, "\\esc") != NULL || strstr(input, "\\ansi") != NULL ||
           strstr(input, "\\cursor") != NULL || strstr(input, "\\clear") != NULL ||
           strstr(input, "\\setclearbg") != NULL ||
           // Terminal size
           strstr(input, "\\term_rows") != NULL || strstr(input, "\\term_cols") != NULL ||
           // New CL-style syntax aliases
           strstr(input, "\\invoke<") != NULL || strstr(input, "${") != NULL;
}

// ============================================================================
// Command Expansion
// ============================================================================

// Handle ${name} - unified variable access (CL-style alias for interpreter)
// Returns expanded value and sets end_pos, or NULL if not a ${...} pattern
char *calc_try_expand_dollar(const char *p, int *end_pos, CalcContext *ctx) {
    *end_pos = 0;

    // Must start with ${
    if (p[0] != '$' || p[1] != '{') {
        return NULL;
    }

    const char *q = p + 2;

    // Find closing brace (handle nested ${} for dynamic names like ${item${i}})
    int depth = 1;
    const char *name_start = q;
    while (*q && depth > 0) {
        if (*q == '$' && *(q + 1) == '{') {
            depth++;
            q += 2;
        } else if (*q == '{') {
            depth++;
            q++;
        } else if (*q == '}') {
            depth--;
            if (depth > 0) q++;
        } else {
            q++;
        }
    }

    if (*q != '}') {
        return NULL; // Unclosed ${
    }

    size_t name_len = q - name_start;
    char *name = malloc(name_len + 1);
    memcpy(name, name_start, name_len);
    name[name_len] = '\0';

    // Check if name contains nested ${...} - expand recursively
    if (strstr(name, "${")) {
        char *expanded_name = expand_calc(name, ctx);
        free(name);
        name = expanded_name;
    }

    *end_pos = (int)(q + 1 - p); // Include closing }

    // Try counter first (like \valueof) - check scope chain, then legacy registry
    bool found = false;
    int counter_val = scope_lookup_counter(ctx->current_scope, name, &found);
    if (!found && find_counter(ctx->counters, name)) {
        counter_val = counter_get(ctx->counters, name);
        found = true;
    }
    if (found) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", counter_val);
        free(name);
        return strdup(buf);
    }

    // Try with hygiene prefix for lowercase names
    if (name[0] >= 'a' && name[0] <= 'z') {
        char *hyg_name = malloc(strlen(name) + 5);
        sprintf(hyg_name, "_m0_%s", name);
        counter_val = scope_lookup_counter(ctx->current_scope, hyg_name, &found);
        if (found) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", counter_val);
            free(hyg_name);
            free(name);
            return strdup(buf);
        }

        // Try content with hygiene prefix
        const char *content = scope_lookup_value(ctx->current_scope, hyg_name);
        if (content) {
            free(hyg_name);
            free(name);
            return strdup(content);
        }
        free(hyg_name);
    }

    // Try content store (like \recall for values)
    const char *content = scope_lookup_value(ctx->current_scope, name);
    if (content) {
        free(name);
        return strdup(content);
    }

    // Try legacy content store
    const char *legacy_content = content_get(ctx->contents, name);
    if (legacy_content) {
        free(name);
        return strdup(legacy_content);
    }

    // Not found - return "0" for counter compatibility
    if (diag_is_enabled(DIAG_CALC)) {
        diag_log(DIAG_CALC, 0, "${%s}: not found, returning 0", name);
    }
    free(name);
    return strdup("0");
}

char *calc_try_expand(const char *p, int *end_pos, CalcContext *ctx, char *error_msg,
                      int error_size) {
    *end_pos = 0;

    // ========== \width ==========
    if (strncmp(p, "\\width", 6) == 0 && !isalpha((unsigned char)p[6])) {
        *end_pos = 6;
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", ctx->width);
        return strdup(buf);
    }

    // ========== \mathmode ==========
    // Returns 1 if inside math mode ($ or $$), 0 otherwise
    if (strncmp(p, "\\mathmode", 9) == 0 && !isalpha((unsigned char)p[9])) {
        *end_pos = 9;
        return strdup(ctx->in_math_mode ? "1" : "0");
    }

    // ========== \displaymath ==========
    // Returns 1 if inside display math ($$), 0 otherwise
    if (strncmp(p, "\\displaymath", 12) == 0 && !isalpha((unsigned char)p[12])) {
        *end_pos = 12;
        return strdup(ctx->is_display_math ? "1" : "0");
    }

    // ========== \unicode ==========
    // Returns 1 if unicode mode is enabled, 0 otherwise
    if (strncmp(p, "\\unicode", 8) == 0 && !isalpha((unsigned char)p[8])) {
        *end_pos = 8;
        return strdup(get_unicode_mode() ? "1" : "0");
    }

    // ========== \getlinebreaker ==========
    // Returns current linebreaker: "greedy", "knuth", or "raggedright"
    if (strncmp(p, "\\getlinebreaker", 15) == 0 && !isalpha((unsigned char)p[15])) {
        *end_pos = 15;
        return strdup(get_linebreaker_mode());
    }

    // ========== \return{value} ==========
    // Return a value from a computational (#{}) lambda.
    // For scalars: \return{value} - returns the value directly
    // For arrays: \return{\ref<arr>} - moves array to caller's scope as rvalue
    if (strncmp(p, "\\return{", 8) == 0) {
        const char *q = p + 8;
        int brace_depth = 1;
        const char *val_start = q;
        while (*q && brace_depth > 0) {
            if (*q == '{')
                brace_depth++;
            else if (*q == '}')
                brace_depth--;
            if (brace_depth > 0) q++;
        }

        if (*q == '}') {
            size_t val_len = q - val_start;
            char *val_str = malloc(val_len + 1);
            memcpy(val_str, val_start, val_len);
            val_str[val_len] = '\0';

            // Expand the value
            char *expanded = expand_calc(val_str, ctx);
            free(val_str);

            if (diag_is_enabled(DIAG_CALC)) {
                diag_log(DIAG_CALC, 0, "\\return{%s} in %s context", expanded ? expanded : "(null)",
                         ctx->in_computational ? "computational" : "non-computational");
            }

            // Check if returning an array (value looks like an array name)
            // If in computational mode, check if we're returning an array reference
            if (ctx->in_computational && expanded) {
                // Try to find array - first with hygiene prefix, then without
                // (stdlib lambdas don't have hygienized names, user lambdas do)
                char *lookup_name = expanded;
                bool needs_hyg = (expanded[0] >= 'a' && expanded[0] <= 'z');
                if (needs_hyg) {
                    lookup_name = malloc(strlen(expanded) + 5);
                    sprintf(lookup_name, "_m0_%s", expanded);
                }

                // Check if this is an array (exists in global registry)
                ScopeArray *arr = unified_find_array(ctx->current_scope, ctx->arrays, lookup_name);

                // If not found with hygiene and we added prefix, try without
                if (!arr && needs_hyg) {
                    arr = unified_find_array(ctx->current_scope, ctx->arrays, expanded);
                }
                if (arr) {
                    // Generate rvalue name and move the array
                    char *rv_name = rvalue_generate_name(ctx);

                    // Create new array in global registry with rvalue name
                    Array *new_arr = create_array(ctx->arrays, rv_name, false);
                    for (int i = 0; i < arr->n_elements; i++) {
                        array_push(new_arr, arr->elements[i]);
                    }

                    // Register for cleanup
                    rvalue_register(ctx, rv_name);

                    if (diag_is_enabled(DIAG_CALC)) {
                        diag_log(DIAG_CALC, 0, "  moved array '%s' to rvalue '%s' (%d elements)",
                                 lookup_name, rv_name, arr->n_elements);
                    }

                    // Set return value to the rvalue name
                    if (ctx->return_value) free(ctx->return_value);
                    ctx->return_value = rv_name;
                    ctx->return_requested = true;

                    if (needs_hyg) free(lookup_name);
                    free(expanded);
                    *end_pos = (int)(q + 1 - p);
                    return strdup(""); // Don't output anything
                }
                if (needs_hyg) free(lookup_name);
            }

            // Scalar return (or non-computational context)
            if (ctx->in_computational) {
                if (ctx->return_value) free(ctx->return_value);
                ctx->return_value = expanded ? strdup(expanded) : strdup("");
                ctx->return_requested = true;
                free(expanded);
                *end_pos = (int)(q + 1 - p);
                return strdup(""); // Don't output anything
            } else {
                // Not in computational mode - just return the value
                // (backwards compatibility, though this is technically misuse)
                *end_pos = (int)(q + 1 - p);
                return expanded ? expanded : strdup("");
            }
        }
    }

    // ========== \diag_decompile<name> ==========
    // Logs disassembly of a computational lambda's bytecode to diagnostics
#ifdef SUBNIVEAN_ENABLED
    if (strncmp(p, "\\diag_decompile<", 16) == 0) {
        const char *q = p + 15;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            // Look up the lambda
            Lambda *lambda = scope_lookup_lambda(ctx->current_scope, name);
            if (!lambda) {
                // Try with hygiene prefix
                char *hyg_name = malloc(strlen(name) + 5);
                sprintf(hyg_name, "_m0_%s", name);
                lambda = scope_lookup_lambda(ctx->current_scope, hyg_name);
                free(hyg_name);
            }

            if (lambda && lambda->is_computational && lambda->compiled) {
                // Disassemble to diagnostics
                char *disasm = subnivean_disassemble(lambda->compiled);
                if (disasm) {
                    diag_log(DIAG_SUBNIVEAN, 0, "Disassembly of \\lambda<%s>:", name);
                    // Log each line
                    const char *line = disasm;
                    while (*line) {
                        const char *eol = line;
                        while (*eol && *eol != '\n') eol++;
                        int len = (int)(eol - line);
                        if (len > 0) {
                            diag_log(DIAG_SUBNIVEAN, 1, "%.*s", len, line);
                        }
                        line = *eol ? eol + 1 : eol;
                    }
                    free(disasm);
                }
            } else if (lambda && lambda->is_computational && !lambda->compiled) {
                diag_log(DIAG_SUBNIVEAN, 0, "\\lambda<%s>: computational but not yet compiled",
                         name);
            } else if (lambda && !lambda->is_computational) {
                diag_log(DIAG_SUBNIVEAN, 0, "\\lambda<%s>: not a computational lambda (no #)",
                         name);
            } else {
                diag_log(DIAG_SUBNIVEAN, 0, "\\lambda<%s>: not found", name);
            }

            free(name);
            *end_pos = 15 + name_consumed;
            return strdup("");
        }
    }
#endif

    // ========== Arithmetic: \add{a,b}, \sub{a,b}, etc. ==========

#define TRY_BINOP(name, namelen, op)                                                               \
    if (strncmp(p, name, namelen) == 0 && p[namelen] == '{') {                                     \
        const char *q = p + namelen;                                                               \
        int consumed = 0;                                                                          \
        int n_args = 0;                                                                            \
        char **args = parse_brace_args(q, &consumed, &n_args);                                     \
        if (args && n_args == 2) {                                                                 \
            int a = eval_int_with_ctx(args[0], ctx);                                               \
            int b = eval_int_with_ctx(args[1], ctx);                                               \
            int result = op;                                                                       \
            free(args[0]);                                                                         \
            free(args[1]);                                                                         \
            free(args);                                                                            \
            *end_pos = namelen + consumed;                                                         \
            char buf[32];                                                                          \
            snprintf(buf, sizeof(buf), "%d", result);                                              \
            return strdup(buf);                                                                    \
        }                                                                                          \
        if (args) {                                                                                \
            for (int i = 0; i < n_args; i++) free(args[i]);                                        \
            free(args);                                                                            \
        }                                                                                          \
    }

    TRY_BINOP("\\add", 4, a + b)
    TRY_BINOP("\\sub", 4, a - b)
    TRY_BINOP("\\mul", 4, a * b)
    TRY_BINOP("\\div", 4, (b != 0 ? a / b : 0))
    TRY_BINOP("\\mod", 4, (b != 0 ? a % b : 0))
    TRY_BINOP("\\max", 4, (a > b ? a : b))
    TRY_BINOP("\\min", 4, (a < b ? a : b))

#undef TRY_BINOP

    // ========== Comparisons: \eq{a,b}, \gt{a,b}, etc. ==========

#define TRY_CMP(name, namelen, op)                                                                 \
    if (strncmp(p, name, namelen) == 0 && p[namelen] == '{') {                                     \
        const char *q = p + namelen;                                                               \
        int consumed = 0;                                                                          \
        int n_args = 0;                                                                            \
        char **args = parse_brace_args(q, &consumed, &n_args);                                     \
        if (args && n_args == 2) {                                                                 \
            int a = eval_int_with_ctx(args[0], ctx);                                               \
            int b = eval_int_with_ctx(args[1], ctx);                                               \
            int result = (op) ? 1 : 0;                                                             \
            free(args[0]);                                                                         \
            free(args[1]);                                                                         \
            free(args);                                                                            \
            *end_pos = namelen + consumed;                                                         \
            char buf[32];                                                                          \
            snprintf(buf, sizeof(buf), "%d", result);                                              \
            return strdup(buf);                                                                    \
        }                                                                                          \
        if (args) {                                                                                \
            for (int i = 0; i < n_args; i++) free(args[i]);                                        \
            free(args);                                                                            \
        }                                                                                          \
    }

    TRY_CMP("\\eq", 3, a == b)
    TRY_CMP("\\ne", 3, a != b)
    TRY_CMP("\\gt", 3, a > b)
    TRY_CMP("\\lt", 3, a < b)
    TRY_CMP("\\ge", 3, a >= b)
    TRY_CMP("\\le", 3, a <= b)

#undef TRY_CMP

    // ========== Logical operators: \and{a,b}, \or{a,b}, \not{a} ==========
    // \and{a,b} - returns 1 if both a and b are non-zero
    if (strncmp(p, "\\and{", 5) == 0) {
        const char *q = p + 4;
        int consumed = 0;
        int n_args = 0;
        char **args = parse_brace_args(q, &consumed, &n_args);
        if (args && n_args == 2) {
            int a = eval_int_with_ctx(args[0], ctx);
            int b = eval_int_with_ctx(args[1], ctx);
            int result = (a && b) ? 1 : 0;
            free(args[0]);
            free(args[1]);
            free(args);
            *end_pos = 4 + consumed;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", result);
            return strdup(buf);
        }
        if (args) {
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
        }
    }

    // \or{a,b} - returns 1 if either a or b is non-zero
    if (strncmp(p, "\\or{", 4) == 0) {
        const char *q = p + 3;
        int consumed = 0;
        int n_args = 0;
        char **args = parse_brace_args(q, &consumed, &n_args);
        if (args && n_args == 2) {
            int a = eval_int_with_ctx(args[0], ctx);
            int b = eval_int_with_ctx(args[1], ctx);
            int result = (a || b) ? 1 : 0;
            free(args[0]);
            free(args[1]);
            free(args);
            *end_pos = 3 + consumed;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", result);
            return strdup(buf);
        }
        if (args) {
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
        }
    }

    // \not{a} - returns 1 if a is zero, 0 otherwise
    if (strncmp(p, "\\not{", 5) == 0) {
        const char *q = p + 4;
        int consumed = 0;
        char *content = parse_brace_content(q, &consumed);
        if (content) {
            int a = eval_int_with_ctx(content, ctx);
            int result = (!a) ? 1 : 0;
            free(content);
            *end_pos = 4 + consumed;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", result);
            return strdup(buf);
        }
    }

    // ========== \let<name>{value} or \let<name[]>{[...]} or \let<name>#{[...]} ==========
    if (strncmp(p, "\\let<", 5) == 0) {
        const char *q = p + 4; // Start at '<'
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            q += name_consumed;
            q = skip_ws(q);

#ifdef SUBNIVEAN_ENABLED
            // Check for persistent map: \let<name>#{|k:v,...|}
            // Creates map in Subnivean persistent store, stores address in name
            if (strncmp(q, "#{|", 3) == 0) {
                q += 3; // Skip '#{|'
                // Find matching '|}'
                const char *content_start = q;
                while (*q && !(q[0] == '|' && q[1] == '}')) {
                    q++;
                }
                if (q[0] == '|' && q[1] == '}') {
                    size_t content_len = q - content_start;
                    char *content = malloc(content_len + 1);
                    memcpy(content, content_start, content_len);
                    content[content_len] = '\0';
                    q += 2; // Skip '|}'

                    // Create empty map in persistent store
                    int addr = sn_store_create_map();

                    // Parse key->value or key:value pairs
                    if (content_len > 0) {
                        char *saveptr;
                        char *pair = strtok_r(content, ",", &saveptr);
                        while (pair) {
                            // Skip whitespace
                            while (*pair == ' ' || *pair == '\t') pair++;
                            // Find separator: try -> first, then :
                            char *sep = strstr(pair, "->");
                            int sep_len = 2;
                            if (!sep) {
                                sep = strchr(pair, ':');
                                sep_len = 1;
                            }
                            if (sep) {
                                *sep = '\0';
                                char *key_str = pair;
                                char *val_str = sep + sep_len;
                                // Trim whitespace from key and value
                                while (*key_str == ' ' || *key_str == '\t') key_str++;
                                while (*val_str == ' ' || *val_str == '\t') val_str++;
                                char *key_end = key_str + strlen(key_str) - 1;
                                while (key_end > key_str && (*key_end == ' ' || *key_end == '\t'))
                                    *key_end-- = '\0';
                                char *val_end = val_str + strlen(val_str) - 1;
                                while (val_end > val_str && (*val_end == ' ' || *val_end == '\t'))
                                    *val_end-- = '\0';

                                int64_t key = eval_int_with_ctx(key_str, ctx);
                                int64_t val = eval_int_with_ctx(val_str, ctx);
                                sn_store_map_set(addr, key, val);
                            }
                            pair = strtok_r(NULL, ",", &saveptr);
                        }
                    }
                    free(content);

                    // Store address as counter value
                    scope_set_counter(ctx->current_scope, name, addr);
                    counter_set(ctx->counters, name, addr);

                    free(name);
                    *end_pos = (int)(q - p);
                    return strdup("");
                }
                free(name);
                return NULL;
            }

            // Check for persistent array: \let<name>#{[...]}
            // Or computational expression: \let<name>#{expr}
            // Creates array in Subnivean persistent store, stores address in name
            if (strncmp(q, "#{", 2) == 0) {
                q += 2; // Skip '#{'
                // Find matching '}'
                int depth = 1;
                const char *content_start = q;
                while (*q && depth > 0) {
                    if (*q == '{')
                        depth++;
                    else if (*q == '}')
                        depth--;
                    if (depth > 0) q++;
                }
                if (*q == '}') {
                    size_t content_len = q - content_start;
                    char *content = malloc(content_len + 1);
                    memcpy(content, content_start, content_len);
                    content[content_len] = '\0';
                    q++; // Skip '}'

                    // Check if content starts with '[' (after skipping whitespace)
                    const char *check = content;
                    while (*check == ' ' || *check == '\t' || *check == '\n') check++;

                    if (*check == '[') {
                        // Parse array initializer
                        int n_elems = 0;
                        char **elems = parse_array_initializer(content, &n_elems, true);
                        free(content);

                        // Check if all elements are integers or if any are strings
                        bool all_integers = true;
                        if (n_elems > 0) {
                            for (int i = 0; i < n_elems && all_integers; i++) {
                                char *endptr;
                                strtol(elems[i], &endptr, 10);
                                // Not an integer if there are leftover chars or empty string
                                if (*endptr != '\0' || elems[i][0] == '\0') {
                                    all_integers = false;
                                }
                            }
                        }

                        int addr;
                        if (all_integers) {
                            // Convert to int64_t array and create integer array
                            int64_t *int_elems = NULL;
                            if (n_elems > 0) {
                                int_elems = malloc(n_elems * sizeof(int64_t));
                                for (int i = 0; i < n_elems; i++) {
                                    int_elems[i] = strtol(elems[i], NULL, 10);
                                    free(elems[i]);
                                }
                                free(elems);
                            }
                            addr = sn_store_create_array(int_elems, n_elems);
                            free(int_elems);
                        } else {
                            // Create string array
                            addr = sn_store_create_string_array((const char **)elems, n_elems);
                            // Free elements (sn_store_create_string_array strdup's them)
                            for (int i = 0; i < n_elems; i++) {
                                free(elems[i]);
                            }
                            free(elems);
                        }

                        // Store address as counter value
                        scope_set_counter(ctx->current_scope, name, addr);
                        counter_set(ctx->counters, name, addr);

                        free(name);
                        *end_pos = (int)(q - p);
                        return strdup("");
                    } else {
                        // Computational expression: evaluate and store result
                        // Process compact mode (strip whitespace/newlines)
                        char *compact = process_compact_body(content);
                        free(content);

                        // Evaluate in computational mode
                        bool saved_in_computational = ctx->in_computational;
                        char *saved_return_value = ctx->return_value;
                        bool saved_return_requested = ctx->return_requested;

                        ctx->in_computational = true;
                        ctx->return_value = NULL;
                        ctx->return_requested = false;

                        char *eval_result = expand_calc(compact, ctx);
                        free(compact);

                        // Get the return value or use empty string
                        char *result_value = NULL;
                        if (ctx->return_requested && ctx->return_value) {
                            result_value = strdup(ctx->return_value);
                        } else if (eval_result && eval_result[0]) {
                            result_value = strdup(eval_result);
                        } else {
                            result_value = strdup("");
                        }
                        free(eval_result);

                        // Restore computational state
                        if (ctx->return_value) free(ctx->return_value);
                        ctx->in_computational = saved_in_computational;
                        ctx->return_value = saved_return_value;
                        ctx->return_requested = saved_return_requested;

                        // Store result as value (for \recall) and counter (for \valueof)
                        scope_bind_value(ctx->current_scope, name, result_value);
                        // Try to convert to integer for counter storage
                        char *endptr;
                        long int_val = strtol(result_value, &endptr, 10);
                        if (*endptr == '\0' && result_value[0] != '\0') {
                            // Valid integer - store as counter
                            scope_set_counter(ctx->current_scope, name, (int)int_val);
                            counter_set(ctx->counters, name, (int)int_val);
                        }
                        free(result_value);

                        free(name);
                        *end_pos = (int)(q - p);
                        return strdup("");
                    }
                }
                free(name);
                return NULL;
            }
#endif // SUBNIVEAN_ENABLED

            // Check if this is a map (name ends with ||)
            size_t name_len = strlen(name);
            bool is_map = (name_len >= 2 && name[name_len - 2] == '|' && name[name_len - 1] == '|');

#ifdef SUBNIVEAN_ENABLED
            if (is_map) {
                // Strip || from name
                name[name_len - 2] = '\0';

                // Parse {|...|}  map initializer
                if (strncmp(q, "{|", 2) == 0) {
                    const char *content_start = q + 2;
                    const char *end = content_start;
                    while (*end && !(end[0] == '|' && end[1] == '}')) {
                        end++;
                    }
                    if (end[0] == '|' && end[1] == '}') {
                        size_t content_len = end - content_start;
                        char *content = malloc(content_len + 1);
                        memcpy(content, content_start, content_len);
                        content[content_len] = '\0';

                        // Create map in persistent store
                        int addr = sn_store_create_map();

                        // Parse key:value pairs
                        if (content_len > 0) {
                            char *saveptr;
                            char *pair = strtok_r(content, ",", &saveptr);
                            while (pair) {
                                // Skip whitespace
                                while (*pair == ' ' || *pair == '\t') pair++;
                                // Find colon separator
                                char *colon = strchr(pair, ':');
                                if (colon) {
                                    *colon = '\0';
                                    char *key_str = pair;
                                    char *val_str = colon + 1;
                                    // Trim whitespace
                                    while (*key_str == ' ' || *key_str == '\t') key_str++;
                                    while (*val_str == ' ' || *val_str == '\t') val_str++;
                                    char *key_end = key_str + strlen(key_str) - 1;
                                    while (key_end > key_str &&
                                           (*key_end == ' ' || *key_end == '\t'))
                                        *key_end-- = '\0';
                                    char *val_end = val_str + strlen(val_str) - 1;
                                    while (val_end > val_str &&
                                           (*val_end == ' ' || *val_end == '\t'))
                                        *val_end-- = '\0';

                                    int64_t key = eval_int_with_ctx(key_str, ctx);
                                    int64_t val = eval_int_with_ctx(val_str, ctx);
                                    sn_store_map_set(addr, key, val);
                                }
                                pair = strtok_r(NULL, ",", &saveptr);
                            }
                        }
                        free(content);

                        // Store address as counter value
                        scope_set_counter(ctx->current_scope, name, addr);
                        counter_set(ctx->counters, name, addr);

                        free(name);
                        *end_pos = (int)(end + 2 - p); // +2 for |}
                        return strdup("");
                    }
                }
                free(name);
                return NULL;
            }
#endif // SUBNIVEAN_ENABLED

            // Check if this is an array (name ends with [])
            bool is_array =
                (name_len >= 2 && name[name_len - 2] == '[' && name[name_len - 1] == ']');

            if (is_array) {
                // Strip [] from name
                name[name_len - 2] = '\0';

                // Parse brace content as array initializer
                int brace_consumed = 0;
                char *content = parse_brace_content(q, &brace_consumed);
                if (content) {
                    // Arrays always go to global registry to ensure they're accessible
                    // across scopes (e.g., when passed to stdlib functions).
                    // Scalar variables (counters, content) are scoped.
                    bool use_global = true;
                    (void)is_global_name; // Suppress unused warning

                    // Parse and add elements
                    int n_elems = 0;
                    char **elems = parse_array_initializer(content, &n_elems, true);

                    if (use_global) {
                        // Create in global registry for ALL_CAPS names
                        Array *arr = find_array(ctx->arrays, name);
                        if (arr) {
                            // Clear existing array
                            for (int i = 0; i < arr->n_elements; i++) free(arr->elements[i]);
                            arr->n_elements = 0;
                        } else {
                            arr = create_array(ctx->arrays, name, true);
                        }
                        if (elems) {
                            for (int i = 0; i < n_elems; i++) {
                                int val = eval_int_with_ctx(elems[i], ctx);
                                char buf[32];
                                snprintf(buf, sizeof(buf), "%d", val);
                                array_push(arr, buf);
                                free(elems[i]);
                            }
                            free(elems);
                        }
                        // Register type for hygienized names
                        if (name[0] == '_' && name[1] == 'm') {
                            type_registry_set(ctx->types, name, 'a');
                        }
                    } else {
                        // Create in current scope for lowercase names
                        ScopeArray *arr = scope_create_array(ctx->current_scope, name, true);
                        if (elems) {
                            for (int i = 0; i < n_elems; i++) {
                                int val = eval_int_with_ctx(elems[i], ctx);
                                char buf[32];
                                snprintf(buf, sizeof(buf), "%d", val);
                                scope_array_push(arr, buf);
                                free(elems[i]);
                            }
                            free(elems);
                        }
                        // Register type for hygienized names
                        if (name[0] == '_' && name[1] == 'm') {
                            type_registry_set(ctx->types, name, 'a');
                        }
                    }

                    free(content);
                    free(name);
                    *end_pos =
                        4 + name_consumed + (int)(q - (p + 4 + name_consumed)) + brace_consumed;
                    return strdup("");
                }
            } else {
                // Scalar value — polymorphic: numeric → counter + string, non-numeric → string only
                // Use parse_brace_content (not parse_brace_args) to avoid splitting on commas
                // inside angle brackets, e.g. \measure<c,w,h>{...} from expanded macros
                int brace_consumed = 0;
                char *content = parse_brace_content(q, &brace_consumed);
                if (content) {
                    // Expand the value first (this may call lambdas, etc.)
                    char *expanded = expand_calc(content, ctx);
                    free(content);

                    // Check if value is numeric
                    char *endptr;
                    long int_val = strtol(expanded, &endptr, 10);
                    bool is_numeric = (*endptr == '\0' && expanded[0] != '\0');

                    if (is_numeric) {
                        // Numeric: set counter in scope chain and legacy registry
                        scope_set_counter(ctx->current_scope, name, (int)int_val);
                        counter_set(ctx->counters, name, (int)int_val);
                    } else {
                        // Non-numeric content: store in legacy content store (like \assign)
                        // so \recall<name> and ${name} can retrieve it
                        content_store(ctx->contents, name, expanded, strlen(expanded), 1);
                    }

                    // Always store the expanded string value so \recall<name> can retrieve it
                    scope_bind_value(ctx->current_scope, name, expanded);
                    // Register type for hygienized names (optimization for \recall lookups)
                    if (name[0] == '_' && name[1] == 'm') {
                        type_registry_set(ctx->types, name, 'v');
                    }
                    free(expanded);

                    free(name);

                    *end_pos =
                        4 + name_consumed + (int)(q - (p + 4 + name_consumed)) + brace_consumed;
                    return strdup(""); // \let produces no output
                }
            }
            free(name);
        }
    }

    // ========== \inc<name> ==========
    // Increments counter, produces no output (like \let)
    if (strncmp(p, "\\inc<", 5) == 0) {
        const char *q = p + 4;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            // Increment in scope chain and legacy registry
            scope_inc_counter(ctx->current_scope, name);
            counter_inc(ctx->counters, name);
            free(name);
            *end_pos = 4 + name_consumed;
            return strdup(""); // No output
        }
    }

    // ========== \dec<name> ==========
    // Decrements counter, produces no output (like \let)
    if (strncmp(p, "\\dec<", 5) == 0) {
        const char *q = p + 4;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            // Decrement in scope chain and legacy registry
            scope_dec_counter(ctx->current_scope, name);
            counter_dec(ctx->counters, name);
            free(name);
            *end_pos = 4 + name_consumed;
            return strdup(""); // No output
        }
    }

    // ========== \valueof<name> or \valueof<name>[index] ==========
    if (strncmp(p, "\\valueof<", 9) == 0) {
        const char *q = p + 8;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            q += name_consumed;

            // Check for [index] suffix (array access)
            if (*q == '[') {
                q++; // Skip '['
                const char *idx_start = q;
                int depth = 1;
                while (*q && depth > 0) {
                    if (*q == '[')
                        depth++;
                    else if (*q == ']')
                        depth--;
                    if (depth > 0) q++;
                }
                if (*q == ']') {
                    // Parse index
                    size_t idx_len = q - idx_start;
                    char *idx_str = malloc(idx_len + 1);
                    memcpy(idx_str, idx_start, idx_len);
                    idx_str[idx_len] = '\0';

                    int index = eval_int_with_ctx(idx_str, ctx);
                    free(idx_str);
                    q++; // Skip ']'

                    // Look up array (scope first, then global)
                    ScopeArray *arr = unified_find_array(ctx->current_scope, ctx->arrays, name);
                    free(name);

                    if (arr && index >= 0 && index < arr->n_elements) {
                        *end_pos = (int)(q - p);
                        return strdup(arr->elements[index]);
                    } else {
                        // Out of bounds or array not found
                        *end_pos = (int)(q - p);
                        return strdup("0");
                    }
                }
            }

            // Scalar counter - check scope chain first, then legacy registry
            bool found = false;
            int val = scope_lookup_counter(ctx->current_scope, name, &found);
            if (!found) {
                val = counter_get(ctx->counters, name);
            }
            free(name);
            *end_pos = 8 + name_consumed;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", val);
            return strdup(buf);
        }
    }

    // ========== \len<name> ==========
    // Returns the length of an array
    // Supports nested content like \len<\recall<arr>>
    if (strncmp(p, "\\len<", 5) == 0) {
        const char *q = p + 4;
        int content_consumed = 0;
        char *content = parse_angle_content_nested(q, &content_consumed);
        if (content) {
            // Expand any calc commands in the content (e.g., \recall<arr>)
            char *name_expanded = expand_calc(content, ctx);
            free(content);

            // Try to find array - first with hygiene prefix, then without
            // (stdlib lambdas don't have hygienized names, user lambdas do)
            char *hyg_name = NULL;
            bool needs_hyg = (name_expanded[0] >= 'a' && name_expanded[0] <= 'z');
            if (needs_hyg) {
                hyg_name = malloc(strlen(name_expanded) + 5);
                sprintf(hyg_name, "_m0_%s", name_expanded);
            }

            ScopeArray *arr = unified_find_array(ctx->current_scope, ctx->arrays,
                                                 needs_hyg ? hyg_name : name_expanded);

            // If not found with hygiene and we added prefix, try without
            if (!arr && needs_hyg) {
                arr = unified_find_array(ctx->current_scope, ctx->arrays, name_expanded);
            }
            int len = arr ? arr->n_elements : 0;
            diag_log(DIAG_CALC, 0, "\\len<%s> -> %d", name_expanded, len);
            if (hyg_name) free(hyg_name);
            free(name_expanded);
            *end_pos = 4 + content_consumed;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", len);
            return strdup(buf);
        }
    }

    // ========== \push<name>{value} ==========
    // Appends value to end of array (stack push)
    if (strncmp(p, "\\push<", 6) == 0) {
        const char *q = p + 5;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            q += name_consumed;
            q = skip_ws(q);

            int brace_consumed = 0;
            char *content = parse_brace_content(q, &brace_consumed);
            if (content) {
                // Expand any calc commands in the content before storing
                char *expanded_content = expand_calc(content, ctx);
                free(content);
                content = expanded_content;

                ScopeArray *arr = unified_find_array(ctx->current_scope, ctx->arrays, name);
                if (!arr) {
                    // Create in global registry (arrays are always global)
                    Array *global_arr = create_array(ctx->arrays, name, false);
                    arr = (ScopeArray *)global_arr;
                }
                scope_array_push(arr, content);
                diag_log(DIAG_CALC, 0, "\\push<%s>{'%.40s%s'}", name, content,
                         strlen(content) > 40 ? "..." : "");

                free(content);
                free(name);
                *end_pos = 5 + name_consumed + (int)(q - (p + 5 + name_consumed)) + brace_consumed;
                return strdup("");
            }
            free(name);
        }
    }

    // ========== \pop<name> ==========
    // Removes and returns last element from array (stack pop)
    if (strncmp(p, "\\pop<", 5) == 0) {
        const char *q = p + 4;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            ScopeArray *arr = unified_find_array(ctx->current_scope, ctx->arrays, name);
            char *value = arr ? scope_array_pop(arr) : NULL;
            diag_log(DIAG_CALC, 0, "\\pop<%s> -> '%s'", name, value ? value : "(empty)");
            free(name);
            *end_pos = 4 + name_consumed;
            if (value) {
                return value; // Transfer ownership
            }
            return strdup("");
        }
    }

    // ========== \peek<name> ==========
    // Returns last element without removing (stack peek)
    if (strncmp(p, "\\peek<", 6) == 0) {
        const char *q = p + 5;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            ScopeArray *arr = unified_find_array(ctx->current_scope, ctx->arrays, name);
            const char *value = arr ? scope_array_peek(arr) : NULL;
            diag_log(DIAG_CALC, 0, "\\peek<%s> -> '%s'", name, value ? value : "(empty)");
            free(name);
            *end_pos = 5 + name_consumed;
            return strdup(value ? value : "");
        }
    }

    // ========== \enqueue<name>{value} ==========
    // Appends value to end of array (queue enqueue - same as push)
    if (strncmp(p, "\\enqueue<", 9) == 0) {
        const char *q = p + 8;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            q += name_consumed;
            q = skip_ws(q);

            int brace_consumed = 0;
            char *content = parse_brace_content(q, &brace_consumed);
            if (content) {
                // Expand any calc commands in the content before storing
                char *expanded_content = expand_calc(content, ctx);
                free(content);
                content = expanded_content;

                ScopeArray *arr = unified_find_array(ctx->current_scope, ctx->arrays, name);
                if (!arr) {
                    // Create in global registry (arrays are always global)
                    Array *global_arr = create_array(ctx->arrays, name, false);
                    arr = (ScopeArray *)global_arr;
                }
                scope_array_push(arr, content); // enqueue = push to back
                diag_log(DIAG_CALC, 0, "\\enqueue<%s>{'%.40s%s'}", name, content,
                         strlen(content) > 40 ? "..." : "");

                free(content);
                free(name);
                *end_pos = 8 + name_consumed + (int)(q - (p + 8 + name_consumed)) + brace_consumed;
                return strdup("");
            }
            free(name);
        }
    }

    // ========== \dequeue<name> ==========
    // Removes and returns first element from array (queue dequeue)
    if (strncmp(p, "\\dequeue<", 9) == 0) {
        const char *q = p + 8;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            ScopeArray *arr = unified_find_array(ctx->current_scope, ctx->arrays, name);
            char *value = arr ? scope_array_dequeue(arr) : NULL;
            diag_log(DIAG_CALC, 0, "\\dequeue<%s> -> '%s'", name, value ? value : "(empty)");
            free(name);
            *end_pos = 8 + name_consumed;
            if (value) {
                return value; // Transfer ownership
            }
            return strdup("");
        }
    }

    // ========== \setelement<name>[index]{value} ==========
    // Sets array element at index to value
    if (strncmp(p, "\\setelement<", 12) == 0) {
        const char *q = p + 11;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            q += name_consumed;
            q = skip_ws(q);

            // Parse [index]
            if (*q == '[') {
                q++;
                const char *idx_start = q;
                int bracket_depth = 1;
                while (*q && bracket_depth > 0) {
                    if (*q == '[')
                        bracket_depth++;
                    else if (*q == ']')
                        bracket_depth--;
                    if (bracket_depth > 0) q++;
                }
                if (bracket_depth == 0) {
                    size_t idx_len = q - idx_start;
                    char *idx_str = malloc(idx_len + 1);
                    memcpy(idx_str, idx_start, idx_len);
                    idx_str[idx_len] = '\0';
                    q++; // Skip ']'

                    // Expand and evaluate index
                    char *expanded_idx = expand_calc(idx_str, ctx);
                    int index = atoi(expanded_idx);
                    free(expanded_idx);
                    free(idx_str);

                    q = skip_ws(q);

                    // Parse {value}
                    int brace_consumed = 0;
                    char *content = parse_brace_content(q, &brace_consumed);
                    if (content) {
                        // Expand any calc commands in the content
                        char *expanded_content = expand_calc(content, ctx);
                        free(content);
                        content = expanded_content;

                        ScopeArray *arr = unified_find_array(ctx->current_scope, ctx->arrays, name);
                        if (arr) {
                            if (scope_array_set(arr, index, content)) {
                                diag_log(DIAG_CALC, 0, "\\setelement<%s>[%d]{'%.40s%s'}", name,
                                         index, content, strlen(content) > 40 ? "..." : "");
                            } else {
                                diag_log(DIAG_CALC, 0,
                                         "\\setelement<%s>[%d] - index out of bounds (len=%d)",
                                         name, index, arr->n_elements);
                            }
                        } else {
                            diag_log(DIAG_CALC, 0, "\\setelement<%s> - array not found", name);
                        }

                        free(content);
                        free(name);
                        *end_pos = (int)(q - p) + brace_consumed;
                        return strdup("");
                    }
                }
            }
            free(name);
        }
    }

    // ========== \if{cond}{true}\else{false} ==========
    if (strncmp(p, "\\if{", 4) == 0) {
        const char *q = p + 3; // Start at first '{'

        // Parse condition
        int cond_consumed = 0;
        int n_cond = 0;
        char **cond_args = parse_brace_args(q, &cond_consumed, &n_cond);
        if (!cond_args || n_cond < 1) {
            if (cond_args) {
                for (int i = 0; i < n_cond; i++) free(cond_args[i]);
                free(cond_args);
            }
            return NULL;
        }

        int cond_val = eval_int_with_ctx(cond_args[0], ctx);
        for (int i = 0; i < n_cond; i++) free(cond_args[i]);
        free(cond_args);

        q += cond_consumed;
        q = skip_ws(q);

        // Parse true branch
        int true_consumed = 0;
        int n_true = 0;
        char **true_args = parse_brace_args(q, &true_consumed, &n_true);
        if (!true_args || n_true < 1) {
            if (true_args) {
                for (int i = 0; i < n_true; i++) free(true_args[i]);
                free(true_args);
            }
            return NULL;
        }

        char *true_branch = strdup(true_args[0]);
        for (int i = 0; i < n_true; i++) free(true_args[i]);
        free(true_args);

        q += true_consumed;

        // Skip whitespace including newlines before potential \else
        while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;

        // Check for \else
        char *false_branch = NULL;
        int else_consumed = 0;

        if (strncmp(q, "\\else", 5) == 0) {
            q += 5;
            q = skip_ws(q);

            int false_consumed = 0;
            int n_false = 0;
            char **false_args = parse_brace_args(q, &false_consumed, &n_false);
            if (false_args && n_false >= 1) {
                false_branch = strdup(false_args[0]);
                else_consumed =
                    5 + (q - (p + 3 + cond_consumed + true_consumed) - 5) + false_consumed;
                q += false_consumed;
            }
            if (false_args) {
                for (int i = 0; i < n_false; i++) free(false_args[i]);
                free(false_args);
            }
        }

        *end_pos = (int)(q - p);

        char *result;
        if (cond_val != 0) {
            // Expand calc commands in the true branch before returning
            // This ensures \assign inside \if is executed immediately
            result = expand_calc_recursive(true_branch, ctx);
            free(true_branch);
            free(false_branch);
        } else {
            if (false_branch) {
                // Expand calc commands in the false branch before returning
                result = expand_calc_recursive(false_branch, ctx);
                free(false_branch);
            } else {
                result = strdup("");
            }
            free(true_branch);
        }

        return result;
    }

    // ========== \begin{loop}...\end{loop} ==========
    // Iterative expansion with shared CalcContext
    if (strncmp(p, "\\begin{loop}", 12) == 0) {
        // Find matching \end{loop}
        const char *body_start = p + 12;
        const char *body_end = NULL;
        int loop_depth = 1;
        const char *scan = body_start;
        size_t chars_scanned = 0;

        while (*scan && loop_depth > 0) {
            // Defensive: prevent infinite loop from memory corruption
            if (++chars_scanned > MAX_SCAN_CHARS) {
                snprintf(error_msg, error_size, "\\begin{loop}: scan limit exceeded (corruption?)");
                return NULL;
            }
            if (strncmp(scan, "\\begin{loop}", 12) == 0) {
                loop_depth++;
                scan += 12;
                chars_scanned += 11; // Account for skip
            } else if (strncmp(scan, "\\end{loop}", 10) == 0) {
                loop_depth--;
                if (loop_depth == 0) {
                    body_end = scan;
                } else {
                    scan += 10;
                    chars_scanned += 9; // Account for skip
                }
            } else {
                scan++;
            }
        }

        if (!body_end) {
            // No matching \end{loop} found
            snprintf(error_msg, error_size, "\\begin{loop} without matching \\end{loop}");
            return NULL;
        }

        // Extract the body
        size_t body_len = body_end - body_start;
        char *body = malloc(body_len + 1);
        memcpy(body, body_start, body_len);
        body[body_len] = '\0';

        if (diag_is_enabled(DIAG_CALC)) {
            diag_log(DIAG_CALC, 0, "\\begin{loop}: body length %zu", body_len);
        }

        // Maximum iterations to prevent infinite loops
        const int MAX_LOOP_ITERATIONS = 1000000;

        // Result accumulator
        size_t result_capacity = 1024;
        size_t result_len = 0;
        char *result = malloc(result_capacity);
        result[0] = '\0';

        // Save the outer scope for lambda storage
        Scope *loop_outer_scope = ctx->current_scope;

        for (int iter = 0; iter < MAX_LOOP_ITERATIONS; iter++) {
            // Clear exit flag at start of each iteration
            ctx->exit_loop_requested = false;

            // NOTE: We do NOT create per-iteration scopes for plain \begin{loop}.
            // Per-iteration scopes are only for enumerate where each iteration
            // needs to capture different values. For plain loops, variables
            // should persist across iterations (accumulator pattern).

            if (diag_is_enabled(DIAG_CALC)) {
                diag_log(DIAG_CALC, 1, "loop iteration %d", iter);
            }

            // Expand calc commands in the body
            char *expanded_body = expand_calc_recursive(body, ctx);

            // Check for exit marker in expanded result
            char *exit_marker = strstr(expanded_body, "@@EXIT_LOOP@@");
            if (exit_marker) {
                // Found exit marker - truncate at marker and stop
                *exit_marker = '\0';

                if (diag_is_enabled(DIAG_CALC)) {
                    diag_log(DIAG_CALC, 1, "loop exit marker found at iteration %d", iter);
                }

                // Append content before marker to result
                size_t exp_len = strlen(expanded_body);
                while (result_len + exp_len + 1 > result_capacity) {
                    size_t new_capacity = result_capacity * 2;
                    char *new_result = safe_realloc(result, new_capacity);
                    if (!new_result) {
                        // Memory allocation failed
                        free(expanded_body);
                        result[result_len] = '\0';
                        free(body);
                        *end_pos = (int)(body_end - p) + 10;
                        return result;
                    }
                    result = new_result;
                    result_capacity = new_capacity;
                }
                memcpy(result + result_len, expanded_body, exp_len);
                result_len += exp_len;
                result[result_len] = '\0';

                free(expanded_body);
                break;
            }

            // Append expanded body to result
            size_t exp_len = strlen(expanded_body);
            while (result_len + exp_len + 1 > result_capacity) {
                size_t new_capacity = result_capacity * 2;
                char *new_result = safe_realloc(result, new_capacity);
                if (!new_result) {
                    // Memory allocation failed
                    free(expanded_body);
                    result[result_len] = '\0';
                    free(body);
                    *end_pos = (int)(body_end - p) + 10;
                    return result;
                }
                result = new_result;
                result_capacity = new_capacity;
            }
            memcpy(result + result_len, expanded_body, exp_len);
            result_len += exp_len;
            result[result_len] = '\0';

            free(expanded_body);
        }

        free(body);

        // Clear exit flag so outer expansion continues after the loop
        ctx->exit_loop_requested = false;

        // Set end_pos to skip past \end{loop}
        *end_pos = (int)(body_end - p) + 10;

        return result;
    }

    // ========== \exit_when{condition} ==========
    // Used inside \begin{loop}...\end{loop}
    // Returns @@EXIT_LOOP@@ marker if condition is true (non-zero)
    // Otherwise returns empty string
    if (strncmp(p, "\\exit_when{", 11) == 0) {
        const char *q = p + 10; // Start at '{'
        int brace_consumed = 0;
        char *content = parse_brace_content(q, &brace_consumed);
        if (content) {
            // Evaluate the condition
            int cond_val = eval_int_with_ctx(content, ctx);
            free(content);

            *end_pos = 10 + brace_consumed;

            if (cond_val != 0) {
                ctx->exit_loop_requested = true; // Signal immediate exit
                return strdup("@@EXIT_LOOP@@");
            } else {
                return strdup("");
            }
        }
    }

    // ========== \wait{milliseconds} ==========
    // For continuous mode: pause execution for specified milliseconds
    // Returns empty string
    if (strncmp(p, "\\wait{", 6) == 0) {
        const char *q = p + 5; // Start at '{'
        int brace_consumed = 0;
        char *content = parse_brace_content(q, &brace_consumed);
        if (content) {
            int ms = eval_int_with_ctx(content, ctx);
            free(content);
            *end_pos = 5 + brace_consumed;

            // Sleep for the specified milliseconds
            if (ms > 0) {
#ifdef _WIN32
                Sleep(ms);
#else
                usleep(ms * 1000);
#endif
            }
            return strdup("");
        }
    }

    // ========== Terminal Input Commands (continuous mode only) ==========

    // \haskey - returns 1 if a key is available, 0 otherwise
    if (strncmp(p, "\\haskey", 7) == 0 && (p[7] == '\0' || !isalnum((unsigned char)p[7]))) {
        *end_pos = 7;
        if (terminal_input_is_active()) {
            terminal_input_poll();
            bool has = terminal_has_key();
            return strdup(has ? "1" : "0");
        }
        return strdup("0");
    }

    // \getkey - returns the last key pressed (consumes it)
    if (strncmp(p, "\\getkey", 7) == 0 && (p[7] == '\0' || !isalnum((unsigned char)p[7]))) {
        *end_pos = 7;
        if (terminal_input_is_active()) {
            terminal_input_poll();
            const char *key = terminal_get_key();
            return strdup(key ? key : "");
        }
        return strdup("");
    }

    // \getmouseX - returns mouse X position (column, 0-based)
    if (strncmp(p, "\\getmouseX", 10) == 0 && (p[10] == '\0' || !isalnum((unsigned char)p[10]))) {
        *end_pos = 10;
        if (terminal_input_is_active()) {
            terminal_input_poll();
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", terminal_get_mouse_x());
            return strdup(buf);
        }
        return strdup("-1");
    }

    // \getmouseY - returns mouse Y position (row, 0-based)
    if (strncmp(p, "\\getmouseY", 10) == 0 && (p[10] == '\0' || !isalnum((unsigned char)p[10]))) {
        *end_pos = 10;
        if (terminal_input_is_active()) {
            terminal_input_poll();
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", terminal_get_mouse_y());
            return strdup(buf);
        }
        return strdup("-1");
    }

    // \getmousebutton - returns mouse button (0=none, 1=left, 2=middle, 3=right)
    if (strncmp(p, "\\getmousebutton", 15) == 0 &&
        (p[15] == '\0' || !isalnum((unsigned char)p[15]))) {
        *end_pos = 15;
        if (terminal_input_is_active()) {
            terminal_input_poll();
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", terminal_get_mouse_button());
            return strdup(buf);
        }
        return strdup("0");
    }

    // \gettime - returns milliseconds since program start (for frame timing)
    if (strncmp(p, "\\gettime", 8) == 0 && (p[8] == '\0' || !isalnum((unsigned char)p[8]))) {
        *end_pos = 8;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", terminal_get_time_ms());
        return strdup(buf);
    }

    // \time - returns seconds since Unix epoch (for seeding RNG)
    if (strncmp(p, "\\time", 5) == 0 && (p[5] == '\0' || !isalnum((unsigned char)p[5]))) {
        *end_pos = 5;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", (int)time(NULL));
        return strdup(buf);
    }

    // \rand{max} - returns random integer from 0 to max-1
    if (strncmp(p, "\\rand{", 6) == 0) {
        const char *q = p + 5; // Point at '{' for parse_brace_content
        int brace_consumed = 0;
        char *max_str = parse_brace_content(q, &brace_consumed);
        if (max_str) {
            int max_val = eval_int_with_ctx(max_str, ctx);
            free(max_str);
            int result = 0;
            if (max_val > 0) {
                result = rand() % max_val;
            }
            *end_pos = 5 + brace_consumed; // 5 for "\rand" + brace_consumed
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", result);
            return strdup(buf);
        }
    }

    // \srand{seed} - seed the random number generator
    if (strncmp(p, "\\srand{", 7) == 0) {
        const char *q = p + 6; // Point at '{' for parse_brace_content
        int brace_consumed = 0;
        char *seed_str = parse_brace_content(q, &brace_consumed);
        if (seed_str) {
            int seed = eval_int_with_ctx(seed_str, ctx);
            free(seed_str);
            srand((unsigned int)seed);
            *end_pos = 6 + brace_consumed; // 6 for "\srand" + brace_consumed
            return strdup("");
        }
    }

    // \term_rows - returns number of rows in terminal
    if (strncmp(p, "\\term_rows", 10) == 0 && (p[10] == '\0' || !isalnum((unsigned char)p[10]))) {
        *end_pos = 10;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", terminal_get_rows());
        return strdup(buf);
    }

    // \term_cols - returns number of columns in terminal
    if (strncmp(p, "\\term_cols", 10) == 0 && (p[10] == '\0' || !isalnum((unsigned char)p[10]))) {
        *end_pos = 10;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", terminal_get_cols());
        return strdup(buf);
    }

    // ========== Terminal Output / ANSI Escape Codes ==========

    // \esc - outputs the ESC character (0x1B) for ANSI sequences
    if (strncmp(p, "\\esc", 4) == 0 && (p[4] == '\0' || !isalnum((unsigned char)p[4]))) {
        *end_pos = 4;
        return strdup("\033");
    }

    // \ansi{codes} - produces a zero-width marker that becomes a BOX_TYPE_ANSI layout node.
    // The marker format is \x01A:codes\x01 (SOH delimited).
    // Whitespace in codes is stripped to handle macro-generated codes cleanly.
    if (strncmp(p, "\\ansi{", 6) == 0) {
        const char *q = p + 5;
        int brace_consumed = 0;
        char *codes = parse_brace_content(q, &brace_consumed);
        if (codes) {
            *end_pos = 5 + brace_consumed;
            // Strip whitespace from codes (macro expansion may add spaces)
            char *src = codes;
            char *dst = codes;
            while (*src) {
                if (*src != ' ' && *src != '\t' && *src != '\n' && *src != '\r') {
                    *dst++ = *src;
                }
                src++;
            }
            *dst = '\0';
            // Build marker: \x01A:codes\x01
            size_t len = strlen(codes);
            char *result = malloc(len + 5); // \x01 A : codes \x01 \0
            result[0] = '\x01';
            result[1] = 'A';
            result[2] = ':';
            memcpy(result + 3, codes, len);
            result[len + 3] = '\x01';
            result[len + 4] = '\0';
            free(codes);
            return result;
        }
    }

    // \ansi_reset - shorthand for \ansi{0} to reset all attributes
    if (strncmp(p, "\\ansi_reset", 11) == 0 && (p[11] == '\0' || !isalnum((unsigned char)p[11]))) {
        *end_pos = 11;
        return strdup("\x01"
                      "A:0"
                      "\x01");
    }

    // ========== Cursor Control (works without full interactive mode) ==========

    // \cursor{row,col} - Move cursor to position (1-based)
    if (strncmp(p, "\\cursor{", 8) == 0) {
        const char *q = p + 7;
        int brace_consumed = 0;
        int n_args = 0;
        char **args = parse_brace_args(q, &brace_consumed, &n_args);
        if (args && n_args == 2) {
            int row = eval_int_with_ctx(args[0], ctx);
            int col = eval_int_with_ctx(args[1], ctx);
            free(args[0]);
            free(args[1]);
            free(args);
            *end_pos = 7 + brace_consumed;
            char buf[32];
            snprintf(buf, sizeof(buf), "\033[%d;%dH", row, col);
            return strdup(buf);
        }
        if (args) {
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
        }
    }

    // \cursor_up{n} - Move cursor up n lines
    if (strncmp(p, "\\cursor_up{", 11) == 0) {
        const char *q = p + 10;
        int brace_consumed = 0;
        char *content = parse_brace_content(q, &brace_consumed);
        if (content) {
            int n = eval_int_with_ctx(content, ctx);
            free(content);
            *end_pos = 10 + brace_consumed;
            if (n <= 0) return strdup("");
            char buf[16];
            snprintf(buf, sizeof(buf), "\033[%dA", n);
            return strdup(buf);
        }
    }

    // \cursor_down{n} - Move cursor down n lines
    if (strncmp(p, "\\cursor_down{", 13) == 0) {
        const char *q = p + 12;
        int brace_consumed = 0;
        char *content = parse_brace_content(q, &brace_consumed);
        if (content) {
            int n = eval_int_with_ctx(content, ctx);
            free(content);
            *end_pos = 12 + brace_consumed;
            if (n <= 0) return strdup("");
            char buf[16];
            snprintf(buf, sizeof(buf), "\033[%dB", n);
            return strdup(buf);
        }
    }

    // \cursor_right{n} - Move cursor right n columns
    if (strncmp(p, "\\cursor_right{", 14) == 0) {
        const char *q = p + 13;
        int brace_consumed = 0;
        char *content = parse_brace_content(q, &brace_consumed);
        if (content) {
            int n = eval_int_with_ctx(content, ctx);
            free(content);
            *end_pos = 13 + brace_consumed;
            if (n <= 0) return strdup("");
            char buf[16];
            snprintf(buf, sizeof(buf), "\033[%dC", n);
            return strdup(buf);
        }
    }

    // \cursor_left{n} - Move cursor left n columns
    if (strncmp(p, "\\cursor_left{", 13) == 0) {
        const char *q = p + 12;
        int brace_consumed = 0;
        char *content = parse_brace_content(q, &brace_consumed);
        if (content) {
            int n = eval_int_with_ctx(content, ctx);
            free(content);
            *end_pos = 12 + brace_consumed;
            if (n <= 0) return strdup("");
            char buf[16];
            snprintf(buf, sizeof(buf), "\033[%dD", n);
            return strdup(buf);
        }
    }

    // \cursor_save - Save cursor position
    if (strncmp(p, "\\cursor_save", 12) == 0 && (p[12] == '\0' || !isalnum((unsigned char)p[12]))) {
        *end_pos = 12;
        return strdup("\033[s");
    }

    // \cursor_restore - Restore cursor position
    if (strncmp(p, "\\cursor_restore", 15) == 0 &&
        (p[15] == '\0' || !isalnum((unsigned char)p[15]))) {
        *end_pos = 15;
        return strdup("\033[u");
    }

    // \cursor_hide - Hide cursor
    if (strncmp(p, "\\cursor_hide", 12) == 0 && (p[12] == '\0' || !isalnum((unsigned char)p[12]))) {
        *end_pos = 12;
        return strdup("\033[?25l");
    }

    // \cursor_show - Show cursor
    if (strncmp(p, "\\cursor_show", 12) == 0 && (p[12] == '\0' || !isalnum((unsigned char)p[12]))) {
        *end_pos = 12;
        return strdup("\033[?25h");
    }

    // \clear_screen - Clear entire screen
    if (strncmp(p, "\\clear_screen", 13) == 0 &&
        (p[13] == '\0' || !isalnum((unsigned char)p[13]))) {
        *end_pos = 13;
        return strdup("\033[2J");
    }

    // \clear_line - Clear current line
    if (strncmp(p, "\\clear_line", 11) == 0 && (p[11] == '\0' || !isalnum((unsigned char)p[11]))) {
        *end_pos = 11;
        return strdup("\033[2K");
    }

    // \clear_to_eol - Clear from cursor to end of line
    if (strncmp(p, "\\clear_to_eol", 13) == 0 &&
        (p[13] == '\0' || !isalnum((unsigned char)p[13]))) {
        *end_pos = 13;
        return strdup("\033[K");
    }

    // \setclearbg{code} - Set background color for \main{} screen clears
    // Use ANSI background codes: 40=black, 41=red, 42=green, etc.
    // Use -1 or empty to reset to terminal default
    if (strncmp(p, "\\setclearbg{", 12) == 0) {
        const char *q = p + 12;
        const char *start = q;
        while (*q && *q != '}') q++;
        if (*q == '}') {
            size_t len = q - start;
            char *code_str = malloc(len + 1);
            memcpy(code_str, start, len);
            code_str[len] = '\0';

            // Parse the code (-1 or empty means default)
            if (len == 0 || strcmp(code_str, "-1") == 0) {
                ctx->clear_bg_code = -1;
            } else {
                ctx->clear_bg_code = atoi(code_str);
            }
            free(code_str);

            *end_pos = (int)(q - p) + 1;
            return strdup("");
        }
    }

    // \cursor_home - Move cursor to top-left (1,1)
    if (strncmp(p, "\\cursor_home", 12) == 0 && (p[12] == '\0' || !isalnum((unsigned char)p[12]))) {
        *end_pos = 12;
        return strdup("\033[H");
    }

    // ========== Persistent Array Store (Subnivean) ==========
#ifdef SUBNIVEAN_ENABLED
    // \sn_array{addr}[idx] - read from persistent store
    if (strncmp(p, "\\sn_array{", 10) == 0) {
        const char *q = p + 9;
        int brace_consumed = 0;
        char *addr_str = parse_brace_content(q, &brace_consumed);
        if (addr_str) {
            int addr = eval_int_with_ctx(addr_str, ctx);
            free(addr_str);
            q += brace_consumed;

            // Expect [index]
            if (*q == '[') {
                q++;
                const char *idx_start = q;
                int depth = 1;
                while (*q && depth > 0) {
                    if (*q == '[')
                        depth++;
                    else if (*q == ']')
                        depth--;
                    if (depth > 0) q++;
                }
                if (*q == ']') {
                    size_t idx_len = q - idx_start;
                    char *idx_str = malloc(idx_len + 1);
                    memcpy(idx_str, idx_start, idx_len);
                    idx_str[idx_len] = '\0';

                    int idx = eval_int_with_ctx(idx_str, ctx);
                    free(idx_str);
                    q++; // Skip ']'

                    *end_pos = (int)(q - p);

                    // Check if it's a string array
                    if (sn_store_is_string_array(addr)) {
                        const char *str = sn_store_string_array_get(addr, idx);
                        return str ? strdup(str) : strdup("");
                    }

                    // Integer array
                    int64_t val = sn_store_array_get(addr, idx);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%lld", (long long)val);
                    return strdup(buf);
                }
            }
        }
    }

    // \sn_setelement{addr,idx,val} - write to persistent store
    if (strncmp(p, "\\sn_setelement{", 15) == 0) {
        const char *q = p + 14;
        int consumed = 0;
        int n_args = 0;
        char **args = parse_brace_args(q, &consumed, &n_args);
        if (args && n_args == 3) {
            int addr = eval_int_with_ctx(args[0], ctx);
            int idx = eval_int_with_ctx(args[1], ctx);
            int64_t val = eval_int_with_ctx(args[2], ctx);

            sn_store_array_set(addr, idx, val);

            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
            *end_pos = 14 + consumed;
            return strdup("");
        }
        if (args) {
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
        }
    }

    // \sn_len{addr} - get length of persistent array
    if (strncmp(p, "\\sn_len{", 8) == 0) {
        const char *q = p + 7;
        int brace_consumed = 0;
        char *addr_str = parse_brace_content(q, &brace_consumed);
        if (addr_str) {
            int addr = eval_int_with_ctx(addr_str, ctx);
            free(addr_str);

            int len = sn_store_array_len(addr);
            *end_pos = 7 + brace_consumed;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", len);
            return strdup(buf);
        }
    }

    // ========== Map Operations ==========

    // \map_get<m>{key} - get value from map (returns 0 if missing)
    if (strncmp(p, "\\map_get<", 9) == 0) {
        const char *q = p + 8;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            q += name_consumed;
            // Get map address from counter (like heap array addresses)
            bool found = false;
            int addr = scope_lookup_counter(ctx->current_scope, name, &found);
            if (!found) addr = counter_get(ctx->counters, name);
            free(name);

            // Parse {key}
            if (*q == '{') {
                int brace_consumed = 0;
                char *key_str = parse_brace_content(q, &brace_consumed);
                if (key_str) {
                    int64_t key = eval_int_with_ctx(key_str, ctx);
                    free(key_str);

                    int64_t val = sn_store_map_get(addr, key);
                    *end_pos = (int)(q - p) + brace_consumed;
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%lld", (long long)val);
                    return strdup(buf);
                }
            }
        }
    }

    // \map_set<m>{key, value} - set value in map
    if (strncmp(p, "\\map_set<", 9) == 0) {
        const char *q = p + 8;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            q += name_consumed;
            // Get map address from counter
            bool found = false;
            int addr = scope_lookup_counter(ctx->current_scope, name, &found);
            if (!found) addr = counter_get(ctx->counters, name);
            free(name);

            // Parse {key, value}
            int brace_consumed = 0;
            int n_args = 0;
            char **args = parse_brace_args(q, &brace_consumed, &n_args);
            if (args && n_args == 2) {
                int64_t key = eval_int_with_ctx(args[0], ctx);
                int64_t val = eval_int_with_ctx(args[1], ctx);

                sn_store_map_set(addr, key, val);

                for (int i = 0; i < n_args; i++) free(args[i]);
                free(args);
                *end_pos = (int)(q - p) + brace_consumed;
                return strdup("");
            }
            if (args) {
                for (int i = 0; i < n_args; i++) free(args[i]);
                free(args);
            }
        }
    }

    // \map_has<m>{key} - check if key exists (returns 0 or 1)
    if (strncmp(p, "\\map_has<", 9) == 0) {
        const char *q = p + 8;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            q += name_consumed;
            // Get map address from counter
            bool found = false;
            int addr = scope_lookup_counter(ctx->current_scope, name, &found);
            if (!found) addr = counter_get(ctx->counters, name);
            free(name);

            // Parse {key}
            if (*q == '{') {
                int brace_consumed = 0;
                char *key_str = parse_brace_content(q, &brace_consumed);
                if (key_str) {
                    int64_t key = eval_int_with_ctx(key_str, ctx);
                    free(key_str);

                    bool has = sn_store_map_has(addr, key);
                    *end_pos = (int)(q - p) + brace_consumed;
                    return strdup(has ? "1" : "0");
                }
            }
        }
    }

    // \map_del<m>{key} - delete key from map (returns 0 or 1)
    if (strncmp(p, "\\map_del<", 9) == 0) {
        const char *q = p + 8;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            q += name_consumed;
            // Get map address from counter
            bool found = false;
            int addr = scope_lookup_counter(ctx->current_scope, name, &found);
            if (!found) addr = counter_get(ctx->counters, name);
            free(name);

            // Parse {key}
            if (*q == '{') {
                int brace_consumed = 0;
                char *key_str = parse_brace_content(q, &brace_consumed);
                if (key_str) {
                    int64_t key = eval_int_with_ctx(key_str, ctx);
                    free(key_str);

                    bool deleted = sn_store_map_del(addr, key);
                    *end_pos = (int)(q - p) + brace_consumed;
                    return strdup(deleted ? "1" : "0");
                }
            }
        }
    }

    // \map_len<m> - get number of entries in map
    if (strncmp(p, "\\map_len<", 9) == 0) {
        const char *q = p + 8;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            // Get map address from counter
            bool found = false;
            int addr = scope_lookup_counter(ctx->current_scope, name, &found);
            if (!found) addr = counter_get(ctx->counters, name);
            free(name);

            int len = sn_store_map_len(addr);
            *end_pos = 8 + name_consumed;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", len);
            return strdup(buf);
        }
    }

    // \map_keys<m> - get array of all keys (returns array address)
    if (strncmp(p, "\\map_keys<", 10) == 0) {
        const char *q = p + 9;
        int name_consumed = 0;
        char *name = parse_angle_name(q, &name_consumed);
        if (name) {
            // Get map address from counter
            bool found = false;
            int addr = scope_lookup_counter(ctx->current_scope, name, &found);
            if (!found) addr = counter_get(ctx->counters, name);
            free(name);

            int arr_addr = sn_store_map_keys(addr);
            *end_pos = 9 + name_consumed;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", arr_addr);
            return strdup(buf);
        }
    }
#endif // SUBNIVEAN_ENABLED

    // ========== \exit ==========
    // For continuous mode: signal exit from the main loop
    // Returns a special marker that hyades_main.c detects
    if (strncmp(p, "\\exit", 5) == 0 && (p[5] == '\0' || !isalpha(p[5]))) {
        *end_pos = 5;
        // Return special marker that signals exit to continuous mode
        return strdup("\x1F"
                      "EXIT"
                      "\x1F");
    }

    // ========== \main{body} ==========
    // Main loop for continuous mode (-c flag)
    // In normal mode: expands body once
    // In continuous mode: loops with cursor home between iterations until \exit or Ctrl+C
    if (strncmp(p, "\\main{", 6) == 0) {
        const char *q = p + 5; // Start at '{'
        int brace_consumed = 0;
        char *body = parse_brace_content(q, &brace_consumed);
        if (body) {
            *end_pos = 5 + brace_consumed;

            if (!macro_is_continuous_mode()) {
                // Normal mode: expand body once
                char *result = expand_calc(body, ctx);
                free(body);
                return result ? result : strdup("");
            }

            // Continuous mode: loop with terminal control
            volatile int *exit_flag = macro_get_exit_flag();

            // Build settings prefix for compositor
            char settings[256];
            snprintf(settings, sizeof(settings), "\\setunicode{true}\\setwidth{%d}",
                     ctx->width > 0 ? ctx->width : 80);

            while (!exit_flag || !*exit_flag) {
                // Poll for input at the start of each frame
                if (terminal_input_is_active()) {
                    terminal_input_poll();
                }

                // Expand body for this frame
                char *frame = expand_calc(body, ctx);

                if (frame) {
                    // Check for exit marker
                    if (strstr(frame, "\x1F"
                                      "EXIT"
                                      "\x1F") != NULL) {
                        free(frame);
                        break;
                    }

                    // Route through compositor to handle \\ and $...$
                    size_t full_len = strlen(settings) + strlen(frame) + 2;
                    char *full_input = malloc(full_len);
                    snprintf(full_input, full_len, "%s\n%s", settings, frame);
                    free(frame);

                    CompOptions opt = {0};
                    ParseError err = {0};
                    char *rendered = compose_document(full_input, &opt, &err);
                    free(full_input);

                    if (rendered) {
                        // Clear screen and move home, then print content
                        // If clear_bg_code is set, use it for the clear background
                        if (ctx->clear_bg_code >= 0) {
                            char clear_seq[32];
                            snprintf(clear_seq, sizeof(clear_seq), "\033[%dm\033[2J\033[H",
                                     ctx->clear_bg_code);
                            write(STDOUT_FILENO, clear_seq, strlen(clear_seq));
                        } else {
                            write(STDOUT_FILENO, "\033[2J\033[H", 7);
                        }
                        write(STDOUT_FILENO, rendered, strlen(rendered));
                        free(rendered);
                    }
                }

                // Check exit request
                if (ctx->exit_loop_requested) {
                    ctx->exit_loop_requested = false;
                    break;
                }
            }

            free(body);
            // Return empty string - output was already printed
            return strdup("\n");
        }
    }

    // ========== \begin<arr>[i,item]{enumerate}...body...\end{enumerate} ==========
    // Iterates over array, binding index and element variables per iteration
    if (strncmp(p, "\\begin<", 7) == 0) {
        const char *q = p + 7;

        // Parse array name (may contain \recall<...> which needs expansion)
        const char *arr_name_start = q;
        int angle_depth = 1;
        while (*q && angle_depth > 0) {
            if (*q == '<')
                angle_depth++;
            else if (*q == '>')
                angle_depth--;
            if (angle_depth > 0) q++;
        }
        if (*q == '>' && angle_depth == 0) {
            size_t arr_name_len = q - arr_name_start;
            char *arr_name_raw = malloc(arr_name_len + 1);
            memcpy(arr_name_raw, arr_name_start, arr_name_len);
            arr_name_raw[arr_name_len] = '\0';
            q++; // Skip '>'

            // Expand the array name (handles \recall<arr> -> actual name)
            char *arr_name_expanded = expand_calc(arr_name_raw, ctx);
            free(arr_name_raw);

            // Apply hygiene prefix if the result is a lowercase name
            // This handles the case where stdlib passes "nums" which should become "_m0_nums"
            char *arr_name = arr_name_expanded;
            bool arr_needs_hyg = (arr_name_expanded[0] >= 'a' && arr_name_expanded[0] <= 'z');
            if (arr_needs_hyg) {
                arr_name = malloc(strlen(arr_name_expanded) + 5);
                sprintf(arr_name, "_m0_%s", arr_name_expanded);
                free(arr_name_expanded);
            }

            // Check for [i,item] binding
            char *idx_var = NULL;
            char *elem_var = NULL;

            if (*q == '[') {
                q++; // Skip '['
                // Parse index variable name
                const char *idx_start = q;
                while (*q && *q != ',' && *q != ']') q++;
                if (*q == ',') {
                    size_t idx_len = q - idx_start;
                    idx_var = malloc(idx_len + 1);
                    memcpy(idx_var, idx_start, idx_len);
                    idx_var[idx_len] = '\0';
                    q++; // Skip ','

                    // Parse element variable name
                    const char *elem_start = q;
                    while (*q && *q != ']') q++;
                    if (*q == ']') {
                        size_t elem_len = q - elem_start;
                        elem_var = malloc(elem_len + 1);
                        memcpy(elem_var, elem_start, elem_len);
                        elem_var[elem_len] = '\0';
                        q++; // Skip ']'
                    }
                }
            }

            // Check for {enumerate}
            if (*q == '{' && strncmp(q, "{enumerate}", 11) == 0) {
                q += 11; // Skip {enumerate}

                // Find matching \end{enumerate}
                const char *body_start = q;
                const char *body_end = NULL;
                int depth = 1;
                const char *scan = q;
                size_t enum_chars_scanned = 0;

                while (*scan && depth > 0) {
                    // Defensive: prevent infinite loop from memory corruption
                    if (++enum_chars_scanned > MAX_SCAN_CHARS) {
                        snprintf(error_msg, error_size,
                                 "\\begin{enumerate}: scan limit exceeded (corruption?)");
                        return NULL;
                    }
                    if (strncmp(scan, "\\begin{enumerate}", 17) == 0 ||
                        strncmp(scan, "\\begin<", 7) == 0) {
                        // Check if this is another enumerate
                        if (strncmp(scan, "\\begin{enumerate}", 17) == 0) {
                            depth++;
                            scan += 17;
                        } else {
                            // Check if \begin<...>{enumerate}
                            const char *check = scan + 7;
                            while (*check && *check != '>') check++;
                            if (*check == '>') {
                                check++;
                                if (*check == '[') {
                                    while (*check && *check != ']') check++;
                                    if (*check == ']') check++;
                                }
                                if (strncmp(check, "{enumerate}", 11) == 0) {
                                    depth++;
                                    scan = check + 11;
                                    continue;
                                }
                            }
                            scan++;
                        }
                    } else if (strncmp(scan, "\\end{enumerate}", 15) == 0) {
                        depth--;
                        if (depth == 0) {
                            body_end = scan;
                        } else {
                            scan += 15;
                        }
                    } else {
                        scan++;
                    }
                }

                if (body_end && idx_var && elem_var) {
                    // Extract body
                    size_t body_len = body_end - body_start;
                    char *body = malloc(body_len + 1);
                    memcpy(body, body_start, body_len);
                    body[body_len] = '\0';

                    // Use variable names directly - proper scoping handles isolation
                    // (hygienization is now done at lambda expansion time, not here)

                    if (diag_is_enabled(DIAG_CALC)) {
                        diag_log(DIAG_CALC, 0, "\\begin<%s>[%s,%s]{enumerate}: body length %zu",
                                 arr_name, idx_var, elem_var, body_len);
                    }

                    // Build result
                    size_t result_capacity = 1024;
                    size_t result_len = 0;
                    char *result = malloc(result_capacity);
                    result[0] = '\0';

                    // Iterate over array (scope first, then global)
                    int arr_len = unified_array_len(ctx->current_scope, ctx->arrays, arr_name);

                    for (int i = 0; i < arr_len; i++) {

                        // Clear exit flag at start of each iteration
                        ctx->exit_loop_requested = false;

                        // Create per-iteration scope for proper closure behavior
                        Scope *iter_scope = scope_new(ctx->current_scope);
                        Scope *saved_scope = ctx->current_scope;
                        ctx->current_scope = iter_scope;

                        // Set index counter in iteration scope
                        scope_bind_counter(iter_scope, idx_var, i);
                        counter_set(ctx->counters, idx_var, i);

                        // Set element content in iteration scope
                        const char *elem = unified_array_get(saved_scope, ctx->arrays, arr_name, i);
                        if (elem) {
                            scope_bind_value(iter_scope, elem_var, elem);
                            content_store(ctx->contents, elem_var, elem, (int)strlen(elem), 1);
                        }

                        if (diag_is_enabled(DIAG_CALC)) {
                            diag_log(DIAG_CALC, 1, "enumerate iteration %d: %s=%d, %s='%.30s%s'", i,
                                     idx_var, i, elem_var, elem ? elem : "(null)",
                                     (elem && strlen(elem) > 30) ? "..." : "");
                        }

                        // Set lambda_storage_scope for closures defined in loop
                        ctx->lambda_storage_scope = saved_scope;

                        // Expand body with current bindings
                        char *expanded_body = expand_calc_recursive(body, ctx);

                        // Check for exit marker
                        char *exit_marker = strstr(expanded_body, "@@EXIT_LOOP@@");
                        if (exit_marker) {
                            *exit_marker = '\0';

                            if (diag_is_enabled(DIAG_CALC)) {
                                diag_log(DIAG_CALC, 1,
                                         "enumerate exit marker found at iteration %d", i);
                            }

                            // Append content before marker
                            size_t exp_len = strlen(expanded_body);
                            while (result_len + exp_len + 1 > result_capacity) {
                                size_t new_capacity = result_capacity * 2;
                                char *new_result = safe_realloc(result, new_capacity);
                                if (!new_result) {
                                    // Memory allocation failed
                                    free(expanded_body);
                                    ctx->lambda_storage_scope = NULL;
                                    ctx->current_scope = saved_scope;
                                    scope_decref(iter_scope);
                                    result[result_len] = '\0';
                                    free(body);
                                    free(arr_name);
                                    free(idx_var);
                                    free(elem_var);
                                    *end_pos = (int)(body_end - p) + 15;
                                    return result;
                                }
                                result = new_result;
                                result_capacity = new_capacity;
                            }
                            memcpy(result + result_len, expanded_body, exp_len);
                            result_len += exp_len;
                            result[result_len] = '\0';

                            free(expanded_body);
                            ctx->lambda_storage_scope = NULL;
                            ctx->current_scope = saved_scope;
                            scope_decref(iter_scope);
                            break;
                        }

                        // Append expanded body to result
                        size_t exp_len = strlen(expanded_body);
                        while (result_len + exp_len + 1 > result_capacity) {
                            size_t new_capacity = result_capacity * 2;
                            char *new_result = safe_realloc(result, new_capacity);
                            if (!new_result) {
                                // Memory allocation failed
                                free(expanded_body);
                                ctx->lambda_storage_scope = NULL;
                                ctx->current_scope = saved_scope;
                                scope_decref(iter_scope);
                                result[result_len] = '\0';
                                free(body);
                                free(arr_name);
                                free(idx_var);
                                free(elem_var);
                                *end_pos = (int)(body_end - p) + 15;
                                return result;
                            }
                            result = new_result;
                            result_capacity = new_capacity;
                        }
                        memcpy(result + result_len, expanded_body, exp_len);
                        result_len += exp_len;
                        result[result_len] = '\0';

                        free(expanded_body);
                        ctx->lambda_storage_scope = NULL;
                        ctx->current_scope = saved_scope;
                        scope_decref(iter_scope);
                    }

                    if (diag_is_enabled(DIAG_CALC)) {
                        diag_log(DIAG_CALC, 1, "enumerate: iterated %d elements", arr_len);
                    }

                    ctx->exit_loop_requested = false;

                    free(body);
                    free(arr_name);
                    free(idx_var);
                    free(elem_var);

                    *end_pos = (int)(body_end - p) + 15; // Skip \end{enumerate}
                    return result;
                }

                // Cleanup on partial match
                free(arr_name);
                if (idx_var) free(idx_var);
                if (elem_var) free(elem_var);
            } else {
                free(arr_name);
                if (idx_var) free(idx_var);
                if (elem_var) free(elem_var);
            }
        }
    }

    // ========== \measure<name,w,h>{content} ==========
    // Stores content and sets width/height counters
    if (strncmp(p, "\\measure<", 9) == 0) {
        const char *q = p + 9;

        // Parse <name,w,h>
        const char *start = q;
        while (*q && *q != '>' && *q != '\n') q++;
        if (*q != '>') return NULL;

        // Split by commas
        size_t params_len = q - start;
        char *params = malloc(params_len + 1);
        memcpy(params, start, params_len);
        params[params_len] = '\0';

        // Parse three names separated by commas
        char *name = NULL;
        char *w_name = NULL;
        char *h_name = NULL;

        char *comma1 = strchr(params, ',');
        if (comma1) {
            *comma1 = '\0';
            char *comma2 = strchr(comma1 + 1, ',');
            if (comma2) {
                *comma2 = '\0';
                name = params;
                w_name = comma1 + 1;
                h_name = comma2 + 1;
                // Trim whitespace
                while (*name == ' ') name++;
                while (*w_name == ' ') w_name++;
                while (*h_name == ' ') h_name++;
            }
        }

        if (!name || !w_name || !h_name) {
            free(params);
            return NULL;
        }

        diag_log(DIAG_CALC, 0, "\\measure<%s,%s,%s>{...}", name, w_name, h_name);

        q++; // Skip '>'
        q = skip_ws(q);

        // Parse {content} - use parse_brace_content to get full content without splitting
        // NOTE: This is critical - we must NOT split by commas here, as the content
        // may contain literal commas (e.g., \<c,w,h\> in escaped text)
        int brace_consumed = 0;
        char *content = parse_brace_content(q, &brace_consumed);
        if (!content) {
            free(params);
            return NULL;
        }

        diag_log(DIAG_CALC, 1, "content (len=%zu): '%.60s%s'", strlen(content), content,
                 strlen(content) > 60 ? "..." : "");

        // Expand calc commands in the content (e.g., \recall<lambda>)
        // This is necessary because lambdas are in the calc/scope system, not user macros
        char *expanded_content = expand_calc_recursive(content, ctx);
        free(content);
        content = expanded_content;

        diag_log(DIAG_CALC, 1, "after calc expansion: '%.60s%s'", content,
                 strlen(content) > 60 ? "..." : "");

        // Measure the content
        int width = 0, height = 0;
        measure_content(content, ctx->user_macros, &width, &height);

        diag_result(DIAG_CALC, 0, "stored as '%s', counters %s=%d, %s=%d", name, w_name, width,
                    h_name, height);

        // Store content and set counters
        if (ctx->contents) {
            content_store(ctx->contents, name, content, width, height);
        }
        counter_set(ctx->counters, w_name, width);
        counter_set(ctx->counters, h_name, height);

        // Return empty string (measure is silent, use \recall to output)
        free(content);
        free(params);

        *end_pos = (int)(q + brace_consumed - p);
        return strdup("");
    }

    // ========== \measureref<name,w,h,ref> ==========
    // Measures stored content by reference (looks up content from registry)
    // Use this when content contains \verb that needs the current context
    if (strncmp(p, "\\measureref<", 12) == 0) {
        const char *q = p + 12;

        // Parse <name,w,h,ref>
        const char *start = q;
        while (*q && *q != '>' && *q != '\n') q++;
        if (*q != '>') return NULL;

        // Split by commas
        size_t params_len = q - start;
        char *params = malloc(params_len + 1);
        memcpy(params, start, params_len);
        params[params_len] = '\0';

        // Parse four names separated by commas
        char *name = NULL;
        char *w_name = NULL;
        char *h_name = NULL;
        char *ref_name = NULL;

        char *comma1 = strchr(params, ',');
        if (comma1) {
            *comma1 = '\0';
            char *comma2 = strchr(comma1 + 1, ',');
            if (comma2) {
                *comma2 = '\0';
                char *comma3 = strchr(comma2 + 1, ',');
                if (comma3) {
                    *comma3 = '\0';
                    name = params;
                    w_name = comma1 + 1;
                    h_name = comma2 + 1;
                    ref_name = comma3 + 1;
                    // Trim whitespace
                    while (*name == ' ') name++;
                    while (*w_name == ' ') w_name++;
                    while (*h_name == ' ') h_name++;
                    while (*ref_name == ' ') ref_name++;
                }
            }
        }

        if (!name || !w_name || !h_name || !ref_name) {
            free(params);
            return NULL;
        }

        diag_log(DIAG_CALC, 0, "\\measureref<%s,%s,%s,%s>", name, w_name, h_name, ref_name);

        q++; // Skip '>'

        // Look up content by reference name
        const char *ref_content = NULL;
        if (ctx->contents) {
            ref_content = content_get(ctx->contents, ref_name);
        }

        if (!ref_content) {
            diag_result(DIAG_CALC, 0, "ref '%s' not found, using empty", ref_name);
            counter_set(ctx->counters, w_name, 0);
            counter_set(ctx->counters, h_name, 0);
            free(params);
            *end_pos = (int)(q - p);
            return strdup("");
        }

        diag_log(DIAG_CALC, 1, "ref content (len=%zu): '%.60s%s'", strlen(ref_content), ref_content,
                 strlen(ref_content) > 60 ? "..." : "");

        // Measure the referenced content
        int width = 0, height = 0;
        measure_content(ref_content, ctx->user_macros, &width, &height);

        diag_result(DIAG_CALC, 0, "stored as '%s', counters %s=%d, %s=%d", name, w_name, width,
                    h_name, height);

        // Store content and set counters
        if (ctx->contents) {
            content_store(ctx->contents, name, ref_content, width, height);
        }
        counter_set(ctx->counters, w_name, width);
        counter_set(ctx->counters, h_name, height);

        free(params);
        *end_pos = (int)(q - p);
        return strdup("");
    }

    // ========== \lambda<name>[params]{body} ==========
    // Creates a lambda (thunk if no params) and stores in current scope
    if (strncmp(p, "\\lambda<", 8) == 0) {
        const char *q = p + 8;

        // Parse <name>
        const char *name_start = q;
        while (*q && *q != '>' && *q != '\n') q++;
        if (*q != '>') return NULL;

        size_t name_len = q - name_start;
        char *name = malloc(name_len + 1);
        if (!name) return NULL;
        memcpy(name, name_start, name_len);
        name[name_len] = '\0';

        // Expand calc in the name (e.g., \lambda<f\valueof<i>>)
        char *expanded_name = expand_calc(name, ctx);
        free(name);
        name = expanded_name;

        q++; // Skip '>'
        q = skip_ws(q);

        // Parse optional [params]
        char **param_names = NULL;
        int n_params = 0;

        if (*q == '[') {
            q++; // Skip '['
            const char *params_start = q;
            int depth = 1;
            while (*q && depth > 0) {
                if (*q == '[')
                    depth++;
                else if (*q == ']')
                    depth--;
                if (depth > 0) q++;
            }
            if (*q != ']') {
                free(name);
                return NULL;
            }

            // Parse comma-separated parameter names
            size_t params_len = q - params_start;
            if (params_len > 0) {
                char *params_str = malloc(params_len + 1);
                memcpy(params_str, params_start, params_len);
                params_str[params_len] = '\0';

                // Count commas to determine array size
                int n_commas = 0;
                for (size_t i = 0; i < params_len; i++) {
                    if (params_str[i] == ',') n_commas++;
                }
                n_params = n_commas + 1;
                param_names = calloc(n_params, sizeof(char *));

                // Split on commas
                char *tok = params_str;
                int idx = 0;
                for (size_t i = 0; i <= params_len && idx < n_params; i++) {
                    if (params_str[i] == ',' || params_str[i] == '\0') {
                        params_str[i] = '\0';
                        // Trim whitespace
                        while (*tok == ' ' || *tok == '\t') tok++;
                        char *end = tok + strlen(tok) - 1;
                        while (end > tok && (*end == ' ' || *end == '\t')) *end-- = '\0';
                        // Strip type annotation (e.g., "name:int" -> "name")
                        char *colon = strchr(tok, ':');
                        if (colon) {
                            param_names[idx++] = strndup(tok, colon - tok);
                        } else {
                            param_names[idx++] = strdup(tok);
                        }
                        tok = params_str + i + 1;
                    }
                }
                free(params_str);
            }

            q++; // Skip ']'
            q = skip_ws(q);
        }

        // Parse {body} or #{body} (compact mode)
        int brace_consumed = 0;
        char *raw_body = NULL;
        char *body = NULL;
        bool compact_mode = false;

        if (*q == '#' && *(q + 1) == '{') {
            // Compact mode: #{...} - strips whitespace and newlines
            compact_mode = true;
            q++; // Skip '#'
            raw_body = parse_brace_content(q, &brace_consumed);
            if (raw_body) {
                body = process_compact_body(raw_body);
                free(raw_body);
            }
        } else {
            // Normal mode: {...} - process % comment markers
            raw_body = parse_brace_content(q, &brace_consumed);
            if (raw_body) {
                body = process_comment_markers(raw_body);
                free(raw_body);
            }
        }

        if (!body) {
            free(name);
            for (int i = 0; i < n_params; i++) free(param_names[i]);
            free(param_names);
            return NULL;
        }

        // Create and store lambda (compact_mode = computational, requires \return{})
        Lambda *lambda = lambda_new_ex(param_names, n_params, body, compact_mode);
        if (lambda && ctx->current_scope) {
            // Register type for hygienized names
            if (name[0] == '_' && name[1] == 'm') {
                type_registry_set(ctx->types, name, 'l');
            }
            if (ctx->lambda_storage_scope) {
                // Store in outer scope but capture current scope
                // This is used in iteration contexts (enumerate/loop)
                scope_bind_lambda_with_capture(ctx->lambda_storage_scope, ctx->current_scope, name,
                                               lambda);
                if (diag_is_enabled(DIAG_CALC)) {
                    diag_log(DIAG_CALC, 0, "\\lambda<%s> defined (iteration context)", name);
                    if (n_params > 0) {
                        // Build param list for logging
                        char params_buf[128] = "";
                        int pos = 0;
                        for (int i = 0; i < n_params && pos < 120; i++) {
                            pos += snprintf(params_buf + pos, 128 - pos, "%s%s", i > 0 ? ", " : "",
                                            param_names[i]);
                        }
                        diag_log(DIAG_CALC, 1, "params: [%s]", params_buf);
                    }
                    diag_log(DIAG_CALC, 1, "body: {%.50s%s}", body, strlen(body) > 50 ? "..." : "");
                }
            } else {
                // Normal case: store and capture in current scope
                scope_bind_lambda(ctx->current_scope, name, lambda);
                if (diag_is_enabled(DIAG_CALC)) {
                    diag_log(DIAG_CALC, 0, "\\lambda<%s> defined", name);
                    if (n_params > 0) {
                        // Build param list for logging
                        char params_buf[128] = "";
                        int pos = 0;
                        for (int i = 0; i < n_params && pos < 120; i++) {
                            pos += snprintf(params_buf + pos, 128 - pos, "%s%s", i > 0 ? ", " : "",
                                            param_names[i]);
                        }
                        diag_log(DIAG_CALC, 1, "params: [%s]", params_buf);
                    }
                    diag_log(DIAG_CALC, 1, "body: {%.50s%s}", body, strlen(body) > 50 ? "..." : "");
                }
            }

            // Record symbol for LSP (if symbol table is available)
            if (ctx->symbols) {
                // Build signature from parameters
                char sig[256] = {0};
                int sig_pos = 0;
                for (int i = 0; i < n_params && sig_pos < 250; i++) {
                    sig_pos += snprintf(sig + sig_pos, sizeof(sig) - sig_pos, "%s%s",
                                        i > 0 ? ", " : "", param_names[i]);
                }

                Symbol *sym = lsp_symbol_table_add(ctx->symbols, name, SYMKIND_LAMBDA,
                                                   ctx->current_line, ctx->current_col, 0, 0);
                if (sym) {
                    symbol_set_signature(sym, sig);
                    // Truncate body preview if too long
                    if (body && strlen(body) > 80) {
                        char preview[84];
                        strncpy(preview, body, 80);
                        strcpy(preview + 80, "...");
                        symbol_set_body_preview(sym, preview);
                    } else if (body) {
                        symbol_set_body_preview(sym, body);
                    }
                }
            }
        }

        // Cleanup
        free(name);
        free(body);
        for (int i = 0; i < n_params; i++) free(param_names[i]);
        free(param_names);

        *end_pos = (int)((q + brace_consumed) - p);
        return strdup(""); // Lambda definition produces no output
    }

    // ========== \invoke<name>[args] ==========
    // CL-style alias for calling lambdas (lambda-only, no value retrieval)
    if (strncmp(p, "\\invoke<", 8) == 0) {
        const char *q = p + 7;
        int content_consumed = 0;
        char *content = parse_angle_content_nested(q, &content_consumed);
        if (content) {
            // Expand any calc commands in the content (e.g., \invoke<${fn_name}>)
            char *name = expand_calc(content, ctx);
            free(content);
            int name_consumed = content_consumed;
            q += name_consumed;

            // Look up lambda - try direct name first, then with hygiene prefix
            Lambda *lambda = scope_lookup_lambda(ctx->current_scope, name);
            if (!lambda && name[0] >= 'a' && name[0] <= 'z') {
                char *hyg_name = malloc(strlen(name) + 5);
                sprintf(hyg_name, "_m0_%s", name);
                lambda = scope_lookup_lambda(ctx->current_scope, hyg_name);
                free(hyg_name);
            }

            if (!lambda) {
                // Not a lambda - \invoke ONLY works for lambdas
                if (diag_is_enabled(DIAG_CALC)) {
                    diag_log(DIAG_CALC, 0, "\\invoke<%s>: ERROR not a lambda", name);
                }
                free(name);
                *end_pos = (int)(q - p);
                return strdup("");
            }

            // Parse [args] (required for non-thunks, optional for thunks)
            char **args = NULL;
            int n_args = 0;

            if (*q == '[') {
                q++; // Skip '['
                const char *args_start = q;
                int depth = 1;
                while (*q && depth > 0) {
                    if (*q == '[')
                        depth++;
                    else if (*q == ']')
                        depth--;
                    if (depth > 0) q++;
                }
                if (*q == ']') {
                    size_t args_len = q - args_start;
                    if (args_len > 0) {
                        char *args_str = malloc(args_len + 1);
                        memcpy(args_str, args_start, args_len);
                        args_str[args_len] = '\0';

                        // Count commas (respecting braces and brackets)
                        int n_commas = 0;
                        int brace_depth = 0;
                        int bracket_depth = 0;
                        for (size_t i = 0; i < args_len; i++) {
                            if (args_str[i] == '{')
                                brace_depth++;
                            else if (args_str[i] == '}')
                                brace_depth--;
                            else if (args_str[i] == '[')
                                bracket_depth++;
                            else if (args_str[i] == ']')
                                bracket_depth--;
                            else if (args_str[i] == ',' && brace_depth == 0 && bracket_depth == 0)
                                n_commas++;
                        }
                        n_args = n_commas + 1;
                        args = calloc(n_args, sizeof(char *));

                        // Split on commas
                        char *tok_start = args_str;
                        int idx = 0;
                        brace_depth = 0;
                        bracket_depth = 0;
                        for (size_t i = 0; i <= args_len && idx < n_args; i++) {
                            if (args_str[i] == '{')
                                brace_depth++;
                            else if (args_str[i] == '}')
                                brace_depth--;
                            else if (args_str[i] == '[')
                                bracket_depth++;
                            else if (args_str[i] == ']')
                                bracket_depth--;
                            else if ((args_str[i] == ',' && brace_depth == 0 &&
                                      bracket_depth == 0) ||
                                     args_str[i] == '\0') {
                                args_str[i] = '\0';
                                while (*tok_start == ' ' || *tok_start == '\t') tok_start++;
                                char *tok_end = tok_start + strlen(tok_start) - 1;
                                while (tok_end > tok_start && (*tok_end == ' ' || *tok_end == '\t'))
                                    *tok_end-- = '\0';
                                args[idx++] = expand_calc(tok_start, ctx);
                                tok_start = args_str + i + 1;
                            }
                        }
                        free(args_str);
                    }
                    q++; // Skip ']'
                }
            }

            // Check argument count
            int expected = lambda_param_count(lambda);
            if (n_args != expected) {
                if (diag_is_enabled(DIAG_CALC)) {
                    diag_log(DIAG_CALC, 0,
                             "\\invoke<%s>: ERROR arg count mismatch, expected %d got %d", name,
                             expected, n_args);
                }
                free(name);
                for (int i = 0; i < n_args; i++) free(args[i]);
                free(args);
                *end_pos = (int)(q - p);
                return strdup("");
            }

            if (diag_is_enabled(DIAG_CALC)) {
                if (n_args == 0) {
                    diag_log(DIAG_CALC, 0, "\\invoke<%s> (thunk call)", name);
                } else {
                    diag_log(DIAG_CALC, 0, "\\invoke<%s>[%d args] (lambda call)", name, n_args);
                }
            }

            // Execute lambda - same logic as \recall
            bool is_computational = lambda_is_computational(lambda);
            char *result = NULL;

#ifdef SUBNIVEAN_ENABLED
            if (is_computational) {
                result = try_subnivean_execute(ctx, lambda, args, n_args);
                if (result) {
                    for (int i = 0; i < n_args; i++) free(args[i]);
                    free(args);
                    if (diag_is_enabled(DIAG_CALC)) {
                        diag_result(DIAG_CALC, 0, "'%.60s%s'", result,
                                    strlen(result) > 60 ? "..." : "");
                    }
                    free(name);
                    *end_pos = (int)(q - p);
                    return result;
                }
                // Subnivean required for computational lambdas
                fprintf(stderr, "Error: Computational lambda '%s' failed to compile or execute\n",
                        name);
                for (int i = 0; i < n_args; i++) free(args[i]);
                free(args);
                free(name);
                *end_pos = (int)(q - p);
                return strdup("");
            }
#endif

            // Non-computational lambdas use interpreter
            Scope *call_scope = scope_new(lambda->captured_scope);

            for (int i = 0; i < n_args; i++) {
                const char *param_name = lambda_get_param(lambda, i);
                if (param_name) {
                    scope_bind_value(call_scope, param_name, args[i]);
                    if (diag_is_enabled(DIAG_CALC)) {
                        diag_log(DIAG_CALC, 1, "arg %s = '%.40s%s'", param_name, args[i],
                                 strlen(args[i]) > 40 ? "..." : "");
                    }
                }
                free(args[i]);
            }
            free(args);

            Scope *saved_scope = ctx->current_scope;
            ctx->current_scope = call_scope;

            bool saved_in_computational = ctx->in_computational;
            char *saved_return_value = ctx->return_value;
            bool saved_return_requested = ctx->return_requested;

            if (is_computational) {
                ctx->in_computational = true;
                ctx->return_value = NULL;
                ctx->return_requested = false;
            }

            const char *body = lambda_get_body(lambda);
            result = expand_calc(body ? body : "", ctx);

            if (is_computational) {
                if (ctx->return_requested && ctx->return_value) {
                    free(result);
                    result = strdup(ctx->return_value);
                } else {
                    free(result);
                    result = strdup("");
                }
                if (ctx->return_value) free(ctx->return_value);
            }

            ctx->in_computational = saved_in_computational;
            ctx->return_value = saved_return_value;
            ctx->return_requested = saved_return_requested;

            ctx->current_scope = saved_scope;
            scope_decref(call_scope);

            if (diag_is_enabled(DIAG_CALC)) {
                diag_result(DIAG_CALC, 0, "'%.60s%s'", result, strlen(result) > 60 ? "..." : "");
            }
            free(name);
            *end_pos = (int)(q - p);
            return result;
        }
    }

    // ========== \recall<name> or \recall<name>[args] ==========
    // Outputs stored content or calls lambda
    // Supports nested content like \recall<\recall<arr>>
    if (strncmp(p, "\\recall<", 8) == 0) {
        const char *q = p + 7;
        int content_consumed = 0;
        char *content = parse_angle_content_nested(q, &content_consumed);
        if (content) {
            // Expand any calc commands in the content (e.g., \recall<arr>)
            char *name = expand_calc(content, ctx);
            free(content);
            int name_consumed = content_consumed;
            q += name_consumed;

            // Check TypeRegistry to optimize lookup - skip lambda search for values/arrays
            char known_type = 0;
            if (name[0] == '_' && name[1] == 'm') {
                known_type = type_registry_get(ctx->types, name);
            } else if (name[0] >= 'a' && name[0] <= 'z') {
                // Try hygienized name
                char *hyg_name = malloc(strlen(name) + 5);
                sprintf(hyg_name, "_m0_%s", name);
                known_type = type_registry_get(ctx->types, hyg_name);
                free(hyg_name);
            }

            // First check if this is a lambda call (skip if we know it's not a lambda)
            // Try direct name first, then with hygiene prefix
            Lambda *lambda = NULL;
            if (known_type == 0 || known_type == 'l') {
                lambda = scope_lookup_lambda(ctx->current_scope, name);
                if (!lambda && name[0] >= 'a' && name[0] <= 'z') {
                    // Try with _m0_ hygiene prefix
                    char *hyg_name = malloc(strlen(name) + 5);
                    sprintf(hyg_name, "_m0_%s", name);
                    lambda = scope_lookup_lambda(ctx->current_scope, hyg_name);
                    free(hyg_name);
                }
            }
            if (lambda) {
                // Parse optional [args]
                char **args = NULL;
                int n_args = 0;

                if (*q == '[') {
                    q++; // Skip '['
                    const char *args_start = q;
                    int depth = 1;
                    while (*q && depth > 0) {
                        if (*q == '[')
                            depth++;
                        else if (*q == ']')
                            depth--;
                        if (depth > 0) q++;
                    }
                    if (*q == ']') {
                        // Parse comma-separated arguments
                        size_t args_len = q - args_start;
                        if (args_len > 0) {
                            char *args_str = malloc(args_len + 1);
                            memcpy(args_str, args_start, args_len);
                            args_str[args_len] = '\0';

                            // Count commas (respecting braces and brackets)
                            int n_commas = 0;
                            int brace_depth = 0;
                            int bracket_depth = 0;
                            for (size_t i = 0; i < args_len; i++) {
                                if (args_str[i] == '{')
                                    brace_depth++;
                                else if (args_str[i] == '}')
                                    brace_depth--;
                                else if (args_str[i] == '[')
                                    bracket_depth++;
                                else if (args_str[i] == ']')
                                    bracket_depth--;
                                else if (args_str[i] == ',' && brace_depth == 0 &&
                                         bracket_depth == 0)
                                    n_commas++;
                            }
                            n_args = n_commas + 1;
                            args = calloc(n_args, sizeof(char *));

                            // Split on commas (respecting braces and brackets)
                            char *tok_start = args_str;
                            int idx = 0;
                            brace_depth = 0;
                            bracket_depth = 0;
                            for (size_t i = 0; i <= args_len && idx < n_args; i++) {
                                if (args_str[i] == '{')
                                    brace_depth++;
                                else if (args_str[i] == '}')
                                    brace_depth--;
                                else if (args_str[i] == '[')
                                    bracket_depth++;
                                else if (args_str[i] == ']')
                                    bracket_depth--;
                                else if ((args_str[i] == ',' && brace_depth == 0 &&
                                          bracket_depth == 0) ||
                                         args_str[i] == '\0') {
                                    args_str[i] = '\0';
                                    // Trim whitespace
                                    while (*tok_start == ' ' || *tok_start == '\t') tok_start++;
                                    char *tok_end = tok_start + strlen(tok_start) - 1;
                                    while (tok_end > tok_start &&
                                           (*tok_end == ' ' || *tok_end == '\t'))
                                        *tok_end-- = '\0';
                                    // Evaluate the argument
                                    args[idx++] = expand_calc(tok_start, ctx);
                                    tok_start = args_str + i + 1;
                                }
                            }
                            free(args_str);
                        }
                        q++; // Skip ']'
                    }
                }

                // Check argument count
                int expected = lambda_param_count(lambda);
                if (n_args != expected) {
                    if (diag_is_enabled(DIAG_CALC)) {
                        diag_log(DIAG_CALC, 0,
                                 "\\recall<%s>: ERROR arg count mismatch, expected %d got %d", name,
                                 expected, n_args);
                    }
                    free(name);
                    for (int i = 0; i < n_args; i++) free(args[i]);
                    free(args);
                    *end_pos = (int)(q - p);
                    return strdup("");
                }

                if (diag_is_enabled(DIAG_CALC)) {
                    if (n_args == 0) {
                        diag_log(DIAG_CALC, 0, "\\recall<%s> (thunk call)", name);
                    } else {
                        diag_log(DIAG_CALC, 0, "\\recall<%s>[%d args] (lambda call)", name, n_args);
                    }
                }

                // Check if this is a computational lambda
                bool is_computational = lambda_is_computational(lambda);
                char *result = NULL;

#ifdef SUBNIVEAN_ENABLED
                // Computational lambdas MUST use bytecode - no interpreter fallback
                if (is_computational) {
                    result = try_subnivean_execute(ctx, lambda, args, n_args);
                    if (result) {
                        // Success! Clean up and return
                        for (int i = 0; i < n_args; i++) free(args[i]);
                        free(args);
                        if (diag_is_enabled(DIAG_CALC)) {
                            diag_result(DIAG_CALC, 0, "'%.60s%s'", result,
                                        strlen(result) > 60 ? "..." : "");
                        }
                        free(name);
                        *end_pos = (int)(q - p);
                        return result;
                    }
                    // Subnivean REQUIRED for computational lambdas - no fallback
                    fprintf(stderr,
                            "Error: Computational lambda '%s' failed to compile or execute\n",
                            name);
                    for (int i = 0; i < n_args; i++) free(args[i]);
                    free(args);
                    free(name);
                    *end_pos = (int)(q - p);
                    return strdup("");
                }
#endif

                // Non-computational lambdas use interpreter
                // Create call scope with lambda's captured scope as parent
                Scope *call_scope = scope_new(lambda->captured_scope);

                // Bind parameters
                for (int i = 0; i < n_args; i++) {
                    const char *param_name = lambda_get_param(lambda, i);
                    if (param_name) {
                        scope_bind_value(call_scope, param_name, args[i]);
                        if (diag_is_enabled(DIAG_CALC)) {
                            diag_log(DIAG_CALC, 1, "arg %s = '%.40s%s'", param_name, args[i],
                                     strlen(args[i]) > 40 ? "..." : "");
                        }
                    }
                    free(args[i]);
                }
                free(args);

                // Evaluate body in call scope
                Scope *saved_scope = ctx->current_scope;
                ctx->current_scope = call_scope;

                bool saved_in_computational = ctx->in_computational;
                char *saved_return_value = ctx->return_value;
                bool saved_return_requested = ctx->return_requested;

                if (is_computational) {
                    ctx->in_computational = true;
                    ctx->return_value = NULL;
                    ctx->return_requested = false;
                }

                const char *body = lambda_get_body(lambda);
                result = expand_calc(body ? body : "", ctx);

                // For computational lambdas, use return_value instead of body output
                if (is_computational) {
                    if (ctx->return_requested && ctx->return_value) {
                        free(result);
                        result = strdup(ctx->return_value);
                        if (diag_is_enabled(DIAG_CALC)) {
                            diag_log(DIAG_CALC, 1, "computational lambda returned: '%.40s%s'",
                                     result, strlen(result) > 40 ? "..." : "");
                        }
                    } else {
                        // No \return{} called - computational lambda returns empty
                        free(result);
                        result = strdup("");
                        if (diag_is_enabled(DIAG_CALC)) {
                            diag_log(DIAG_CALC, 1,
                                     "computational lambda: no \\return{} called, returning empty");
                        }
                    }
                    // Clean up
                    if (ctx->return_value) free(ctx->return_value);
                }

                // Restore saved state
                ctx->in_computational = saved_in_computational;
                ctx->return_value = saved_return_value;
                ctx->return_requested = saved_return_requested;

                ctx->current_scope = saved_scope;
                scope_decref(call_scope);

                if (diag_is_enabled(DIAG_CALC)) {
                    diag_result(DIAG_CALC, 0, "'%.60s%s'", result ? result : "",
                                (result && strlen(result) > 60) ? "..." : "");
                }
                free(name);
                *end_pos = (int)(q - p);
                return result ? result : strdup("");
            }

            // Not a direct lambda - check for indirect lambda call (value contains lambda name)
            if (*q == '[') {
                // Could be array access OR indirect lambda call
                // First, try to get the value of name
                const char *indirect_name = scope_lookup_value(ctx->current_scope, name);
                if (!indirect_name && ctx->contents) {
                    indirect_name = content_get(ctx->contents, name);
                }

                if (indirect_name) {
                    // Try to look up a lambda with this value as the name
                    Lambda *indirect_lambda =
                        scope_lookup_lambda(ctx->current_scope, indirect_name);

                    // If not found, try with hygiene prefix (in case argument was unhygienized)
                    char *hygienized_indirect = NULL;
                    if (!indirect_lambda && indirect_name[0] != '_') {
                        // Try _m0_ prefix (common hygiene level)
                        hygienized_indirect = malloc(strlen(indirect_name) + 5);
                        sprintf(hygienized_indirect, "_m0_%s", indirect_name);
                        indirect_lambda =
                            scope_lookup_lambda(ctx->current_scope, hygienized_indirect);
                        if (indirect_lambda) {
                            indirect_name = hygienized_indirect;
                        }
                    }
                    if (indirect_lambda) {
                        // Parse [args] for indirect lambda call
                        const char *args_q = q + 1; // Skip '['
                        const char *args_start = args_q;
                        int args_depth = 1;
                        while (*args_q && args_depth > 0) {
                            if (*args_q == '[')
                                args_depth++;
                            else if (*args_q == ']')
                                args_depth--;
                            if (args_depth > 0) args_q++;
                        }

                        char **args = NULL;
                        int n_args = 0;

                        if (*args_q == ']') {
                            size_t args_len = args_q - args_start;
                            if (args_len > 0) {
                                char *args_str = malloc(args_len + 1);
                                memcpy(args_str, args_start, args_len);
                                args_str[args_len] = '\0';

                                // Count commas
                                int n_commas = 0;
                                int brace_depth = 0;
                                for (size_t i = 0; i < args_len; i++) {
                                    if (args_str[i] == '{')
                                        brace_depth++;
                                    else if (args_str[i] == '}')
                                        brace_depth--;
                                    else if (args_str[i] == ',' && brace_depth == 0)
                                        n_commas++;
                                }
                                n_args = n_commas + 1;
                                args = calloc(n_args, sizeof(char *));

                                char *tok_start = args_str;
                                int idx = 0;
                                brace_depth = 0;
                                for (size_t i = 0; i <= args_len && idx < n_args; i++) {
                                    if (args_str[i] == '{')
                                        brace_depth++;
                                    else if (args_str[i] == '}')
                                        brace_depth--;
                                    else if ((args_str[i] == ',' && brace_depth == 0) ||
                                             args_str[i] == '\0') {
                                        args_str[i] = '\0';
                                        while (*tok_start == ' ' || *tok_start == '\t') tok_start++;
                                        char *tok_end = tok_start + strlen(tok_start) - 1;
                                        while (tok_end > tok_start &&
                                               (*tok_end == ' ' || *tok_end == '\t'))
                                            *tok_end-- = '\0';
                                        args[idx++] = expand_calc(tok_start, ctx);
                                        tok_start = args_str + i + 1;
                                    }
                                }
                                free(args_str);
                            }
                            args_q++; // Skip ']'
                        }

                        // Check argument count
                        int expected = lambda_param_count(indirect_lambda);
                        if (n_args != expected) {
                            if (diag_is_enabled(DIAG_CALC)) {
                                diag_log(DIAG_CALC, 0,
                                         "\\recall<%s> (indirect %s): ERROR arg count mismatch, "
                                         "expected %d got %d",
                                         name, indirect_name, expected, n_args);
                            }
                            free(name);
                            for (int i = 0; i < n_args; i++) free(args[i]);
                            free(args);
                            if (hygienized_indirect) free(hygienized_indirect);
                            *end_pos = (int)(args_q - p);
                            return strdup("");
                        }

                        if (diag_is_enabled(DIAG_CALC)) {
                            diag_log(DIAG_CALC, 0,
                                     "\\recall<%s>[%d args] (indirect lambda call to '%s')", name,
                                     n_args, indirect_name);
                        }

                        // Check if this is a computational lambda
                        bool is_computational = lambda_is_computational(indirect_lambda);
                        char *result = NULL;

#ifdef SUBNIVEAN_ENABLED
                        // Computational lambdas MUST use bytecode - no interpreter fallback
                        if (is_computational) {
                            result = try_subnivean_execute(ctx, indirect_lambda, args, n_args);
                            if (result) {
                                // Success! Clean up and return
                                for (int i = 0; i < n_args; i++) free(args[i]);
                                free(args);
                                if (diag_is_enabled(DIAG_CALC)) {
                                    diag_result(DIAG_CALC, 0, "'%.60s%s'", result,
                                                strlen(result) > 60 ? "..." : "");
                                }
                                free(name);
                                if (hygienized_indirect) free(hygienized_indirect);
                                *end_pos = (int)(args_q - p);
                                return result;
                            }
                            // Subnivean REQUIRED for computational lambdas - no fallback
                            fprintf(stderr, "Error: Computational lambda (indirect) failed to "
                                            "compile or execute\n");
                            for (int i = 0; i < n_args; i++) free(args[i]);
                            free(args);
                            free(name);
                            if (hygienized_indirect) free(hygienized_indirect);
                            *end_pos = (int)(args_q - p);
                            return strdup("");
                        }
#endif

                        // Non-computational lambdas use interpreter
                        // Create call scope
                        Scope *call_scope = scope_new(indirect_lambda->captured_scope);

                        // Bind parameters
                        for (int i = 0; i < n_args; i++) {
                            const char *param_name = lambda_get_param(indirect_lambda, i);
                            if (param_name) {
                                scope_bind_value(call_scope, param_name, args[i]);
                            }
                            free(args[i]);
                        }
                        free(args);

                        // Evaluate body
                        Scope *saved_scope = ctx->current_scope;
                        ctx->current_scope = call_scope;

                        bool saved_in_computational = ctx->in_computational;
                        char *saved_return_value = ctx->return_value;
                        bool saved_return_requested = ctx->return_requested;

                        if (is_computational) {
                            ctx->in_computational = true;
                            ctx->return_value = NULL;
                            ctx->return_requested = false;
                        }

                        const char *body = lambda_get_body(indirect_lambda);
                        result = expand_calc(body ? body : "", ctx);

                        // For computational lambdas, use return_value instead of body output
                        if (is_computational) {
                            if (ctx->return_requested && ctx->return_value) {
                                free(result);
                                result = strdup(ctx->return_value);
                            } else {
                                free(result);
                                result = strdup("");
                            }
                            if (ctx->return_value) free(ctx->return_value);
                        }

                        // Restore saved state
                        ctx->in_computational = saved_in_computational;
                        ctx->return_value = saved_return_value;
                        ctx->return_requested = saved_return_requested;

                        ctx->current_scope = saved_scope;
                        scope_decref(call_scope);

                        if (diag_is_enabled(DIAG_CALC)) {
                            diag_result(DIAG_CALC, 0, "'%.60s%s'", result ? result : "",
                                        (result && strlen(result) > 60) ? "..." : "");
                        }
                        free(name);
                        if (hygienized_indirect) free(hygienized_indirect);
                        *end_pos = (int)(args_q - p);
                        return result ? result : strdup("");
                    }
                    if (hygienized_indirect) free(hygienized_indirect);
                }
            }

            // Check for [index] suffix (array access)
            if (*q == '[') {
                q++; // Skip '['
                const char *idx_start = q;
                int depth = 1;
                while (*q && depth > 0) {
                    if (*q == '[')
                        depth++;
                    else if (*q == ']')
                        depth--;
                    if (depth > 0) q++;
                }
                if (*q == ']') {
                    // Parse index
                    size_t idx_len = q - idx_start;
                    char *idx_str = malloc(idx_len + 1);
                    memcpy(idx_str, idx_start, idx_len);
                    idx_str[idx_len] = '\0';

                    int index = eval_int_with_ctx(idx_str, ctx);
                    free(idx_str);
                    q++; // Skip ']'

                    // The name might be a variable containing the actual array name
                    // (e.g., parameter 'arr' containing "_RV_0" or "myarray")
                    char *array_name = name;
                    const char *indirect = scope_lookup_value(ctx->current_scope, name);
                    if (indirect && *indirect) {
                        // The variable contains an array name, use that instead
                        array_name = strdup(indirect);
                        free(name);
                        name = array_name;
                    }

                    // Look up array (scope first, then global)
                    // Try with hygiene prefix first, then without (for stdlib compatibility)
                    ScopeArray *arr =
                        unified_find_array(ctx->current_scope, ctx->arrays, array_name);
                    if (!arr && array_name[0] >= 'a' && array_name[0] <= 'z') {
                        // Try with _m0_ prefix
                        char *hyg_name = malloc(strlen(array_name) + 5);
                        sprintf(hyg_name, "_m0_%s", array_name);
                        arr = unified_find_array(ctx->current_scope, ctx->arrays, hyg_name);
                        free(hyg_name);
                    }
                    if (diag_is_enabled(DIAG_CALC)) {
                        diag_log(DIAG_CALC, 0, "\\recall<%s>[%d] (array access)", array_name,
                                 index);
                    }
                    free(name);

                    if (arr && index >= 0 && index < arr->n_elements) {
                        if (diag_is_enabled(DIAG_CALC)) {
                            diag_result(DIAG_CALC, 0, "'%.60s%s'", arr->elements[index],
                                        strlen(arr->elements[index]) > 60 ? "..." : "");
                        }
                        *end_pos = (int)(q - p);
                        return strdup(arr->elements[index]);
                    } else {
                        if (diag_is_enabled(DIAG_CALC)) {
                            diag_result(DIAG_CALC, 0, "(out of bounds or not found)");
                        }
                        *end_pos = (int)(q - p);
                        return strdup("");
                    }
                }
            }

            // Scalar content - first check scope, then legacy registry
            const char *content = scope_lookup_value(ctx->current_scope, name);
            if (!content && ctx->contents) {
                content = content_get(ctx->contents, name);
            }
            if (diag_is_enabled(DIAG_CALC)) {
                diag_log(DIAG_CALC, 0, "\\recall<%s> (value)", name);
                diag_result(DIAG_CALC, 0, "'%.60s%s'", content ? content : "(not found)",
                            (content && strlen(content) > 60) ? "..." : "");
            }
            free(name);
            *end_pos = 7 + name_consumed;
            return strdup(content ? content : "");
        }
    }

    // ========== \assign<name>{content} or \assign<name[]>{[...]} ==========
    // Immediately expands and stores content (without measuring dimensions)
    // Simpler than \measure - just captures the current value of calc commands
    if (strncmp(p, "\\assign<", 8) == 0) {
        const char *q = p + 8;

        // Parse <name>
        const char *name_start = q;
        while (*q && *q != '>' && *q != '\n') q++;
        if (*q != '>') return NULL;

        size_t name_len = q - name_start;
        char *name = malloc(name_len + 1);
        if (!name) return NULL;
        memcpy(name, name_start, name_len);
        name[name_len] = '\0';

        // Expand calc in the name (e.g., \assign<item\valueof<i>>)
        char *expanded_name = expand_calc(name, ctx);
        free(name);
        name = expanded_name;

        q++; // Skip '>'
        q = skip_ws(q);

        // Check if this is an array (name ends with [])
        // Use actual length of expanded name
        size_t actual_name_len = strlen(name);
        bool is_array = (actual_name_len >= 2 && name[actual_name_len - 2] == '[' &&
                         name[actual_name_len - 1] == ']');

        // Parse {content}
        int brace_consumed = 0;
        char *content = parse_brace_content(q, &brace_consumed);
        if (!content) {
            free(name);
            return NULL;
        }

        if (is_array) {
            // Strip [] from name
            name[actual_name_len - 2] = '\0';

            if (diag_is_enabled(DIAG_CALC)) {
                diag_log(DIAG_CALC, 0, "\\assign<%s[]>{%.60s%s}", name, content,
                         strlen(content) > 60 ? "..." : "");
            }

            // Parse elements
            int n_elems = 0;
            char **elems = parse_array_initializer(content, &n_elems, false);

            // Arrays always go to global registry to ensure they're accessible
            // across scopes (e.g., when passed to stdlib functions).
            // Scalar variables remain scoped, but arrays need global visibility.
            Array *arr = find_array(ctx->arrays, name);
            if (arr) {
                for (int i = 0; i < arr->n_elements; i++) free(arr->elements[i]);
                arr->n_elements = 0;
            } else {
                arr = create_array(ctx->arrays, name, false);
            }
            if (elems) {
                for (int i = 0; i < n_elems; i++) {
                    array_push(arr, elems[i]);
                    free(elems[i]);
                }
                free(elems);
            }

            // Register type for hygienized names
            if (name[0] == '_' && name[1] == 'm') {
                type_registry_set(ctx->types, name, 'a');
            }

            free(name);
            free(content);
            *end_pos = (int)(q + brace_consumed - p);
            return strdup("");
        } else {
            // Scalar content
            diag_log(DIAG_CALC, 0, "\\assign<%s>{%.60s%s}", name, content,
                     strlen(content) > 60 ? "..." : "");

            // Fully expand all calc commands in content
            // This is needed for cases like \assign<_R>{\recall<_R>X} where
            // we need to expand \recall<_R> and then concatenate with X
            char *expanded = expand_calc_recursive(content, ctx);

            diag_result(DIAG_CALC, 0, "expanded and stored as '%s': '%.60s%s'", name, expanded,
                        strlen(expanded) > 60 ? "..." : "");
            content_store(ctx->contents, name, expanded, strlen(expanded), 1);
            // Also store in current scope for new scoping system
            if (ctx->current_scope) {
                scope_bind_value(ctx->current_scope, name, expanded);
            }
            // Register type for hygienized names
            if (name[0] == '_' && name[1] == 'm') {
                type_registry_set(ctx->types, name, 'v');
            }
            free(expanded);

            free(name);
            free(content);

            *end_pos = (int)(q + brace_consumed - p);
            return strdup(""); // Return empty string (nothing to output)
        }
    }

// ========== String Primitives ==========

// Helper: Check if string is a lineinsert marker and resolve it
// Returns resolved text (caller must free) or NULL if not a marker
#define RESOLVE_LINEINSERT(str)                                                                    \
    (strncmp((str), "__lr_", 5) == 0 ? line_registry_get_text(str) : NULL)

    // \streq{str1,str2} - Returns 1 if strings are equal, 0 otherwise
    if (strncmp(p, "\\streq{", 7) == 0) {
        const char *q = p + 6;
        int consumed = 0;
        int n_args = 0;
        char **args = parse_brace_args(q, &consumed, &n_args);
        if (args && n_args == 2) {
            // Expand calc commands in both arguments
            char *str1 = expand_calc(args[0], ctx);
            char *str2 = expand_calc(args[1], ctx);
            const char *actual_str1 = str1 ? str1 : args[0];
            const char *actual_str2 = str2 ? str2 : args[1];

            int result = (strcmp(actual_str1, actual_str2) == 0) ? 1 : 0;
            diag_log(DIAG_CALC, 0, "\\streq{'%.30s','%.30s'} -> %d", actual_str1, actual_str2,
                     result);

            free(str1); // Safe even if NULL
            free(str2);
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
            *end_pos = 6 + consumed;
            return strdup(result ? "1" : "0");
        }
        if (args) {
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
        }
    }

    // \startswith{str,prefix} - Returns 1 if str starts with prefix, 0 otherwise
    if (strncmp(p, "\\startswith{", 12) == 0) {
        const char *q = p + 11;
        int consumed = 0;
        int n_args = 0;
        char **args = parse_brace_args(q, &consumed, &n_args);
        if (args && n_args == 2) {
            // Expand calc commands in the arguments
            char *str = expand_calc(args[0], ctx);
            char *prefix = expand_calc(args[1], ctx);
            const char *actual_str = str ? str : args[0];
            const char *actual_prefix = prefix ? prefix : args[1];

            int result = (strncmp(actual_str, actual_prefix, strlen(actual_prefix)) == 0) ? 1 : 0;
            diag_log(DIAG_CALC, 0, "\\startswith{'%.30s','%.20s'} -> %d", actual_str, actual_prefix,
                     result);

            free(prefix);
            free(str);
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
            *end_pos = 11 + consumed;
            return strdup(result ? "1" : "0");
        }
        if (args) {
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
        }
    }

    // \endswith{str,suffix} - Returns 1 if str ends with suffix, 0 otherwise
    if (strncmp(p, "\\endswith{", 10) == 0) {
        const char *q = p + 9;
        int consumed = 0;
        int n_args = 0;
        char **args = parse_brace_args(q, &consumed, &n_args);
        if (args && n_args == 2) {
            // Expand calc commands in arguments
            char *str = expand_calc(args[0], ctx);
            char *suffix = expand_calc(args[1], ctx);
            const char *actual_str = str ? str : args[0];
            const char *actual_suffix = suffix ? suffix : args[1];

            size_t str_len = strlen(actual_str);
            size_t suf_len = strlen(actual_suffix);
            int result = 0;
            if (suf_len <= str_len) {
                result = (strcmp(actual_str + str_len - suf_len, actual_suffix) == 0) ? 1 : 0;
            }
            diag_log(DIAG_CALC, 0, "\\endswith{'%.30s','%.20s'} -> %d", actual_str, actual_suffix,
                     result);

            free(suffix);
            free(str);
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
            *end_pos = 9 + consumed;
            return strdup(result ? "1" : "0");
        }
        if (args) {
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
        }
    }

    // \contains{str,needle} - Returns 1 if str contains needle, 0 otherwise
    if (strncmp(p, "\\contains{", 10) == 0) {
        const char *q = p + 9;
        int consumed = 0;
        int n_args = 0;
        char **args = parse_brace_args(q, &consumed, &n_args);
        if (args && n_args == 2) {
            // Expand calc commands in arguments
            char *str = expand_calc(args[0], ctx);
            char *needle = expand_calc(args[1], ctx);
            const char *actual_str = str ? str : args[0];
            const char *actual_needle = needle ? needle : args[1];

            int result = (strstr(actual_str, actual_needle) != NULL) ? 1 : 0;
            diag_log(DIAG_CALC, 0, "\\contains{'%.30s','%.20s'} -> %d", actual_str, actual_needle,
                     result);

            free(needle);
            free(str);
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
            *end_pos = 9 + consumed;
            return strdup(result ? "1" : "0");
        }
        if (args) {
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
        }
    }

    // \concat{a,b,...} - Concatenates all arguments into a single string
    if (strncmp(p, "\\concat{", 8) == 0) {
        const char *q = p + 7;
        int consumed = 0;
        int n_args = 0;
        char **args = parse_brace_args(q, &consumed, &n_args);
        if (args && n_args >= 1) {
            // Calculate total length
            size_t total_len = 0;
            for (int i = 0; i < n_args; i++) {
                if (args[i]) total_len += strlen(args[i]);
            }

            // Allocate and concatenate
            char *result = malloc(total_len + 1);
            result[0] = '\0';
            for (int i = 0; i < n_args; i++) {
                if (args[i]) strcat(result, args[i]);
            }

            diag_log(DIAG_CALC, 0, "\\concat{%d args} -> '%.60s%s'", n_args, result,
                     strlen(result) > 60 ? "..." : "");

            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
            *end_pos = 7 + consumed;
            return result;
        }
        if (args) {
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
        }
    }

    // \strlen{str} - Returns the length of the string
    if (strncmp(p, "\\strlen{", 8) == 0) {
        const char *q = p + 7;
        int consumed = 0;
        char *content = parse_brace_content(q, &consumed);
        if (content) {
            // Expand calc commands in the argument
            char *str = expand_calc(content, ctx);
            const char *actual_str = str ? str : content;

            int len = (int)strlen(actual_str);
            diag_log(DIAG_CALC, 0, "\\strlen{'%.40s%s'} -> %d", actual_str,
                     strlen(actual_str) > 40 ? "..." : "", len);

            free(str);
            free(content);
            *end_pos = 7 + consumed;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", len);
            return strdup(buf);
        }
    }

    // \trim{str} - Returns the string with leading/trailing whitespace removed
    if (strncmp(p, "\\trim{", 6) == 0) {
        const char *q = p + 5;
        int consumed = 0;
        char *content = parse_brace_content(q, &consumed);
        if (content) {
            // Expand calc commands in the argument
            char *str = expand_calc(content, ctx);
            const char *actual_str = str ? str : content;

            // Trim leading and trailing whitespace
            const char *start = actual_str;
            while (*start && isspace((unsigned char)*start)) start++;

            const char *end = actual_str + strlen(actual_str);
            while (end > start && isspace((unsigned char)*(end - 1))) end--;

            size_t result_len = end - start;
            char *result = malloc(result_len + 1);
            memcpy(result, start, result_len);
            result[result_len] = '\0';

            diag_log(DIAG_CALC, 0, "\\trim{'%.40s%s'} -> '%.40s'", actual_str,
                     strlen(actual_str) > 40 ? "..." : "", result);

            free(str);
            free(content);
            *end_pos = 5 + consumed;
            return result;
        }
    }

    // \stripbraces{str} - Removes outermost {} braces if present
    if (strncmp(p, "\\stripbraces{", 13) == 0) {
        const char *q = p + 12;
        int consumed = 0;
        char *content = parse_brace_content(q, &consumed);
        if (content) {
            // Expand calc commands in the argument
            char *str = expand_calc(content, ctx);
            const char *actual_str = str ? str : content;

            // Check if string starts with { and ends with }
            size_t len = strlen(actual_str);
            const char *result_str;
            size_t result_len;
            if (len >= 2 && actual_str[0] == '{' && actual_str[len - 1] == '}') {
                result_str = actual_str + 1;
                result_len = len - 2;
            } else {
                result_str = actual_str;
                result_len = len;
            }

            char *result = malloc(result_len + 1);
            memcpy(result, result_str, result_len);
            result[result_len] = '\0';

            diag_log(DIAG_CALC, 0, "\\stripbraces{'%.40s%s'} -> '%.40s'", actual_str,
                     strlen(actual_str) > 40 ? "..." : "", result);

            free(str);
            free(content);
            *end_pos = 12 + consumed;
            return result;
        }
    }

    // \indexof{str,char} - Returns index of first occurrence of char, or -1
    if (strncmp(p, "\\indexof{", 9) == 0) {
        const char *q = p + 8;
        int consumed = 0;
        int n_args = 0;
        char **args = parse_brace_args(q, &consumed, &n_args);
        if (args && n_args == 2) {
            // Expand calc commands in the string argument
            char *str = expand_calc(args[0], ctx);
            const char *actual_str = str ? str : args[0];

            // Expand the needle argument too
            char *needle_expanded = expand_calc(args[1], ctx);
            char needle = needle_expanded ? needle_expanded[0] : args[1][0];

            const char *found = strchr(actual_str, needle);
            int result = found ? (int)(found - actual_str) : -1;
            diag_log(DIAG_CALC, 0, "\\indexof{'%.30s','%c'} -> %d", actual_str, needle, result);

            free(needle_expanded);
            free(str);
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
            *end_pos = 8 + consumed;
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", result);
            return strdup(buf);
        }
        if (args) {
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
        }
    }

    // \substr{str,start,len} - Returns substring starting at index 'start' with length 'len'
    // If len is omitted or -1, returns from start to end
    if (strncmp(p, "\\substr{", 8) == 0) {
        const char *q = p + 7;
        int consumed = 0;
        int n_args = 0;
        char **args = parse_brace_args(q, &consumed, &n_args);
        if (args && n_args >= 2) {
            // Expand calc commands in the string argument
            char *str = expand_calc(args[0], ctx);
            const char *actual_str = str ? str : args[0];

            int start = eval_int_with_ctx(args[1], ctx);
            int len = (n_args >= 3) ? eval_int_with_ctx(args[2], ctx) : -1;

            size_t str_len = strlen(actual_str);

            // Clamp start to valid range
            if (start < 0) start = 0;
            if ((size_t)start > str_len) start = (int)str_len;

            // Calculate actual length
            size_t remaining = str_len - start;
            size_t actual_len = (len < 0 || (size_t)len > remaining) ? remaining : (size_t)len;

            char *raw_result = malloc(actual_len + 1);
            if (raw_result) {
                memcpy(raw_result, actual_str + start, actual_len);
                raw_result[actual_len] = '\0';
            }

            // Escape braces to prevent re-parsing issues
            char *result = raw_result ? escape_braces(raw_result) : strdup("");
            free(raw_result);

            diag_log(DIAG_CALC, 0, "\\substr{'%.30s',%d,%d} -> '%.40s'", actual_str, start, len,
                     result ? result : "");

            free(str);
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
            *end_pos = 7 + consumed;
            return result;
        }
        if (args) {
            for (int i = 0; i < n_args; i++) free(args[i]);
            free(args);
        }
    }

    // \split<arr>{str}{delim} - Split string by delimiter, store parts in array
    // Returns the number of parts. Array is cleared first.
    if (strncmp(p, "\\split<", 7) == 0) {
        const char *q = p + 6;
        int name_consumed = 0;
        char *arr_name = parse_angle_name(q, &name_consumed);
        if (arr_name) {
            q += name_consumed;
            q = skip_ws(q);

            // Parse first brace: string to split
            int str_consumed = 0;
            char *str_arg = parse_brace_content(q, &str_consumed);
            if (str_arg) {
                q += str_consumed;
                q = skip_ws(q);

                // Parse second brace: delimiter
                int delim_consumed = 0;
                char *delim_arg = parse_brace_content(q, &delim_consumed);
                if (delim_arg) {
                    // Expand both arguments
                    char *str = expand_calc(str_arg, ctx);
                    char *delim = expand_calc(delim_arg, ctx);

                    // Clear or create the array
                    Array *arr = find_array(ctx->arrays, arr_name);
                    if (arr) {
                        // Clear existing array
                        for (int i = 0; i < arr->n_elements; i++) {
                            free(arr->elements[i]);
                        }
                        arr->n_elements = 0;
                    } else {
                        arr = create_array(ctx->arrays, arr_name, false);
                    }

                    // Split the string (brace-aware: respects {} nesting)
                    int count = 0;
                    if (str && delim && strlen(delim) > 0) {
                        size_t delim_len = strlen(delim);
                        const char *start = str;
                        const char *p = str;
                        int brace_depth = 0;

                        while (*p) {
                            // Track brace depth
                            if (*p == '{') {
                                brace_depth++;
                                p++;
                                continue;
                            } else if (*p == '}') {
                                if (brace_depth > 0) brace_depth--;
                                p++;
                                continue;
                            }

                            // Only look for delimiter when at brace depth 0
                            if (brace_depth == 0 && strncmp(p, delim, delim_len) == 0) {
                                // Found delimiter at top level - extract part before it
                                size_t part_len = p - start;
                                char *part = malloc(part_len + 1);
                                if (part) {
                                    memcpy(part, start, part_len);
                                    part[part_len] = '\0';
                                    array_push(arr, part);
                                    free(part);
                                    count++;
                                }
                                p += delim_len;
                                start = p;
                            } else {
                                p++;
                            }
                        }

                        // Add remaining part after last delimiter
                        if (*start || count > 0) { // Include empty string if we found delimiters
                            array_push(arr, start);
                            count++;
                        }
                    } else if (str && strlen(str) > 0) {
                        // No delimiter or empty delimiter: store whole string as single element
                        array_push(arr, str);
                        count = 1;
                    }

                    diag_log(DIAG_CALC, 0, "\\split<%s>{'%.30s','%.10s'} -> %d parts", arr_name,
                             str ? str : "", delim ? delim : "", count);

                    // Register type for hygienized names
                    if (arr_name[0] == '_' && arr_name[1] == 'm') {
                        type_registry_set(ctx->types, arr_name, 'a');
                    }

                    free(str);
                    free(delim);
                    free(delim_arg);
                    free(str_arg);
                    free(arr_name);

                    *end_pos =
                        6 + name_consumed + (int)(q - (p + 6 + name_consumed)) + delim_consumed;

                    char buf[32];
                    snprintf(buf, sizeof(buf), "%d", count);
                    return strdup(buf);
                }
                free(str_arg);
            }
            free(arr_name);
        }
    }

#undef RESOLVE_LINEINSERT

    // ========== \sn{assembly} - Inline Subnivean Assembly ==========
#ifdef SUBNIVEAN_ENABLED
    if (strncmp(p, "\\sn{", 4) == 0) {
        const char *q = p + 3; // point at '{'
        int brace_consumed = 0;
        char *asm_source = parse_brace_content(q, &brace_consumed);
        if (asm_source) {
            // Lazy init VM
            if (!ctx->subnivean_vm) {
                ctx->subnivean_vm = subnivean_vm_new();
            }
            VM *vm = (VM *)ctx->subnivean_vm;

            // Set callbacks (same as computational lambdas)
            subnivean_set_array_lookup(vm, subnivean_array_lookup_callback, ctx);
            subnivean_set_array_set(vm, subnivean_array_set_callback, ctx);
            subnivean_set_lambda_compile(vm, subnivean_lambda_compile_callback, ctx);

            char compile_error[256] = {0};
            SubniveanFunction *func =
                subnivean_assemble(vm, asm_source, compile_error, sizeof(compile_error));
            if (!func) {
                fprintf(stderr, "\\sn assembly error: %s\n", compile_error);
                free(asm_source);
                *end_pos = 3 + brace_consumed;
                return strdup("");
            }

            char *result = subnivean_execute(vm, func, NULL, 0);
            subnivean_function_decref(func);
            free(asm_source);
            *end_pos = 3 + brace_consumed;
            return result ? result : strdup("");
        }
    }
#endif

    return NULL; // Not a calc command
}

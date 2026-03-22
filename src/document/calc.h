// calc.h - Computational features for Hyades
//
// Provides:
// - Arithmetic: \add{a,b}, \sub{a,b}, \mul{a,b}, \div{a,b}, \mod{a,b}
// - Comparisons: \eq{a,b}, \ne{a,b}, \gt{a,b}, \lt{a,b}, \ge{a,b}, \le{a,b}
// - String ops: \streq{str1,str2}, \startswith{str,prefix}, \endswith{str,suffix}, ...
// - Counters: \let<name>{value}, \inc<name>, \dec<name>, \valueof<name>
// - Built-ins: \width, \mathmode, \displaymath, \unicode, \getlinebreaker
// - Conditionals: \if{cond}{true}\else{false}
// - Storage: \measure<name,w,h>{content}, \assign<name>{content}, \recall<name>

#ifndef CALC_H
#define CALC_H

#include <stdbool.h>

// Forward declaration for macro registry (used for expanding content in \measure)
typedef struct MacroRegistry MacroRegistry;

// Forward declaration for LSP symbol table (if not already defined)
#ifndef SYMBOL_TABLE_H_INCLUDED
struct LspSymbolTable;
#endif

// ============================================================================
// Counter Registry
// ============================================================================

typedef struct CounterRegistry CounterRegistry;

CounterRegistry *counter_registry_new(void);
void counter_registry_free(CounterRegistry *reg);

// Set a counter value (creates if doesn't exist)
void counter_set(CounterRegistry *reg, const char *name, int value);

// Get a counter value (returns 0 if doesn't exist)
int counter_get(CounterRegistry *reg, const char *name);

// Increment counter, returns new value
int counter_inc(CounterRegistry *reg, const char *name);

// Decrement counter, returns new value
int counter_dec(CounterRegistry *reg, const char *name);

// ============================================================================
// Content Storage (for \measure and \recall)
// ============================================================================

typedef struct ContentRegistry ContentRegistry;

ContentRegistry *content_registry_new(void);
void content_registry_free(ContentRegistry *reg);

// Store content with dimensions
void content_store(ContentRegistry *reg, const char *name, const char *content, int width,
                   int height);

// Get stored content (returns NULL if not found)
const char *content_get(ContentRegistry *reg, const char *name);

// Get stored dimensions (returns 0 if not found)
int content_get_width(ContentRegistry *reg, const char *name);
int content_get_height(ContentRegistry *reg, const char *name);

// ============================================================================
// Array Storage (for \let<name[]> and \assign<name[]>)
// ============================================================================

typedef struct ArrayRegistry ArrayRegistry;

ArrayRegistry *array_registry_new(void);
void array_registry_free(ArrayRegistry *reg);

// ============================================================================
// Type Registry (maps hygienized names to their registry type)
// ============================================================================

typedef struct TypeRegistry TypeRegistry;

TypeRegistry *type_registry_new(void);
void type_registry_free(TypeRegistry *reg);

// Register a name with its type ('v' = value, 'l' = lambda, 'c' = counter, 'a' = array)
void type_registry_set(TypeRegistry *reg, const char *name, char type);

// Get type for a name (returns 0 if not found)
char type_registry_get(TypeRegistry *reg, const char *name);

// ============================================================================
// Lambda Storage (for \lambda<name>[params]{body})
// ============================================================================

typedef struct Lambda Lambda;
typedef struct LambdaRegistry LambdaRegistry;

LambdaRegistry *lambda_registry_new(void);
void lambda_registry_free(LambdaRegistry *reg);

// Create a new lambda
// params: array of parameter names (NULL for thunks)
// n_params: number of parameters (0 for thunks)
// body: the unevaluated expression
// Returns: newly allocated Lambda (caller owns), or NULL on error
Lambda *lambda_new(char **params, int n_params, const char *body);
void lambda_free(Lambda *l);

// Store a lambda (takes ownership of the Lambda pointer)
void lambda_store(LambdaRegistry *reg, const char *name, Lambda *lambda);

// Get a lambda (returns NULL if not found, does NOT transfer ownership)
Lambda *lambda_get(LambdaRegistry *reg, const char *name);

// Get lambda info
int lambda_param_count(Lambda *l);
const char *lambda_get_body(Lambda *l);
const char *lambda_get_param(Lambda *l, int index);

// ============================================================================
// Scope (lexical scoping with reference counting)
// ============================================================================

typedef struct Scope Scope;

// Create a new scope with optional parent (parent can be NULL for global)
// If parent is non-NULL, increments parent's refcount
Scope *scope_new(Scope *parent);

// Increment refcount
void scope_incref(Scope *s);

// Decrement refcount; frees when it reaches 0
void scope_decref(Scope *s);

// Get parent scope (for scope chain walking)
Scope *scope_get_parent(Scope *s);

// Bind a value in this scope (immediate)
void scope_bind_value(Scope *s, const char *name, const char *content);

// Bind a lambda in this scope (takes ownership of Lambda)
// The lambda's captured_scope is set to this same scope
void scope_bind_lambda(Scope *s, const char *name, Lambda *lambda);

// Bind a lambda in one scope but capture a different scope
// store_in: scope where lambda will be stored (for lookup accessibility)
// capture: scope the lambda captures (for lexical closure)
// This is needed when lambdas are defined in iteration scopes but need to be
// accessible after the loop completes
void scope_bind_lambda_with_capture(Scope *store_in, Scope *capture, const char *name,
                                    Lambda *lambda);

// Bind a counter in this scope
void scope_bind_counter(Scope *s, const char *name, int value);

// Lookup (walks scope chain); returns NULL if not found
// For values: returns the content string (do not free)
// For lambdas: returns Lambda* cast to void* - check with scope_is_lambda
const char *scope_lookup_value(Scope *s, const char *name);
Lambda *scope_lookup_lambda(Scope *s, const char *name);
int scope_lookup_counter(Scope *s, const char *name, bool *found);

// Set counter value (creates if doesn't exist in current scope)
void scope_set_counter(Scope *s, const char *name, int value);
int scope_inc_counter(Scope *s, const char *name);
int scope_dec_counter(Scope *s, const char *name);

// Get array length (returns 0 if array not found)
int array_registry_len(ArrayRegistry *reg, const char *name);

// Get array element at index (returns NULL if not found or out of bounds)
const char *array_registry_get(ArrayRegistry *reg, const char *name, int index);

// ============================================================================
// Expression Expansion Context
// ============================================================================

typedef struct {
    // Legacy registries (kept for backwards compatibility during transition)
    CounterRegistry *counters;
    ContentRegistry *contents;
    ArrayRegistry *arrays; // For \let<name[]> and \assign<name[]>
    TypeRegistry *types;   // Maps hygienized names to their registry type

    // New scope-based storage
    Scope *current_scope;        // Current scope in chain
    Scope *global_scope;         // Root scope (same as current_scope initially)
    Scope *lambda_storage_scope; // If non-NULL, store new lambdas here instead of current_scope
                                 // Used to make lambdas in iteration blocks accessible after loop

    MacroRegistry *user_macros; // For expanding content in \measure
    int width;                  // Current document width
    bool in_math_mode;          // True if inside $ or $$
    bool is_display_math;       // True if inside $$ (display math)
    const char *linebreaker;    // Current linebreaker: "greedy", "knuth", or "raggedright"

    // Loop control
    bool exit_loop_requested; // Set by \exit_when when condition is true
                              // Causes expand_calc to stop immediately

    // Rvalue tracking for move semantics (computational lambdas)
    int rvalue_counter;    // Counter for generating unique _rv_N names
    char **active_rvalues; // List of rvalue names that can be cleaned up
    int n_rvalues;         // Number of active rvalues
    int rvalues_cap;       // Capacity of rvalues array

    // Return value from computational lambda
    char *return_value;    // Set by \return{}, read by lambda caller
    bool return_requested; // True when \return{} was called
    bool in_computational; // True when executing a computational #{} lambda

    // LSP integration (optional)
    struct LspSymbolTable *symbols; // Symbol table to populate (may be NULL)
    int current_line;               // Current line for symbol position tracking
    int current_col;                // Current column for symbol position tracking

    // Subnivean VM (optional, for compiled computational lambdas)
    void *subnivean_vm; // VM* when SUBNIVEAN_ENABLED, NULL otherwise

    // Terminal settings for \main{} loop
    int clear_bg_code; // ANSI background code for screen clears (-1 = default)
} CalcContext;

// ============================================================================
// Context Management
// ============================================================================

// Initialize a CalcContext with new registries
void calc_context_init(CalcContext *ctx);

// Free resources in a CalcContext (but not the ctx struct itself)
void calc_context_free(CalcContext *ctx);

// ============================================================================
// Expansion Functions
// ============================================================================

// Try to expand a calc command at position p
// Returns: newly allocated expanded string, or NULL if not a calc command
// Sets *end_pos to characters consumed
// Sets error_msg on error (and returns NULL)
char *calc_try_expand(const char *p, int *end_pos, CalcContext *ctx, char *error_msg,
                      int error_size);

// Expand all calc commands in a string
// Returns: newly allocated string with all calc commands expanded
// Caller must free()
char *expand_calc(const char *input, CalcContext *ctx);

// Check if input contains any calc commands (for quick skip)
bool calc_has_commands(const char *input);

// Try to expand a ${name} variable access at position p
// Returns: newly allocated string with value, or NULL if not ${...}
// Sets *end_pos to characters consumed
char *calc_try_expand_dollar(const char *p, int *end_pos, CalcContext *ctx);

#endif // CALC_H

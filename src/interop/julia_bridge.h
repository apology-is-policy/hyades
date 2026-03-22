// julia_bridge.h — Julia computation bridge for Hyades
// Dynamic loading: Julia is detected and loaded at runtime if available
// No compile-time dependency on Julia

#ifndef JULIA_BRIDGE_H
#define JULIA_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

// Result types that map to Hyades rendering
typedef enum {
    JULIA_RESULT_ERROR,
    JULIA_RESULT_NIL,
    JULIA_RESULT_INT,
    JULIA_RESULT_FLOAT,
    JULIA_RESULT_STRING,
    JULIA_RESULT_VECTOR,
    JULIA_RESULT_MATRIX,
    JULIA_RESULT_TEX // Raw TeX string (Julia code returns TeX directly)
} JuliaResultType;

typedef struct {
    JuliaResultType type;

    union {
        int64_t int_val;
        double float_val;
        char *string_val; // Also used for error message and TeX

        struct {
            double *data;
            int len;
        } vector;

        struct {
            double *data;
            int rows;
            int cols;
            bool is_column_major; // Julia uses column-major order
        } matrix;
    };
} JuliaResult;

// ============================================================================
// Lifecycle
// ============================================================================

// Initialize Julia runtime (call once at startup)
// Attempts to find and load Julia dynamically
// Returns true on success, false if Julia unavailable or init failed
bool julia_init(void);

// Shutdown Julia runtime (call at program exit)
void julia_shutdown(void);

// Check if Julia is initialized and available
bool julia_available(void);

// ============================================================================
// Computation Registry
// ============================================================================

// Register a named computation
// name: identifier for \call[name]{...}
// params: parameter specification like "a, b" or "matrix::Matrix, v::Vector" (can be NULL)
// code: Julia source code (the function body)
// Returns true on success
bool julia_register(const char *name, const char *params, const char *code);

// Check if a computation is registered
bool julia_is_registered(const char *name);

// Unregister a computation (returns true if it existed)
bool julia_unregister(const char *name);

// Clear all registered computations
void julia_clear_registry(void);

// ============================================================================
// Execution
// ============================================================================

// Execute a registered computation with arguments
// name: the registered computation name
// args: Julia expression for arguments, e.g. "[1,2,3], [4,5,6]"
// Returns result that must be freed with julia_result_free()
JuliaResult *julia_call(const char *name, const char *args);

// Execute arbitrary Julia code directly
// Returns result that must be freed with julia_result_free()
JuliaResult *julia_eval(const char *code);

// ============================================================================
// Result Handling
// ============================================================================

// Free a result
void julia_result_free(JuliaResult *result);

// Convert result to Hyades TeX source
// Returns newly allocated string, caller must free
// Examples:
//   NIL           -> ""
//   INT 42        -> "42"
//   FLOAT 3.14159 -> "3.14159"
//   VECTOR [1,2,3] -> "\\pmatrix{1 \\\\ 2 \\\\ 3}"
//   MATRIX 2x2    -> "\\pmatrix{1 & 2 \\\\ 3 & 4}"
//   STRING "hi"   -> "hi"
//   TEX "..."     -> "..." (passthrough)
//   ERROR "..."   -> "\\text{Error: ...}"
char *julia_result_to_tex(const JuliaResult *result);

// Get human-readable type name
const char *julia_result_type_name(JuliaResultType type);

// ============================================================================
// Configuration
// ============================================================================

// Set float formatting precision (default: 6)
void julia_set_float_precision(int digits);

// Set whether to use scientific notation for large/small numbers (default: true)
void julia_set_scientific_notation(bool enabled);

#endif // JULIA_BRIDGE_H
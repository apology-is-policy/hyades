// subnivean.h - Subnivean bytecode engine for Hyades
//
// Public API for the Subnivean compiler and virtual machine.
//
// Subnivean compiles Hyades computational expressions (\let, \if, \lambda, etc.)
// into bytecode for efficient execution, while passing through typesetting
// commands to the existing text-based interpreter.
//
// Usage:
//
//     // Compile source to bytecode
//     SubniveanModule *module = subnivean_compile(source);
//     if (!module) {
//         fprintf(stderr, "Compile error: %s\n", subnivean_last_error());
//         return;
//     }
//
//     // Create execution context
//     SubniveanContext *ctx = subnivean_context_new();
//
//     // Execute and get output
//     char *output = subnivean_execute(module, ctx);
//
//     // Clean up
//     subnivean_context_free(ctx);
//     subnivean_module_free(module);
//     free(output);
//

#ifndef SUBNIVEAN_H
#define SUBNIVEAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ============================================================================
// Opaque Types
// ============================================================================

typedef struct SubniveanModule SubniveanModule;
typedef struct SubniveanContext SubniveanContext;

// ============================================================================
// Compilation
// ============================================================================

// Compile source code into a module
// Returns NULL on error; call subnivean_last_error() for message
SubniveanModule *subnivean_compile(const char *source);

// Compile from AST (for integration with existing parser)
struct AstNode;
SubniveanModule *subnivean_compile_ast(struct AstNode *ast);

// Free a compiled module
void subnivean_module_free(SubniveanModule *module);

// Get last error message (thread-local)
const char *subnivean_last_error(void);

// ============================================================================
// Execution Context
// ============================================================================

// Create a new execution context
SubniveanContext *subnivean_context_new(void);

// Free an execution context
void subnivean_context_free(SubniveanContext *ctx);

// Set a global variable (counter) before execution
void subnivean_set_global_int(SubniveanContext *ctx, const char *name, int32_t value);

// Set a global string variable before execution
void subnivean_set_global_string(SubniveanContext *ctx, const char *name, const char *value);

// Get a global variable after execution
int32_t subnivean_get_global_int(SubniveanContext *ctx, const char *name, bool *found);
const char *subnivean_get_global_string(SubniveanContext *ctx, const char *name, bool *found);

// ============================================================================
// Execution
// ============================================================================

// Execute a module in a context
// Returns the output string (caller owns), or NULL on error
char *subnivean_execute(SubniveanModule *module, SubniveanContext *ctx);

// Execute with a callback for text splices
// This allows integration with the existing typesetting engine
typedef void (*SubniveanTextCallback)(const char *text, size_t len, void *user_data);
char *subnivean_execute_with_callback(SubniveanModule *module, SubniveanContext *ctx,
                                      SubniveanTextCallback callback, void *user_data);

// ============================================================================
// Debugging
// ============================================================================

// Disassemble a module to a string (caller owns)
char *subnivean_disassemble(SubniveanModule *module);

// Print disassembly to stderr
void subnivean_disassemble_print(SubniveanModule *module);

// Enable/disable execution tracing (prints each instruction)
void subnivean_set_trace(bool enabled);

// ============================================================================
// Integration with Hyades
// ============================================================================

// Check if source contains any computational commands
// Used to decide whether to use Subnivean or fall back to text interpreter
bool subnivean_has_computational(const char *source);

// Extract computational regions from mixed source
// Returns array of regions (start, length pairs)
// Caller owns the returned array
typedef struct {
    int start;  // Offset into source
    int length; // Length of region
    bool is_computational;
} SubniveanRegion;

SubniveanRegion *subnivean_analyze_regions(const char *source, int *n_regions);
void subnivean_regions_free(SubniveanRegion *regions);

// ============================================================================
// Module Inspection
// ============================================================================

// Get number of functions in module
int subnivean_module_func_count(SubniveanModule *module);

// Get function name by index (NULL for top-level)
const char *subnivean_module_func_name(SubniveanModule *module, int index);

// Get bytecode size (for statistics)
size_t subnivean_module_code_size(SubniveanModule *module);

// Get constant pool size
size_t subnivean_module_const_size(SubniveanModule *module);

// ============================================================================
// Version
// ============================================================================

// Version string
#define SUBNIVEAN_VERSION "0.1.0"

// Get version at runtime
const char *subnivean_version(void);

#endif // SUBNIVEAN_H

// vm.h - Subnivean 2.0 Virtual Machine
//
// Stack-based bytecode interpreter with first-class scopes.

#ifndef SUBNIVEAN_VM_H
#define SUBNIVEAN_VM_H

#include "function.h"
#include "scope.h"
#include "value.h"
#include <stdbool.h>

// ============================================================================
// Configuration
// ============================================================================

#define VM_STACK_MAX 4096
#define VM_FRAMES_MAX 256

// ============================================================================
// Call Frame
// ============================================================================

typedef struct {
    Closure *closure; // Function being executed
    Instruction *ip;  // Instruction pointer
    Value *bp;        // Base pointer (start of locals on stack)
    Scope *scope;     // Scope for this call
} CallFrame;

// ============================================================================
// External Callbacks (for interpreter integration)
// ============================================================================

// Callback to look up arrays from external context (e.g., interpreter's registry)
// Returns: array elements count and fills elements, or -1 if not found
// The caller must free the returned strings
typedef int (*ExternalArrayLookup)(void *ctx, const char *name, char ***elements, int *n_elements);

// Callback to set an element in an external array
// Returns: 0 on success, -1 if array not found
typedef int (*ExternalArraySet)(void *ctx, const char *name, int index, const char *value);

// Callback to get a compiled Subnivean function for a lambda
// Returns: Function* on success, NULL if not found or not compilable
// The returned Function is owned by the interpreter (don't decref in VM)
typedef struct Function *(*ExternalLambdaCompile)(void *ctx, const char *name);

// Callback to call a lambda defined in the interpreter (fallback for non-computational)
// Returns: result string (caller must free), or NULL if lambda not found
typedef char *(*ExternalLambdaCall)(void *ctx, const char *name, const char **args, int n_args);

// ============================================================================
// Virtual Machine
// ============================================================================

typedef struct VM {
    // Value stack
    Value stack[VM_STACK_MAX];
    Value *sp; // Stack pointer (next free slot)

    // Call stack
    CallFrame frames[VM_FRAMES_MAX];
    int frame_count;

    // Current scope (may differ from frame scope during SCOPE_NEW/POP)
    Scope *scope;

    // Global scope
    Scope *global;

    // Symbol table (interned strings)
    SymbolTable symbols;

    // Pre-interned common symbols (for performance)
    struct {
        Symbol *nil;
        Symbol *true_;
        Symbol *false_;
    } syms;

    // Output buffer (for text-generating languages)
    char *output;
    size_t output_len;
    size_t output_cap;

    // Execution state
    bool running;
    bool had_error;
    char error_msg[512];

    // Debug options
    bool trace;       // Print each instruction
    bool trace_stack; // Print stack after each instruction

    // External callbacks (for interpreter integration)
    ExternalArrayLookup external_array_lookup;
    ExternalArraySet external_array_set;
    ExternalLambdaCompile external_lambda_compile;
    ExternalLambdaCall external_lambda_call;
    void *external_ctx;
} VM;

// ============================================================================
// VM Lifecycle
// ============================================================================

// Initialize a new VM
void vm_init(VM *vm);

// Free VM resources
void vm_free(VM *vm);

// Reset VM for new execution (keeps globals and symbols)
void vm_reset(VM *vm);

// ============================================================================
// Execution
// ============================================================================

// Execute a function in the VM
// Returns the output string (caller owns), or NULL on error
char *vm_execute(VM *vm, Function *main);

// Call a closure with arguments already on stack
// Returns true on success, false on error
bool vm_call(VM *vm, Closure *closure, int argc);

// Run the VM until completion (after vm_call sets up the frame)
// Returns true on success, false on error
bool vm_run(VM *vm);

// ============================================================================
// Stack Operations
// ============================================================================

void vm_push(VM *vm, Value v);
Value vm_pop(VM *vm);
Value vm_peek(VM *vm, int distance);

// ============================================================================
// Global Bindings (convenience for setting up builtins)
// ============================================================================

// Bind a value in the global scope
void vm_define_global(VM *vm, const char *name, Value v);

// Lookup in global scope
bool vm_get_global(VM *vm, const char *name, Value *out);

// ============================================================================
// Error Handling
// ============================================================================

// Set runtime error
void vm_error(VM *vm, const char *fmt, ...);

// Get error message (NULL if no error)
const char *vm_get_error(VM *vm);

// ============================================================================
// Debug
// ============================================================================

// Enable/disable execution tracing
void vm_set_trace(VM *vm, bool enabled);

// Print current stack
void vm_print_stack(VM *vm);

// ============================================================================
// Symbol Helpers
// ============================================================================

// Intern a symbol in the VM's symbol table
Symbol *vm_intern(VM *vm, const char *name);
Symbol *vm_intern_len(VM *vm, const char *name, size_t len);

#endif // SUBNIVEAN_VM_H

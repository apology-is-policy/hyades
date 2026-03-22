// subnivean_api.h - Clean API for Subnivean VM integration
//
// This provides a stable, conflict-free API for integrating Subnivean
// into other parts of the codebase (like calc.c) without exposing
// internal types that might conflict.

#ifndef SUBNIVEAN_API_H
#define SUBNIVEAN_API_H

// Opaque types to avoid header conflicts
typedef struct VM VM;
typedef struct SubniveanFunction SubniveanFunction;

// Callback type for external array lookup (e.g., from interpreter's registry)
// Returns: 0 on success (fills elements and n_elements), -1 if not found
// The callback must allocate strings that the caller will free
typedef int (*SubniveanArrayLookup)(void *ctx, const char *name, char ***elements, int *n_elements);

// Callback type for external array set (for indirect array writes)
// Returns: 0 on success, -1 if array not found
typedef int (*SubniveanArraySet)(void *ctx, const char *name, int index, const char *value);

// Callback type for external lambda compilation
// When Subnivean encounters a lambda it doesn't know, it calls this to get
// the compiled function. Returns: SubniveanFunction* on success, NULL if not found
// The returned function is owned by the interpreter (cached in Lambda.compiled)
typedef SubniveanFunction *(*SubniveanLambdaCompile)(void *ctx, const char *name);

// Create a new VM instance
VM *subnivean_vm_new(void);

// Free a VM instance
void subnivean_vm_free(VM *vm);

// Compile a computational lambda body to bytecode
// Returns NULL on error (error_msg will contain details)
SubniveanFunction *subnivean_compile(VM *vm, const char *name, char **params, int n_params,
                                     const char *source, char *error_msg, int error_size);

// Execute a compiled function with arguments
// Returns the result as a string (caller must free), or NULL on error
char *subnivean_execute(VM *vm, SubniveanFunction *func, const char **args, int n_args);

// Decrement reference count on a function
void subnivean_function_decref(SubniveanFunction *func);

// Assemble inline Subnivean assembly source into a function
// Returns NULL on error (error_msg filled with details)
SubniveanFunction *subnivean_assemble(VM *vm, const char *source, char *error_msg, int error_size);

// Disassemble a compiled function to a string (caller must free)
char *subnivean_disassemble(SubniveanFunction *func);

// Set external array lookup callback (for accessing interpreter's arrays)
void subnivean_set_array_lookup(VM *vm, SubniveanArrayLookup lookup, void *ctx);

// Set external array set callback (for indirect array writes)
void subnivean_set_array_set(VM *vm, SubniveanArraySet set_fn, void *ctx);

// Set external lambda compile callback (for calling other lambdas)
void subnivean_set_lambda_compile(VM *vm, SubniveanLambdaCompile compile, void *ctx);

#endif // SUBNIVEAN_API_H

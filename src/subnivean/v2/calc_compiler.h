// calc_compiler.h - Compiler from Hyades Calc AST to Subnivean Bytecode

#ifndef CALC_COMPILER_H
#define CALC_COMPILER_H

#include "calc_ast.h"
#include "function.h"
#include "vm.h"

// ============================================================================
// Compiler State
// ============================================================================

typedef struct {
    VM *vm;         // For symbol interning
    Function *func; // Function being compiled

    // Error handling
    char error_msg[256];
    bool had_error;

    // Loop context for exit_when
    int loop_exit_jumps[64];
    int n_loop_exits;
    int loop_depth;

    // Name tracking for arrays vs lambdas vs cells
    char *arrays[64]; // Names bound as arrays
    int n_arrays;
    char *lambdas[64]; // Names bound as lambdas
    int n_lambdas;
    char *params[64]; // Lambda parameter names (not cells)
    int n_params;
} CalcCompiler;

// ============================================================================
// Compiler API
// ============================================================================

// Initialize compiler
void calc_compiler_init(CalcCompiler *cc, VM *vm);

// Compile a computational lambda body to a Function
// Returns NULL on error
Function *calc_compile(CalcCompiler *cc, const char *name, char **params, int n_params,
                       AstNode *body);

// Compile a standalone expression (for testing)
Function *calc_compile_expr(CalcCompiler *cc, const char *name, AstNode *expr);

// Get error message
const char *calc_compiler_error(CalcCompiler *cc);

// ============================================================================
// High-Level API
// ============================================================================

// Parse and compile a computational lambda body in one step
Function *calc_compile_source(VM *vm, const char *name, char **params, int n_params,
                              const char *source, char *error_msg, int error_size);

#endif // CALC_COMPILER_H

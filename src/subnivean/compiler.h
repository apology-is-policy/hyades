// compiler.h - Subnivean bytecode compiler
//
// Compiles AST to bytecode for the Subnivean VM.

#ifndef SUBNIVEAN_COMPILER_H
#define SUBNIVEAN_COMPILER_H

#include "ast.h"
#include "opcodes.h"
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// Bytecode Instruction
// ============================================================================

typedef struct {
    uint8_t op;      // OpCode
    int32_t operand; // Operand (immediate value, offset, or index)
} Instruction;

// ============================================================================
// Compiled Function
// ============================================================================

typedef struct CompiledFunc CompiledFunc;

struct CompiledFunc {
    char *name; // Function name (NULL for top-level)

    // Bytecode
    Instruction *code;
    int code_len;
    int code_cap;

    // Parameters and locals
    int n_params;       // Parameters are locals[0..n_params-1]
    int n_locals;       // Total local slots needed
    char **local_names; // Names for debugging (optional)

    // Upvalues (captured from enclosing scope)
    int n_upvalues;
    struct {
        uint8_t index; // Index in enclosing function
        bool is_local; // true = local in enclosing, false = upvalue in enclosing
    } *upvalues;

    // Constant pool (strings, nested functions)
    struct {
        enum { CONST_STRING, CONST_INT, CONST_FUNC } type;
        union {
            char *string;
            int32_t integer;
            CompiledFunc *func;
        };
    } *constants;
    int n_constants;
    int constants_cap;

    // Metadata
    bool is_computational; // true for #{ } lambdas
    int source_line;       // Starting line in source
};

// ============================================================================
// Compiler State
// ============================================================================

typedef struct {
    // Current function being compiled
    CompiledFunc *function;

    // Local variable tracking
    struct {
        char *name;
        int depth;        // Scope nesting depth
        bool is_captured; // true if captured by a closure
    } locals[256];
    int local_count;
    int scope_depth;

    // Upvalue tracking
    struct {
        uint8_t index;
        bool is_local;
    } upvalues[256];

    // Loop tracking (for exit_when compilation)
    struct {
        int loop_start;  // Instruction index of loop start
        int *exit_jumps; // Indices of OP_JUMP_IF_TRUE to patch
        int exit_jump_count;
        int exit_jump_cap;
    } loops[32];
    int loop_depth;

    // Enclosing compiler (for nested functions)
    struct Compiler *enclosing;

    // Error state
    bool had_error;
    char error_msg[512];
} Compiler;

// ============================================================================
// Compilation Result
// ============================================================================

typedef struct {
    CompiledFunc *main;   // Top-level function
    CompiledFunc **funcs; // All compiled functions (including nested)
    int n_funcs;

    bool success;
    char error_msg[512];
} CompileResult;

// ============================================================================
// Compiler Interface
// ============================================================================

// Compile an AST into bytecode
// Returns a CompileResult with the compiled functions
// Caller owns the result and must call compile_result_free()
CompileResult compile(AstNode *ast);

// Compile a single lambda into a function
// Used internally and for JIT-style compilation
CompiledFunc *compile_lambda(AstNode *lambda_node, Compiler *enclosing);

// Free compilation result
void compile_result_free(CompileResult *result);

// Free a single compiled function
void compiled_func_free(CompiledFunc *func);

// ============================================================================
// Bytecode Utilities
// ============================================================================

// Disassemble a function to a string (for debugging)
// Caller owns the returned string
char *disassemble(CompiledFunc *func);

// Disassemble a single instruction
// Returns number of bytes consumed
int disassemble_instruction(CompiledFunc *func, int offset, char *buf, int buf_size);

// Print disassembly to stderr
void disassemble_print(CompiledFunc *func);

// ============================================================================
// Constant Pool
// ============================================================================

// Add a string constant, return its index
int compiler_add_string_constant(Compiler *c, const char *str);

// Add an integer constant, return its index
int compiler_add_int_constant(Compiler *c, int32_t value);

// Add a function constant, return its index
int compiler_add_func_constant(Compiler *c, CompiledFunc *func);

// ============================================================================
// Local Variable Resolution
// ============================================================================

// Resolve a local variable by name
// Returns local slot index, or -1 if not found
int compiler_resolve_local(Compiler *c, const char *name);

// Resolve an upvalue (captured variable from enclosing scope)
// Returns upvalue index, or -1 if not found
int compiler_resolve_upvalue(Compiler *c, const char *name);

// Add a local variable to current scope
// Returns local slot index
int compiler_add_local(Compiler *c, const char *name);

// Begin a new scope
void compiler_begin_scope(Compiler *c);

// End current scope (pops locals)
void compiler_end_scope(Compiler *c);

// ============================================================================
// Code Generation
// ============================================================================

// Emit an instruction with operand
void emit(Compiler *c, OpCode op, int32_t operand);

// Emit an instruction without operand
void emit_simple(Compiler *c, OpCode op);

// Emit a jump instruction, return the index to patch later
int emit_jump(Compiler *c, OpCode op);

// Patch a jump instruction at given index to jump to current position
void patch_jump(Compiler *c, int jump_index);

// Emit a loop instruction (backward jump)
void emit_loop(Compiler *c, int loop_start);

// Current bytecode position
int current_offset(Compiler *c);

#endif // SUBNIVEAN_COMPILER_H

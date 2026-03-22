// function.h - Subnivean 2.0 Function and Closure Types
//
// Functions are compiled code. Closures are functions + captured scope.

#ifndef SUBNIVEAN_FUNCTION_H
#define SUBNIVEAN_FUNCTION_H

#include "opcode.h"
#include "value.h"

// ============================================================================
// Constant Pool Entry
// ============================================================================

typedef enum {
    CONST_INT,
    CONST_STRING,
    CONST_SYMBOL,
    CONST_FUNCTION,
} ConstantKind;

typedef struct {
    ConstantKind kind;
    union {
        int64_t as_int;
        String *as_string;
        Symbol *as_symbol;
        struct Function *as_func;
    };
} Constant;

// ============================================================================
// Function (Compiled Code)
// ============================================================================

struct Function {
    // Identity
    char *name;  // Function name (NULL for anonymous/main)
    uint32_t id; // Unique ID

    // Parameters
    int arity;       // Number of parameters
    Symbol **params; // Parameter names (for binding on call)

    // Bytecode
    Instruction *code;
    int code_len;
    int code_cap;

    // Constant pool
    Constant *constants;
    int n_constants;
    int constants_cap;

    // Debug info
    int *lines;         // Source line per instruction (optional)
    const char *source; // Source filename (optional)

    // Refcount (functions can be shared in constant pools)
    int refcount;
};

Function *function_new(const char *name, int arity);
void function_incref(Function *f);
void function_decref(Function *f);

// Add to constant pool, return index
int function_add_int(Function *f, int64_t value);
int function_add_string(Function *f, String *s);
int function_add_symbol(Function *f, Symbol *sym);
int function_add_function(Function *f, Function *inner);

// Emit instruction
void function_emit(Function *f, OpCode op, int32_t operand);
void function_emit_simple(Function *f, OpCode op);
int function_emit_jump(Function *f, OpCode op); // Returns index to patch
void function_patch_jump(Function *f, int index);
void function_emit_loop(Function *f, int loop_start);

// Current bytecode position
int function_offset(Function *f);

// Disassembly
void function_disassemble(Function *f);
char *function_disassemble_to_string(Function *f); // Caller must free

// ============================================================================
// Closure (Function + Captured Scope)
// ============================================================================

struct Closure {
    Function *func;  // The compiled code
    Scope *captured; // Scope at definition time

    int refcount;
};

Closure *closure_new(Function *func, Scope *captured);
void closure_incref(Closure *cl);
void closure_decref(Closure *cl);

// Now we can define value_closure
static inline Value value_closure(Closure *cl) {
    closure_incref(cl);
    return (Value){.kind = VAL_CLOSURE, .as_closure = cl};
}

#endif // SUBNIVEAN_FUNCTION_H

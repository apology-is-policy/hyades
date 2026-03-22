// vm.h - Subnivean Virtual Machine
//
// Stack-based bytecode interpreter.

#ifndef SUBNIVEAN_VM_H
#define SUBNIVEAN_VM_H

#include "compiler.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ============================================================================
// Value Type
// ============================================================================

typedef enum {
    VAL_INT,
    VAL_STRING,
    VAL_ARRAY,
    VAL_CLOSURE,
    VAL_NULL,
} ValueType;

typedef struct Value Value;
typedef struct Array Array;
typedef struct Closure Closure;

struct Value {
    ValueType type;
    union {
        int32_t integer;
        char *string;     // Owned
        Array *array;     // Shared (refcounted)
        Closure *closure; // Shared (refcounted)
    };
};

// ============================================================================
// Array
// ============================================================================

struct Array {
    Value *elements;
    int length;
    int capacity;
    int refcount;
};

Array *array_new(int initial_capacity);
void array_free(Array *arr);
void array_incref(Array *arr);
void array_decref(Array *arr);
void array_push(Array *arr, Value val);
Value array_pop(Array *arr);
Value array_get(Array *arr, int index);
void array_set(Array *arr, int index, Value val);

// ============================================================================
// Closure
// ============================================================================

typedef struct Upvalue Upvalue;

struct Upvalue {
    Value *location; // Points to stack slot or closed value
    Value closed;    // Storage when closed
    Upvalue *next;   // Linked list of open upvalues
    int refcount;
};

struct Closure {
    CompiledFunc *func;
    Upvalue **upvalues;
    int n_upvalues;
    int refcount;
};

Closure *closure_new(CompiledFunc *func);
void closure_free(Closure *closure);
void closure_incref(Closure *closure);
void closure_decref(Closure *closure);

// ============================================================================
// Call Frame
// ============================================================================

typedef struct {
    Closure *closure;
    Instruction *ip; // Instruction pointer
    Value *slots;    // Base of local variable slots on stack
} CallFrame;

// ============================================================================
// Virtual Machine
// ============================================================================

#define STACK_MAX 1024
#define FRAMES_MAX 64

typedef struct {
    // Call stack
    CallFrame frames[FRAMES_MAX];
    int frame_count;

    // Value stack
    Value stack[STACK_MAX];
    Value *stack_top;

    // Global variables (hash table would be better, but simple array for now)
    struct {
        char *name;
        Value value;
    } globals[256];
    int n_globals;

    // Open upvalues (linked list)
    Upvalue *open_upvalues;

    // Output buffer
    char *output;
    size_t output_len;
    size_t output_cap;

    // Execution state
    bool running;
    bool had_error;
    char error_msg[256];

    // Debug
    bool trace;
} VM;

// ============================================================================
// VM Lifecycle
// ============================================================================

// Initialize VM
void vm_init(VM *vm);

// Free VM resources
void vm_free(VM *vm);

// Reset VM for new execution (keeps globals)
void vm_reset(VM *vm);

// ============================================================================
// Execution
// ============================================================================

// Execute a compiled function
// Returns output string (caller owns), or NULL on error
char *vm_execute(VM *vm, CompiledFunc *func);

// Execute with existing closure (for calling lambdas)
bool vm_call(VM *vm, Closure *closure, int arg_count);

// ============================================================================
// Global Variables
// ============================================================================

// Set a global variable
void vm_set_global(VM *vm, const char *name, Value value);

// Get a global variable
Value vm_get_global(VM *vm, const char *name, bool *found);

// ============================================================================
// Value Utilities
// ============================================================================

// Create values
Value value_int(int32_t n);
Value value_string(const char *s);
Value value_array(Array *arr);
Value value_closure(Closure *c);
Value value_null(void);

// Copy a value (increments refcount for heap objects)
Value value_copy(Value v);

// Free a value (decrements refcount for heap objects)
void value_free(Value v);

// Print value for debugging
void value_print(Value v);

// Convert value to int (for conditions)
int32_t value_to_int(Value v);

// Convert value to string (for output)
char *value_to_string(Value v);

// ============================================================================
// Debug
// ============================================================================

// Enable/disable execution tracing
void vm_set_trace(VM *vm, bool enabled);

#endif // SUBNIVEAN_VM_H

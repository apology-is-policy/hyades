// opcodes.h - Subnivean bytecode opcodes
//
// Stack-based virtual machine instruction set for Hyades computation.

#ifndef SUBNIVEAN_OPCODES_H
#define SUBNIVEAN_OPCODES_H

typedef enum {
    // ========================================================================
    // Stack Operations
    // ========================================================================
    OP_PUSH_INT,   // Push immediate int32: operand = value
    OP_PUSH_TRUE,  // Push 1 (shorthand)
    OP_PUSH_FALSE, // Push 0 (shorthand)
    OP_PUSH_CONST, // Push constant: operand = constant pool index
    OP_POP,        // Discard top of stack
    OP_DUP,        // Duplicate top of stack
    OP_SWAP,       // Swap top two stack elements

    // ========================================================================
    // Local Variables (function scope, by index)
    // ========================================================================
    OP_LOAD_LOCAL,  // Push locals[operand]
    OP_STORE_LOCAL, // Pop into locals[operand]
    OP_INC_LOCAL,   // locals[operand]++, push new value
    OP_DEC_LOCAL,   // locals[operand]--, push new value

    // ========================================================================
    // Global/Captured Variables (by name via constant pool)
    // ========================================================================
    OP_LOAD_GLOBAL,  // Push global[constants[operand]]
    OP_STORE_GLOBAL, // Pop into global[constants[operand]]
    OP_INC_GLOBAL,   // global[name]++, push new value
    OP_DEC_GLOBAL,   // global[name]--, push new value

    // ========================================================================
    // Upvalues (for closures - captured from enclosing scope)
    // ========================================================================
    OP_LOAD_UPVALUE,  // Push upvalues[operand]
    OP_STORE_UPVALUE, // Pop into upvalues[operand]
    OP_CLOSE_UPVALUE, // Close upvalue (move to heap)

    // ========================================================================
    // Arithmetic (pop operands, push result)
    // ========================================================================
    OP_ADD, // a + b
    OP_SUB, // a - b
    OP_MUL, // a * b
    OP_DIV, // a / b (integer division)
    OP_MOD, // a % b
    OP_NEG, // -a (unary)

    // ========================================================================
    // Comparison (pop 2, push 0 or 1)
    // ========================================================================
    OP_EQ, // a == b
    OP_NE, // a != b
    OP_LT, // a < b
    OP_GT, // a > b
    OP_LE, // a <= b
    OP_GE, // a >= b

    // ========================================================================
    // Logic (pop operand(s), push 0 or 1)
    // ========================================================================
    OP_AND, // a && b (non-short-circuit for now)
    OP_OR,  // a || b
    OP_NOT, // !a

    // ========================================================================
    // Control Flow
    // ========================================================================
    OP_JUMP,          // Unconditional: ip += operand (signed offset)
    OP_JUMP_IF_FALSE, // If top == 0: ip += operand, always pop
    OP_JUMP_IF_TRUE,  // If top != 0: ip += operand, always pop
    OP_LOOP,          // Unconditional backward: ip -= operand

    // ========================================================================
    // Function Calls
    // ========================================================================
    OP_CALL,         // Call function: operand = arg count
                     // Stack: [args...][func_ref] → [result]
    OP_CALL_GLOBAL,  // Call global function by name: operand = name constant
    OP_RETURN,       // Return from function (no value, computational lambda)
    OP_RETURN_VALUE, // Return top of stack

    // ========================================================================
    // Closures
    // ========================================================================
    OP_CLOSURE,       // Create closure: operand = function constant index
                      // Followed by upvalue descriptors
    OP_DEFINE_GLOBAL, // Define global: constants[operand] = pop()

    // ========================================================================
    // Arrays
    // ========================================================================
    OP_ARRAY_NEW,  // Create array: operand = initial size
                   // Pops 'size' elements, pushes array ref
    OP_ARRAY_GET,  // [arr][idx] → [element]
    OP_ARRAY_SET,  // [arr][idx][val] → [] (modifies arr)
    OP_ARRAY_LEN,  // [arr] → [length]
    OP_ARRAY_PUSH, // [arr][val] → [] (modifies arr)
    OP_ARRAY_POP,  // [arr] → [popped_val] (modifies arr)

    // ========================================================================
    // Text Interop / Output
    // ========================================================================
    OP_TEXT_SPLICE, // Emit text: operand = constant pool index
                    // The text is passed to the output buffer as-is
    OP_OUTPUT,      // Pop value and output it (convert to string)

    // ========================================================================
    // Debug / Special
    // ========================================================================
    OP_NOP,        // No operation
    OP_HALT,       // Stop execution
    OP_BREAKPOINT, // Debugger hook

    OP_COUNT // Number of opcodes (for validation)
} OpCode;

// Opcode metadata for disassembly and debugging
typedef struct {
    const char *name;
    int operand_bytes; // 0 = none, 4 = int32
    int stack_effect;  // Net change to stack depth (-1 = pops 1, +1 = pushes 1)
                       // Use -128 for "variable" (calls, etc.)
} OpCodeInfo;

// Opcode info table (defined in opcodes.c)
extern const OpCodeInfo opcode_info[OP_COUNT];

// Helper macros
#define OP_HAS_OPERAND(op) (opcode_info[op].operand_bytes > 0)
#define OP_NAME(op) (opcode_info[op].name)

#endif // SUBNIVEAN_OPCODES_H

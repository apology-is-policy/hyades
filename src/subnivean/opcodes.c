// opcodes.c - Subnivean opcode metadata
//
// Information about each opcode for disassembly and debugging.

#include "opcodes.h"

// Stack effect: -128 means "variable" (depends on runtime)
const OpCodeInfo opcode_info[OP_COUNT] = {
    // Stack operations
    [OP_PUSH_INT] = {"PUSH_INT", 4, 1},
    [OP_PUSH_TRUE] = {"PUSH_TRUE", 0, 1},
    [OP_PUSH_FALSE] = {"PUSH_FALSE", 0, 1},
    [OP_PUSH_CONST] = {"PUSH_CONST", 4, 1},
    [OP_POP] = {"POP", 0, -1},
    [OP_DUP] = {"DUP", 0, 1},
    [OP_SWAP] = {"SWAP", 0, 0},

    // Local variables
    [OP_LOAD_LOCAL] = {"LOAD_LOCAL", 4, 1},
    [OP_STORE_LOCAL] = {"STORE_LOCAL", 4, -1},
    [OP_INC_LOCAL] = {"INC_LOCAL", 4, 1},
    [OP_DEC_LOCAL] = {"DEC_LOCAL", 4, 1},

    // Global variables
    [OP_LOAD_GLOBAL] = {"LOAD_GLOBAL", 4, 1},
    [OP_STORE_GLOBAL] = {"STORE_GLOBAL", 4, -1},
    [OP_INC_GLOBAL] = {"INC_GLOBAL", 4, 1},
    [OP_DEC_GLOBAL] = {"DEC_GLOBAL", 4, 1},

    // Upvalues
    [OP_LOAD_UPVALUE] = {"LOAD_UPVALUE", 4, 1},
    [OP_STORE_UPVALUE] = {"STORE_UPVALUE", 4, -1},
    [OP_CLOSE_UPVALUE] = {"CLOSE_UPVALUE", 0, 0},

    // Arithmetic
    [OP_ADD] = {"ADD", 0, -1},
    [OP_SUB] = {"SUB", 0, -1},
    [OP_MUL] = {"MUL", 0, -1},
    [OP_DIV] = {"DIV", 0, -1},
    [OP_MOD] = {"MOD", 0, -1},
    [OP_NEG] = {"NEG", 0, 0},

    // Comparison
    [OP_EQ] = {"EQ", 0, -1},
    [OP_NE] = {"NE", 0, -1},
    [OP_LT] = {"LT", 0, -1},
    [OP_GT] = {"GT", 0, -1},
    [OP_LE] = {"LE", 0, -1},
    [OP_GE] = {"GE", 0, -1},

    // Logic
    [OP_AND] = {"AND", 0, -1},
    [OP_OR] = {"OR", 0, -1},
    [OP_NOT] = {"NOT", 0, 0},

    // Control flow
    [OP_JUMP] = {"JUMP", 4, 0},
    [OP_JUMP_IF_FALSE] = {"JUMP_IF_FALSE", 4, -1},
    [OP_JUMP_IF_TRUE] = {"JUMP_IF_TRUE", 4, -1},
    [OP_LOOP] = {"LOOP", 4, 0},

    // Functions
    [OP_CALL] = {"CALL", 4, -128},
    [OP_CALL_GLOBAL] = {"CALL_GLOBAL", 4, -128},
    [OP_RETURN] = {"RETURN", 0, 0},
    [OP_RETURN_VALUE] = {"RETURN_VALUE", 0, 0},

    // Closures
    [OP_CLOSURE] = {"CLOSURE", 4, 1},
    [OP_DEFINE_GLOBAL] = {"DEFINE_GLOBAL", 4, -1},

    // Arrays
    [OP_ARRAY_NEW] = {"ARRAY_NEW", 4, -128},
    [OP_ARRAY_GET] = {"ARRAY_GET", 0, -1},
    [OP_ARRAY_SET] = {"ARRAY_SET", 0, -3},
    [OP_ARRAY_LEN] = {"ARRAY_LEN", 0, 0},
    [OP_ARRAY_PUSH] = {"ARRAY_PUSH", 0, -2},
    [OP_ARRAY_POP] = {"ARRAY_POP", 0, 0},

    // Text interop / Output
    [OP_TEXT_SPLICE] = {"TEXT_SPLICE", 4, 0},
    [OP_OUTPUT] = {"OUTPUT", 0, -1},

    // Debug/special
    [OP_NOP] = {"NOP", 0, 0},
    [OP_HALT] = {"HALT", 0, 0},
    [OP_BREAKPOINT] = {"BREAKPOINT", 0, 0},
};

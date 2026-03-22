// opcode.h - Subnivean 2.0 Bytecode Instructions
//
// Generic instruction set designed for multiple source languages.
// Key abstractions: values, scopes, cells, closures.

#ifndef SUBNIVEAN_OPCODE_H
#define SUBNIVEAN_OPCODE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    // =========================================================================
    // Stack Operations
    // =========================================================================
    OP_NOP,  // No operation
    OP_POP,  // Discard TOS
    OP_DUP,  // Duplicate TOS
    OP_SWAP, // Swap top two elements
    OP_ROT,  // Rotate top three: [a b c] -> [b c a]

    // =========================================================================
    // Constants
    // =========================================================================
    OP_PUSH_NIL,   // Push nil
    OP_PUSH_TRUE,  // Push integer 1
    OP_PUSH_FALSE, // Push integer 0
    OP_PUSH_INT,   // Push immediate i32 (operand)
    OP_PUSH_CONST, // Push constant pool[operand]

    // =========================================================================
    // Scope Operations
    // =========================================================================
    OP_SCOPE_NEW,     // Create child scope of current, enter it
    OP_SCOPE_POP,     // Exit to parent scope (scope freed if refcount=0)
    OP_SCOPE_CAPTURE, // Push current scope as value
    OP_SCOPE_RESTORE, // Pop scope value, make it current

    // =========================================================================
    // Binding Operations (Static names via symbols)
    // =========================================================================
    OP_BIND,        // [value, sym] -> []; bind in current scope
    OP_LOOKUP,      // [sym] -> [value]; search scope chain
    OP_LOOKUP_HERE, // [sym] -> [value]; current scope only
    OP_SET,         // [value, sym] -> []; update in chain

    // =========================================================================
    // Binding Operations (Dynamic names via strings)
    // =========================================================================
    OP_BIND_DYN,   // [value, str] -> []; intern str, bind
    OP_LOOKUP_DYN, // [str] -> [value]; intern str, lookup
    OP_SET_DYN,    // [value, str] -> []; intern str, set

    // =========================================================================
    // Cell Operations (Mutable boxes for capture-by-reference)
    // =========================================================================
    OP_CELL_NEW, // [value] -> [cell]; box the value
    OP_CELL_GET, // [cell] -> [value]; unbox (copy)
    OP_CELL_SET, // [value, cell] -> []; mutate cell contents
    OP_CELL_INC, // [cell] -> [new_val]; increment integer in cell
    OP_CELL_DEC, // [cell] -> [new_val]; decrement integer in cell

    // =========================================================================
    // Arithmetic
    // =========================================================================
    OP_ADD,  // [b, a] -> [a+b]
    OP_SUB,  // [b, a] -> [a-b]
    OP_MUL,  // [b, a] -> [a*b]
    OP_DIV,  // [b, a] -> [a/b]
    OP_MOD,  // [b, a] -> [a%b]
    OP_NEG,  // [a] -> [-a]
    OP_RAND, // [max] -> [rand() % max]

    // =========================================================================
    // Comparison (push 1 or 0)
    // =========================================================================
    OP_EQ, // [b, a] -> [a==b]
    OP_NE, // [b, a] -> [a!=b]
    OP_LT, // [b, a] -> [a<b]
    OP_GT, // [b, a] -> [a>b]
    OP_LE, // [b, a] -> [a<=b]
    OP_GE, // [b, a] -> [a>=b]

    // =========================================================================
    // Logic
    // =========================================================================
    OP_AND, // [b, a] -> [a&&b] (non-short-circuit)
    OP_OR,  // [b, a] -> [a||b]
    OP_NOT, // [a] -> [!a]

    // =========================================================================
    // Control Flow
    // =========================================================================
    OP_JUMP,        // Jump by operand (signed offset)
    OP_JUMP_IF,     // [cond] -> []; jump if truthy
    OP_JUMP_UNLESS, // [cond] -> []; jump if falsy
    OP_LOOP,        // Backward jump by operand

    // =========================================================================
    // Functions/Closures
    // =========================================================================
    OP_CLOSURE,    // [scope] -> [closure]; operand = func index
    OP_CALL,       // [args..., closure] -> [result]; operand = argc
    OP_TAIL_CALL,  // Like CALL but reuses frame (optimization)
    OP_INVOKE_DYN, // [args..., val] -> [result]; if closure: call, if array+argc==1: get
    OP_RETURN,     // Return nil
    OP_RETURN_VAL, // [value] -> ; return TOS

    // =========================================================================
    // Arrays
    // =========================================================================
    OP_ARRAY_NEW,     // [elems...] -> [array]; operand = count
    OP_ARRAY_GET,     // [idx, arr] -> [elem]
    OP_ARRAY_SET,     // [val, idx, arr] -> []
    OP_ARRAY_SET_DYN, // [val, idx, name_str] -> []; set external array element
    OP_ARRAY_LEN,     // [arr] -> [len]
    OP_ARRAY_PUSH,    // [val, arr] -> []
    OP_ARRAY_POP,     // [arr] -> [val]
    OP_COPYARRAY,     // [src_name_str, dest_sym] -> []; copy external array to local

    // =========================================================================
    // Heap Memory (persistent store - survives across calls)
    // =========================================================================
    OP_MEM_LOAD,  // [idx, addr] -> [value]; load heap[addr][idx]
    OP_MEM_STORE, // [val, idx, addr] -> []; store heap[addr][idx] = val
    OP_MEM_LEN,   // [addr] -> [len]; get length of heap block
    OP_MEM_ALLOC, // [count] -> [addr]; allocate new block with count elements

    // =========================================================================
    // Maps (Robin Hood hash tables in persistent store)
    // =========================================================================
    OP_MAP_NEW,  // [] -> [addr]; create empty map
    OP_MAP_GET,  // [key, addr] -> [value]; get map[key] (0 if missing)
    OP_MAP_SET,  // [val, key, addr] -> []; set map[key] = val
    OP_MAP_HAS,  // [key, addr] -> [0/1]; check if key exists
    OP_MAP_DEL,  // [key, addr] -> [0/1]; delete key, return if existed
    OP_MAP_LEN,  // [addr] -> [count]; number of entries
    OP_MAP_KEYS, // [addr] -> [arr_addr]; array of all keys

    // =========================================================================
    // String Arrays (persistent store)
    // =========================================================================
    OP_STRING_ARRAY_NEW, // [strs...] -> [addr]; operand = count, create string array
    OP_STRING_ARRAY_GET, // [idx, addr] -> [str]; get string at index

    // =========================================================================
    // Strings
    // =========================================================================
    OP_CONCAT,    // [b, a] -> [a+b]
    OP_STRINGIFY, // [val] -> [str]
    OP_SYMBOL,    // [str] -> [sym]; intern the string

    // =========================================================================
    // Output (for text-generating languages like Hyades)
    // =========================================================================
    OP_OUTPUT,      // [val] -> []; stringify and append to output
    OP_OUTPUT_RAW,  // Emit literal text; operand = const index
    OP_EMIT_CURSOR, // [row, col] -> []; emit \x1b[row;colH

    // =========================================================================
    // Unified Operations
    // =========================================================================
    OP_LEN, // [val] -> [len]; unified length (array, map, or heap)

    // =========================================================================
    // Special
    // =========================================================================
    OP_HALT,       // Stop execution
    OP_BREAKPOINT, // Debugger hook
    OP_ASSERT,     // [cond, msg] -> []; error if falsy

    OP_COUNT // Number of opcodes
} OpCode;

// =========================================================================
// Instruction Format
// =========================================================================

// Instructions are 8 bytes: 4-byte opcode + 4-byte operand
// This is simple and avoids alignment issues.

typedef struct {
    uint32_t op;     // OpCode
    int32_t operand; // Signed operand (immediate, offset, or index)
} Instruction;

// =========================================================================
// Opcode Metadata
// =========================================================================

typedef struct {
    const char *name;
    int stack_effect; // Net change to stack (-128 = variable)
    bool has_operand;
} OpCodeInfo;

extern const OpCodeInfo opcode_info[OP_COUNT];

const char *opcode_name(OpCode op);

#endif // SUBNIVEAN_OPCODE_H

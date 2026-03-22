// function.c - Subnivean 2.0 Function and Closure Implementation

#include "function.h"
#include "scope.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Function ID Counter
// ============================================================================

static uint32_t next_function_id = 0;

// ============================================================================
// Function Lifecycle
// ============================================================================

Function *function_new(const char *name, int arity) {
    Function *f = calloc(1, sizeof(Function));

    f->name = name ? strdup(name) : NULL;
    f->id = next_function_id++;
    f->arity = arity;

    f->params = arity > 0 ? calloc(arity, sizeof(Symbol *)) : NULL;

    f->code_cap = 64;
    f->code = calloc(f->code_cap, sizeof(Instruction));
    f->code_len = 0;

    f->constants_cap = 16;
    f->constants = calloc(f->constants_cap, sizeof(Constant));
    f->n_constants = 0;

    f->lines = NULL;
    f->source = NULL;

    f->refcount = 1;

    return f;
}

void function_incref(Function *f) {
    if (f) f->refcount++;
}

void function_decref(Function *f) {
    if (!f) return;
    f->refcount--;

    if (f->refcount <= 0) {
        free(f->name);
        free(f->params);
        free(f->code);
        free(f->lines);

        // Free constants
        for (int i = 0; i < f->n_constants; i++) {
            switch (f->constants[i].kind) {
            case CONST_STRING: string_decref(f->constants[i].as_string); break;
            case CONST_FUNCTION: function_decref(f->constants[i].as_func); break;
            default: break;
            }
        }
        free(f->constants);

        free(f);
    }
}

// ============================================================================
// Constant Pool
// ============================================================================

static void ensure_constants_cap(Function *f) {
    if (f->n_constants >= f->constants_cap) {
        f->constants_cap *= 2;
        f->constants = realloc(f->constants, f->constants_cap * sizeof(Constant));
    }
}

int function_add_int(Function *f, int64_t value) {
    // Check for existing
    for (int i = 0; i < f->n_constants; i++) {
        if (f->constants[i].kind == CONST_INT && f->constants[i].as_int == value) {
            return i;
        }
    }

    ensure_constants_cap(f);
    int idx = f->n_constants++;
    f->constants[idx].kind = CONST_INT;
    f->constants[idx].as_int = value;
    return idx;
}

int function_add_string(Function *f, String *s) {
    // Check for existing
    for (int i = 0; i < f->n_constants; i++) {
        if (f->constants[i].kind == CONST_STRING && string_eq(f->constants[i].as_string, s)) {
            return i;
        }
    }

    ensure_constants_cap(f);
    string_incref(s);
    int idx = f->n_constants++;
    f->constants[idx].kind = CONST_STRING;
    f->constants[idx].as_string = s;
    return idx;
}

int function_add_symbol(Function *f, Symbol *sym) {
    // Check for existing
    for (int i = 0; i < f->n_constants; i++) {
        if (f->constants[i].kind == CONST_SYMBOL && f->constants[i].as_symbol == sym) {
            return i;
        }
    }

    ensure_constants_cap(f);
    int idx = f->n_constants++;
    f->constants[idx].kind = CONST_SYMBOL;
    f->constants[idx].as_symbol = sym;
    return idx;
}

int function_add_function(Function *f, Function *inner) {
    ensure_constants_cap(f);
    function_incref(inner);
    int idx = f->n_constants++;
    f->constants[idx].kind = CONST_FUNCTION;
    f->constants[idx].as_func = inner;
    return idx;
}

// ============================================================================
// Code Emission
// ============================================================================

static void ensure_code_cap(Function *f) {
    if (f->code_len >= f->code_cap) {
        f->code_cap *= 2;
        f->code = realloc(f->code, f->code_cap * sizeof(Instruction));
    }
}

void function_emit(Function *f, OpCode op, int32_t operand) {
    ensure_code_cap(f);
    f->code[f->code_len].op = op;
    f->code[f->code_len].operand = operand;
    f->code_len++;
}

void function_emit_simple(Function *f, OpCode op) {
    function_emit(f, op, 0);
}

int function_emit_jump(Function *f, OpCode op) {
    function_emit(f, op, 0); // Placeholder
    return f->code_len - 1;
}

void function_patch_jump(Function *f, int index) {
    int offset = f->code_len - index - 1;
    f->code[index].operand = offset;
}

void function_emit_loop(Function *f, int loop_start) {
    int offset = f->code_len - loop_start + 1;
    function_emit(f, OP_LOOP, offset);
}

int function_offset(Function *f) {
    return f->code_len;
}

// ============================================================================
// Disassembly
// ============================================================================

// Opcode metadata table
const OpCodeInfo opcode_info[OP_COUNT] = {
    [OP_NOP] = {"NOP", 0, false},
    [OP_POP] = {"POP", -1, false},
    [OP_DUP] = {"DUP", 1, false},
    [OP_SWAP] = {"SWAP", 0, false},
    [OP_ROT] = {"ROT", 0, false},

    [OP_PUSH_NIL] = {"PUSH_NIL", 1, false},
    [OP_PUSH_TRUE] = {"PUSH_TRUE", 1, false},
    [OP_PUSH_FALSE] = {"PUSH_FALSE", 1, false},
    [OP_PUSH_INT] = {"PUSH_INT", 1, true},
    [OP_PUSH_CONST] = {"PUSH_CONST", 1, true},

    [OP_SCOPE_NEW] = {"SCOPE_NEW", 0, false},
    [OP_SCOPE_POP] = {"SCOPE_POP", 0, false},
    [OP_SCOPE_CAPTURE] = {"SCOPE_CAPTURE", 1, false},
    [OP_SCOPE_RESTORE] = {"SCOPE_RESTORE", -1, false},

    [OP_BIND] = {"BIND", -2, false},
    [OP_LOOKUP] = {"LOOKUP", 0, false},
    [OP_LOOKUP_HERE] = {"LOOKUP_HERE", 0, false},
    [OP_SET] = {"SET", -2, false},

    [OP_BIND_DYN] = {"BIND_DYN", -2, false},
    [OP_LOOKUP_DYN] = {"LOOKUP_DYN", 0, false},
    [OP_SET_DYN] = {"SET_DYN", -2, false},

    [OP_CELL_NEW] = {"CELL_NEW", 0, false},
    [OP_CELL_GET] = {"CELL_GET", 0, false},
    [OP_CELL_SET] = {"CELL_SET", -2, false},
    [OP_CELL_INC] = {"CELL_INC", 0, false},
    [OP_CELL_DEC] = {"CELL_DEC", 0, false},

    [OP_ADD] = {"ADD", -1, false},
    [OP_SUB] = {"SUB", -1, false},
    [OP_MUL] = {"MUL", -1, false},
    [OP_DIV] = {"DIV", -1, false},
    [OP_MOD] = {"MOD", -1, false},
    [OP_NEG] = {"NEG", 0, false},
    [OP_RAND] = {"RAND", 0, false},

    [OP_EQ] = {"EQ", -1, false},
    [OP_NE] = {"NE", -1, false},
    [OP_LT] = {"LT", -1, false},
    [OP_GT] = {"GT", -1, false},
    [OP_LE] = {"LE", -1, false},
    [OP_GE] = {"GE", -1, false},

    [OP_AND] = {"AND", -1, false},
    [OP_OR] = {"OR", -1, false},
    [OP_NOT] = {"NOT", 0, false},

    [OP_JUMP] = {"JUMP", 0, true},
    [OP_JUMP_IF] = {"JUMP_IF", -1, true},
    [OP_JUMP_UNLESS] = {"JUMP_UNLESS", -1, true},
    [OP_LOOP] = {"LOOP", 0, true},

    [OP_CLOSURE] = {"CLOSURE", 0, true},
    [OP_CALL] = {"CALL", -128, true}, // Variable
    [OP_TAIL_CALL] = {"TAIL_CALL", -128, true},
    [OP_INVOKE_DYN] = {"INVOKE_DYN", -128, true}, // Variable: call or array get
    [OP_RETURN] = {"RETURN", 0, false},
    [OP_RETURN_VAL] = {"RETURN_VAL", -1, false},

    [OP_ARRAY_NEW] = {"ARRAY_NEW", -128, true},
    [OP_ARRAY_GET] = {"ARRAY_GET", -1, false},
    [OP_ARRAY_SET] = {"ARRAY_SET", -3, false},
    [OP_ARRAY_SET_DYN] = {"ARRAY_SET_DYN", -3, false}, // [val, idx, name_str] -> []
    [OP_ARRAY_LEN] = {"ARRAY_LEN", 0, false},
    [OP_ARRAY_PUSH] = {"ARRAY_PUSH", -2, false},
    [OP_ARRAY_POP] = {"ARRAY_POP", 0, false},
    [OP_COPYARRAY] = {"COPYARRAY", -2, false}, // [src_name_str, dest_sym] -> []

    [OP_MEM_LOAD] = {"MEM_LOAD", -1, false},   // [idx, addr] -> [value]
    [OP_MEM_STORE] = {"MEM_STORE", -3, false}, // [val, idx, addr] -> []
    [OP_MEM_LEN] = {"MEM_LEN", 0, false},      // [addr] -> [len]
    [OP_MEM_ALLOC] = {"MEM_ALLOC", 0, false},  // [count] -> [addr]

    [OP_MAP_NEW] = {"MAP_NEW", 1, false},   // [] -> [addr]
    [OP_MAP_GET] = {"MAP_GET", -1, false},  // [key, addr] -> [value]
    [OP_MAP_SET] = {"MAP_SET", -3, false},  // [val, key, addr] -> []
    [OP_MAP_HAS] = {"MAP_HAS", -1, false},  // [key, addr] -> [0/1]
    [OP_MAP_DEL] = {"MAP_DEL", -1, false},  // [key, addr] -> [0/1]
    [OP_MAP_LEN] = {"MAP_LEN", 0, false},   // [addr] -> [count]
    [OP_MAP_KEYS] = {"MAP_KEYS", 0, false}, // [addr] -> [arr_addr]

    [OP_STRING_ARRAY_NEW] = {"STRING_ARRAY_NEW", -128,
                             true},                          // [strs...] -> [addr]; operand = count
    [OP_STRING_ARRAY_GET] = {"STRING_ARRAY_GET", -1, false}, // [idx, addr] -> [str]

    [OP_CONCAT] = {"CONCAT", -1, false},
    [OP_STRINGIFY] = {"STRINGIFY", 0, false},
    [OP_SYMBOL] = {"SYMBOL", 0, false},

    [OP_OUTPUT] = {"OUTPUT", -1, false},
    [OP_OUTPUT_RAW] = {"OUTPUT_RAW", 0, true},
    [OP_EMIT_CURSOR] = {"EMIT_CURSOR", -2, false},

    [OP_LEN] = {"LEN", 0, false}, // [val] -> [len]

    [OP_HALT] = {"HALT", 0, false},
    [OP_BREAKPOINT] = {"BREAKPOINT", 0, false},
    [OP_ASSERT] = {"ASSERT", -2, false},
};

const char *opcode_name(OpCode op) {
    if (op >= 0 && op < OP_COUNT) {
        return opcode_info[op].name;
    }
    return "UNKNOWN";
}

void function_disassemble(Function *f) {
    fprintf(stderr, "== %s (arity=%d) ==\n", f->name ? f->name : "<main>", f->arity);

    for (int i = 0; i < f->code_len; i++) {
        Instruction *inst = &f->code[i];
        const OpCodeInfo *info = &opcode_info[inst->op];

        fprintf(stderr, "%04d  %-14s", i, info->name);

        if (info->has_operand) {
            fprintf(stderr, " %d", inst->operand);

            // Show constant value for PUSH_CONST, OUTPUT_RAW
            if ((inst->op == OP_PUSH_CONST || inst->op == OP_OUTPUT_RAW) && inst->operand >= 0 &&
                inst->operand < f->n_constants) {
                Constant *c = &f->constants[inst->operand];
                switch (c->kind) {
                case CONST_INT: fprintf(stderr, " (int %lld)", (long long)c->as_int); break;
                case CONST_STRING:
                    fprintf(stderr, " (str \"%.*s\")", (int)c->as_string->len, c->as_string->data);
                    break;
                case CONST_SYMBOL: fprintf(stderr, " (#%s)", c->as_symbol->name); break;
                case CONST_FUNCTION:
                    fprintf(stderr, " (fn %s)", c->as_func->name ? c->as_func->name : "anon");
                    break;
                }
            }

            // Show jump target
            if (inst->op == OP_JUMP || inst->op == OP_JUMP_IF || inst->op == OP_JUMP_UNLESS) {
                fprintf(stderr, " -> %d", i + 1 + inst->operand);
            }
            if (inst->op == OP_LOOP) {
                fprintf(stderr, " -> %d", i + 1 - inst->operand);
            }
        }

        fprintf(stderr, "\n");
    }

    // Disassemble nested functions
    for (int i = 0; i < f->n_constants; i++) {
        if (f->constants[i].kind == CONST_FUNCTION) {
            fprintf(stderr, "\n");
            function_disassemble(f->constants[i].as_func);
        }
    }
}

// Helper for string-based disassembly
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} DisasmBuf;

static void disasm_buf_init(DisasmBuf *b) {
    b->cap = 512;
    b->data = malloc(b->cap);
    b->data[0] = '\0';
    b->len = 0;
}

static void disasm_buf_append(DisasmBuf *b, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    // Try to format into remaining space
    size_t remaining = b->cap - b->len;
    int written = vsnprintf(b->data + b->len, remaining, fmt, args);
    va_end(args);

    if ((size_t)written >= remaining) {
        // Need more space
        while (b->cap - b->len <= (size_t)written) {
            b->cap *= 2;
        }
        b->data = realloc(b->data, b->cap);

        // Retry
        va_start(args, fmt);
        written = vsnprintf(b->data + b->len, b->cap - b->len, fmt, args);
        va_end(args);
    }

    b->len += written;
}

static void function_disassemble_to_buf(Function *f, DisasmBuf *b, int indent) {
    // Indent prefix
    char ind[32] = "";
    for (int i = 0; i < indent && i < 15; i++) {
        strcat(ind, "  ");
    }

    disasm_buf_append(b, "%s== %s (arity=%d) ==\n", ind, f->name ? f->name : "<main>", f->arity);

    for (int i = 0; i < f->code_len; i++) {
        Instruction *inst = &f->code[i];
        const OpCodeInfo *info = &opcode_info[inst->op];

        disasm_buf_append(b, "%s%04d  %-14s", ind, i, info->name);

        if (info->has_operand) {
            disasm_buf_append(b, " %d", inst->operand);

            // Show constant value for PUSH_CONST, OUTPUT_RAW
            if ((inst->op == OP_PUSH_CONST || inst->op == OP_OUTPUT_RAW) && inst->operand >= 0 &&
                inst->operand < f->n_constants) {
                Constant *c = &f->constants[inst->operand];
                switch (c->kind) {
                case CONST_INT: disasm_buf_append(b, " (int %lld)", (long long)c->as_int); break;
                case CONST_STRING:
                    disasm_buf_append(b, " (str \"%.20s%s\")", c->as_string->data,
                                      c->as_string->len > 20 ? "..." : "");
                    break;
                case CONST_SYMBOL: disasm_buf_append(b, " (#%s)", c->as_symbol->name); break;
                case CONST_FUNCTION:
                    disasm_buf_append(b, " (fn %s)", c->as_func->name ? c->as_func->name : "anon");
                    break;
                }
            }

            // Show jump target
            if (inst->op == OP_JUMP || inst->op == OP_JUMP_IF || inst->op == OP_JUMP_UNLESS) {
                disasm_buf_append(b, " -> %d", i + 1 + inst->operand);
            }
            if (inst->op == OP_LOOP) {
                disasm_buf_append(b, " -> %d", i + 1 - inst->operand);
            }
        }

        disasm_buf_append(b, "\n");
    }

    // Disassemble nested functions
    for (int i = 0; i < f->n_constants; i++) {
        if (f->constants[i].kind == CONST_FUNCTION) {
            disasm_buf_append(b, "\n");
            function_disassemble_to_buf(f->constants[i].as_func, b, indent + 1);
        }
    }
}

char *function_disassemble_to_string(Function *f) {
    if (!f) return strdup("(null function)");

    DisasmBuf b;
    disasm_buf_init(&b);
    function_disassemble_to_buf(f, &b, 0);
    return b.data;
}

// ============================================================================
// Closure
// ============================================================================

Closure *closure_new(Function *func, Scope *captured) {
    Closure *cl = malloc(sizeof(Closure));
    cl->func = func;
    function_incref(func);
    cl->captured = captured;
    sn_scope_incref(captured);
    cl->refcount = 1;
    return cl;
}

void closure_incref(Closure *cl) {
    if (cl) cl->refcount++;
}

void closure_decref(Closure *cl) {
    if (!cl) return;
    cl->refcount--;

    if (cl->refcount <= 0) {
        function_decref(cl->func);
        sn_scope_decref(cl->captured);
        free(cl);
    }
}

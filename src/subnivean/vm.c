// vm.c - Subnivean Virtual Machine implementation
//
// Stack-based bytecode interpreter.

#include "vm.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Value Utilities
// ============================================================================

Value value_int(int32_t n) {
    Value v;
    v.type = VAL_INT;
    v.integer = n;
    return v;
}

Value value_string(const char *s) {
    Value v;
    v.type = VAL_STRING;
    v.string = strdup(s);
    return v;
}

Value value_array(Array *arr) {
    Value v;
    v.type = VAL_ARRAY;
    v.array = arr;
    array_incref(arr);
    return v;
}

Value value_closure(Closure *c) {
    Value v;
    v.type = VAL_CLOSURE;
    v.closure = c;
    closure_incref(c);
    return v;
}

Value value_null(void) {
    Value v;
    v.type = VAL_NULL;
    v.integer = 0;
    return v;
}

Value value_copy(Value v) {
    switch (v.type) {
    case VAL_STRING: return value_string(v.string);
    case VAL_ARRAY: array_incref(v.array); return v;
    case VAL_CLOSURE: closure_incref(v.closure); return v;
    default: return v;
    }
}

void value_free(Value v) {
    switch (v.type) {
    case VAL_STRING: free(v.string); break;
    case VAL_ARRAY: array_decref(v.array); break;
    case VAL_CLOSURE: closure_decref(v.closure); break;
    default: break;
    }
}

void value_print(Value v) {
    switch (v.type) {
    case VAL_INT: fprintf(stderr, "%d", v.integer); break;
    case VAL_STRING: fprintf(stderr, "\"%s\"", v.string); break;
    case VAL_ARRAY: fprintf(stderr, "[array %d]", v.array->length); break;
    case VAL_CLOSURE:
        fprintf(stderr, "<fn %s>", v.closure->func->name ? v.closure->func->name : "anon");
        break;
    case VAL_NULL: fprintf(stderr, "null"); break;
    }
}

int32_t value_to_int(Value v) {
    switch (v.type) {
    case VAL_INT: return v.integer;
    case VAL_STRING: return v.string ? atoi(v.string) : 0;
    case VAL_ARRAY: return v.array ? v.array->length : 0;
    case VAL_CLOSURE: return 1; // Truthy
    case VAL_NULL: return 0;
    }
    return 0;
}

char *value_to_string(Value v) {
    char buf[64];
    switch (v.type) {
    case VAL_INT: snprintf(buf, sizeof(buf), "%d", v.integer); return strdup(buf);
    case VAL_STRING: return strdup(v.string ? v.string : "");
    case VAL_ARRAY:
        snprintf(buf, sizeof(buf), "[array %d]", v.array ? v.array->length : 0);
        return strdup(buf);
    case VAL_CLOSURE:
        snprintf(buf, sizeof(buf), "<fn %s>",
                 v.closure && v.closure->func->name ? v.closure->func->name : "anon");
        return strdup(buf);
    case VAL_NULL: return strdup("");
    }
    return strdup("");
}

// ============================================================================
// Array Implementation
// ============================================================================

Array *array_new(int initial_capacity) {
    Array *arr = calloc(1, sizeof(Array));
    arr->capacity = initial_capacity > 0 ? initial_capacity : 8;
    arr->elements = calloc(arr->capacity, sizeof(Value));
    arr->length = 0;
    arr->refcount = 1;
    return arr;
}

void array_free(Array *arr) {
    if (!arr) return;
    for (int i = 0; i < arr->length; i++) {
        value_free(arr->elements[i]);
    }
    free(arr->elements);
    free(arr);
}

void array_incref(Array *arr) {
    if (arr) arr->refcount++;
}

void array_decref(Array *arr) {
    if (!arr) return;
    arr->refcount--;
    if (arr->refcount <= 0) {
        array_free(arr);
    }
}

void array_push(Array *arr, Value val) {
    if (arr->length >= arr->capacity) {
        arr->capacity *= 2;
        arr->elements = realloc(arr->elements, arr->capacity * sizeof(Value));
    }
    arr->elements[arr->length++] = val;
}

Value array_pop(Array *arr) {
    if (arr->length == 0) return value_null();
    return arr->elements[--arr->length];
}

Value array_get(Array *arr, int index) {
    if (index < 0 || index >= arr->length) return value_null();
    return value_copy(arr->elements[index]);
}

void array_set(Array *arr, int index, Value val) {
    // Grow if needed
    while (index >= arr->capacity) {
        arr->capacity *= 2;
        arr->elements = realloc(arr->elements, arr->capacity * sizeof(Value));
    }
    // Fill gaps with null
    while (arr->length <= index) {
        arr->elements[arr->length++] = value_null();
    }
    value_free(arr->elements[index]);
    arr->elements[index] = val;
}

// ============================================================================
// Closure Implementation
// ============================================================================

Closure *closure_new(CompiledFunc *func) {
    Closure *c = calloc(1, sizeof(Closure));
    c->func = func;
    c->refcount = 1;
    if (func->n_upvalues > 0) {
        c->upvalues = calloc(func->n_upvalues, sizeof(Upvalue *));
        c->n_upvalues = func->n_upvalues;
    }
    return c;
}

void closure_free(Closure *c) {
    if (!c) return;
    for (int i = 0; i < c->n_upvalues; i++) {
        if (c->upvalues[i]) {
            c->upvalues[i]->refcount--;
            if (c->upvalues[i]->refcount <= 0) {
                value_free(c->upvalues[i]->closed);
                free(c->upvalues[i]);
            }
        }
    }
    free(c->upvalues);
    free(c);
}

void closure_incref(Closure *c) {
    if (c) c->refcount++;
}

void closure_decref(Closure *c) {
    if (!c) return;
    c->refcount--;
    if (c->refcount <= 0) {
        closure_free(c);
    }
}

// ============================================================================
// VM Implementation
// ============================================================================

void vm_init(VM *vm) {
    memset(vm, 0, sizeof(VM));
    vm->stack_top = vm->stack;
    vm->output_cap = 1024;
    vm->output = malloc(vm->output_cap);
    vm->output[0] = '\0';
    vm->output_len = 0;
}

void vm_free(VM *vm) {
    // Free stack values
    while (vm->stack_top > vm->stack) {
        vm->stack_top--;
        value_free(*vm->stack_top);
    }

    // Free globals
    for (int i = 0; i < vm->n_globals; i++) {
        free(vm->globals[i].name);
        value_free(vm->globals[i].value);
    }

    // Free output buffer
    free(vm->output);
}

void vm_reset(VM *vm) {
    // Free stack values
    while (vm->stack_top > vm->stack) {
        vm->stack_top--;
        value_free(*vm->stack_top);
    }
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->output_len = 0;
    vm->output[0] = '\0';
    vm->had_error = false;
    vm->running = false;
}

void vm_set_trace(VM *vm, bool enabled) {
    vm->trace = enabled;
}

// ============================================================================
// Stack Operations
// ============================================================================

static void push(VM *vm, Value v) {
    if (vm->stack_top - vm->stack >= STACK_MAX) {
        vm->had_error = true;
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Stack overflow");
        return;
    }
    *vm->stack_top++ = v;
}

static Value pop(VM *vm) {
    if (vm->stack_top == vm->stack) {
        vm->had_error = true;
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Stack underflow");
        return value_null();
    }
    return *--vm->stack_top;
}

static Value peek(VM *vm, int distance) {
    return vm->stack_top[-1 - distance];
}

// ============================================================================
// Output
// ============================================================================

static void output_append(VM *vm, const char *s, size_t len) {
    if (vm->output_len + len + 1 > vm->output_cap) {
        vm->output_cap = vm->output_cap * 2 + len;
        vm->output = realloc(vm->output, vm->output_cap);
    }
    memcpy(vm->output + vm->output_len, s, len);
    vm->output_len += len;
    vm->output[vm->output_len] = '\0';
}

static void output_value(VM *vm, Value v) {
    char *s = value_to_string(v);
    output_append(vm, s, strlen(s));
    free(s);
}

// ============================================================================
// Global Variables
// ============================================================================

void vm_set_global(VM *vm, const char *name, Value value) {
    // Check if exists
    for (int i = 0; i < vm->n_globals; i++) {
        if (strcmp(vm->globals[i].name, name) == 0) {
            value_free(vm->globals[i].value);
            vm->globals[i].value = value;
            return;
        }
    }

    // Add new
    if (vm->n_globals >= 256) {
        vm->had_error = true;
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Too many globals");
        return;
    }

    vm->globals[vm->n_globals].name = strdup(name);
    vm->globals[vm->n_globals].value = value;
    vm->n_globals++;
}

Value vm_get_global(VM *vm, const char *name, bool *found) {
    for (int i = 0; i < vm->n_globals; i++) {
        if (strcmp(vm->globals[i].name, name) == 0) {
            if (found) *found = true;
            return value_copy(vm->globals[i].value);
        }
    }
    if (found) *found = false;
    return value_null();
}

// ============================================================================
// Execution
// ============================================================================

static void runtime_error(VM *vm, const char *format, ...) {
    vm->had_error = true;
    vm->running = false;

    va_list args;
    va_start(args, format);
    vsnprintf(vm->error_msg, sizeof(vm->error_msg), format, args);
    va_end(args);

    fprintf(stderr, "Runtime error: %s\n", vm->error_msg);
}

static bool call_closure(VM *vm, Closure *closure, int arg_count) {
    if (arg_count != closure->func->n_params) {
        runtime_error(vm, "Expected %d arguments but got %d", closure->func->n_params, arg_count);
        return false;
    }

    if (vm->frame_count >= FRAMES_MAX) {
        runtime_error(vm, "Call stack overflow");
        return false;
    }

    CallFrame *frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->ip = closure->func->code;
    frame->slots = vm->stack_top - arg_count;

    return true;
}

static bool call_value(VM *vm, Value callee, int arg_count) {
    if (callee.type == VAL_CLOSURE) {
        return call_closure(vm, callee.closure, arg_count);
    }
    runtime_error(vm, "Can only call functions");
    return false;
}

#define READ_BYTE() (*frame->ip++)
#define READ_OP() (frame->ip++->op)
#define READ_OPERAND() (frame->ip++->operand)
#define BINARY_OP(op)                                                                              \
    do {                                                                                           \
        Value b = pop(vm);                                                                         \
        Value a = pop(vm);                                                                         \
        push(vm, value_int(value_to_int(a) op value_to_int(b)));                                   \
        value_free(a);                                                                             \
        value_free(b);                                                                             \
    } while (0)

static bool run(VM *vm) {
    CallFrame *frame = &vm->frames[vm->frame_count - 1];

    while (vm->running) {
        // Trace
        if (vm->trace) {
            fprintf(stderr, "          ");
            for (Value *slot = vm->stack; slot < vm->stack_top; slot++) {
                fprintf(stderr, "[ ");
                value_print(*slot);
                fprintf(stderr, " ]");
            }
            fprintf(stderr, "\n");

            char buf[128];
            int offset = (int)(frame->ip - frame->closure->func->code);
            disassemble_instruction(frame->closure->func, offset, buf, sizeof(buf));
            fprintf(stderr, "%s\n", buf);
        }

        Instruction inst = *frame->ip++;

        switch (inst.op) {
        case OP_PUSH_INT: push(vm, value_int(inst.operand)); break;

        case OP_PUSH_TRUE: push(vm, value_int(1)); break;

        case OP_PUSH_FALSE: push(vm, value_int(0)); break;

        case OP_PUSH_CONST: {
            CompiledFunc *func = frame->closure->func;
            if (inst.operand < func->n_constants) {
                if (func->constants[inst.operand].type == CONST_STRING) {
                    push(vm, value_string(func->constants[inst.operand].string));
                } else if (func->constants[inst.operand].type == CONST_INT) {
                    push(vm, value_int(func->constants[inst.operand].integer));
                }
            }
            break;
        }

        case OP_POP: value_free(pop(vm)); break;

        case OP_DUP: push(vm, value_copy(peek(vm, 0))); break;

        case OP_SWAP: {
            Value a = pop(vm);
            Value b = pop(vm);
            push(vm, a);
            push(vm, b);
            break;
        }

        case OP_LOAD_LOCAL: push(vm, value_copy(frame->slots[inst.operand])); break;

        case OP_STORE_LOCAL:
            value_free(frame->slots[inst.operand]);
            frame->slots[inst.operand] = value_copy(peek(vm, 0));
            break;

        case OP_INC_LOCAL: {
            Value *slot = &frame->slots[inst.operand];
            int32_t val = value_to_int(*slot) + 1;
            value_free(*slot);
            *slot = value_int(val);
            push(vm, value_int(val));
            break;
        }

        case OP_DEC_LOCAL: {
            Value *slot = &frame->slots[inst.operand];
            int32_t val = value_to_int(*slot) - 1;
            value_free(*slot);
            *slot = value_int(val);
            push(vm, value_int(val));
            break;
        }

        case OP_LOAD_GLOBAL: {
            CompiledFunc *func = frame->closure->func;
            const char *name = func->constants[inst.operand].string;
            bool found;
            Value val = vm_get_global(vm, name, &found);
            if (!found) {
                runtime_error(vm, "Undefined variable '%s'", name);
                return false;
            }
            push(vm, val);
            break;
        }

        case OP_STORE_GLOBAL: {
            CompiledFunc *func = frame->closure->func;
            const char *name = func->constants[inst.operand].string;
            vm_set_global(vm, name, value_copy(peek(vm, 0)));
            break;
        }

        case OP_DEFINE_GLOBAL: {
            CompiledFunc *func = frame->closure->func;
            const char *name = func->constants[inst.operand].string;
            vm_set_global(vm, name, pop(vm));
            break;
        }

        case OP_INC_GLOBAL: {
            CompiledFunc *func = frame->closure->func;
            const char *name = func->constants[inst.operand].string;
            bool found;
            Value val = vm_get_global(vm, name, &found);
            int32_t n = value_to_int(val) + 1;
            value_free(val);
            vm_set_global(vm, name, value_int(n));
            push(vm, value_int(n));
            break;
        }

        case OP_DEC_GLOBAL: {
            CompiledFunc *func = frame->closure->func;
            const char *name = func->constants[inst.operand].string;
            bool found;
            Value val = vm_get_global(vm, name, &found);
            int32_t n = value_to_int(val) - 1;
            value_free(val);
            vm_set_global(vm, name, value_int(n));
            push(vm, value_int(n));
            break;
        }

        case OP_LOAD_UPVALUE:
            push(vm, value_copy(*frame->closure->upvalues[inst.operand]->location));
            break;

        case OP_STORE_UPVALUE: {
            Upvalue *upval = frame->closure->upvalues[inst.operand];
            value_free(*upval->location);
            *upval->location = value_copy(peek(vm, 0));
            break;
        }

        case OP_ADD: BINARY_OP(+); break;
        case OP_SUB: BINARY_OP(-); break;
        case OP_MUL: BINARY_OP(*); break;
        case OP_DIV: {
            Value b = pop(vm);
            Value a = pop(vm);
            int32_t bv = value_to_int(b);
            if (bv == 0) {
                runtime_error(vm, "Division by zero");
                value_free(a);
                value_free(b);
                return false;
            }
            push(vm, value_int(value_to_int(a) / bv));
            value_free(a);
            value_free(b);
            break;
        }
        case OP_MOD: {
            Value b = pop(vm);
            Value a = pop(vm);
            int32_t bv = value_to_int(b);
            if (bv == 0) {
                runtime_error(vm, "Modulo by zero");
                value_free(a);
                value_free(b);
                return false;
            }
            push(vm, value_int(value_to_int(a) % bv));
            value_free(a);
            value_free(b);
            break;
        }

        case OP_NEG: {
            Value a = pop(vm);
            push(vm, value_int(-value_to_int(a)));
            value_free(a);
            break;
        }

        case OP_EQ: {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, value_int(value_to_int(a) == value_to_int(b) ? 1 : 0));
            value_free(a);
            value_free(b);
            break;
        }

        case OP_NE: BINARY_OP(!=); break;
        case OP_LT: BINARY_OP(<); break;
        case OP_GT: BINARY_OP(>); break;
        case OP_LE: BINARY_OP(<=); break;
        case OP_GE: BINARY_OP(>=); break;

        case OP_AND: {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, value_int((value_to_int(a) && value_to_int(b)) ? 1 : 0));
            value_free(a);
            value_free(b);
            break;
        }

        case OP_OR: {
            Value b = pop(vm);
            Value a = pop(vm);
            push(vm, value_int((value_to_int(a) || value_to_int(b)) ? 1 : 0));
            value_free(a);
            value_free(b);
            break;
        }

        case OP_NOT: {
            Value a = pop(vm);
            push(vm, value_int(value_to_int(a) ? 0 : 1));
            value_free(a);
            break;
        }

        case OP_JUMP: frame->ip += inst.operand; break;

        case OP_JUMP_IF_FALSE: {
            Value cond = pop(vm);
            if (value_to_int(cond) == 0) {
                frame->ip += inst.operand;
            }
            value_free(cond);
            break;
        }

        case OP_JUMP_IF_TRUE: {
            Value cond = pop(vm);
            if (value_to_int(cond) != 0) {
                frame->ip += inst.operand;
            }
            value_free(cond);
            break;
        }

        case OP_LOOP: frame->ip -= inst.operand; break;

        case OP_CALL: {
            int arg_count = inst.operand;
            Value callee = peek(vm, arg_count);
            if (!call_value(vm, callee, arg_count)) {
                return false;
            }
            frame = &vm->frames[vm->frame_count - 1];
            break;
        }

        case OP_CALL_GLOBAL: {
            CompiledFunc *func = frame->closure->func;
            const char *name = func->constants[inst.operand].string;

            // Read arg count from next instruction
            int arg_count = (frame->ip++)->operand;

            bool found;
            Value callee = vm_get_global(vm, name, &found);
            if (!found) {
                runtime_error(vm, "Undefined function '%s'", name);
                return false;
            }
            if (!call_value(vm, callee, arg_count)) {
                value_free(callee);
                return false;
            }
            value_free(callee);
            frame = &vm->frames[vm->frame_count - 1];
            break;
        }

        case OP_RETURN: {
            // Close any upvalues
            vm->frame_count--;
            if (vm->frame_count == 0) {
                vm->running = false;
                return true;
            }
            // Discard locals
            vm->stack_top = frame->slots;
            frame = &vm->frames[vm->frame_count - 1];
            push(vm, value_null()); // Return null
            break;
        }

        case OP_RETURN_VALUE: {
            Value result = pop(vm);
            vm->frame_count--;
            if (vm->frame_count == 0) {
                // Top-level return - output the value
                output_value(vm, result);
                value_free(result);
                vm->running = false;
                return true;
            }
            // Discard locals
            vm->stack_top = frame->slots;
            frame = &vm->frames[vm->frame_count - 1];
            push(vm, result);
            break;
        }

        case OP_CLOSURE: {
            CompiledFunc *func_template = frame->closure->func->constants[inst.operand].func;
            Closure *closure = closure_new(func_template);

            // Capture upvalues
            for (int i = 0; i < func_template->n_upvalues; i++) {
                Instruction upval_inst = *frame->ip++;
                bool is_local = upval_inst.op != 0;
                int index = upval_inst.operand;

                if (is_local) {
                    // Capture local from current frame
                    Upvalue *upval = calloc(1, sizeof(Upvalue));
                    upval->location = &frame->slots[index];
                    upval->refcount = 1;
                    closure->upvalues[i] = upval;
                } else {
                    // Capture upvalue from enclosing closure
                    closure->upvalues[i] = frame->closure->upvalues[index];
                    closure->upvalues[i]->refcount++;
                }
            }

            push(vm, value_closure(closure));
            closure_decref(closure); // Balance the push's incref
            break;
        }

        case OP_ARRAY_NEW: {
            int n = inst.operand;
            Array *arr = array_new(n);
            // Pop elements in reverse order
            for (int i = n - 1; i >= 0; i--) {
                arr->elements[i] = pop(vm);
            }
            arr->length = n;
            push(vm, value_array(arr));
            array_decref(arr); // Balance the push's incref
            break;
        }

        case OP_ARRAY_GET: {
            Value idx_val = pop(vm);
            Value arr_val = pop(vm);
            int idx = value_to_int(idx_val);
            if (arr_val.type != VAL_ARRAY) {
                runtime_error(vm, "Cannot index non-array");
                value_free(idx_val);
                value_free(arr_val);
                return false;
            }
            push(vm, array_get(arr_val.array, idx));
            value_free(idx_val);
            value_free(arr_val);
            break;
        }

        case OP_ARRAY_SET: {
            Value val = pop(vm);
            Value idx_val = pop(vm);
            Value arr_val = pop(vm);
            int idx = value_to_int(idx_val);
            if (arr_val.type != VAL_ARRAY) {
                runtime_error(vm, "Cannot index non-array");
                value_free(val);
                value_free(idx_val);
                value_free(arr_val);
                return false;
            }
            array_set(arr_val.array, idx, val);
            value_free(idx_val);
            value_free(arr_val);
            break;
        }

        case OP_ARRAY_LEN: {
            Value arr_val = pop(vm);
            if (arr_val.type != VAL_ARRAY) {
                push(vm, value_int(0));
            } else {
                push(vm, value_int(arr_val.array->length));
            }
            value_free(arr_val);
            break;
        }

        case OP_ARRAY_PUSH: {
            Value val = pop(vm);
            Value arr_val = pop(vm);
            if (arr_val.type == VAL_ARRAY) {
                array_push(arr_val.array, val);
            } else {
                value_free(val);
            }
            value_free(arr_val);
            break;
        }

        case OP_ARRAY_POP: {
            Value arr_val = pop(vm);
            if (arr_val.type == VAL_ARRAY) {
                push(vm, array_pop(arr_val.array));
            } else {
                push(vm, value_null());
            }
            value_free(arr_val);
            break;
        }

        case OP_TEXT_SPLICE: {
            CompiledFunc *func = frame->closure->func;
            const char *text = func->constants[inst.operand].string;
            output_append(vm, text, strlen(text));
            break;
        }

        case OP_OUTPUT: {
            Value v = pop(vm);
            output_value(vm, v);
            value_free(v);
            break;
        }

        case OP_NOP: break;

        case OP_HALT: vm->running = false; return true;

        case OP_BREAKPOINT:
            // Could trigger debugger here
            break;

        default: runtime_error(vm, "Unknown opcode %d", inst.op); return false;
        }

        if (vm->had_error) return false;
    }

    return true;
}

#undef READ_BYTE
#undef READ_OP
#undef READ_OPERAND
#undef BINARY_OP

char *vm_execute(VM *vm, CompiledFunc *func) {
    vm_reset(vm);

    // Create closure for top-level function
    Closure *main_closure = closure_new(func);

    // Set up initial call frame
    push(vm, value_closure(main_closure));
    call_closure(vm, main_closure, 0);
    closure_decref(main_closure);

    vm->running = true;
    bool success = run(vm);

    if (!success) {
        return NULL;
    }

    // Return output
    char *result = strdup(vm->output);
    return result;
}

bool vm_call(VM *vm, Closure *closure, int arg_count) {
    return call_closure(vm, closure, arg_count);
}

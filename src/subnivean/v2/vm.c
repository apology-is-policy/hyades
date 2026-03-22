// vm.c - Subnivean 2.0 Virtual Machine Implementation

#include "vm.h"
#include "persistent_store.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// VM Lifecycle
// ============================================================================

void vm_init(VM *vm) {
    memset(vm, 0, sizeof(VM));

    // Initialize stack
    vm->sp = vm->stack;

    // Initialize symbol table
    symbol_table_init(&vm->symbols);

    // Pre-intern common symbols
    vm->syms.nil = symbol_intern_cstr(&vm->symbols, "nil");
    vm->syms.true_ = symbol_intern_cstr(&vm->symbols, "true");
    vm->syms.false_ = symbol_intern_cstr(&vm->symbols, "false");

    // Create global scope
    vm->global = sn_scope_new_global();
    vm->scope = vm->global;
    sn_scope_incref(vm->scope);

    // Initialize output buffer
    vm->output_cap = 1024;
    vm->output = calloc(1, vm->output_cap);
    vm->output[0] = '\0';
    vm->output_len = 0;
}

void vm_free(VM *vm) {
    // Free stack values
    while (vm->sp > vm->stack) {
        vm->sp--;
        value_free(*vm->sp);
    }

    // Free scopes
    sn_scope_decref(vm->scope);
    sn_scope_decref(vm->global);

    // Free symbol table
    symbol_table_free(&vm->symbols);

    // Free output buffer
    free(vm->output);
}

void vm_reset(VM *vm) {
    // Free stack values
    while (vm->sp > vm->stack) {
        vm->sp--;
        value_free(*vm->sp);
    }
    vm->sp = vm->stack;

    // Reset to global scope
    sn_scope_decref(vm->scope);
    vm->scope = vm->global;
    sn_scope_incref(vm->scope);

    // Reset call stack
    vm->frame_count = 0;

    // Reset output
    vm->output_len = 0;
    vm->output[0] = '\0';

    // Reset error state
    vm->had_error = false;
    vm->running = false;
}

// ============================================================================
// Stack Operations
// ============================================================================

void vm_push(VM *vm, Value v) {
    if (vm->sp - vm->stack >= VM_STACK_MAX) {
        vm_error(vm, "Stack overflow");
        return;
    }
    *vm->sp++ = v;
}

Value vm_pop(VM *vm) {
    if (vm->sp == vm->stack) {
        vm_error(vm, "Stack underflow");
        return value_nil();
    }
    return *--vm->sp;
}

Value vm_peek(VM *vm, int distance) {
    return vm->sp[-1 - distance];
}

// ============================================================================
// Output Buffer
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
    String *s = value_to_string(v, &vm->symbols);
    output_append(vm, s->data, s->len);
    string_decref(s);
}

// ============================================================================
// Error Handling
// ============================================================================

void vm_error(VM *vm, const char *fmt, ...) {
    vm->had_error = true;
    vm->running = false;

    va_list args;
    va_start(args, fmt);
    vsnprintf(vm->error_msg, sizeof(vm->error_msg), fmt, args);
    va_end(args);

    fprintf(stderr, "Runtime error: %s\n", vm->error_msg);
}

const char *vm_get_error(VM *vm) {
    return vm->had_error ? vm->error_msg : NULL;
}

// ============================================================================
// Helpers
// ============================================================================

Symbol *vm_intern(VM *vm, const char *name) {
    return symbol_intern_cstr(&vm->symbols, name);
}

Symbol *vm_intern_len(VM *vm, const char *name, size_t len) {
    return symbol_intern(&vm->symbols, name, len);
}

void vm_define_global(VM *vm, const char *name, Value v) {
    Symbol *sym = vm_intern(vm, name);
    sn_scope_bind(vm->global, sym, v);
}

bool vm_get_global(VM *vm, const char *name, Value *out) {
    Symbol *sym = vm_intern(vm, name);
    return sn_scope_lookup(vm->global, sym, out);
}

void vm_set_trace(VM *vm, bool enabled) {
    vm->trace = enabled;
}

void vm_print_stack(VM *vm) {
    fprintf(stderr, "Stack: [");
    for (Value *v = vm->stack; v < vm->sp; v++) {
        if (v != vm->stack) fprintf(stderr, ", ");
        value_print(*v);
    }
    fprintf(stderr, "]\n");
}

// ============================================================================
// Call a Closure
// ============================================================================

static bool call_closure(VM *vm, Closure *closure, int argc) {
    Function *func = closure->func;

    if (argc != func->arity) {
        vm_error(vm, "Expected %d arguments but got %d", func->arity, argc);
        return false;
    }

    if (vm->frame_count >= VM_FRAMES_MAX) {
        vm_error(vm, "Call stack overflow");
        return false;
    }

    // Create new scope with captured scope as parent
    Scope *call_scope = sn_scope_new(closure->captured);

    // Bind arguments to parameters
    // Arguments are on stack: [arg0, arg1, ..., argN-1]
    // with argN-1 at top
    for (int i = 0; i < argc; i++) {
        Value arg = vm->sp[-argc + i];
        sn_scope_bind(call_scope, func->params[i], arg);
    }

    // Pop arguments from stack
    vm->sp -= argc;

    // Push call frame
    CallFrame *frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    closure_incref(closure);
    frame->ip = func->code;
    frame->bp = vm->sp;
    frame->scope = call_scope;

    // Set current scope
    sn_scope_decref(vm->scope);
    vm->scope = call_scope;
    sn_scope_incref(vm->scope);

    return true;
}

bool vm_call(VM *vm, Closure *closure, int argc) {
    return call_closure(vm, closure, argc);
}

// ============================================================================
// Main Interpreter Loop
// ============================================================================

#define BINARY_INT_OP(op_char)                                                                     \
    do {                                                                                           \
        Value b = vm_pop(vm);                                                                      \
        Value a = vm_pop(vm);                                                                      \
        int64_t av = value_to_int(a);                                                              \
        int64_t bv = value_to_int(b);                                                              \
        vm_push(vm, value_int(av op_char bv));                                                     \
        value_free(a);                                                                             \
        value_free(b);                                                                             \
    } while (0)

static bool run(VM *vm) {
    CallFrame *frame = &vm->frames[vm->frame_count - 1];

    while (vm->running) {
        // Fetch current instruction and advance ip
        Instruction *inst = frame->ip++;
        uint32_t op = inst->op;
        int32_t operand = inst->operand;

        // Trace execution
        if (vm->trace) {
            int offset = (int)(inst - frame->closure->func->code);
            fprintf(stderr, "%04d: %-14s", offset, opcode_name(op));
            if (opcode_info[op].has_operand) {
                fprintf(stderr, " %d", operand);
            }
            fprintf(stderr, "\n");

            if (vm->trace_stack) {
                vm_print_stack(vm);
            }
        }

        switch (op) {
            // =================================================================
            // Stack Operations
            // =================================================================

        case OP_NOP: break;

        case OP_POP: value_free(vm_pop(vm)); break;

        case OP_DUP: vm_push(vm, value_copy(vm_peek(vm, 0))); break;

        case OP_SWAP: {
            Value a = vm_pop(vm);
            Value b = vm_pop(vm);
            vm_push(vm, a);
            vm_push(vm, b);
            break;
        }

        case OP_ROT: {
            // [a b c] -> [b c a]
            Value c = vm_pop(vm);
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);
            vm_push(vm, b);
            vm_push(vm, c);
            vm_push(vm, a);
            break;
        }

            // =================================================================
            // Constants
            // =================================================================

        case OP_PUSH_NIL: vm_push(vm, value_nil()); break;

        case OP_PUSH_TRUE: vm_push(vm, value_int(1)); break;

        case OP_PUSH_FALSE: vm_push(vm, value_int(0)); break;

        case OP_PUSH_INT: vm_push(vm, value_int(operand)); break;

        case OP_PUSH_CONST: {
            int32_t idx = operand;
            Constant *c = &frame->closure->func->constants[idx];
            switch (c->kind) {
            case CONST_INT: vm_push(vm, value_int(c->as_int)); break;
            case CONST_STRING: vm_push(vm, value_string(c->as_string)); break;
            case CONST_SYMBOL: vm_push(vm, value_symbol(c->as_symbol)); break;
            case CONST_FUNCTION:
                // Functions become closures when pushed
                // But CLOSURE opcode handles this with scope capture
                vm_error(vm, "Cannot push function directly; use CLOSURE");
                return false;
            }
            break;
        }

            // =================================================================
            // Scope Operations
            // =================================================================

        case OP_SCOPE_NEW: {
            Scope *new_scope = sn_scope_new(vm->scope);
            sn_scope_decref(vm->scope);
            vm->scope = new_scope;
            break;
        }

        case OP_SCOPE_POP: {
            Scope *parent = vm->scope->parent;
            if (!parent) {
                vm_error(vm, "Cannot pop global scope");
                return false;
            }
            sn_scope_incref(parent);
            sn_scope_decref(vm->scope);
            vm->scope = parent;
            break;
        }

        case OP_SCOPE_CAPTURE: vm_push(vm, value_scope(vm->scope)); break;

        case OP_SCOPE_RESTORE: {
            Value v = vm_pop(vm);
            if (v.kind != VAL_SCOPE) {
                vm_error(vm, "Expected scope, got %s", value_kind_name(v.kind));
                value_free(v);
                return false;
            }
            sn_scope_decref(vm->scope);
            vm->scope = v.as_scope;
            // Don't decref - we're taking ownership
            break;
        }

            // =================================================================
            // Binding Operations (Static names)
            // =================================================================

        case OP_BIND: {
            Value name_val = vm_pop(vm);
            Value val = vm_pop(vm);
            if (name_val.kind != VAL_SYMBOL) {
                vm_error(vm, "BIND requires symbol name");
                value_free(name_val);
                value_free(val);
                return false;
            }
            sn_scope_bind(vm->scope, name_val.as_symbol, val);
            // val ownership transferred to scope
            break;
        }

        case OP_LOOKUP: {
            Value name_val = vm_pop(vm);
            if (name_val.kind != VAL_SYMBOL) {
                vm_error(vm, "LOOKUP requires symbol name");
                value_free(name_val);
                return false;
            }
            Value result;
            if (sn_scope_lookup(vm->scope, name_val.as_symbol, &result)) {
                // Found in local scope
                vm_push(vm, result);
            } else {
                // Not found locally - try external lookups
                bool found = false;

                // Try external lambda compile first (for calling other lambdas)
                if (!found && vm->external_lambda_compile) {
                    Function *ext_func =
                        vm->external_lambda_compile(vm->external_ctx, name_val.as_symbol->name);
                    if (ext_func) {
                        // Create closure and bind to global scope for future lookups
                        Closure *cl = closure_new(ext_func, vm->global);
                        result = value_closure(cl);
                        sn_scope_bind(vm->global, name_val.as_symbol, result);
                        // Push a copy (the original is now owned by global scope)
                        result = value_copy(result);
                        closure_decref(cl);
                        vm_push(vm, result);
                        found = true;
                    }
                }

                // Try external array lookup
                if (!found && vm->external_array_lookup) {
                    char **elements = NULL;
                    int n_elements = 0;
                    int ext_found = vm->external_array_lookup(
                        vm->external_ctx, name_val.as_symbol->name, &elements, &n_elements);
                    if (ext_found >= 0) {
                        // Create Subnivean array from external elements
                        Array *arr = array_new(n_elements);
                        for (int i = 0; i < n_elements; i++) {
                            // Preserve string elements (e.g., katakana in K[])
                            char *endptr;
                            int64_t val = strtoll(elements[i], &endptr, 10);
                            if (*endptr == '\0' && elements[i][0] != '\0') {
                                array_push(arr, value_int(val));
                            } else {
                                String *s = string_new(elements[i], strlen(elements[i]));
                                array_push(arr, value_string(s));
                                string_decref(s);
                            }
                            free(elements[i]);
                        }
                        free(elements);
                        result = value_array(arr);
                        // Cache in global scope for fast subsequent lookups
                        sn_scope_bind(vm->global, name_val.as_symbol, value_copy(result));
                        array_decref(arr);
                        vm_push(vm, result);
                        found = true;
                    }
                }

                if (!found) {
                    vm_error(vm, "Undefined variable '%s'", name_val.as_symbol->name);
                    return false;
                }
            }
            break;
        }

        case OP_LOOKUP_HERE: {
            Value name_val = vm_pop(vm);
            if (name_val.kind != VAL_SYMBOL) {
                vm_error(vm, "LOOKUP_HERE requires symbol name");
                value_free(name_val);
                return false;
            }
            Value result;
            if (!sn_scope_lookup_here(vm->scope, name_val.as_symbol, &result)) {
                vm_error(vm, "Variable '%s' not in current scope", name_val.as_symbol->name);
                return false;
            }
            vm_push(vm, result);
            break;
        }

        case OP_SET: {
            Value name_val = vm_pop(vm);
            Value val = vm_pop(vm);
            if (name_val.kind != VAL_SYMBOL) {
                vm_error(vm, "SET requires symbol name");
                value_free(name_val);
                value_free(val);
                return false;
            }
            if (!sn_scope_set(vm->scope, name_val.as_symbol, val)) {
                vm_error(vm, "Cannot set undefined variable '%s'", name_val.as_symbol->name);
                value_free(val);
                return false;
            }
            break;
        }

            // =================================================================
            // Binding Operations (Dynamic names)
            // =================================================================

        case OP_BIND_DYN: {
            Value name_val = vm_pop(vm);
            Value val = vm_pop(vm);
            if (name_val.kind != VAL_STRING) {
                vm_error(vm, "BIND_DYN requires string name");
                value_free(name_val);
                value_free(val);
                return false;
            }
            sn_scope_bind_dynamic(vm->scope, &vm->symbols, name_val.as_string, val);
            value_free(name_val);
            break;
        }

        case OP_LOOKUP_DYN: {
            Value name_val = vm_pop(vm);
            if (name_val.kind != VAL_STRING) {
                vm_error(vm, "LOOKUP_DYN requires string name");
                value_free(name_val);
                return false;
            }
            Value result;
            bool found = false;

            // Try external array lookup FIRST (arrays are a separate namespace)
            if (vm->external_array_lookup) {
                char **elements = NULL;
                int n_elements = 0;
                int ext_found = vm->external_array_lookup(
                    vm->external_ctx, name_val.as_string->data, &elements, &n_elements);
                if (ext_found >= 0) {
                    // Create Subnivean array from external elements
                    Array *arr = array_new(n_elements);
                    for (int i = 0; i < n_elements; i++) {
                        // Preserve string elements (e.g., katakana in K[])
                        char *endptr;
                        int64_t val = strtoll(elements[i], &endptr, 10);
                        if (*endptr == '\0' && elements[i][0] != '\0') {
                            array_push(arr, value_int(val));
                        } else {
                            String *s = string_new(elements[i], strlen(elements[i]));
                            array_push(arr, value_string(s));
                            string_decref(s);
                        }
                        free(elements[i]);
                    }
                    free(elements);
                    result = value_array(arr);
                    // Cache in global scope for fast subsequent lookups
                    Symbol *sym = symbol_intern(&vm->symbols, name_val.as_string->data,
                                                name_val.as_string->len);
                    sn_scope_bind(vm->global, sym, value_copy(result));
                    array_decref(arr);
                    found = true;
                }
            }

            // Fall back to local scope if not found in external arrays
            if (!found &&
                sn_scope_lookup_dynamic(vm->scope, &vm->symbols, name_val.as_string, &result)) {
                found = true;
            }

            if (found) {
                value_free(name_val);
                vm_push(vm, result);
            } else {
                vm_error(vm, "Undefined variable '%s'", name_val.as_string->data);
                value_free(name_val);
                return false;
            }
            break;
        }

        case OP_SET_DYN: {
            Value name_val = vm_pop(vm);
            Value val = vm_pop(vm);
            if (name_val.kind != VAL_STRING) {
                vm_error(vm, "SET_DYN requires string name");
                value_free(name_val);
                value_free(val);
                return false;
            }
            if (!sn_scope_set_dynamic(vm->scope, &vm->symbols, name_val.as_string, val)) {
                vm_error(vm, "Cannot set undefined variable '%s'", name_val.as_string->data);
                value_free(name_val);
                value_free(val);
                return false;
            }
            value_free(name_val);
            break;
        }

            // =================================================================
            // Cell Operations
            // =================================================================

        case OP_CELL_NEW: {
            Value v = vm_pop(vm);
            Cell *cell = cell_new(v);
            vm_push(vm, value_cell(cell));
            cell_decref(cell); // value_cell incremented
            break;
        }

        case OP_CELL_GET: {
            Value v = vm_pop(vm);
            if (v.kind != VAL_CELL) {
                vm_error(vm, "CELL_GET requires cell, got %s", value_kind_name(v.kind));
                value_free(v);
                return false;
            }
            vm_push(vm, cell_get(v.as_cell));
            value_free(v);
            break;
        }

        case OP_CELL_SET: {
            Value cell_val = vm_pop(vm);
            Value new_val = vm_pop(vm);
            if (cell_val.kind != VAL_CELL) {
                vm_error(vm, "CELL_SET requires cell");
                value_free(cell_val);
                value_free(new_val);
                return false;
            }
            cell_set(cell_val.as_cell, new_val);
            value_free(cell_val);
            break;
        }

        case OP_CELL_INC: {
            Value v = vm_pop(vm);
            if (v.kind != VAL_CELL) {
                vm_error(vm, "CELL_INC requires cell");
                value_free(v);
                return false;
            }
            int64_t n = value_to_int(v.as_cell->value) + 1;
            value_free(v.as_cell->value);
            v.as_cell->value = value_int(n);
            vm_push(vm, value_int(n));
            value_free(v);
            break;
        }

        case OP_CELL_DEC: {
            Value v = vm_pop(vm);
            if (v.kind != VAL_CELL) {
                vm_error(vm, "CELL_DEC requires cell");
                value_free(v);
                return false;
            }
            int64_t n = value_to_int(v.as_cell->value) - 1;
            value_free(v.as_cell->value);
            v.as_cell->value = value_int(n);
            vm_push(vm, value_int(n));
            value_free(v);
            break;
        }

            // =================================================================
            // Arithmetic
            // =================================================================

        case OP_ADD: BINARY_INT_OP(+); break;
        case OP_SUB: BINARY_INT_OP(-); break;
        case OP_MUL: BINARY_INT_OP(*); break;

        case OP_DIV: {
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);
            int64_t bv = value_to_int(b);
            if (bv == 0) {
                vm_error(vm, "Division by zero");
                value_free(a);
                value_free(b);
                return false;
            }
            vm_push(vm, value_int(value_to_int(a) / bv));
            value_free(a);
            value_free(b);
            break;
        }

        case OP_MOD: {
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);
            int64_t bv = value_to_int(b);
            if (bv == 0) {
                vm_error(vm, "Modulo by zero");
                value_free(a);
                value_free(b);
                return false;
            }
            vm_push(vm, value_int(value_to_int(a) % bv));
            value_free(a);
            value_free(b);
            break;
        }

        case OP_NEG: {
            Value a = vm_pop(vm);
            vm_push(vm, value_int(-value_to_int(a)));
            value_free(a);
            break;
        }

        case OP_RAND: {
            Value a = vm_pop(vm);
            int max_val = (int)value_to_int(a);
            int result = (max_val > 0) ? (rand() % max_val) : 0;
            vm_push(vm, value_int(result));
            value_free(a);
            break;
        }

            // =================================================================
            // Comparison
            // =================================================================

        case OP_EQ: {
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);
            vm_push(vm, value_int(value_eq(a, b) ? 1 : 0));
            value_free(a);
            value_free(b);
            break;
        }

        case OP_NE: {
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);
            vm_push(vm, value_int(!value_eq(a, b) ? 1 : 0));
            value_free(a);
            value_free(b);
            break;
        }

        case OP_LT: BINARY_INT_OP(<); break;
        case OP_GT: BINARY_INT_OP(>); break;
        case OP_LE: BINARY_INT_OP(<=); break;
        case OP_GE:
            BINARY_INT_OP(>=);
            break;

            // =================================================================
            // Logic
            // =================================================================

        case OP_AND: {
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);
            vm_push(vm, value_int(value_truthy(a) && value_truthy(b) ? 1 : 0));
            value_free(a);
            value_free(b);
            break;
        }

        case OP_OR: {
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);
            vm_push(vm, value_int(value_truthy(a) || value_truthy(b) ? 1 : 0));
            value_free(a);
            value_free(b);
            break;
        }

        case OP_NOT: {
            Value a = vm_pop(vm);
            vm_push(vm, value_int(!value_truthy(a) ? 1 : 0));
            value_free(a);
            break;
        }

            // =================================================================
            // Control Flow
            // =================================================================

        case OP_JUMP: {
            int32_t offset = operand;
            frame->ip += offset;
            break;
        }

        case OP_JUMP_IF: {
            int32_t offset = operand;
            Value cond = vm_pop(vm);
            if (value_truthy(cond)) {
                frame->ip += offset;
            }
            value_free(cond);
            break;
        }

        case OP_JUMP_UNLESS: {
            int32_t offset = operand;
            Value cond = vm_pop(vm);
            if (!value_truthy(cond)) {
                frame->ip += offset;
            }
            value_free(cond);
            break;
        }

        case OP_LOOP: {
            int32_t offset = operand;
            frame->ip -= offset;
            break;
        }

            // =================================================================
            // Functions/Closures
            // =================================================================

        case OP_CLOSURE: {
            int32_t func_idx = operand;
            Value scope_val = vm_pop(vm);
            if (scope_val.kind != VAL_SCOPE) {
                vm_error(vm, "CLOSURE requires scope");
                value_free(scope_val);
                return false;
            }
            Function *func = frame->closure->func->constants[func_idx].as_func;
            Closure *cl = closure_new(func, scope_val.as_scope);
            vm_push(vm, value_closure(cl));
            closure_decref(cl); // value_closure incremented
            value_free(scope_val);
            break;
        }

        case OP_CALL: {
            int32_t argc = operand;
            Value callee = vm_pop(vm);
            if (callee.kind != VAL_CLOSURE) {
                vm_error(vm, "Can only call closures, got %s", value_kind_name(callee.kind));
                value_free(callee);
                return false;
            }
            if (!call_closure(vm, callee.as_closure, argc)) {
                value_free(callee);
                return false;
            }
            value_free(callee);
            frame = &vm->frames[vm->frame_count - 1];
            break;
        }

        case OP_INVOKE_DYN: {
            // Dynamic invoke: call closure, index array, or access map
            int32_t argc = operand;
            Value target = vm_pop(vm);

            if (target.kind == VAL_CLOSURE) {
                // It's a closure - call it
                if (!call_closure(vm, target.as_closure, argc)) {
                    value_free(target);
                    return false;
                }
                value_free(target);
                frame = &vm->frames[vm->frame_count - 1];
            } else if (target.kind == VAL_ARRAY && argc == 1) {
                // It's an array with one arg - array access
                Value idx = vm_pop(vm);
                int64_t i = value_to_int(idx);
                value_free(idx);

                if (i < 0 || (size_t)i >= target.as_array->len) {
                    vm_error(vm, "Array index %lld out of bounds (len %d)", (long long)i,
                             target.as_array->len);
                    value_free(target);
                    return false;
                }
                Value elem = value_copy(target.as_array->data[i]);
                value_free(target);
                vm_push(vm, elem);
            } else if (target.kind == VAL_INT && argc == 1) {
                // It's an int (persistent store handle) with one arg
                // Could be a map, string array, or integer array
                Value key_val = vm_pop(vm);
                int addr = (int)target.as_int;
                int64_t key = value_to_int(key_val);
                value_free(key_val);
                value_free(target);

                if (sn_store_is_string_array(addr)) {
                    // String array access
                    const char *str = sn_store_string_array_get(addr, (int)key);
                    if (str) {
                        String *s = string_new(str, strlen(str));
                        vm_push(vm, value_string(s));
                        string_decref(s);
                    } else {
                        vm_push(vm, value_nil());
                    }
                } else if (sn_store_is_map(addr)) {
                    // Map access
                    int64_t val = sn_store_map_get(addr, key);
                    vm_push(vm, value_int(val));
                } else {
                    // Assume integer array
                    int64_t val = sn_store_array_get(addr, (int)key);
                    vm_push(vm, value_int(val));
                }
            } else {
                vm_error(vm,
                         "INVOKE_DYN: expected closure, array, or map handle, got %s with %d args",
                         value_kind_name(target.kind), argc);
                value_free(target);
                return false;
            }
            break;
        }

        case OP_RETURN: {
            // Clean up frame
            closure_decref(frame->closure);
            sn_scope_decref(frame->scope);
            vm->frame_count--;

            if (vm->frame_count == 0) {
                vm->running = false;
                return true;
            }

            // Restore previous frame
            frame = &vm->frames[vm->frame_count - 1];
            sn_scope_decref(vm->scope);
            vm->scope = frame->scope;
            sn_scope_incref(vm->scope);

            // Push nil as return value
            vm_push(vm, value_nil());
            break;
        }

        case OP_RETURN_VAL: {
            Value result = vm_pop(vm);

            // Clean up frame
            closure_decref(frame->closure);
            sn_scope_decref(frame->scope);
            vm->frame_count--;

            if (vm->frame_count == 0) {
                // Top-level return - output the value
                output_value(vm, result);
                value_free(result);
                vm->running = false;
                return true;
            }

            // Restore previous frame
            frame = &vm->frames[vm->frame_count - 1];
            sn_scope_decref(vm->scope);
            vm->scope = frame->scope;
            sn_scope_incref(vm->scope);

            // Push return value
            vm_push(vm, result);
            break;
        }

            // =================================================================
            // Arrays
            // =================================================================

        case OP_ARRAY_NEW: {
            int32_t count = operand;
            Array *arr = array_new(count > 0 ? count : 8);
            // Elements are in reverse order on stack
            for (int i = count - 1; i >= 0; i--) {
                Value v = vm->sp[-count + i];
                array_push(arr, v);
            }
            vm->sp -= count;
            vm_push(vm, value_array(arr));
            array_decref(arr);
            break;
        }

        case OP_ARRAY_GET: {
            Value idx_val = vm_pop(vm);
            Value arr_val = vm_pop(vm);
            if (arr_val.kind != VAL_ARRAY) {
                vm_error(vm, "Cannot index non-array");
                value_free(arr_val);
                value_free(idx_val);
                return false;
            }
            vm_push(vm, array_get(arr_val.as_array, value_to_int(idx_val)));
            value_free(arr_val);
            value_free(idx_val);
            break;
        }

        case OP_ARRAY_SET: {
            Value val = vm_pop(vm);
            Value idx_val = vm_pop(vm);
            Value arr_val = vm_pop(vm);
            if (arr_val.kind == VAL_ARRAY) {
                // In-memory array
                array_set(arr_val.as_array, value_to_int(idx_val), val);
                value_free(arr_val);
                value_free(idx_val);
            } else if (arr_val.kind == VAL_INT) {
                // Persistent store handle - dispatch based on type
                int addr = (int)arr_val.as_int;
                int64_t key = value_to_int(idx_val);
                if (sn_store_is_map(addr)) {
                    // Map
                    int64_t map_val = value_to_int(val);
                    sn_store_map_set(addr, key, map_val);
                } else {
                    // Assume it's an integer array
                    int64_t v = value_to_int(val);
                    sn_store_array_set(addr, (int)key, v);
                }
                value_free(arr_val);
                value_free(idx_val);
                value_free(val);
            } else {
                vm_error(vm, "Cannot index non-array/non-map");
                value_free(arr_val);
                value_free(idx_val);
                value_free(val);
                return false;
            }
            break;
        }

        case OP_ARRAY_SET_DYN: {
            // Set element in external array: [val, idx, name_str] -> []
            Value val = vm_pop(vm);
            Value idx_val = vm_pop(vm);
            Value name_val = vm_pop(vm);

            if (name_val.kind != VAL_STRING) {
                vm_error(vm, "ARRAY_SET_DYN requires string name");
                value_free(name_val);
                value_free(idx_val);
                value_free(val);
                return false;
            }

            if (!vm->external_array_set) {
                vm_error(vm, "No external array set callback");
                value_free(name_val);
                value_free(idx_val);
                value_free(val);
                return false;
            }

            // Convert value to string for the callback
            String *val_str = value_to_string(val, &vm->symbols);
            int result = vm->external_array_set(vm->external_ctx, name_val.as_string->data,
                                                (int)value_to_int(idx_val), val_str->data);
            string_decref(val_str);

            if (result < 0) {
                vm_error(vm, "Failed to set element in external array '%s'",
                         name_val.as_string->data);
                value_free(name_val);
                value_free(idx_val);
                value_free(val);
                return false;
            }

            value_free(name_val);
            value_free(idx_val);
            value_free(val);
            break;
        }

        case OP_ARRAY_LEN: {
            Value arr_val = vm_pop(vm);
            if (arr_val.kind != VAL_ARRAY) {
                vm_push(vm, value_int(0));
            } else {
                vm_push(vm, value_int(array_len(arr_val.as_array)));
            }
            value_free(arr_val);
            break;
        }

        case OP_ARRAY_PUSH: {
            Value val = vm_pop(vm);
            Value arr_val = vm_pop(vm);
            if (arr_val.kind == VAL_ARRAY) {
                array_push(arr_val.as_array, val);
            } else {
                value_free(val);
            }
            value_free(arr_val);
            break;
        }

        case OP_ARRAY_POP: {
            Value arr_val = vm_pop(vm);
            if (arr_val.kind == VAL_ARRAY) {
                vm_push(vm, array_pop(arr_val.as_array));
            } else {
                vm_push(vm, value_nil());
            }
            value_free(arr_val);
            break;
        }

        case OP_COPYARRAY: {
            // Copy external array to local: [src_name_str, dest_sym] -> []
            Value src_name = vm_pop(vm);
            Value dest_sym = vm_pop(vm);

            if (src_name.kind != VAL_STRING) {
                vm_error(vm, "COPYARRAY: source must be string, got %s",
                         value_kind_name(src_name.kind));
                value_free(src_name);
                value_free(dest_sym);
                return false;
            }
            if (dest_sym.kind != VAL_SYMBOL) {
                vm_error(vm, "COPYARRAY: dest must be symbol, got %s",
                         value_kind_name(dest_sym.kind));
                value_free(src_name);
                value_free(dest_sym);
                return false;
            }

            // Fetch external array
            if (!vm->external_array_lookup) {
                vm_error(vm, "COPYARRAY: no external array lookup");
                value_free(src_name);
                value_free(dest_sym);
                return false;
            }

            char **elements = NULL;
            int n_elements = 0;
            int found = vm->external_array_lookup(vm->external_ctx, src_name.as_string->data,
                                                  &elements, &n_elements);

            if (found < 0) {
                vm_error(vm, "COPYARRAY: external array '%s' not found", src_name.as_string->data);
                value_free(src_name);
                value_free(dest_sym);
                return false;
            }

            // Create local Subnivean array
            Array *arr = array_new(n_elements);
            for (int i = 0; i < n_elements; i++) {
                // Preserve string elements (e.g., katakana in K[])
                char *endptr;
                int64_t val = strtoll(elements[i], &endptr, 10);
                if (*endptr == '\0' && elements[i][0] != '\0') {
                    array_push(arr, value_int(val));
                } else {
                    String *s = string_new(elements[i], strlen(elements[i]));
                    array_push(arr, value_string(s));
                    string_decref(s);
                }
                free(elements[i]);
            }
            free(elements);

            // Bind to destination symbol in current scope
            sn_scope_bind(vm->scope, dest_sym.as_symbol, value_array(arr));
            array_decref(arr);

            value_free(src_name);
            // dest_sym ownership transferred to scope via bind
            break;
        }

            // =================================================================
            // Heap Memory (persistent store)
            // =================================================================

        case OP_MEM_LOAD: {
            // [idx, addr] -> [value]
            Value idx_val = vm_pop(vm);
            Value addr_val = vm_pop(vm);
            int addr = (int)value_to_int(addr_val);
            int idx = (int)value_to_int(idx_val);
            int64_t val = sn_store_array_get(addr, idx);
            vm_push(vm, value_int(val));
            value_free(idx_val);
            value_free(addr_val);
            break;
        }

        case OP_MEM_STORE: {
            // [val, idx, addr] -> []
            Value val = vm_pop(vm);
            Value idx_val = vm_pop(vm);
            Value addr_val = vm_pop(vm);
            int addr = (int)value_to_int(addr_val);
            int idx = (int)value_to_int(idx_val);
            int64_t v = value_to_int(val);
            sn_store_array_set(addr, idx, v);
            value_free(val);
            value_free(idx_val);
            value_free(addr_val);
            break;
        }

        case OP_MEM_LEN: {
            // [addr] -> [len]
            Value addr_val = vm_pop(vm);
            int addr = (int)value_to_int(addr_val);
            int len = sn_store_array_len(addr);
            vm_push(vm, value_int(len));
            value_free(addr_val);
            break;
        }

        case OP_MEM_ALLOC: {
            // [count] -> [addr]
            Value count_val = vm_pop(vm);
            int count = (int)value_to_int(count_val);
            int addr = sn_store_create_array(NULL, count);
            vm_push(vm, value_int(addr));
            value_free(count_val);
            break;
        }

            // =================================================================
            // Maps
            // =================================================================

        case OP_MAP_NEW: {
            // [] -> [addr]
            int addr = sn_store_create_map();
            vm_push(vm, value_int(addr));
            break;
        }

        case OP_MAP_GET: {
            // [key, addr] -> [value]
            Value key_val = vm_pop(vm);
            Value addr_val = vm_pop(vm);
            int addr = (int)value_to_int(addr_val);
            int64_t key = value_to_int(key_val);
            int64_t val = sn_store_map_get(addr, key);
            vm_push(vm, value_int(val));
            value_free(key_val);
            value_free(addr_val);
            break;
        }

        case OP_MAP_SET: {
            // [val, key, addr] -> []
            Value val = vm_pop(vm);
            Value key_val = vm_pop(vm);
            Value addr_val = vm_pop(vm);
            int addr = (int)value_to_int(addr_val);
            int64_t key = value_to_int(key_val);
            int64_t v = value_to_int(val);
            sn_store_map_set(addr, key, v);
            value_free(val);
            value_free(key_val);
            value_free(addr_val);
            break;
        }

        case OP_MAP_HAS: {
            // [key, addr] -> [0/1]
            Value key_val = vm_pop(vm);
            Value addr_val = vm_pop(vm);
            int addr = (int)value_to_int(addr_val);
            int64_t key = value_to_int(key_val);
            bool has = sn_store_map_has(addr, key);
            vm_push(vm, value_int(has ? 1 : 0));
            value_free(key_val);
            value_free(addr_val);
            break;
        }

        case OP_MAP_DEL: {
            // [key, addr] -> [0/1]
            Value key_val = vm_pop(vm);
            Value addr_val = vm_pop(vm);
            int addr = (int)value_to_int(addr_val);
            int64_t key = value_to_int(key_val);
            bool deleted = sn_store_map_del(addr, key);
            vm_push(vm, value_int(deleted ? 1 : 0));
            value_free(key_val);
            value_free(addr_val);
            break;
        }

        case OP_MAP_LEN: {
            // [addr] -> [count]
            Value addr_val = vm_pop(vm);
            int addr = (int)value_to_int(addr_val);
            int len = sn_store_map_len(addr);
            vm_push(vm, value_int(len));
            value_free(addr_val);
            break;
        }

        case OP_MAP_KEYS: {
            // [addr] -> [arr_addr]
            Value addr_val = vm_pop(vm);
            int addr = (int)value_to_int(addr_val);
            int arr_addr = sn_store_map_keys(addr);
            vm_push(vm, value_int(arr_addr));
            value_free(addr_val);
            break;
        }

            // =================================================================
            // String Arrays
            // =================================================================

        case OP_STRING_ARRAY_NEW: {
            // [strs...] -> [addr]; operand = count
            // Compiler pushes strings[n-1], strings[n-2], ..., strings[0]
            // So stack has: [strings[n-1] (bottom), ..., strings[0] (top)]
            // Pop order: strings[0], strings[1], ..., strings[n-1]
            int count = operand;
            const char **strings = calloc(count, sizeof(char *));

            // Pop from top to bottom, which gives us the original order
            for (int i = 0; i < count; i++) {
                Value v = vm_pop(vm);
                String *s = value_to_string(v, &vm->symbols);
                strings[i] = s->data;
                // Note: we pass the pointer but sn_store_create_string_array will strdup
                string_decref(s);
                value_free(v);
            }

            int addr = sn_store_create_string_array(strings, count);
            free(strings);
            vm_push(vm, value_int(addr));
            break;
        }

        case OP_STRING_ARRAY_GET: {
            // [idx, addr] -> [str]
            Value idx_val = vm_pop(vm);
            Value addr_val = vm_pop(vm);
            int addr = (int)value_to_int(addr_val);
            int idx = (int)value_to_int(idx_val);
            const char *str = sn_store_string_array_get(addr, idx);
            if (str) {
                String *s = string_new(str, strlen(str));
                vm_push(vm, value_string(s));
                string_decref(s);
            } else {
                vm_push(vm, value_nil());
            }
            value_free(idx_val);
            value_free(addr_val);
            break;
        }

            // =================================================================
            // Strings
            // =================================================================

        case OP_CONCAT: {
            Value b = vm_pop(vm);
            Value a = vm_pop(vm);
            String *sa = value_to_string(a, &vm->symbols);
            String *sb = value_to_string(b, &vm->symbols);
            String *result = string_concat(sa, sb);
            vm_push(vm, value_string(result));
            string_decref(result);
            string_decref(sa);
            string_decref(sb);
            value_free(a);
            value_free(b);
            break;
        }

        case OP_STRINGIFY: {
            Value v = vm_pop(vm);
            String *s = value_to_string(v, &vm->symbols);
            vm_push(vm, value_string(s));
            string_decref(s);
            value_free(v);
            break;
        }

        case OP_SYMBOL: {
            Value v = vm_pop(vm);
            if (v.kind != VAL_STRING) {
                vm_error(vm, "SYMBOL requires string");
                value_free(v);
                return false;
            }
            Symbol *sym = symbol_intern(&vm->symbols, v.as_string->data, v.as_string->len);
            vm_push(vm, value_symbol(sym));
            value_free(v);
            break;
        }

            // =================================================================
            // Output
            // =================================================================

        case OP_OUTPUT: {
            Value v = vm_pop(vm);
            output_value(vm, v);
            value_free(v);
            break;
        }

        case OP_OUTPUT_RAW: {
            int32_t idx = operand;
            Constant *c = &frame->closure->func->constants[idx];
            if (c->kind == CONST_STRING) {
                output_append(vm, c->as_string->data, c->as_string->len);
            }
            break;
        }

        case OP_EMIT_CURSOR: {
            Value col = vm_pop(vm);
            Value row = vm_pop(vm);
            int r = (int)value_to_int(row);
            int c = (int)value_to_int(col);
            value_free(row);
            value_free(col);
            char buf[32];
            int len = snprintf(buf, sizeof(buf), "\033[%d;%dH", r, c);
            output_append(vm, buf, len);
            break;
        }

            // =================================================================
            // Unified Operations
            // =================================================================

        case OP_LEN: {
            // Unified length: works on arrays, maps (via int addr), and heap
            Value v = vm_pop(vm);
            int64_t len = 0;

            if (v.kind == VAL_ARRAY) {
                // Direct array
                len = array_len(v.as_array);
            } else if (v.kind == VAL_INT) {
                // Could be map addr or heap block addr
                // Try persistent store (checks both map and array blocks)
                int addr = (int)v.as_int;
                // First try as map
                len = sn_store_map_len(addr);
                if (len == 0) {
                    // Try as heap array
                    len = sn_store_array_len(addr);
                }
            }

            vm_push(vm, value_int(len));
            value_free(v);
            break;
        }

            // =================================================================
            // Special
            // =================================================================

        case OP_HALT: vm->running = false; return true;

        case OP_BREAKPOINT:
            // Could trigger debugger here
            break;

        case OP_ASSERT: {
            Value msg = vm_pop(vm);
            Value cond = vm_pop(vm);
            if (!value_truthy(cond)) {
                String *s = value_to_string(msg, &vm->symbols);
                vm_error(vm, "Assertion failed: %s", s->data);
                string_decref(s);
                value_free(msg);
                value_free(cond);
                return false;
            }
            value_free(msg);
            value_free(cond);
            break;
        }

        default: vm_error(vm, "Unknown opcode %u", op); return false;
        }

        if (vm->had_error) return false;
    }

    return true;
}

#undef BINARY_INT_OP

// ============================================================================
// Execute
// ============================================================================

char *vm_execute(VM *vm, Function *main) {
    vm_reset(vm);

    // Create closure for main function with global scope
    Closure *main_closure = closure_new(main, vm->global);

    // Set up initial call frame
    CallFrame *frame = &vm->frames[vm->frame_count++];
    frame->closure = main_closure;
    frame->ip = main->code;
    frame->bp = vm->sp;
    frame->scope = vm->scope;
    sn_scope_incref(frame->scope);

    vm->running = true;
    bool success = run(vm);

    if (!success) {
        return NULL;
    }

    return strdup(vm->output);
}

bool vm_run(VM *vm) {
    vm->running = true;
    return run(vm);
}

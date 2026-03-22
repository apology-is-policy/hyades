// subnivean_api.c - Clean API implementation for Subnivean VM
//
// Provides wrapper functions that bridge the internal Subnivean types
// to the external API used by calc.c
//
// Note: We don't include subnivean_api.h here because it declares opaque
// types that would conflict with the real definitions. The .h file is
// only for external consumers.

#include "calc_compiler.h"
#include "function.h"
#include "sn_assembler.h"
#include "value.h"
#include "vm.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External API uses "SubniveanFunction" which is actually "Function" internally
// The typedef in subnivean_api.h makes it opaque to consumers

VM *subnivean_vm_new(void) {
    VM *vm = calloc(1, sizeof(VM));
    if (vm) {
        vm_init(vm);
    }
    return vm;
}

void subnivean_vm_free(VM *vm) {
    if (vm) {
        vm_free(vm);
        free(vm);
    }
}

// SubniveanFunction* is actually Function* - the cast is safe
Function *subnivean_compile(VM *vm, const char *name, char **params, int n_params,
                            const char *source, char *error_msg, int error_size) {
    if (!vm || !source) {
        if (error_msg && error_size > 0) {
            snprintf(error_msg, error_size, "Invalid arguments");
        }
        return NULL;
    }

    return calc_compile_source(vm, name ? name : "lambda", params, n_params, source, error_msg,
                               error_size);
}

char *subnivean_execute(VM *vm, Function *func, const char **args, int n_args) {
    if (!vm || !func) {
        return NULL;
    }

    // Reset VM state
    vm_reset(vm);

    // Create closure from function
    Closure *cl = closure_new(func, vm->global);
    if (!cl) {
        snprintf(vm->error_msg, sizeof(vm->error_msg), "Failed to create closure");
        return NULL;
    }

    // Push arguments - detect if numeric or string
    for (int i = 0; i < n_args; i++) {
        if (!args[i] || !args[i][0]) {
            // Empty arg -> 0
            vm_push(vm, value_int(0));
        } else {
            // Check if arg is purely numeric (optional leading - then digits)
            const char *p = args[i];
            if (*p == '-') p++;
            bool is_numeric = (*p != '\0'); // Must have at least one char after optional -
            while (*p) {
                if (*p < '0' || *p > '9') {
                    is_numeric = false;
                    break;
                }
                p++;
            }

            if (is_numeric) {
                // Numeric argument
                int64_t val = strtoll(args[i], NULL, 10);
                vm_push(vm, value_int(val));
            } else {
                // String argument (e.g., variable name for indirect lookup)
                String *s = string_new(args[i], strlen(args[i]));
                vm_push(vm, value_string(s));
                string_decref(s);
            }
        }
    }

    // Call the function
    if (!vm_call(vm, cl, n_args)) {
        closure_decref(cl);
        return NULL;
    }

    // Run the VM to completion
    if (!vm_run(vm)) {
        closure_decref(cl);
        return NULL;
    }

    // Result is in vm->output (set by OP_RETURN_VAL at top level)
    char *result = vm->output ? strdup(vm->output) : strdup("");

    closure_decref(cl);
    return result;
}

Function *subnivean_assemble(VM *vm, const char *source, char *error_msg, int error_size) {
    if (!vm || !source) {
        if (error_msg && error_size > 0) {
            snprintf(error_msg, error_size, "Invalid arguments");
        }
        return NULL;
    }
    return sn_assemble(vm, source, error_msg, error_size);
}

void subnivean_function_decref(Function *func) {
    if (func) {
        function_decref(func);
    }
}

char *subnivean_disassemble(Function *func) {
    if (!func) {
        return strdup("(null function)");
    }
    return function_disassemble_to_string(func);
}

void subnivean_set_array_lookup(VM *vm, int (*lookup)(void *, const char *, char ***, int *),
                                void *ctx) {
    if (vm) {
        vm->external_array_lookup = lookup;
        vm->external_ctx = ctx;
    }
}

void subnivean_set_array_set(VM *vm, int (*set_fn)(void *, const char *, int, const char *),
                             void *ctx) {
    if (vm) {
        vm->external_array_set = set_fn;
        vm->external_ctx = ctx;
    }
}

void subnivean_set_lambda_compile(VM *vm, Function *(*compile)(void *, const char *), void *ctx) {
    if (vm) {
        vm->external_lambda_compile = compile;
        vm->external_ctx = ctx;
    }
}

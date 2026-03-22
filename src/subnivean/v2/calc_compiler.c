// calc_compiler.c - Compiler from Hyades Calc AST to Subnivean Bytecode

#include "calc_compiler.h"
#include "calc_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Compiler Initialization
// ============================================================================

void calc_compiler_init(CalcCompiler *cc, VM *vm) {
    cc->vm = vm;
    cc->func = NULL;
    cc->error_msg[0] = '\0';
    cc->had_error = false;
    cc->n_loop_exits = 0;
    cc->loop_depth = 0;
    cc->n_arrays = 0;
    cc->n_lambdas = 0;
    cc->n_params = 0;
}

// Check if name is an array
static bool is_array(CalcCompiler *cc, const char *name) {
    for (int i = 0; i < cc->n_arrays; i++) {
        if (strcmp(cc->arrays[i], name) == 0) return true;
    }
    return false;
}

// Check if name is a lambda
static bool is_lambda(CalcCompiler *cc, const char *name) {
    for (int i = 0; i < cc->n_lambdas; i++) {
        if (strcmp(cc->lambdas[i], name) == 0) return true;
    }
    return false;
}

// Check if name is a parameter (not a cell)
static bool is_param(CalcCompiler *cc, const char *name) {
    for (int i = 0; i < cc->n_params; i++) {
        if (strcmp(cc->params[i], name) == 0) return true;
    }
    return false;
}

// Track an array name
static void track_array(CalcCompiler *cc, const char *name) {
    if (cc->n_arrays < 64) {
        cc->arrays[cc->n_arrays++] = strdup(name);
    }
}

// Track a lambda name
static void track_lambda(CalcCompiler *cc, const char *name) {
    if (cc->n_lambdas < 64) {
        cc->lambdas[cc->n_lambdas++] = strdup(name);
    }
}

// Track a parameter name
static void track_param(CalcCompiler *cc, const char *name) {
    if (cc->n_params < 64) {
        cc->params[cc->n_params++] = strdup(name);
    }
}

static void compiler_error(CalcCompiler *cc, const char *msg) {
    if (!cc->had_error) {
        snprintf(cc->error_msg, sizeof(cc->error_msg), "Compile error: %s", msg);
        cc->had_error = true;
    }
}

const char *calc_compiler_error(CalcCompiler *cc) {
    return cc->had_error ? cc->error_msg : NULL;
}

// ============================================================================
// Compilation Helpers
// ============================================================================

// Get or create symbol index in constant pool
static int get_symbol_idx(CalcCompiler *cc, const char *name) {
    Symbol *sym = vm_intern(cc->vm, name);
    return function_add_symbol(cc->func, sym);
}

// Forward declaration
static void compile_node(CalcCompiler *cc, AstNode *node);
static void compile_node_for_value(CalcCompiler *cc, AstNode *node);

// Compile an expression that should produce a value on the stack
static void compile_node_for_value(CalcCompiler *cc, AstNode *node) {
    if (cc->had_error) return;
    compile_node(cc, node);
}

// Compile a node
static void compile_node(CalcCompiler *cc, AstNode *node) {
    if (cc->had_error || !node) return;

    Function *f = cc->func;

    switch (node->type) {
    case AST_EMPTY:
        // Nothing to emit
        break;

    case AST_INT_LIT: function_emit(f, OP_PUSH_INT, (int32_t)node->int_value); break;

    case AST_STRING_LIT: {
        String *s = string_from_cstr(node->string_value);
        int idx = function_add_string(f, s);
        string_decref(s);
        function_emit(f, OP_PUSH_CONST, idx);
        break;
    }

    case AST_VALUEOF: {
        // \valueof<name> or \valueof<name>[idx]
        int name_idx = get_symbol_idx(cc, node->var.name);

        function_emit(f, OP_PUSH_CONST, name_idx);
        function_emit_simple(f, OP_LOOKUP);

        // Parameters are direct values, not cells
        bool need_cell_get = !is_param(cc, node->var.name);

        if (node->var.index) {
            // Array access
            if (need_cell_get) {
                function_emit_simple(f, OP_CELL_GET);
            }
            compile_node_for_value(cc, node->var.index);
            function_emit_simple(f, OP_ARRAY_GET);
        } else {
            // Simple value access
            if (need_cell_get) {
                function_emit_simple(f, OP_CELL_GET);
            }
        }
        break;
    }

    case AST_RECALL: {
        // \recall<name> - get value/lambda
        // \recall<name>[args...] - call lambda OR array access
        // \recall<\expr> - indirect lookup (expr evaluates to name)
        // \recall<\expr>[args...] - indirect call or array access

        // Check for indirect (target expression instead of literal name)
        if (node->call.target != NULL) {
            // Indirect recall: evaluate target to get name
            if (node->call.n_args == -1) {
                // Indirect array access: \recall<\expr>[idx]
                // First evaluate target to get array
                compile_node_for_value(cc, node->call.target);
                function_emit_simple(f, OP_STRINGIFY);
                function_emit_simple(f, OP_LOOKUP_DYN);

                // Then push index and get element
                compile_node_for_value(cc, node->call.args[0]);
                function_emit_simple(f, OP_ARRAY_GET);
            } else if (node->call.n_args == -2) {
                // Indirect simple lookup: \recall<\expr>
                compile_node_for_value(cc, node->call.target);
                function_emit_simple(f, OP_STRINGIFY);
                function_emit_simple(f, OP_LOOKUP_DYN);
            } else {
                // Indirect call/access: \recall<\expr>[args...]
                // Could be function call OR array access - decided at runtime
                // Push arguments first
                for (int i = 0; i < node->call.n_args; i++) {
                    compile_node_for_value(cc, node->call.args[i]);
                }

                // Evaluate target and lookup dynamically
                compile_node_for_value(cc, node->call.target);
                function_emit_simple(f, OP_STRINGIFY);
                function_emit_simple(f, OP_LOOKUP_DYN);

                // Invoke dynamically (will call if closure, index if array)
                function_emit(f, OP_INVOKE_DYN, node->call.n_args);
            }
        } else if (node->call.n_args > 0) {
            // Direct recall with args
            // Check if it's an array access or function call
            if (is_array(cc, node->call.name)) {
                // Array access: \recall<arr>[idx]
                // Push array
                int name_idx = get_symbol_idx(cc, node->call.name);
                function_emit(f, OP_PUSH_CONST, name_idx);
                function_emit_simple(f, OP_LOOKUP);

                // Push index (only use first arg)
                compile_node_for_value(cc, node->call.args[0]);
                function_emit_simple(f, OP_ARRAY_GET);
            } else if (is_lambda(cc, node->call.name)) {
                // Known function call: \recall<func>[args]
                // Push arguments first
                for (int i = 0; i < node->call.n_args; i++) {
                    compile_node_for_value(cc, node->call.args[i]);
                }

                // Push closure
                int name_idx = get_symbol_idx(cc, node->call.name);
                function_emit(f, OP_PUSH_CONST, name_idx);
                function_emit_simple(f, OP_LOOKUP);

                // Call
                function_emit(f, OP_CALL, node->call.n_args);
            } else {
                // Unknown name - could be array or closure, decide at runtime
                // Push arguments first
                for (int i = 0; i < node->call.n_args; i++) {
                    compile_node_for_value(cc, node->call.args[i]);
                }

                // Lookup and invoke dynamically
                int name_idx = get_symbol_idx(cc, node->call.name);
                function_emit(f, OP_PUSH_CONST, name_idx);
                function_emit_simple(f, OP_LOOKUP);
                function_emit(f, OP_INVOKE_DYN, node->call.n_args);
            }
        } else if (node->var.index) {
            // Array index: \recall<arr>[i] (legacy path)
            int name_idx = get_symbol_idx(cc, node->var.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);

            compile_node_for_value(cc, node->var.index);
            function_emit_simple(f, OP_ARRAY_GET);
        } else {
            // Simple value lookup
            int name_idx = get_symbol_idx(cc, node->var.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);
        }
        break;
    }

    case AST_REF: {
        // \ref<name> - return the NAME as a string reference (for indirect lookup)
        // This allows patterns like \recall<\recall<grid>> where grid contains the array name
        String *s = string_from_cstr(node->var.name);
        int name_idx = function_add_string(f, s);
        string_decref(s);
        function_emit(f, OP_PUSH_CONST, name_idx);
        break;
    }

    case AST_LET: {
        // \let<name>{value} - create mutable counter (cell)
        // \let<name[]>{[...]} - create array
        compile_node_for_value(cc, node->binding.value);

        if (node->binding.is_array) {
            // Track this name as an array
            track_array(cc, node->binding.name);
        } else {
            // Wrap in cell for mutable counter
            function_emit_simple(f, OP_CELL_NEW);
        }

        int name_idx = get_symbol_idx(cc, node->binding.name);
        function_emit(f, OP_PUSH_CONST, name_idx);
        function_emit_simple(f, OP_BIND);
        break;
    }

    case AST_ASSIGN: {
        // \assign<name>{value} - immutable binding (no cell)
        compile_node_for_value(cc, node->binding.value);

        int name_idx = get_symbol_idx(cc, node->binding.name);
        function_emit(f, OP_PUSH_CONST, name_idx);
        function_emit_simple(f, OP_BIND);
        break;
    }

    case AST_INC: {
        // \inc<name>, \inc<*name>, or \inc<${expr}> - increment counter
        // Get the cell that holds the integer
        if (node->collection.target) {
            // Dynamic target: \inc<${expr}>
            compile_node_for_value(cc, node->collection.target);
            function_emit_simple(f, OP_STRINGIFY);
            function_emit_simple(f, OP_LOOKUP_DYN);
            // Dynamic lookup returns the cell directly
        } else if (node->collection.is_deref) {
            // Dereference: \inc<*name> - name holds a string variable name
            int name_idx = get_symbol_idx(cc, node->collection.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);
            bool need_cell_get = !is_param(cc, node->collection.name);
            if (need_cell_get) {
                function_emit_simple(f, OP_CELL_GET);
            }
            // Now have the string/name - look it up dynamically
            function_emit_simple(f, OP_STRINGIFY);
            function_emit_simple(f, OP_LOOKUP_DYN);
        } else {
            // Direct static: \inc<name>
            int name_idx = get_symbol_idx(cc, node->collection.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);
        }

        function_emit_simple(f, OP_CELL_INC);
        function_emit_simple(f, OP_POP); // Discard result
        break;
    }

    case AST_DEC: {
        // \dec<name>, \dec<*name>, or \dec<${expr}> - decrement counter
        // Get the cell that holds the integer
        if (node->collection.target) {
            // Dynamic target: \dec<${expr}>
            compile_node_for_value(cc, node->collection.target);
            function_emit_simple(f, OP_STRINGIFY);
            function_emit_simple(f, OP_LOOKUP_DYN);
            // Dynamic lookup returns the cell directly
        } else if (node->collection.is_deref) {
            // Dereference: \dec<*name> - name holds a string variable name
            int name_idx = get_symbol_idx(cc, node->collection.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);
            bool need_cell_get = !is_param(cc, node->collection.name);
            if (need_cell_get) {
                function_emit_simple(f, OP_CELL_GET);
            }
            // Now have the string/name - look it up dynamically
            function_emit_simple(f, OP_STRINGIFY);
            function_emit_simple(f, OP_LOOKUP_DYN);
        } else {
            // Direct static: \dec<name>
            int name_idx = get_symbol_idx(cc, node->collection.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);
        }

        function_emit_simple(f, OP_CELL_DEC);
        function_emit_simple(f, OP_POP); // Discard result
        break;
    }

    case AST_SETELEMENT: {
        // \setelement<arr>[idx]{value} or \setelement<\expr>[idx]{value}

        if (node->setelement.target != NULL) {
            // Indirect setelement: use external callback to set element
            // Stack order for ARRAY_SET_DYN: [name_str, idx, val]
            compile_node_for_value(cc, node->setelement.target);
            function_emit_simple(f, OP_STRINGIFY);              // name_str
            compile_node_for_value(cc, node->setelement.index); // idx
            compile_node_for_value(cc, node->setelement.value); // val
            function_emit_simple(f, OP_ARRAY_SET_DYN);
        } else {
            // Direct setelement with literal name
            // Stack order for ARRAY_SET: [arr, idx, val]
            int name_idx = get_symbol_idx(cc, node->setelement.array_name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);                 // array
            compile_node_for_value(cc, node->setelement.index); // idx
            compile_node_for_value(cc, node->setelement.value); // val
            function_emit_simple(f, OP_ARRAY_SET);
        }
        break;
    }

    case AST_LEN: {
        // \len<name> or \len<*name> - unified length (arrays, maps, heap)
        compile_node_for_value(cc, node->unary.operand);
        function_emit_simple(f, OP_LEN);
        break;
    }

    case AST_COPYARRAY: {
        // \copyarray<dest>{source_name_expr}
        // Stack: [dest_sym, src_name_str] -> COPYARRAY -> []
        int dest_idx = get_symbol_idx(cc, node->copyarray.dest_name);
        function_emit(f, OP_PUSH_CONST, dest_idx);          // dest symbol
        compile_node_for_value(cc, node->copyarray.source); // source name expr
        function_emit_simple(f, OP_STRINGIFY);              // ensure it's a string
        function_emit_simple(f, OP_COPYARRAY);
        break;
    }

    // Binary operations
    case AST_ADD:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_ADD);
        break;

    case AST_SUB:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_SUB);
        break;

    case AST_MUL:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_MUL);
        break;

    case AST_DIV:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_DIV);
        break;

    case AST_MOD:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_MOD);
        break;

    case AST_MAX:
        // max(a,b) = a > b ? a : b
        // Implemented as: if a > b then a else b
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        // Stack: [a, b]
        function_emit_simple(f, OP_DUP); // [a, b, b]
        function_emit_simple(f, OP_ROT); // [b, b, a]
        function_emit_simple(f, OP_DUP); // [b, b, a, a]
        function_emit_simple(f, OP_ROT); // [b, a, a, b]
        function_emit_simple(f, OP_GT);  // [b, a, a>b]
        int else_max = function_emit_jump(f, OP_JUMP_UNLESS);
        // Take a: [b, a]
        function_emit_simple(f, OP_SWAP); // [a, b]
        function_emit_simple(f, OP_POP);  // [a]
        int end_max = function_emit_jump(f, OP_JUMP);
        function_patch_jump(f, else_max);
        // Take b: [b, a]
        function_emit_simple(f, OP_POP); // [b]
        function_patch_jump(f, end_max);
        break;

    case AST_MIN:
        // min(a,b) = a < b ? a : b
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_DUP);
        function_emit_simple(f, OP_ROT);
        function_emit_simple(f, OP_DUP);
        function_emit_simple(f, OP_ROT);
        function_emit_simple(f, OP_LT);
        int else_min = function_emit_jump(f, OP_JUMP_UNLESS);
        function_emit_simple(f, OP_SWAP);
        function_emit_simple(f, OP_POP);
        int end_min = function_emit_jump(f, OP_JUMP);
        function_patch_jump(f, else_min);
        function_emit_simple(f, OP_POP);
        function_patch_jump(f, end_min);
        break;

    // Comparison operations
    case AST_EQ:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_EQ);
        break;

    case AST_NE:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_NE);
        break;

    case AST_LT:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_LT);
        break;

    case AST_GT:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_GT);
        break;

    case AST_LE:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_LE);
        break;

    case AST_GE:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_GE);
        break;

    // Logical operations
    case AST_AND:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_AND);
        break;

    case AST_OR:
        compile_node_for_value(cc, node->binary.left);
        compile_node_for_value(cc, node->binary.right);
        function_emit_simple(f, OP_OR);
        break;

    case AST_NOT:
        compile_node_for_value(cc, node->unary.operand);
        function_emit_simple(f, OP_NOT);
        break;

    case AST_NEG:
        compile_node_for_value(cc, node->unary.operand);
        function_emit_simple(f, OP_NEG);
        break;

    case AST_RAND:
        compile_node_for_value(cc, node->unary.operand);
        function_emit_simple(f, OP_RAND);
        break;

    case AST_IF: {
        // \if{cond}{then}\else{else}
        compile_node_for_value(cc, node->if_node.condition);

        int else_jump = function_emit_jump(f, OP_JUMP_UNLESS);

        // Then branch
        compile_node(cc, node->if_node.then_branch);

        if (node->if_node.else_branch) {
            int end_jump = function_emit_jump(f, OP_JUMP);
            function_patch_jump(f, else_jump);

            // Else branch
            compile_node(cc, node->if_node.else_branch);

            function_patch_jump(f, end_jump);
        } else {
            function_patch_jump(f, else_jump);
        }
        break;
    }

    case AST_LOOP: {
        // \begin{loop}...\exit_when{cond}...\end{loop}
        int loop_start = function_offset(f);

        // Save loop context
        int saved_n_exits = cc->n_loop_exits;
        cc->loop_depth++;

        // Compile body
        compile_node(cc, node->loop.body);

        // Loop back
        function_emit_loop(f, loop_start);

        // Patch all exit jumps
        for (int i = saved_n_exits; i < cc->n_loop_exits; i++) {
            function_patch_jump(f, cc->loop_exit_jumps[i]);
        }

        cc->n_loop_exits = saved_n_exits;
        cc->loop_depth--;
        break;
    }

    case AST_EXIT_WHEN: {
        // \exit_when{cond}
        if (cc->loop_depth == 0) {
            compiler_error(cc, "\\exit_when outside of loop");
            return;
        }

        compile_node_for_value(cc, node->control.value);

        if (cc->n_loop_exits >= 64) {
            compiler_error(cc, "Too many exit_when in loop");
            return;
        }

        cc->loop_exit_jumps[cc->n_loop_exits++] = function_emit_jump(f, OP_JUMP_IF);
        break;
    }

    case AST_RETURN: {
        // \return{value}
        compile_node_for_value(cc, node->control.value);
        function_emit_simple(f, OP_RETURN_VAL);
        break;
    }

    case AST_ARRAY_LIT: {
        // [e1, e2, e3, ...]
        // Push elements in reverse order (first element deepest)
        for (int i = node->array.n_elements - 1; i >= 0; i--) {
            compile_node_for_value(cc, node->array.elements[i]);
        }
        function_emit(f, OP_ARRAY_NEW, node->array.n_elements);
        break;
    }

    case AST_STRING_ARRAY_LIT: {
        // [str1, str2, str3, ...] - string array for persistent store
        // Push strings in reverse order (first string deepest)
        for (int i = node->string_array.n_strings - 1; i >= 0; i--) {
            String *s =
                string_new(node->string_array.strings[i], strlen(node->string_array.strings[i]));
            int const_idx = function_add_string(f, s);
            function_emit(f, OP_PUSH_CONST, const_idx);
            string_decref(s);
        }
        function_emit(f, OP_STRING_ARRAY_NEW, node->string_array.n_strings);
        break;
    }

    case AST_LAMBDA: {
        // \lambda<name>[params]{body}
        // Track lambda name in outer scope
        if (node->lambda.name) {
            track_lambda(cc, node->lambda.name);
        }

        // Compile the lambda body as a nested function
        Function *lambda_func = function_new(node->lambda.name, node->lambda.n_params);

        // Set parameters
        for (int i = 0; i < node->lambda.n_params; i++) {
            lambda_func->params[i] = vm_intern(cc->vm, node->lambda.params[i]);
        }

        // Compile body in new compiler context
        CalcCompiler lambda_cc;
        calc_compiler_init(&lambda_cc, cc->vm);
        lambda_cc.func = lambda_func;

        // Track parameters in inner context (they're not cells)
        for (int i = 0; i < node->lambda.n_params; i++) {
            track_param(&lambda_cc, node->lambda.params[i]);
        }

        compile_node(&lambda_cc, node->lambda.body);

        if (lambda_cc.had_error) {
            strncpy(cc->error_msg, lambda_cc.error_msg, sizeof(cc->error_msg));
            cc->had_error = true;
            function_decref(lambda_func);
            return;
        }

        // Add implicit return nil if no explicit return
        function_emit_simple(lambda_func, OP_RETURN);

        // Add lambda to parent function's constants
        int func_idx = function_add_function(f, lambda_func);
        function_decref(lambda_func);

        // Create closure and bind to name
        function_emit_simple(f, OP_SCOPE_CAPTURE);
        function_emit(f, OP_CLOSURE, func_idx);

        if (node->lambda.name) {
            int name_idx = get_symbol_idx(cc, node->lambda.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_BIND);
        }
        // If anonymous, leave closure on stack
        break;
    }

    case AST_SEQ: {
        // Sequence of statements
        for (int i = 0; i < node->seq.n_stmts; i++) {
            compile_node(cc, node->seq.stmts[i]);
        }
        break;
    }

    case AST_MEM_LOAD: {
        // \mem_load{addr,idx} -> [value]
        // VM pops: idx (top), then addr
        // So push: addr first, then idx (idx ends up on top)
        compile_node_for_value(cc, node->mem.addr);  // addr
        compile_node_for_value(cc, node->mem.index); // idx (on top)
        function_emit_simple(f, OP_MEM_LOAD);
        break;
    }

    case AST_MEM_STORE: {
        // \mem_store{addr,idx,val} -> []
        // VM pops: val (top), idx, addr
        // So push: addr first, then idx, then val (val ends up on top)
        compile_node_for_value(cc, node->mem.addr);  // addr
        compile_node_for_value(cc, node->mem.index); // idx
        compile_node_for_value(cc, node->mem.value); // val (on top)
        function_emit_simple(f, OP_MEM_STORE);
        break;
    }

    case AST_MEM_LEN: {
        // \mem_len{addr} -> [len]
        // Stack order for MEM_LEN: [addr] -> [len]
        compile_node_for_value(cc, node->mem.addr);
        function_emit_simple(f, OP_MEM_LEN);
        break;
    }

    case AST_MEM_ALLOC: {
        // \mem_alloc{count} -> [addr]
        // Stack order for MEM_ALLOC: [count] -> [addr]
        compile_node_for_value(cc, node->mem.addr); // addr holds count for alloc
        function_emit_simple(f, OP_MEM_ALLOC);
        break;
    }

    case AST_MAP_NEW: {
        // \map_new{} -> [addr]
        function_emit_simple(f, OP_MAP_NEW);
        break;
    }

    case AST_MAP_GET: {
        // \map_get{addr,key} -> [value]
        // VM pops: key (top), then addr
        // So push: addr first, then key (key ends up on top)
        compile_node_for_value(cc, node->map.addr);
        compile_node_for_value(cc, node->map.key);
        function_emit_simple(f, OP_MAP_GET);
        break;
    }

    case AST_MAP_SET: {
        // \map_set{addr,key,val} -> []
        // VM pops: val (top), key, addr
        // So push: addr first, then key, then val (val ends up on top)
        compile_node_for_value(cc, node->map.addr);
        compile_node_for_value(cc, node->map.key);
        compile_node_for_value(cc, node->map.value);
        function_emit_simple(f, OP_MAP_SET);
        break;
    }

    case AST_MAP_HAS: {
        // \map_has{addr,key} -> [0/1]
        // VM pops: key (top), then addr
        compile_node_for_value(cc, node->map.addr);
        compile_node_for_value(cc, node->map.key);
        function_emit_simple(f, OP_MAP_HAS);
        break;
    }

    case AST_MAP_DEL: {
        // \map_del{addr,key} -> [0/1]
        // VM pops: key (top), then addr
        compile_node_for_value(cc, node->map.addr);
        compile_node_for_value(cc, node->map.key);
        function_emit_simple(f, OP_MAP_DEL);
        break;
    }

    case AST_MAP_LEN: {
        // \map_len{addr} -> [count]
        compile_node_for_value(cc, node->map.addr);
        function_emit_simple(f, OP_MAP_LEN);
        break;
    }

    case AST_MAP_KEYS: {
        // \map_keys{addr} -> [arr_addr]
        compile_node_for_value(cc, node->map.addr);
        function_emit_simple(f, OP_MAP_KEYS);
        break;
    }

    case AST_CURSOR: {
        // \cursor{row, col} -> emit \x1b[row;colH
        compile_node_for_value(cc, node->binary.left);  // row
        compile_node_for_value(cc, node->binary.right); // col
        function_emit_simple(f, OP_EMIT_CURSOR);
        break;
    }

    case AST_ANSI: {
        // \ansi{codes} -> emit pre-built escape sequence from constant pool
        // node->string_value contains the pre-built "\x1b[codes m" string
        String *s = string_from_cstr(node->string_value);
        int idx = function_add_string(f, s);
        string_decref(s);
        function_emit(f, OP_OUTPUT_RAW, idx);
        break;
    }

    case AST_EMIT: {
        // \emit{expr} -> evaluate expr, stringify, append to output
        compile_node_for_value(cc, node->unary.operand);
        function_emit_simple(f, OP_OUTPUT);
        break;
    }

        // ============================================================================
        // New CL Syntax AST Nodes
        // ============================================================================

    case AST_VAR_ACCESS: {
        // ${name} or ${*name} - unified variable access
        if (node->var_access.dynamic_name) {
            // Dynamic name: ${${expr}} or ${item${i}}
            compile_node_for_value(cc, node->var_access.dynamic_name);
            function_emit_simple(f, OP_STRINGIFY);
            function_emit_simple(f, OP_LOOKUP_DYN);
            // Dynamic lookup may return a cell - do CELL_GET
            // Note: We can't know at compile time if it's a cell, but CELL_GET
            // on non-cell values will error. For arrays/closures it returns directly.
            // For now, assume dynamic names access cell-wrapped values.
            function_emit_simple(f, OP_CELL_GET);
        } else {
            // Static name
            int name_idx = get_symbol_idx(cc, node->var_access.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);

            // Parameters are direct values, not cells
            bool need_cell_get = !is_param(cc, node->var_access.name) &&
                                 !is_array(cc, node->var_access.name) &&
                                 !is_lambda(cc, node->var_access.name);
            if (need_cell_get) {
                function_emit_simple(f, OP_CELL_GET);
            }
        }

        // For dereference (${*name}), the value we have is a name/address string
        // We need to look it up to get the actual collection/value
        if (node->var_access.is_deref) {
            function_emit_simple(f, OP_STRINGIFY);
            function_emit_simple(f, OP_LOOKUP_DYN);
            // Don't CELL_GET here - the dereferenced value might be an array
        }
        break;
    }

    case AST_VAR_CONCAT: {
        // ${prefix${i}suffix} - concatenated variable name
        // Compile each part, stringify, and concatenate
        // This ONLY produces the computed string name - does NOT do LOOKUP_DYN
        // The lookup is handled by AST_VAR_ACCESS when it wraps this node
        for (int i = 0; i < node->var_concat.n_parts; i++) {
            compile_node_for_value(cc, node->var_concat.parts[i]);
            function_emit_simple(f, OP_STRINGIFY);
            if (i > 0) {
                function_emit_simple(f, OP_CONCAT);
            }
        }
        // Stack now has the concatenated name string
        // No LOOKUP_DYN here - caller will do it if needed
        break;
    }

    case AST_INVOKE: {
        // \invoke<name>[args] - function call
        // Push arguments first
        for (int i = 0; i < node->invoke.n_args; i++) {
            compile_node_for_value(cc, node->invoke.args[i]);
        }

        // Get the function/closure
        if (node->invoke.target) {
            // Dynamic target: \invoke<${expr}>[args]
            compile_node_for_value(cc, node->invoke.target);
            function_emit_simple(f, OP_STRINGIFY);
            function_emit_simple(f, OP_LOOKUP_DYN);
        } else if (node->invoke.is_deref) {
            // Dereference: \invoke<*name>[args] - name holds an address
            int name_idx = get_symbol_idx(cc, node->invoke.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);
            bool need_cell_get = !is_param(cc, node->invoke.name);
            if (need_cell_get) {
                function_emit_simple(f, OP_CELL_GET);
            }
            // Value is an address - look it up dynamically
            function_emit_simple(f, OP_STRINGIFY);
            function_emit_simple(f, OP_LOOKUP_DYN);
        } else {
            // Direct: \invoke<name>[args]
            int name_idx = get_symbol_idx(cc, node->invoke.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);
        }

        // Call
        function_emit(f, OP_CALL, node->invoke.n_args);
        break;
    }

    case AST_COLLECTION_GET: {
        // \at<name>[key] - read collection element
        // Works for arrays and maps
        // OP_INVOKE_DYN expects: [args..., target]
        // So push key first, then collection

        // Push key first
        compile_node_for_value(cc, node->collection.key);

        // Get the collection (pushed second, so it's on top for INVOKE_DYN)
        if (node->collection.target) {
            // Dynamic target
            compile_node_for_value(cc, node->collection.target);
            function_emit_simple(f, OP_STRINGIFY);
            function_emit_simple(f, OP_LOOKUP_DYN);
        } else if (node->collection.is_deref) {
            // Dereference: \at<*name>[key] - name holds an address
            int name_idx = get_symbol_idx(cc, node->collection.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);
            bool need_cell_get = !is_param(cc, node->collection.name);
            if (need_cell_get) {
                function_emit_simple(f, OP_CELL_GET);
            }
        } else {
            // Direct access: \at<name>[key]
            // Arrays are bound directly; other values (maps, integers) are cell-wrapped
            int name_idx = get_symbol_idx(cc, node->collection.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);
            bool need_cell_get =
                !is_param(cc, node->collection.name) && !is_array(cc, node->collection.name);
            if (need_cell_get) {
                function_emit_simple(f, OP_CELL_GET);
            }
        }

        // Use INVOKE_DYN with argc=1 - it handles both array[idx] and map[key]
        function_emit(f, OP_INVOKE_DYN, 1);
        break;
    }

    case AST_COLLECTION_SET: {
        // \set<name>[key]{val} - write collection element
        // Works for arrays and maps via OP_ARRAY_SET runtime dispatch

        // Get the collection
        if (node->collection.target) {
            // Dynamic target
            compile_node_for_value(cc, node->collection.target);
            function_emit_simple(f, OP_STRINGIFY);
            function_emit_simple(f, OP_LOOKUP_DYN);
        } else if (node->collection.is_deref) {
            // Dereference: \set<*name>[key]{val}
            // Deref implies the variable holds a handle (integer), so CELL_GET needed
            int name_idx = get_symbol_idx(cc, node->collection.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);
            bool need_cell_get = !is_param(cc, node->collection.name);
            if (need_cell_get) {
                function_emit_simple(f, OP_CELL_GET);
            }
        } else {
            // Direct access: \set<name>[key]{val}
            // Arrays are bound directly; other values (maps, integers) are cell-wrapped
            int name_idx = get_symbol_idx(cc, node->collection.name);
            function_emit(f, OP_PUSH_CONST, name_idx);
            function_emit_simple(f, OP_LOOKUP);
            bool need_cell_get =
                !is_param(cc, node->collection.name) && !is_array(cc, node->collection.name);
            if (need_cell_get) {
                function_emit_simple(f, OP_CELL_GET);
            }
        }

        // Push key and value
        compile_node_for_value(cc, node->collection.key);
        compile_node_for_value(cc, node->collection.value);

        // OP_ARRAY_SET has runtime dispatch for both arrays and maps
        function_emit_simple(f, OP_ARRAY_SET);
        break;
    }

    case AST_ADDRESSOF: {
        // \addressof<name> - get address/reference to collection
        // For now, this is semantically equivalent to getting the value
        // (arrays and maps are already by reference in the VM)
        int name_idx = get_symbol_idx(cc, node->addressof.name);
        function_emit(f, OP_PUSH_CONST, name_idx);
        function_emit_simple(f, OP_LOOKUP);
        break;
    }

    case AST_ENUMERATE: {
        // \begin<arr>[i,v]{enumerate}...\end{enumerate}
        // Compiled as a loop over array elements

        // Step 1: Get array and its length
        if (node->enumerate.array_expr) {
            compile_node_for_value(cc, node->enumerate.array_expr);
        } else {
            int arr_idx = get_symbol_idx(cc, node->enumerate.array_name);
            function_emit(f, OP_PUSH_CONST, arr_idx);
            function_emit_simple(f, OP_LOOKUP);
        }
        // Stack: [arr]

        // Store array in a temp variable
        static int enum_counter = 0;
        char arr_var[32], len_var[32];
        snprintf(arr_var, sizeof(arr_var), "__enum_arr_%d", enum_counter);
        snprintf(len_var, sizeof(len_var), "__enum_len_%d", enum_counter);
        enum_counter++;

        // Bind array to temp
        function_emit_simple(f, OP_DUP);
        int arr_sym = get_symbol_idx(cc, arr_var);
        function_emit(f, OP_PUSH_CONST, arr_sym);
        function_emit_simple(f, OP_BIND);
        // Stack: [arr]

        // Get length and store
        function_emit_simple(f, OP_ARRAY_LEN);
        function_emit_simple(f, OP_CELL_NEW);
        int len_sym = get_symbol_idx(cc, len_var);
        function_emit(f, OP_PUSH_CONST, len_sym);
        function_emit_simple(f, OP_BIND);
        // Stack: []

        // Initialize index to 0
        function_emit(f, OP_PUSH_INT, 0);
        function_emit_simple(f, OP_CELL_NEW);
        int idx_sym = get_symbol_idx(cc, node->enumerate.idx_var);
        function_emit(f, OP_PUSH_CONST, idx_sym);
        function_emit_simple(f, OP_BIND);
        // Stack: []

        // Loop start
        int loop_start = function_offset(f);

        // Check: idx >= len?
        function_emit(f, OP_PUSH_CONST, idx_sym);
        function_emit_simple(f, OP_LOOKUP);
        function_emit_simple(f, OP_CELL_GET);
        function_emit(f, OP_PUSH_CONST, len_sym);
        function_emit_simple(f, OP_LOOKUP);
        function_emit_simple(f, OP_CELL_GET);
        function_emit_simple(f, OP_GE);
        // Stack: [cond]

        int exit_jump = function_emit_jump(f, OP_JUMP_IF);
        // Stack: []

        // Get arr[idx] and bind to val_var
        function_emit(f, OP_PUSH_CONST, arr_sym);
        function_emit_simple(f, OP_LOOKUP);
        function_emit(f, OP_PUSH_CONST, idx_sym);
        function_emit_simple(f, OP_LOOKUP);
        function_emit_simple(f, OP_CELL_GET);
        function_emit_simple(f, OP_ARRAY_GET);
        function_emit_simple(f, OP_CELL_NEW);
        int val_sym = get_symbol_idx(cc, node->enumerate.val_var);
        function_emit(f, OP_PUSH_CONST, val_sym);
        function_emit_simple(f, OP_BIND);
        // Stack: []

        // Compile body
        compile_node(cc, node->enumerate.body);

        // Increment index
        function_emit(f, OP_PUSH_CONST, idx_sym);
        function_emit_simple(f, OP_LOOKUP);
        function_emit_simple(f, OP_CELL_INC);
        function_emit_simple(f, OP_POP);

        // Jump back to loop start
        function_emit_loop(f, loop_start);

        // Patch exit jump
        function_patch_jump(f, exit_jump);

        // Push nil as result
        function_emit_simple(f, OP_PUSH_NIL);
        break;
    }

    case AST_MAP_LIT: {
        // |1->10, 2->20| - map literal
        // Create new map, then set each key-value pair
        function_emit_simple(f, OP_MAP_NEW);
        // Stack: [addr]

        for (int i = 0; i < node->map_lit.n_pairs; i++) {
            function_emit_simple(f, OP_DUP); // Duplicate addr for MAP_SET
            compile_node_for_value(cc, node->map_lit.keys[i]);
            compile_node_for_value(cc, node->map_lit.values[i]);
            function_emit_simple(f, OP_MAP_SET); // [val, key, addr] -> []
        }
        // Stack: [addr]
        break;
    }

    default: compiler_error(cc, "Unknown AST node type"); break;
    }
}

// ============================================================================
// Public API
// ============================================================================

Function *calc_compile(CalcCompiler *cc, const char *name, char **params, int n_params,
                       AstNode *body) {
    cc->func = function_new(name, n_params);
    cc->had_error = false;

    // Set parameters and track them (so \valueof<param> doesn't use CELL_GET)
    for (int i = 0; i < n_params; i++) {
        cc->func->params[i] = vm_intern(cc->vm, params[i]);
        track_param(cc, params[i]);
    }

    // Compile body
    compile_node(cc, body);

    if (cc->had_error) {
        function_decref(cc->func);
        return NULL;
    }

    // Add implicit return nil if no explicit return
    function_emit_simple(cc->func, OP_RETURN);

    return cc->func;
}

Function *calc_compile_expr(CalcCompiler *cc, const char *name, AstNode *expr) {
    return calc_compile(cc, name, NULL, 0, expr);
}

Function *calc_compile_source(VM *vm, const char *name, char **params, int n_params,
                              const char *source, char *error_msg, int error_size) {
    // Parse
    CalcParser parser;
    calc_parser_init(&parser, source);
    AstNode *ast = calc_parse(&parser);

    if (parser.had_error) {
        if (error_msg) {
            strncpy(error_msg, parser.error_msg, error_size);
        }
        calc_ast_free(ast);
        return NULL;
    }

    // Compile
    CalcCompiler compiler;
    calc_compiler_init(&compiler, vm);
    Function *func = calc_compile(&compiler, name, params, n_params, ast);

    if (compiler.had_error) {
        if (error_msg) {
            strncpy(error_msg, compiler.error_msg, error_size);
        }
        calc_ast_free(ast);
        return NULL;
    }

    calc_ast_free(ast);
    return func;
}

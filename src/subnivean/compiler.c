// compiler.c - Subnivean bytecode compiler
//
// Compiles AST to bytecode for the Subnivean VM.

#include "compiler.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Forward Declarations
// ============================================================================

static void compile_node(Compiler *c, AstNode *node);
static void compile_expression(Compiler *c, AstNode *node);
static void compile_statement(Compiler *c, AstNode *node);

// ============================================================================
// Compiler Utilities
// ============================================================================

static void compiler_error(Compiler *c, const char *format, ...) {
    if (c->had_error) return; // Don't cascade

    c->had_error = true;

    va_list args;
    va_start(args, format);
    vsnprintf(c->error_msg, sizeof(c->error_msg), format, args);
    va_end(args);

    fprintf(stderr, "Compile error: %s\n", c->error_msg);
}

// ============================================================================
// Bytecode Emission
// ============================================================================

static void ensure_code_capacity(CompiledFunc *func, int needed) {
    if (func->code_len + needed > func->code_cap) {
        func->code_cap = func->code_cap * 2 + needed;
        func->code = realloc(func->code, func->code_cap * sizeof(Instruction));
    }
}

void emit(Compiler *c, OpCode op, int32_t operand) {
    ensure_code_capacity(c->function, 1);
    Instruction *inst = &c->function->code[c->function->code_len++];
    inst->op = op;
    inst->operand = operand;
}

void emit_simple(Compiler *c, OpCode op) {
    emit(c, op, 0);
}

int emit_jump(Compiler *c, OpCode op) {
    emit(c, op, 0); // Placeholder, will be patched
    return c->function->code_len - 1;
}

void patch_jump(Compiler *c, int jump_index) {
    // Calculate offset from jump instruction to current position
    int offset = c->function->code_len - jump_index - 1;
    c->function->code[jump_index].operand = offset;
}

void emit_loop(Compiler *c, int loop_start) {
    // Calculate offset to jump backward
    int offset = c->function->code_len - loop_start + 1;
    emit(c, OP_LOOP, offset);
}

int current_offset(Compiler *c) {
    return c->function->code_len;
}

// ============================================================================
// Constant Pool
// ============================================================================

static void ensure_constants_capacity(CompiledFunc *func) {
    if (func->n_constants >= func->constants_cap) {
        func->constants_cap = func->constants_cap * 2 + 8;
        func->constants =
            realloc(func->constants, func->constants_cap * sizeof(func->constants[0]));
    }
}

int compiler_add_string_constant(Compiler *c, const char *str) {
    CompiledFunc *func = c->function;

    // Check if already exists
    for (int i = 0; i < func->n_constants; i++) {
        if (func->constants[i].type == CONST_STRING &&
            strcmp(func->constants[i].string, str) == 0) {
            return i;
        }
    }

    ensure_constants_capacity(func);
    int index = func->n_constants++;
    func->constants[index].type = CONST_STRING;
    func->constants[index].string = strdup(str);
    return index;
}

int compiler_add_int_constant(Compiler *c, int32_t value) {
    CompiledFunc *func = c->function;

    ensure_constants_capacity(func);
    int index = func->n_constants++;
    func->constants[index].type = CONST_INT;
    func->constants[index].integer = value;
    return index;
}

int compiler_add_func_constant(Compiler *c, CompiledFunc *func) {
    CompiledFunc *current = c->function;

    ensure_constants_capacity(current);
    int index = current->n_constants++;
    current->constants[index].type = CONST_FUNC;
    current->constants[index].func = func;
    return index;
}

// ============================================================================
// Local Variable Management
// ============================================================================

int compiler_resolve_local(Compiler *c, const char *name) {
    // Search from innermost scope outward
    for (int i = c->local_count - 1; i >= 0; i--) {
        if (strcmp(c->locals[i].name, name) == 0) {
            return i;
        }
    }
    return -1; // Not found
}

int compiler_add_local(Compiler *c, const char *name) {
    if (c->local_count >= 256) {
        compiler_error(c, "Too many local variables");
        return -1;
    }

    int slot = c->local_count++;
    c->locals[slot].name = strdup(name);
    c->locals[slot].depth = c->scope_depth;
    c->locals[slot].is_captured = false;

    if (slot >= c->function->n_locals) {
        c->function->n_locals = slot + 1;
    }

    return slot;
}

void compiler_begin_scope(Compiler *c) {
    c->scope_depth++;
}

void compiler_end_scope(Compiler *c) {
    c->scope_depth--;

    // Pop locals from ended scope
    while (c->local_count > 0 && c->locals[c->local_count - 1].depth > c->scope_depth) {
        // If captured, close the upvalue
        if (c->locals[c->local_count - 1].is_captured) {
            emit_simple(c, OP_CLOSE_UPVALUE);
        } else {
            emit_simple(c, OP_POP);
        }
        free(c->locals[c->local_count - 1].name);
        c->local_count--;
    }
}

// ============================================================================
// Upvalue Resolution (for closures)
// ============================================================================

static int add_upvalue(Compiler *c, uint8_t index, bool is_local) {
    int upvalue_count = c->function->n_upvalues;

    // Check if already captured
    for (int i = 0; i < upvalue_count; i++) {
        if (c->upvalues[i].index == index && c->upvalues[i].is_local == is_local) {
            return i;
        }
    }

    if (upvalue_count >= 256) {
        compiler_error(c, "Too many closure variables");
        return -1;
    }

    c->upvalues[upvalue_count].index = index;
    c->upvalues[upvalue_count].is_local = is_local;
    return c->function->n_upvalues++;
}

int compiler_resolve_upvalue(Compiler *c, const char *name) {
    Compiler *enclosing = (Compiler *)c->enclosing;
    if (enclosing == NULL) return -1;

    // Try to find as local in enclosing function
    int local = compiler_resolve_local(enclosing, name);
    if (local != -1) {
        enclosing->locals[local].is_captured = true;
        return add_upvalue(c, (uint8_t)local, true);
    }

    // Try to find as upvalue in enclosing function
    int upvalue = compiler_resolve_upvalue(enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(c, (uint8_t)upvalue, false);
    }

    return -1;
}

// ============================================================================
// Expression Compilation
// ============================================================================

static void compile_binary(Compiler *c, AstNode *node, OpCode op) {
    compile_expression(c, node->binary.left);
    compile_expression(c, node->binary.right);
    emit_simple(c, op);
}

static void compile_expression(Compiler *c, AstNode *node) {
    if (!node || c->had_error) return;

    switch (node->type) {
    case AST_INT: emit(c, OP_PUSH_INT, node->int_val); break;

    case AST_VAR: {
        // Try local first
        int slot = compiler_resolve_local(c, node->var.name);
        if (slot != -1) {
            emit(c, OP_LOAD_LOCAL, slot);
        } else {
            // Try upvalue
            int upvalue = compiler_resolve_upvalue(c, node->var.name);
            if (upvalue != -1) {
                emit(c, OP_LOAD_UPVALUE, upvalue);
            } else {
                // Global
                int name_const = compiler_add_string_constant(c, node->var.name);
                emit(c, OP_LOAD_GLOBAL, name_const);
            }
        }
        break;
    }

    case AST_STRING:
    case AST_TEXT_SPLICE: {
        int const_idx = compiler_add_string_constant(c, node->string.text);
        emit(c, OP_TEXT_SPLICE, const_idx);
        break;
    }

    // Arithmetic
    case AST_ADD: compile_binary(c, node, OP_ADD); break;
    case AST_SUB: compile_binary(c, node, OP_SUB); break;
    case AST_MUL: compile_binary(c, node, OP_MUL); break;
    case AST_DIV: compile_binary(c, node, OP_DIV); break;
    case AST_MOD: compile_binary(c, node, OP_MOD); break;

    // Comparison
    case AST_EQ: compile_binary(c, node, OP_EQ); break;
    case AST_NE: compile_binary(c, node, OP_NE); break;
    case AST_LT: compile_binary(c, node, OP_LT); break;
    case AST_GT: compile_binary(c, node, OP_GT); break;
    case AST_LE: compile_binary(c, node, OP_LE); break;
    case AST_GE: compile_binary(c, node, OP_GE); break;

    // Logic
    case AST_AND: compile_binary(c, node, OP_AND); break;
    case AST_OR: compile_binary(c, node, OP_OR); break;

    case AST_NOT:
        compile_expression(c, node->unary.operand);
        emit_simple(c, OP_NOT);
        break;

    case AST_NEG:
        compile_expression(c, node->unary.operand);
        emit_simple(c, OP_NEG);
        break;

    case AST_CALL: {
        // Push arguments
        for (int i = 0; i < node->call.n_args; i++) {
            compile_expression(c, node->call.args[i]);
        }

        // Try local first (for parameters passed by name)
        int slot = compiler_resolve_local(c, node->call.name);
        if (slot != -1) {
            emit(c, OP_LOAD_LOCAL, slot);
            emit(c, OP_CALL, node->call.n_args);
        } else {
            int upvalue = compiler_resolve_upvalue(c, node->call.name);
            if (upvalue != -1) {
                emit(c, OP_LOAD_UPVALUE, upvalue);
                emit(c, OP_CALL, node->call.n_args);
            } else {
                // Global function
                int name_const = compiler_add_string_constant(c, node->call.name);
                emit(c, OP_CALL_GLOBAL, name_const);
                emit(c, OP_PUSH_INT, node->call.n_args); // Arg count follows
            }
        }
        break;
    }

    case AST_ARRAY_GET: {
        // Push array reference
        int slot = compiler_resolve_local(c, node->array_get.name);
        if (slot != -1) {
            emit(c, OP_LOAD_LOCAL, slot);
        } else {
            int name_const = compiler_add_string_constant(c, node->array_get.name);
            emit(c, OP_LOAD_GLOBAL, name_const);
        }
        // Push index
        compile_expression(c, node->array_get.index);
        emit_simple(c, OP_ARRAY_GET);
        break;
    }

    case AST_ARRAY_LEN: {
        int slot = compiler_resolve_local(c, node->array_op.name);
        if (slot != -1) {
            emit(c, OP_LOAD_LOCAL, slot);
        } else {
            int name_const = compiler_add_string_constant(c, node->array_op.name);
            emit(c, OP_LOAD_GLOBAL, name_const);
        }
        emit_simple(c, OP_ARRAY_LEN);
        break;
    }

    case AST_ARRAY_POP:
    case AST_ARRAY_PEEK: {
        int slot = compiler_resolve_local(c, node->array_op.name);
        if (slot != -1) {
            emit(c, OP_LOAD_LOCAL, slot);
        } else {
            int name_const = compiler_add_string_constant(c, node->array_op.name);
            emit(c, OP_LOAD_GLOBAL, name_const);
        }
        emit_simple(c, node->type == AST_ARRAY_POP ? OP_ARRAY_POP : OP_ARRAY_GET);
        // For peek, we need to push -1 as index (last element)
        if (node->type == AST_ARRAY_PEEK) {
            // Actually, let's use a different approach - ARRAY_LEN then subtract 1
            // For now, just use ARRAY_POP semantics
        }
        break;
    }

    case AST_REF: {
        // Reference to a variable name - push the name as a string
        int name_const = compiler_add_string_constant(c, node->var.name);
        emit(c, OP_PUSH_CONST, name_const);
        break;
    }

    default:
        compiler_error(c, "Cannot compile expression of type %s", ast_type_name(node->type));
        break;
    }
}

// ============================================================================
// Statement Compilation
// ============================================================================

static void compile_statement(Compiler *c, AstNode *node) {
    if (!node || c->had_error) return;

    switch (node->type) {
    case AST_LET: {
        // Compile value
        compile_expression(c, node->let.value);

        // Check if variable already exists as local
        int slot = compiler_resolve_local(c, node->let.name);
        if (slot != -1) {
            // Update existing local
            emit(c, OP_STORE_LOCAL, slot);
            emit_simple(c, OP_POP); // Discard result
        } else {
            // Check if it's an upvalue (captured from enclosing scope)
            int upvalue = compiler_resolve_upvalue(c, node->let.name);
            if (upvalue != -1) {
                emit(c, OP_STORE_UPVALUE, upvalue);
                emit_simple(c, OP_POP);
            } else if (c->scope_depth == 0) {
                // At top level, define/update global
                int name_const = compiler_add_string_constant(c, node->let.name);
                emit(c, OP_DEFINE_GLOBAL, name_const);
            } else {
                // Inside a block - always use STORE_GLOBAL to update globals
                // This allows \let inside loops to update outer variables
                int name_const = compiler_add_string_constant(c, node->let.name);
                emit(c, OP_STORE_GLOBAL, name_const);
                emit_simple(c, OP_POP);
            }
        }
        break;
    }

    case AST_LET_ARRAY: {
        // Compile array initializer elements
        AstNode *init = node->let_array.init;
        if (init && init->type == AST_ARRAY_LITERAL) {
            for (int i = 0; i < init->array_lit.n_elements; i++) {
                compile_expression(c, init->array_lit.elements[i]);
            }
            emit(c, OP_ARRAY_NEW, init->array_lit.n_elements);
        } else {
            emit(c, OP_ARRAY_NEW, 0);
        }

        if (c->scope_depth == 0) {
            int name_const = compiler_add_string_constant(c, node->let_array.name);
            emit(c, OP_DEFINE_GLOBAL, name_const);
        } else {
            compiler_add_local(c, node->let_array.name);
        }
        break;
    }

    case AST_ASSIGN: {
        compile_expression(c, node->let.value);

        int slot = compiler_resolve_local(c, node->let.name);
        if (slot != -1) {
            emit(c, OP_STORE_LOCAL, slot);
        } else {
            int upvalue = compiler_resolve_upvalue(c, node->let.name);
            if (upvalue != -1) {
                emit(c, OP_STORE_UPVALUE, upvalue);
            } else {
                int name_const = compiler_add_string_constant(c, node->let.name);
                emit(c, OP_STORE_GLOBAL, name_const);
            }
        }
        emit_simple(c, OP_POP); // Discard result
        break;
    }

    case AST_INC:
    case AST_DEC: {
        OpCode op = (node->type == AST_INC) ? OP_INC_LOCAL : OP_DEC_LOCAL;
        OpCode global_op = (node->type == AST_INC) ? OP_INC_GLOBAL : OP_DEC_GLOBAL;

        int slot = compiler_resolve_local(c, node->var.name);
        if (slot != -1) {
            emit(c, op, slot);
        } else {
            int name_const = compiler_add_string_constant(c, node->var.name);
            emit(c, global_op, name_const);
        }
        // Leave value on stack - will be used or output at top level
        break;
    }

    case AST_IF: {
        // Compile condition
        compile_expression(c, node->if_stmt.condition);

        // Jump over then branch if false
        int then_jump = emit_jump(c, OP_JUMP_IF_FALSE);

        // Compile then branch (no scope - variables persist)
        compile_node(c, node->if_stmt.then_branch);

        if (node->if_stmt.else_branch) {
            // Jump over else branch
            int else_jump = emit_jump(c, OP_JUMP);
            patch_jump(c, then_jump);

            // Compile else branch (no scope - variables persist)
            compile_node(c, node->if_stmt.else_branch);

            patch_jump(c, else_jump);
        } else {
            patch_jump(c, then_jump);
        }
        break;
    }

    case AST_LOOP: {
        // Remember where to loop back to
        int loop_start = current_offset(c);

        // Track this loop for exit_when
        if (c->loop_depth >= 32) {
            compiler_error(c, "Loops nested too deeply");
            break;
        }
        c->loops[c->loop_depth].loop_start = loop_start;
        c->loops[c->loop_depth].exit_jumps = NULL;
        c->loops[c->loop_depth].exit_jump_count = 0;
        c->loops[c->loop_depth].exit_jump_cap = 0;
        c->loop_depth++;

        // Compile body
        compiler_begin_scope(c);
        compile_node(c, node->loop.body);
        compiler_end_scope(c);

        // Loop back
        emit_loop(c, loop_start);

        // Patch all exit jumps
        c->loop_depth--;
        for (int i = 0; i < c->loops[c->loop_depth].exit_jump_count; i++) {
            patch_jump(c, c->loops[c->loop_depth].exit_jumps[i]);
        }
        free(c->loops[c->loop_depth].exit_jumps);
        break;
    }

    case AST_EXIT_WHEN: {
        if (c->loop_depth == 0) {
            compiler_error(c, "\\exit_when outside of loop");
            break;
        }

        // Compile condition
        compile_expression(c, node->exit_when.condition);

        // Jump out if true
        int exit_jump = emit_jump(c, OP_JUMP_IF_TRUE);

        // Record for patching
        int loop_idx = c->loop_depth - 1;
        if (c->loops[loop_idx].exit_jump_count >= c->loops[loop_idx].exit_jump_cap) {
            c->loops[loop_idx].exit_jump_cap = c->loops[loop_idx].exit_jump_cap * 2 + 4;
            c->loops[loop_idx].exit_jumps = realloc(c->loops[loop_idx].exit_jumps,
                                                    c->loops[loop_idx].exit_jump_cap * sizeof(int));
        }
        c->loops[loop_idx].exit_jumps[c->loops[loop_idx].exit_jump_count++] = exit_jump;
        break;
    }

    case AST_LAMBDA: {
        // Create new compiler for the lambda
        Compiler lambda_compiler = {0};
        lambda_compiler.enclosing = (struct Compiler *)c;
        lambda_compiler.scope_depth = 0;
        lambda_compiler.local_count = 0;
        lambda_compiler.loop_depth = 0;

        // Create function object
        CompiledFunc *func = calloc(1, sizeof(CompiledFunc));
        func->name = strdup(node->lambda.name);
        func->is_computational = node->lambda.is_computational;
        func->source_line = node->line;
        func->code_cap = 32;
        func->code = calloc(func->code_cap, sizeof(Instruction));
        func->constants_cap = 8;
        func->constants = calloc(func->constants_cap, sizeof(func->constants[0]));

        lambda_compiler.function = func;

        // Add parameters as locals
        func->n_params = node->lambda.n_params;
        for (int i = 0; i < node->lambda.n_params; i++) {
            compiler_add_local(&lambda_compiler, node->lambda.params[i]);
        }

        // Compile body
        compiler_begin_scope(&lambda_compiler);
        compile_node(&lambda_compiler, node->lambda.body);
        compiler_end_scope(&lambda_compiler);

        // Ensure function returns
        emit_simple(&lambda_compiler, OP_RETURN);

        // Copy upvalue info to function
        if (func->n_upvalues > 0) {
            func->upvalues = calloc(func->n_upvalues, sizeof(func->upvalues[0]));
            for (int i = 0; i < func->n_upvalues; i++) {
                func->upvalues[i].index = lambda_compiler.upvalues[i].index;
                func->upvalues[i].is_local = lambda_compiler.upvalues[i].is_local;
            }
        }

        // Free local names
        for (int i = 0; i < lambda_compiler.local_count; i++) {
            free(lambda_compiler.locals[i].name);
        }

        // Add function to constants and emit closure instruction
        int func_const = compiler_add_func_constant(c, func);
        emit(c, OP_CLOSURE, func_const);

        // Emit upvalue descriptors
        for (int i = 0; i < func->n_upvalues; i++) {
            emit(c, func->upvalues[i].is_local ? 1 : 0, func->upvalues[i].index);
        }

        // Define in current scope
        if (c->scope_depth == 0) {
            int name_const = compiler_add_string_constant(c, node->lambda.name);
            emit(c, OP_DEFINE_GLOBAL, name_const);
        } else {
            compiler_add_local(c, node->lambda.name);
        }
        break;
    }

    case AST_RETURN: {
        if (node->ret.value) {
            compile_expression(c, node->ret.value);
            emit_simple(c, OP_RETURN_VALUE);
        } else {
            emit_simple(c, OP_RETURN);
        }
        break;
    }

    case AST_ARRAY_SET: {
        // Push array, index, value
        int slot = compiler_resolve_local(c, node->array_set.name);
        if (slot != -1) {
            emit(c, OP_LOAD_LOCAL, slot);
        } else {
            int name_const = compiler_add_string_constant(c, node->array_set.name);
            emit(c, OP_LOAD_GLOBAL, name_const);
        }
        compile_expression(c, node->array_set.index);
        compile_expression(c, node->array_set.value);
        emit_simple(c, OP_ARRAY_SET);
        break;
    }

    case AST_ARRAY_PUSH: {
        int slot = compiler_resolve_local(c, node->array_push.name);
        if (slot != -1) {
            emit(c, OP_LOAD_LOCAL, slot);
        } else {
            int name_const = compiler_add_string_constant(c, node->array_push.name);
            emit(c, OP_LOAD_GLOBAL, name_const);
        }
        compile_expression(c, node->array_push.value);
        emit_simple(c, OP_ARRAY_PUSH);
        break;
    }

    case AST_BLOCK: {
        for (int i = 0; i < node->block.n_stmts; i++) {
            compile_node(c, node->block.stmts[i]);
        }
        break;
    }

    case AST_EXPR_STMT: {
        compile_expression(c, node->expr_stmt.expr);
        // Don't pop - expression result might be needed
        // Actually, for statement context, we should pop
        // But text splices don't push anything... this needs refinement
        break;
    }

    case AST_TEXT_SPLICE: {
        int const_idx = compiler_add_string_constant(c, node->string.text);
        emit(c, OP_TEXT_SPLICE, const_idx);
        break;
    }

    default:
        compiler_error(c, "Cannot compile statement of type %s", ast_type_name(node->type));
        break;
    }
}

// Returns true if the node type leaves a value on the stack when compiled
static bool node_produces_value(AstType type) {
    switch (type) {
    // Expressions that produce values
    case AST_INT:
    case AST_VAR:
    case AST_ADD:
    case AST_SUB:
    case AST_MUL:
    case AST_DIV:
    case AST_MOD:
    case AST_NEG:
    case AST_EQ:
    case AST_NE:
    case AST_LT:
    case AST_GT:
    case AST_LE:
    case AST_GE:
    case AST_AND:
    case AST_OR:
    case AST_NOT:
    case AST_CALL:
    case AST_ARRAY_GET:
    case AST_ARRAY_LEN:
    case AST_ARRAY_POP:
    case AST_ARRAY_PEEK:
    case AST_REF:
    // These statements also produce values
    case AST_INC:
    case AST_DEC: return true;
    default: return false;
    }
}

static void compile_node(Compiler *c, AstNode *node) {
    if (!node) return;

    if (node->type == AST_BLOCK) {
        compile_statement(c, node);
    } else if (node->type >= AST_LET && node->type <= AST_TEXT_SPLICE) {
        compile_statement(c, node);
    } else {
        compile_expression(c, node);
    }
}

// Compile a node at top level - emit OUTPUT for value-producing nodes
static void compile_top_level(Compiler *c, AstNode *node) {
    if (!node) return;

    if (node->type == AST_BLOCK) {
        // For blocks at top level, compile each statement with output
        for (int i = 0; i < node->block.n_stmts; i++) {
            compile_top_level(c, node->block.stmts[i]);
        }
    } else if (node->type == AST_EXPR_STMT) {
        // Expression statement - compile the inner expression and output if it produces a value
        compile_expression(c, node->expr_stmt.expr);
        if (node_produces_value(node->expr_stmt.expr->type)) {
            emit_simple(c, OP_OUTPUT);
        }
    } else {
        compile_node(c, node);
        // Emit OUTPUT for value-producing nodes at top level
        if (node_produces_value(node->type)) {
            emit_simple(c, OP_OUTPUT);
        }
    }
}

// ============================================================================
// Public Interface
// ============================================================================

CompileResult compile(AstNode *ast) {
    CompileResult result = {0};

    if (!ast) {
        snprintf(result.error_msg, sizeof(result.error_msg), "NULL AST");
        return result;
    }

    // Create main function
    CompiledFunc *main_func = calloc(1, sizeof(CompiledFunc));
    main_func->name = NULL; // Top-level has no name
    main_func->code_cap = 64;
    main_func->code = calloc(main_func->code_cap, sizeof(Instruction));
    main_func->constants_cap = 16;
    main_func->constants = calloc(main_func->constants_cap, sizeof(main_func->constants[0]));

    // Initialize compiler
    Compiler c = {0};
    c.function = main_func;
    c.scope_depth = 0;
    c.local_count = 0;
    c.loop_depth = 0;
    c.enclosing = NULL;

    // Compile the AST at top level (with output for value-producing expressions)
    compile_top_level(&c, ast);

    // Add halt instruction
    emit_simple(&c, OP_HALT);

    // Free local names
    for (int i = 0; i < c.local_count; i++) {
        free(c.locals[i].name);
    }

    if (c.had_error) {
        compiled_func_free(main_func);
        snprintf(result.error_msg, sizeof(result.error_msg), "%s", c.error_msg);
        return result;
    }

    result.main = main_func;
    result.success = true;

    return result;
}

void compiled_func_free(CompiledFunc *func) {
    if (!func) return;

    free(func->name);
    free(func->code);

    for (int i = 0; i < func->n_locals; i++) {
        // Local names are freed during compilation
    }
    free(func->local_names);

    for (int i = 0; i < func->n_constants; i++) {
        if (func->constants[i].type == CONST_STRING) {
            free(func->constants[i].string);
        } else if (func->constants[i].type == CONST_FUNC) {
            compiled_func_free(func->constants[i].func);
        }
    }
    free(func->constants);

    free(func->upvalues);
    free(func);
}

void compile_result_free(CompileResult *result) {
    if (!result) return;

    compiled_func_free(result->main);

    for (int i = 0; i < result->n_funcs; i++) {
        compiled_func_free(result->funcs[i]);
    }
    free(result->funcs);
}

// ============================================================================
// Disassembly
// ============================================================================

int disassemble_instruction(CompiledFunc *func, int offset, char *buf, int buf_size) {
    if (offset >= func->code_len) {
        snprintf(buf, buf_size, "(end)");
        return 0;
    }

    Instruction *inst = &func->code[offset];
    const OpCodeInfo *info = &opcode_info[inst->op];

    if (info->operand_bytes > 0) {
        // Instruction with operand
        if (inst->op == OP_PUSH_CONST || inst->op == OP_TEXT_SPLICE || inst->op == OP_LOAD_GLOBAL ||
            inst->op == OP_STORE_GLOBAL || inst->op == OP_DEFINE_GLOBAL ||
            inst->op == OP_CALL_GLOBAL) {
            // Show constant value
            if (inst->operand < func->n_constants) {
                if (func->constants[inst->operand].type == CONST_STRING) {
                    snprintf(buf, buf_size, "%04d  %-16s %d (\"%s\")", offset, info->name,
                             inst->operand, func->constants[inst->operand].string);
                } else if (func->constants[inst->operand].type == CONST_FUNC) {
                    snprintf(buf, buf_size, "%04d  %-16s %d (<fn %s>)", offset, info->name,
                             inst->operand,
                             func->constants[inst->operand].func->name
                                 ? func->constants[inst->operand].func->name
                                 : "main");
                } else {
                    snprintf(buf, buf_size, "%04d  %-16s %d", offset, info->name, inst->operand);
                }
            } else {
                snprintf(buf, buf_size, "%04d  %-16s %d", offset, info->name, inst->operand);
            }
        } else {
            snprintf(buf, buf_size, "%04d  %-16s %d", offset, info->name, inst->operand);
        }
    } else {
        snprintf(buf, buf_size, "%04d  %-16s", offset, info->name);
    }

    return 1;
}

void disassemble_print(CompiledFunc *func) {
    fprintf(stderr, "== %s ==\n", func->name ? func->name : "<main>");

    char buf[256];
    for (int offset = 0; offset < func->code_len; offset++) {
        disassemble_instruction(func, offset, buf, sizeof(buf));
        fprintf(stderr, "%s\n", buf);
    }

    // Disassemble nested functions
    for (int i = 0; i < func->n_constants; i++) {
        if (func->constants[i].type == CONST_FUNC) {
            fprintf(stderr, "\n");
            disassemble_print(func->constants[i].func);
        }
    }
}

char *disassemble(CompiledFunc *func) {
    // Allocate a buffer
    size_t cap = 4096;
    char *result = malloc(cap);
    size_t len = 0;

    len += snprintf(result + len, cap - len, "== %s ==\n", func->name ? func->name : "<main>");

    char buf[256];
    for (int offset = 0; offset < func->code_len; offset++) {
        disassemble_instruction(func, offset, buf, sizeof(buf));
        len += snprintf(result + len, cap - len, "%s\n", buf);

        if (len + 256 > cap) {
            cap *= 2;
            result = realloc(result, cap);
        }
    }

    return result;
}

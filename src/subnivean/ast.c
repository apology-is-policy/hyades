// ast.c - Subnivean AST implementation
//
// AST construction, printing, and memory management.

#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// AST Type Names
// ============================================================================

static const char *ast_type_names[AST_COUNT] = {
    [AST_INT] = "INT",
    [AST_STRING] = "STRING",
    [AST_VAR] = "VAR",
    [AST_ADD] = "ADD",
    [AST_SUB] = "SUB",
    [AST_MUL] = "MUL",
    [AST_DIV] = "DIV",
    [AST_MOD] = "MOD",
    [AST_NEG] = "NEG",
    [AST_EQ] = "EQ",
    [AST_NE] = "NE",
    [AST_LT] = "LT",
    [AST_GT] = "GT",
    [AST_LE] = "LE",
    [AST_GE] = "GE",
    [AST_AND] = "AND",
    [AST_OR] = "OR",
    [AST_NOT] = "NOT",
    [AST_LET] = "LET",
    [AST_LET_ARRAY] = "LET_ARRAY",
    [AST_ASSIGN] = "ASSIGN",
    [AST_INC] = "INC",
    [AST_DEC] = "DEC",
    [AST_IF] = "IF",
    [AST_LOOP] = "LOOP",
    [AST_EXIT_WHEN] = "EXIT_WHEN",
    [AST_ENUMERATE] = "ENUMERATE",
    [AST_LAMBDA] = "LAMBDA",
    [AST_CALL] = "CALL",
    [AST_RETURN] = "RETURN",
    [AST_REF] = "REF",
    [AST_ARRAY_LITERAL] = "ARRAY_LITERAL",
    [AST_ARRAY_GET] = "ARRAY_GET",
    [AST_ARRAY_SET] = "ARRAY_SET",
    [AST_ARRAY_LEN] = "ARRAY_LEN",
    [AST_ARRAY_PUSH] = "ARRAY_PUSH",
    [AST_ARRAY_POP] = "ARRAY_POP",
    [AST_ARRAY_PEEK] = "ARRAY_PEEK",
    [AST_BLOCK] = "BLOCK",
    [AST_EXPR_STMT] = "EXPR_STMT",
    [AST_TEXT_SPLICE] = "TEXT_SPLICE",
};

const char *ast_type_name(AstType type) {
    if (type >= 0 && type < AST_COUNT && ast_type_names[type]) {
        return ast_type_names[type];
    }
    return "UNKNOWN";
}

// ============================================================================
// AST Construction
// ============================================================================

AstNode *ast_new(AstType type, int line, int col) {
    AstNode *node = calloc(1, sizeof(AstNode));
    if (!node) return NULL;
    node->type = type;
    node->line = line;
    node->col = col;
    return node;
}

AstNode *ast_int(int32_t value, int line, int col) {
    AstNode *node = ast_new(AST_INT, line, col);
    if (node) {
        node->int_val = value;
    }
    return node;
}

AstNode *ast_string(const char *text, int length, int line, int col) {
    AstNode *node = ast_new(AST_STRING, line, col);
    if (node) {
        node->string.text = malloc(length + 1);
        if (node->string.text) {
            memcpy(node->string.text, text, length);
            node->string.text[length] = '\0';
            node->string.length = length;
        }
    }
    return node;
}

AstNode *ast_var(const char *name, int line, int col) {
    AstNode *node = ast_new(AST_VAR, line, col);
    if (node) {
        node->var.name = strdup(name);
    }
    return node;
}

AstNode *ast_binary(AstType type, AstNode *left, AstNode *right, int line, int col) {
    AstNode *node = ast_new(type, line, col);
    if (node) {
        node->binary.left = left;
        node->binary.right = right;
    }
    return node;
}

AstNode *ast_unary(AstType type, AstNode *operand, int line, int col) {
    AstNode *node = ast_new(type, line, col);
    if (node) {
        node->unary.operand = operand;
    }
    return node;
}

AstNode *ast_let(const char *name, AstNode *value, int line, int col) {
    AstNode *node = ast_new(AST_LET, line, col);
    if (node) {
        node->let.name = strdup(name);
        node->let.value = value;
    }
    return node;
}

AstNode *ast_assign(const char *name, AstNode *value, int line, int col) {
    AstNode *node = ast_new(AST_ASSIGN, line, col);
    if (node) {
        node->let.name = strdup(name);
        node->let.value = value;
    }
    return node;
}

AstNode *ast_inc(const char *name, int line, int col) {
    AstNode *node = ast_new(AST_INC, line, col);
    if (node) {
        node->var.name = strdup(name);
    }
    return node;
}

AstNode *ast_dec(const char *name, int line, int col) {
    AstNode *node = ast_new(AST_DEC, line, col);
    if (node) {
        node->var.name = strdup(name);
    }
    return node;
}

AstNode *ast_if(AstNode *cond, AstNode *then_b, AstNode *else_b, int line, int col) {
    AstNode *node = ast_new(AST_IF, line, col);
    if (node) {
        node->if_stmt.condition = cond;
        node->if_stmt.then_branch = then_b;
        node->if_stmt.else_branch = else_b;
    }
    return node;
}

AstNode *ast_loop(AstNode *body, int line, int col) {
    AstNode *node = ast_new(AST_LOOP, line, col);
    if (node) {
        node->loop.body = body;
    }
    return node;
}

AstNode *ast_exit_when(AstNode *cond, int line, int col) {
    AstNode *node = ast_new(AST_EXIT_WHEN, line, col);
    if (node) {
        node->exit_when.condition = cond;
    }
    return node;
}

AstNode *ast_lambda(const char *name, char **params, int n_params, AstNode *body,
                    bool is_computational, int line, int col) {
    AstNode *node = ast_new(AST_LAMBDA, line, col);
    if (node) {
        node->lambda.name = strdup(name);
        node->lambda.params = params; // Takes ownership
        node->lambda.n_params = n_params;
        node->lambda.body = body;
        node->lambda.is_computational = is_computational;
    }
    return node;
}

AstNode *ast_call(const char *name, AstNode **args, int n_args, int line, int col) {
    AstNode *node = ast_new(AST_CALL, line, col);
    if (node) {
        node->call.name = strdup(name);
        node->call.args = args; // Takes ownership
        node->call.n_args = n_args;
    }
    return node;
}

AstNode *ast_return(AstNode *value, int line, int col) {
    AstNode *node = ast_new(AST_RETURN, line, col);
    if (node) {
        node->ret.value = value;
    }
    return node;
}

AstNode *ast_block(int line, int col) {
    AstNode *node = ast_new(AST_BLOCK, line, col);
    if (node) {
        node->block.capacity = 8;
        node->block.stmts = calloc(node->block.capacity, sizeof(AstNode *));
        node->block.n_stmts = 0;
    }
    return node;
}

void ast_block_append(AstNode *block, AstNode *stmt) {
    if (!block || block->type != AST_BLOCK || !stmt) return;

    if (block->block.n_stmts >= block->block.capacity) {
        block->block.capacity *= 2;
        block->block.stmts = realloc(block->block.stmts, block->block.capacity * sizeof(AstNode *));
    }
    block->block.stmts[block->block.n_stmts++] = stmt;
}

AstNode *ast_text_splice(const char *text, int length, int line, int col) {
    AstNode *node = ast_new(AST_TEXT_SPLICE, line, col);
    if (node) {
        node->string.text = malloc(length + 1);
        if (node->string.text) {
            memcpy(node->string.text, text, length);
            node->string.text[length] = '\0';
            node->string.length = length;
        }
    }
    return node;
}

// ============================================================================
// AST Memory Management
// ============================================================================

void ast_free(AstNode *node) {
    if (!node) return;

    switch (node->type) {
    case AST_INT:
        // Nothing to free
        break;

    case AST_STRING:
    case AST_TEXT_SPLICE: free(node->string.text); break;

    case AST_VAR:
    case AST_INC:
    case AST_DEC:
    case AST_REF:
    case AST_ARRAY_LEN:
    case AST_ARRAY_POP:
    case AST_ARRAY_PEEK: free(node->var.name); break;

    case AST_LET:
    case AST_ASSIGN:
        free(node->let.name);
        ast_free(node->let.value);
        break;

    case AST_LET_ARRAY:
        free(node->let_array.name);
        ast_free(node->let_array.init);
        break;

    case AST_ADD:
    case AST_SUB:
    case AST_MUL:
    case AST_DIV:
    case AST_MOD:
    case AST_EQ:
    case AST_NE:
    case AST_LT:
    case AST_GT:
    case AST_LE:
    case AST_GE:
    case AST_AND:
    case AST_OR:
        ast_free(node->binary.left);
        ast_free(node->binary.right);
        break;

    case AST_NEG:
    case AST_NOT: ast_free(node->unary.operand); break;

    case AST_IF:
        ast_free(node->if_stmt.condition);
        ast_free(node->if_stmt.then_branch);
        ast_free(node->if_stmt.else_branch);
        break;

    case AST_LOOP: ast_free(node->loop.body); break;

    case AST_EXIT_WHEN: ast_free(node->exit_when.condition); break;

    case AST_ENUMERATE:
        free(node->enumerate.array_name);
        free(node->enumerate.index_var);
        free(node->enumerate.element_var);
        ast_free(node->enumerate.body);
        break;

    case AST_LAMBDA:
        free(node->lambda.name);
        for (int i = 0; i < node->lambda.n_params; i++) {
            free(node->lambda.params[i]);
        }
        free(node->lambda.params);
        ast_free(node->lambda.body);
        break;

    case AST_CALL:
        free(node->call.name);
        for (int i = 0; i < node->call.n_args; i++) {
            ast_free(node->call.args[i]);
        }
        free(node->call.args);
        break;

    case AST_RETURN: ast_free(node->ret.value); break;

    case AST_ARRAY_LITERAL:
        for (int i = 0; i < node->array_lit.n_elements; i++) {
            ast_free(node->array_lit.elements[i]);
        }
        free(node->array_lit.elements);
        break;

    case AST_ARRAY_GET:
        free(node->array_get.name);
        ast_free(node->array_get.index);
        break;

    case AST_ARRAY_SET:
        free(node->array_set.name);
        ast_free(node->array_set.index);
        ast_free(node->array_set.value);
        break;

    case AST_ARRAY_PUSH:
        free(node->array_push.name);
        ast_free(node->array_push.value);
        break;

    case AST_BLOCK:
        for (int i = 0; i < node->block.n_stmts; i++) {
            ast_free(node->block.stmts[i]);
        }
        free(node->block.stmts);
        break;

    case AST_EXPR_STMT: ast_free(node->expr_stmt.expr); break;

    default:
        // Unknown type - just free the node
        break;
    }

    free(node);
}

// ============================================================================
// AST Printing (for debugging)
// ============================================================================

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        fprintf(stderr, "  ");
    }
}

void ast_print(const AstNode *node, int indent) {
    if (!node) {
        print_indent(indent);
        fprintf(stderr, "(null)\n");
        return;
    }

    print_indent(indent);
    fprintf(stderr, "%s", ast_type_name(node->type));

    switch (node->type) {
    case AST_INT: fprintf(stderr, "(%d)\n", node->int_val); break;

    case AST_STRING:
    case AST_TEXT_SPLICE:
        fprintf(stderr, "(\"%.*s\")\n", node->string.length > 20 ? 20 : node->string.length,
                node->string.text);
        break;

    case AST_VAR:
    case AST_INC:
    case AST_DEC:
    case AST_REF:
    case AST_ARRAY_LEN:
    case AST_ARRAY_POP:
    case AST_ARRAY_PEEK: fprintf(stderr, "<%s>\n", node->var.name); break;

    case AST_LET:
    case AST_ASSIGN:
        fprintf(stderr, "<%s>\n", node->let.name);
        ast_print(node->let.value, indent + 1);
        break;

    case AST_ADD:
    case AST_SUB:
    case AST_MUL:
    case AST_DIV:
    case AST_MOD:
    case AST_EQ:
    case AST_NE:
    case AST_LT:
    case AST_GT:
    case AST_LE:
    case AST_GE:
    case AST_AND:
    case AST_OR:
        fprintf(stderr, "\n");
        ast_print(node->binary.left, indent + 1);
        ast_print(node->binary.right, indent + 1);
        break;

    case AST_NEG:
    case AST_NOT:
        fprintf(stderr, "\n");
        ast_print(node->unary.operand, indent + 1);
        break;

    case AST_IF:
        fprintf(stderr, "\n");
        print_indent(indent + 1);
        fprintf(stderr, "condition:\n");
        ast_print(node->if_stmt.condition, indent + 2);
        print_indent(indent + 1);
        fprintf(stderr, "then:\n");
        ast_print(node->if_stmt.then_branch, indent + 2);
        if (node->if_stmt.else_branch) {
            print_indent(indent + 1);
            fprintf(stderr, "else:\n");
            ast_print(node->if_stmt.else_branch, indent + 2);
        }
        break;

    case AST_LOOP:
        fprintf(stderr, "\n");
        ast_print(node->loop.body, indent + 1);
        break;

    case AST_EXIT_WHEN:
        fprintf(stderr, "\n");
        ast_print(node->exit_when.condition, indent + 1);
        break;

    case AST_LAMBDA:
        fprintf(stderr, "<%s>[", node->lambda.name);
        for (int i = 0; i < node->lambda.n_params; i++) {
            if (i > 0) fprintf(stderr, ",");
            fprintf(stderr, "%s", node->lambda.params[i]);
        }
        fprintf(stderr, "]%s\n", node->lambda.is_computational ? "#" : "");
        ast_print(node->lambda.body, indent + 1);
        break;

    case AST_CALL:
        fprintf(stderr, "<%s>[%d args]\n", node->call.name, node->call.n_args);
        for (int i = 0; i < node->call.n_args; i++) {
            ast_print(node->call.args[i], indent + 1);
        }
        break;

    case AST_RETURN:
        fprintf(stderr, "\n");
        if (node->ret.value) {
            ast_print(node->ret.value, indent + 1);
        }
        break;

    case AST_BLOCK:
        fprintf(stderr, " (%d stmts)\n", node->block.n_stmts);
        for (int i = 0; i < node->block.n_stmts; i++) {
            ast_print(node->block.stmts[i], indent + 1);
        }
        break;

    case AST_ARRAY_GET:
        fprintf(stderr, "<%s>\n", node->array_get.name);
        ast_print(node->array_get.index, indent + 1);
        break;

    case AST_ARRAY_SET:
        fprintf(stderr, "<%s>\n", node->array_set.name);
        print_indent(indent + 1);
        fprintf(stderr, "index:\n");
        ast_print(node->array_set.index, indent + 2);
        print_indent(indent + 1);
        fprintf(stderr, "value:\n");
        ast_print(node->array_set.value, indent + 2);
        break;

    default: fprintf(stderr, "\n"); break;
    }
}

// ============================================================================
// AST Cloning (deep copy)
// ============================================================================

static char *strdup_safe(const char *s) {
    return s ? strdup(s) : NULL;
}

AstNode *ast_clone(const AstNode *node) {
    if (!node) return NULL;

    AstNode *clone = ast_new(node->type, node->line, node->col);
    if (!clone) return NULL;

    switch (node->type) {
    case AST_INT: clone->int_val = node->int_val; break;

    case AST_STRING:
    case AST_TEXT_SPLICE:
        clone->string.text = malloc(node->string.length + 1);
        if (clone->string.text) {
            memcpy(clone->string.text, node->string.text, node->string.length + 1);
            clone->string.length = node->string.length;
        }
        break;

    case AST_VAR:
    case AST_INC:
    case AST_DEC:
    case AST_REF:
    case AST_ARRAY_LEN:
    case AST_ARRAY_POP:
    case AST_ARRAY_PEEK: clone->var.name = strdup_safe(node->var.name); break;

    case AST_LET:
    case AST_ASSIGN:
        clone->let.name = strdup_safe(node->let.name);
        clone->let.value = ast_clone(node->let.value);
        break;

    case AST_ADD:
    case AST_SUB:
    case AST_MUL:
    case AST_DIV:
    case AST_MOD:
    case AST_EQ:
    case AST_NE:
    case AST_LT:
    case AST_GT:
    case AST_LE:
    case AST_GE:
    case AST_AND:
    case AST_OR:
        clone->binary.left = ast_clone(node->binary.left);
        clone->binary.right = ast_clone(node->binary.right);
        break;

    case AST_NEG:
    case AST_NOT: clone->unary.operand = ast_clone(node->unary.operand); break;

    case AST_IF:
        clone->if_stmt.condition = ast_clone(node->if_stmt.condition);
        clone->if_stmt.then_branch = ast_clone(node->if_stmt.then_branch);
        clone->if_stmt.else_branch = ast_clone(node->if_stmt.else_branch);
        break;

    case AST_LOOP: clone->loop.body = ast_clone(node->loop.body); break;

    case AST_EXIT_WHEN: clone->exit_when.condition = ast_clone(node->exit_when.condition); break;

    case AST_LAMBDA:
        clone->lambda.name = strdup_safe(node->lambda.name);
        clone->lambda.n_params = node->lambda.n_params;
        clone->lambda.is_computational = node->lambda.is_computational;
        if (node->lambda.n_params > 0) {
            clone->lambda.params = calloc(node->lambda.n_params, sizeof(char *));
            for (int i = 0; i < node->lambda.n_params; i++) {
                clone->lambda.params[i] = strdup_safe(node->lambda.params[i]);
            }
        }
        clone->lambda.body = ast_clone(node->lambda.body);
        break;

    case AST_CALL:
        clone->call.name = strdup_safe(node->call.name);
        clone->call.n_args = node->call.n_args;
        if (node->call.n_args > 0) {
            clone->call.args = calloc(node->call.n_args, sizeof(AstNode *));
            for (int i = 0; i < node->call.n_args; i++) {
                clone->call.args[i] = ast_clone(node->call.args[i]);
            }
        }
        break;

    case AST_RETURN: clone->ret.value = ast_clone(node->ret.value); break;

    case AST_BLOCK:
        clone->block.capacity = node->block.n_stmts > 0 ? node->block.n_stmts : 8;
        clone->block.stmts = calloc(clone->block.capacity, sizeof(AstNode *));
        clone->block.n_stmts = node->block.n_stmts;
        for (int i = 0; i < node->block.n_stmts; i++) {
            clone->block.stmts[i] = ast_clone(node->block.stmts[i]);
        }
        break;

    case AST_ARRAY_GET:
        clone->array_get.name = strdup_safe(node->array_get.name);
        clone->array_get.index = ast_clone(node->array_get.index);
        break;

    case AST_ARRAY_SET:
        clone->array_set.name = strdup_safe(node->array_set.name);
        clone->array_set.index = ast_clone(node->array_set.index);
        clone->array_set.value = ast_clone(node->array_set.value);
        break;

    case AST_ARRAY_PUSH:
        clone->array_push.name = strdup_safe(node->array_push.name);
        clone->array_push.value = ast_clone(node->array_push.value);
        break;

    default: break;
    }

    return clone;
}

// calc_parser.c - Parser for Hyades Computational Expressions

#include "calc_parser.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// AST Construction
// ============================================================================

static AstNode *alloc_node(AstNodeType type) {
    AstNode *node = calloc(1, sizeof(AstNode));
    node->type = type;
    return node;
}

AstNode *calc_ast_int_lit(int64_t value) {
    AstNode *n = alloc_node(AST_INT_LIT);
    n->int_value = value;
    return n;
}

AstNode *calc_ast_string_lit(const char *value) {
    AstNode *n = alloc_node(AST_STRING_LIT);
    n->string_value = strdup(value);
    return n;
}

AstNode *calc_ast_valueof(const char *name, AstNode *index) {
    AstNode *n = alloc_node(AST_VALUEOF);
    n->var.name = strdup(name);
    n->var.index = index;
    return n;
}

AstNode *calc_ast_recall(const char *name, AstNode *index) {
    AstNode *n = alloc_node(AST_RECALL);
    n->var.name = strdup(name);
    n->var.index = index;
    return n;
}

AstNode *calc_ast_recall_call(const char *name, AstNode **args, int n_args) {
    AstNode *n = alloc_node(AST_RECALL);
    n->call.name = strdup(name);
    n->call.target = NULL;
    n->call.args = args;
    n->call.n_args = n_args;
    return n;
}

// Indirect recall: \recall<expr> where expr evaluates to a name
AstNode *calc_ast_recall_indirect(AstNode *target, AstNode *index) {
    AstNode *n = alloc_node(AST_RECALL);
    n->call.name = NULL;
    n->call.target = target;
    // For simple indirect (no args), store index in args[0] if present
    if (index) {
        n->call.args = calloc(1, sizeof(AstNode *));
        n->call.args[0] = index;
        n->call.n_args = -1; // -1 signals "indirect array access"
    } else {
        n->call.args = NULL;
        n->call.n_args = -2; // -2 signals "indirect simple lookup"
    }
    return n;
}

// Indirect recall with call: \recall<expr>[args...]
AstNode *calc_ast_recall_call_indirect(AstNode *target, AstNode **args, int n_args) {
    AstNode *n = alloc_node(AST_RECALL);
    n->call.name = NULL;
    n->call.target = target;
    n->call.args = args;
    n->call.n_args = n_args; // Positive = function call with args
    return n;
}

AstNode *calc_ast_ref(const char *name) {
    AstNode *n = alloc_node(AST_REF);
    n->var.name = strdup(name);
    n->var.index = NULL;
    return n;
}

AstNode *calc_ast_let(const char *name, bool is_array, AstNode *value) {
    AstNode *n = alloc_node(AST_LET);
    n->binding.name = strdup(name);
    n->binding.is_array = is_array;
    n->binding.cl_type = CL_TYPE_INFERRED; // Legacy syntax - no type annotation
    n->binding.value = value;
    return n;
}

AstNode *calc_ast_assign(const char *name, bool is_array, AstNode *value) {
    AstNode *n = alloc_node(AST_ASSIGN);
    n->binding.name = strdup(name);
    n->binding.is_array = is_array;
    n->binding.cl_type = CL_TYPE_INFERRED; // Legacy syntax - no type annotation
    n->binding.value = value;
    return n;
}

AstNode *calc_ast_inc_dyn(const char *name, AstNode *target, bool is_deref) {
    AstNode *n = alloc_node(AST_INC);
    n->collection.name = name ? strdup(name) : NULL;
    n->collection.target = target;
    n->collection.key = NULL;
    n->collection.value = NULL;
    n->collection.is_deref = is_deref;
    return n;
}

AstNode *calc_ast_inc(const char *name) {
    return calc_ast_inc_dyn(name, NULL, false);
}

AstNode *calc_ast_dec_dyn(const char *name, AstNode *target, bool is_deref) {
    AstNode *n = alloc_node(AST_DEC);
    n->collection.name = name ? strdup(name) : NULL;
    n->collection.target = target;
    n->collection.key = NULL;
    n->collection.value = NULL;
    n->collection.is_deref = is_deref;
    return n;
}

AstNode *calc_ast_dec(const char *name) {
    return calc_ast_dec_dyn(name, NULL, false);
}

AstNode *calc_ast_setelement(const char *name, AstNode *index, AstNode *value) {
    AstNode *n = alloc_node(AST_SETELEMENT);
    n->setelement.array_name = strdup(name);
    n->setelement.target = NULL;
    n->setelement.index = index;
    n->setelement.value = value;
    return n;
}

AstNode *calc_ast_setelement_indirect(AstNode *target, AstNode *index, AstNode *value) {
    AstNode *n = alloc_node(AST_SETELEMENT);
    n->setelement.array_name = NULL;
    n->setelement.target = target;
    n->setelement.index = index;
    n->setelement.value = value;
    return n;
}

AstNode *calc_ast_binary(AstNodeType type, AstNode *left, AstNode *right) {
    AstNode *n = alloc_node(type);
    n->binary.left = left;
    n->binary.right = right;
    return n;
}

AstNode *calc_ast_unary(AstNodeType type, AstNode *operand) {
    AstNode *n = alloc_node(type);
    n->unary.operand = operand;
    return n;
}

AstNode *calc_ast_if(AstNode *cond, AstNode *then_branch, AstNode *else_branch) {
    AstNode *n = alloc_node(AST_IF);
    n->if_node.condition = cond;
    n->if_node.then_branch = then_branch;
    n->if_node.else_branch = else_branch;
    return n;
}

AstNode *calc_ast_loop(AstNode *body) {
    AstNode *n = alloc_node(AST_LOOP);
    n->loop.body = body;
    return n;
}

AstNode *calc_ast_exit_when(AstNode *cond) {
    AstNode *n = alloc_node(AST_EXIT_WHEN);
    n->control.value = cond;
    return n;
}

AstNode *calc_ast_return(AstNode *value) {
    AstNode *n = alloc_node(AST_RETURN);
    n->control.value = value;
    return n;
}

AstNode *calc_ast_array_lit(AstNode **elements, int n_elements) {
    AstNode *n = alloc_node(AST_ARRAY_LIT);
    n->array.elements = elements;
    n->array.n_elements = n_elements;
    return n;
}

AstNode *calc_ast_string_array_lit(char **strings, int n_strings) {
    AstNode *n = alloc_node(AST_STRING_ARRAY_LIT);
    n->string_array.strings = strings;
    n->string_array.n_strings = n_strings;
    return n;
}

AstNode *calc_ast_len(AstNode *target) {
    AstNode *n = alloc_node(AST_LEN);
    n->unary.operand = target;
    return n;
}

AstNode *calc_ast_copyarray(const char *dest_name, AstNode *source) {
    AstNode *n = alloc_node(AST_COPYARRAY);
    n->copyarray.dest_name = strdup(dest_name);
    n->copyarray.source = source;
    return n;
}

AstNode *calc_ast_mem_load(AstNode *addr, AstNode *index) {
    AstNode *n = alloc_node(AST_MEM_LOAD);
    n->mem.addr = addr;
    n->mem.index = index;
    n->mem.value = NULL;
    return n;
}

AstNode *calc_ast_mem_store(AstNode *addr, AstNode *index, AstNode *value) {
    AstNode *n = alloc_node(AST_MEM_STORE);
    n->mem.addr = addr;
    n->mem.index = index;
    n->mem.value = value;
    return n;
}

AstNode *calc_ast_mem_len(AstNode *addr) {
    AstNode *n = alloc_node(AST_MEM_LEN);
    n->mem.addr = addr;
    n->mem.index = NULL;
    n->mem.value = NULL;
    return n;
}

AstNode *calc_ast_mem_alloc(AstNode *count) {
    AstNode *n = alloc_node(AST_MEM_ALLOC);
    n->mem.addr = count; // For alloc, addr holds the count
    n->mem.index = NULL;
    n->mem.value = NULL;
    return n;
}

AstNode *calc_ast_map_new(void) {
    AstNode *n = alloc_node(AST_MAP_NEW);
    n->map.addr = NULL;
    n->map.key = NULL;
    n->map.value = NULL;
    return n;
}

AstNode *calc_ast_map_get(AstNode *addr, AstNode *key) {
    AstNode *n = alloc_node(AST_MAP_GET);
    n->map.addr = addr;
    n->map.key = key;
    n->map.value = NULL;
    return n;
}

AstNode *calc_ast_map_set(AstNode *addr, AstNode *key, AstNode *value) {
    AstNode *n = alloc_node(AST_MAP_SET);
    n->map.addr = addr;
    n->map.key = key;
    n->map.value = value;
    return n;
}

AstNode *calc_ast_map_has(AstNode *addr, AstNode *key) {
    AstNode *n = alloc_node(AST_MAP_HAS);
    n->map.addr = addr;
    n->map.key = key;
    n->map.value = NULL;
    return n;
}

AstNode *calc_ast_map_del(AstNode *addr, AstNode *key) {
    AstNode *n = alloc_node(AST_MAP_DEL);
    n->map.addr = addr;
    n->map.key = key;
    n->map.value = NULL;
    return n;
}

AstNode *calc_ast_map_len(AstNode *addr) {
    AstNode *n = alloc_node(AST_MAP_LEN);
    n->map.addr = addr;
    n->map.key = NULL;
    n->map.value = NULL;
    return n;
}

AstNode *calc_ast_map_keys(AstNode *addr) {
    AstNode *n = alloc_node(AST_MAP_KEYS);
    n->map.addr = addr;
    n->map.key = NULL;
    n->map.value = NULL;
    return n;
}

AstNode *calc_ast_cursor(AstNode *row, AstNode *col) {
    AstNode *n = alloc_node(AST_CURSOR);
    n->binary.left = row;
    n->binary.right = col;
    return n;
}

AstNode *calc_ast_ansi(const char *codes) {
    AstNode *n = alloc_node(AST_ANSI);
    // Pre-build the escape sequence: "\x1b[" + codes + "m"
    size_t codes_len = strlen(codes);
    size_t total_len = 2 + codes_len + 1; // \x1b[ + codes + m
    char *esc = malloc(total_len + 1);
    esc[0] = '\033';
    esc[1] = '[';
    memcpy(esc + 2, codes, codes_len);
    esc[2 + codes_len] = 'm';
    esc[total_len] = '\0';
    n->string_value = esc;
    return n;
}

AstNode *calc_ast_emit(AstNode *expr) {
    AstNode *n = alloc_node(AST_EMIT);
    n->unary.operand = expr;
    return n;
}

AstNode *calc_ast_lambda(const char *name, char **params, int n_params, AstNode *body,
                         bool is_computational) {
    AstNode *n = alloc_node(AST_LAMBDA);
    n->lambda.name = name ? strdup(name) : NULL;
    n->lambda.params = params;
    n->lambda.param_types = NULL; // No types for legacy syntax
    n->lambda.n_params = n_params;
    n->lambda.body = body;
    n->lambda.is_computational = is_computational;
    return n;
}

AstNode *calc_ast_seq(AstNode **stmts, int n_stmts) {
    if (n_stmts == 0) return calc_ast_empty();
    if (n_stmts == 1) return stmts[0];

    AstNode *n = alloc_node(AST_SEQ);
    n->seq.stmts = stmts;
    n->seq.n_stmts = n_stmts;
    return n;
}

AstNode *calc_ast_empty(void) {
    return alloc_node(AST_EMPTY);
}

// ============================================================================
// New CL Syntax AST Constructors
// ============================================================================

AstNode *calc_ast_var_access(const char *name, AstNode *dynamic_name, bool is_deref) {
    AstNode *n = alloc_node(AST_VAR_ACCESS);
    n->var_access.name = name ? strdup(name) : NULL;
    n->var_access.dynamic_name = dynamic_name;
    n->var_access.is_deref = is_deref;
    return n;
}

AstNode *calc_ast_var_concat(AstNode **parts, int n_parts, bool is_deref) {
    AstNode *n = alloc_node(AST_VAR_CONCAT);
    n->var_concat.parts = parts;
    n->var_concat.n_parts = n_parts;
    n->var_concat.is_deref = is_deref;
    return n;
}

AstNode *calc_ast_invoke(const char *name, AstNode *target, AstNode **args, int n_args,
                         bool is_deref) {
    AstNode *n = alloc_node(AST_INVOKE);
    n->invoke.name = name ? strdup(name) : NULL;
    n->invoke.target = target;
    n->invoke.args = args;
    n->invoke.n_args = n_args;
    n->invoke.is_deref = is_deref;
    return n;
}

AstNode *calc_ast_collection_get(const char *name, AstNode *target, AstNode *key, bool is_deref) {
    AstNode *n = alloc_node(AST_COLLECTION_GET);
    n->collection.name = name ? strdup(name) : NULL;
    n->collection.target = target;
    n->collection.key = key;
    n->collection.value = NULL;
    n->collection.is_deref = is_deref;
    return n;
}

AstNode *calc_ast_collection_set(const char *name, AstNode *target, AstNode *key, AstNode *value,
                                 bool is_deref) {
    AstNode *n = alloc_node(AST_COLLECTION_SET);
    n->collection.name = name ? strdup(name) : NULL;
    n->collection.target = target;
    n->collection.key = key;
    n->collection.value = value;
    n->collection.is_deref = is_deref;
    return n;
}

AstNode *calc_ast_addressof(const char *name) {
    AstNode *n = alloc_node(AST_ADDRESSOF);
    n->addressof.name = strdup(name);
    return n;
}

AstNode *calc_ast_enumerate(const char *array_name, AstNode *array_expr, const char *idx_var,
                            const char *val_var, AstNode *body) {
    AstNode *n = alloc_node(AST_ENUMERATE);
    n->enumerate.array_name = array_name ? strdup(array_name) : NULL;
    n->enumerate.array_expr = array_expr;
    n->enumerate.idx_var = strdup(idx_var);
    n->enumerate.val_var = strdup(val_var);
    n->enumerate.body = body;
    return n;
}

AstNode *calc_ast_let_typed(const char *name, CLType cl_type, AstNode *value) {
    AstNode *n = alloc_node(AST_LET);
    n->binding.name = strdup(name);
    n->binding.is_array = (cl_type == CL_TYPE_INT_ARRAY || cl_type == CL_TYPE_STRING_ARRAY);
    n->binding.cl_type = cl_type;
    n->binding.value = value;
    return n;
}

AstNode *calc_ast_lambda_typed(const char *name, char **params, CLType *param_types, int n_params,
                               AstNode *body, bool is_computational) {
    AstNode *n = alloc_node(AST_LAMBDA);
    n->lambda.name = name ? strdup(name) : NULL;
    n->lambda.params = params;
    n->lambda.param_types = param_types;
    n->lambda.n_params = n_params;
    n->lambda.body = body;
    n->lambda.is_computational = is_computational;
    return n;
}

AstNode *calc_ast_map_lit(AstNode **keys, AstNode **values, int n_pairs) {
    AstNode *n = alloc_node(AST_MAP_LIT);
    n->map_lit.keys = keys;
    n->map_lit.values = values;
    n->map_lit.n_pairs = n_pairs;
    return n;
}

AstNode *calc_ast_len_expr(AstNode *target, bool is_deref) {
    // For unified \len with expression, we reuse AST_LEN but with var.index as target
    // Actually, create a new representation using the collection struct
    AstNode *n = alloc_node(AST_LEN);
    n->collection.name = NULL;
    n->collection.target = target;
    n->collection.key = NULL;
    n->collection.value = NULL;
    n->collection.is_deref = is_deref;
    return n;
}

void calc_ast_free(AstNode *node) {
    if (!node) return;

    switch (node->type) {
    case AST_STRING_LIT:
    case AST_ANSI: free(node->string_value); break;

    case AST_VALUEOF:
    case AST_RECALL:
    case AST_REF:
        free(node->var.name);
        calc_ast_free(node->var.index);
        break;

    case AST_INC:
    case AST_DEC:
        free(node->collection.name);
        calc_ast_free(node->collection.target);
        // key and value are always NULL for inc/dec
        break;

    case AST_LEN: calc_ast_free(node->unary.operand); break;

    case AST_LET:
    case AST_ASSIGN:
        free(node->binding.name);
        calc_ast_free(node->binding.value);
        break;

    case AST_SETELEMENT:
        if (node->setelement.array_name) free(node->setelement.array_name);
        if (node->setelement.target) calc_ast_free(node->setelement.target);
        calc_ast_free(node->setelement.index);
        calc_ast_free(node->setelement.value);
        break;

    case AST_COPYARRAY:
        free(node->copyarray.dest_name);
        calc_ast_free(node->copyarray.source);
        break;

    case AST_MEM_LOAD:
    case AST_MEM_STORE:
    case AST_MEM_LEN:
    case AST_MEM_ALLOC:
        calc_ast_free(node->mem.addr);
        calc_ast_free(node->mem.index);
        calc_ast_free(node->mem.value);
        break;

    case AST_MAP_NEW:
    case AST_MAP_GET:
    case AST_MAP_SET:
    case AST_MAP_HAS:
    case AST_MAP_DEL:
    case AST_MAP_LEN:
    case AST_MAP_KEYS:
        calc_ast_free(node->map.addr);
        calc_ast_free(node->map.key);
        calc_ast_free(node->map.value);
        break;

    case AST_ADD:
    case AST_SUB:
    case AST_MUL:
    case AST_DIV:
    case AST_MOD:
    case AST_MAX:
    case AST_MIN:
    case AST_EQ:
    case AST_NE:
    case AST_LT:
    case AST_GT:
    case AST_LE:
    case AST_GE:
    case AST_AND:
    case AST_OR:
    case AST_CURSOR:
        calc_ast_free(node->binary.left);
        calc_ast_free(node->binary.right);
        break;

    case AST_NEG:
    case AST_RAND:
    case AST_NOT:
    case AST_EMIT: calc_ast_free(node->unary.operand); break;

    case AST_IF:
        calc_ast_free(node->if_node.condition);
        calc_ast_free(node->if_node.then_branch);
        calc_ast_free(node->if_node.else_branch);
        break;

    case AST_LOOP: calc_ast_free(node->loop.body); break;

    case AST_EXIT_WHEN:
    case AST_RETURN: calc_ast_free(node->control.value); break;

    case AST_ARRAY_LIT:
        for (int i = 0; i < node->array.n_elements; i++) {
            calc_ast_free(node->array.elements[i]);
        }
        free(node->array.elements);
        break;

    case AST_STRING_ARRAY_LIT:
        for (int i = 0; i < node->string_array.n_strings; i++) {
            free(node->string_array.strings[i]);
        }
        free(node->string_array.strings);
        break;

    case AST_LAMBDA:
        free(node->lambda.name);
        for (int i = 0; i < node->lambda.n_params; i++) {
            free(node->lambda.params[i]);
        }
        free(node->lambda.params);
        free(node->lambda.param_types); // May be NULL, free(NULL) is safe
        calc_ast_free(node->lambda.body);
        break;

    case AST_SEQ:
        for (int i = 0; i < node->seq.n_stmts; i++) {
            calc_ast_free(node->seq.stmts[i]);
        }
        free(node->seq.stmts);
        break;

    // New CL syntax nodes
    case AST_VAR_ACCESS:
        free(node->var_access.name);
        calc_ast_free(node->var_access.dynamic_name);
        break;

    case AST_VAR_CONCAT:
        for (int i = 0; i < node->var_concat.n_parts; i++) {
            calc_ast_free(node->var_concat.parts[i]);
        }
        free(node->var_concat.parts);
        break;

    case AST_INVOKE:
        free(node->invoke.name);
        calc_ast_free(node->invoke.target);
        for (int i = 0; i < node->invoke.n_args; i++) {
            calc_ast_free(node->invoke.args[i]);
        }
        free(node->invoke.args);
        break;

    case AST_COLLECTION_GET:
    case AST_COLLECTION_SET:
        free(node->collection.name);
        calc_ast_free(node->collection.target);
        calc_ast_free(node->collection.key);
        calc_ast_free(node->collection.value);
        break;

    case AST_ADDRESSOF: free(node->addressof.name); break;

    case AST_ENUMERATE:
        free(node->enumerate.array_name);
        calc_ast_free(node->enumerate.array_expr);
        free(node->enumerate.idx_var);
        free(node->enumerate.val_var);
        calc_ast_free(node->enumerate.body);
        break;

    case AST_MAP_LIT:
        for (int i = 0; i < node->map_lit.n_pairs; i++) {
            calc_ast_free(node->map_lit.keys[i]);
            calc_ast_free(node->map_lit.values[i]);
        }
        free(node->map_lit.keys);
        free(node->map_lit.values);
        break;

    default: break;
    }

    free(node);
}

// ============================================================================
// Parser Helpers
// ============================================================================

void calc_parser_init(CalcParser *parser, const char *input) {
    parser->input = input;
    parser->p = input;
    parser->line = 1;
    parser->col = 1;
    parser->error_msg[0] = '\0';
    parser->had_error = false;
}

static void parser_error(CalcParser *p, const char *msg) {
    if (!p->had_error) {
        snprintf(p->error_msg, sizeof(p->error_msg), "Parse error at line %d, col %d: %s", p->line,
                 p->col, msg);
        p->had_error = true;
    }
}

// Propagate error from a sub-parser to the main parser
static void propagate_error(CalcParser *main, CalcParser *sub) {
    if (sub->had_error && !main->had_error) {
        main->had_error = true;
        strncpy(main->error_msg, sub->error_msg, sizeof(main->error_msg) - 1);
        main->error_msg[sizeof(main->error_msg) - 1] = '\0';
    }
}

const char *calc_parser_error(CalcParser *parser) {
    return parser->had_error ? parser->error_msg : NULL;
}

static void skip_whitespace(CalcParser *p) {
    while (*p->p && isspace(*p->p)) {
        if (*p->p == '\n') {
            p->line++;
            p->col = 1;
        } else {
            p->col++;
        }
        p->p++;
    }
}

static bool match(CalcParser *p, const char *s) {
    size_t len = strlen(s);
    if (strncmp(p->p, s, len) == 0) {
        p->p += len;
        p->col += len;
        return true;
    }
    return false;
}

static bool peek(CalcParser *p, const char *s) {
    return strncmp(p->p, s, strlen(s)) == 0;
}

static bool at_end(CalcParser *p) {
    return *p->p == '\0';
}

// Parse content between matching delimiters, respecting nesting
static char *parse_delimited(CalcParser *p, char open, char close) {
    if (*p->p != open) {
        parser_error(p, "Expected opening delimiter");
        return NULL;
    }
    p->p++;
    p->col++;

    const char *start = p->p;
    int depth = 1;

    while (*p->p && depth > 0) {
        if (*p->p == open)
            depth++;
        else if (*p->p == close)
            depth--;

        if (depth > 0) {
            if (*p->p == '\n') {
                p->line++;
                p->col = 1;
            } else {
                p->col++;
            }
            p->p++;
        }
    }

    if (depth != 0) {
        parser_error(p, "Unmatched delimiter");
        return NULL;
    }

    size_t len = p->p - start;
    char *content = malloc(len + 1);
    memcpy(content, start, len);
    content[len] = '\0';

    p->p++; // Skip closing delimiter
    p->col++;

    return content;
}

// Parse angle bracket name: <name>
static char *parse_angle_name(CalcParser *p) {
    return parse_delimited(p, '<', '>');
}

// Parse brace content: {content}
static char *parse_brace_content(CalcParser *p) {
    return parse_delimited(p, '{', '}');
}

// Parse bracket content: [content]
static char *parse_bracket_content(CalcParser *p) {
    return parse_delimited(p, '[', ']');
}

// Parse an integer literal
static AstNode *parse_int(CalcParser *p) {
    const char *start = p->p;
    bool negative = false;

    if (*p->p == '-') {
        negative = true;
        p->p++;
        p->col++;
    }

    if (!isdigit(*p->p)) {
        parser_error(p, "Expected integer");
        return NULL;
    }

    int64_t value = 0;
    while (isdigit(*p->p)) {
        value = value * 10 + (*p->p - '0');
        p->p++;
        p->col++;
    }

    if (negative) value = -value;
    return calc_ast_int_lit(value);
}

// Forward declaration
static AstNode *parse_expr(CalcParser *p);

// Parse a comma-separated list of expressions
static AstNode **parse_arg_list(CalcParser *p, const char *content, int *n_args) {
    *n_args = 0;

    if (!content || !*content) {
        return NULL;
    }

    // Count commas (roughly)
    int cap = 8;
    AstNode **args = calloc(cap, sizeof(AstNode *));

    CalcParser sub;
    calc_parser_init(&sub, content);

    while (!at_end(&sub)) {
        skip_whitespace(&sub);
        if (at_end(&sub)) break;

        // Find extent of this argument
        const char *arg_start = sub.p;
        int depth = 0;
        while (*sub.p && (depth > 0 || *sub.p != ',')) {
            if (*sub.p == '{' || *sub.p == '[' || *sub.p == '<')
                depth++;
            else if (*sub.p == '}' || *sub.p == ']' || *sub.p == '>')
                depth--;
            sub.p++;
        }

        size_t arg_len = sub.p - arg_start;
        char *arg_str = malloc(arg_len + 1);
        memcpy(arg_str, arg_start, arg_len);
        arg_str[arg_len] = '\0';

        // Trim whitespace
        char *trimmed = arg_str;
        while (*trimmed && isspace(*trimmed)) trimmed++;
        char *end = trimmed + strlen(trimmed) - 1;
        while (end > trimmed && isspace(*end)) *end-- = '\0';

        // Parse the argument
        CalcParser arg_parser;
        calc_parser_init(&arg_parser, trimmed);
        AstNode *arg = parse_expr(&arg_parser);

        // Propagate any error from the argument parser
        if (arg_parser.had_error) {
            propagate_error(p, &arg_parser);
            calc_ast_free(arg);
            free(arg_str);
            // Free already parsed arguments
            for (int i = 0; i < *n_args; i++) {
                calc_ast_free(args[i]);
            }
            free(args);
            return NULL;
        }

        if (arg) {
            if (*n_args >= cap) {
                cap *= 2;
                args = realloc(args, cap * sizeof(AstNode *));
            }
            args[(*n_args)++] = arg;
        }

        free(arg_str);

        if (*sub.p == ',') sub.p++;
    }

    return args;
}

// Parse parameter list: [a,b,c]
static char **parse_param_list(CalcParser *p, int *n_params) {
    *n_params = 0;

    if (*p->p != '[') return NULL;

    char *content = parse_bracket_content(p);
    if (!content) return NULL;

    int cap = 8;
    char **params = calloc(cap, sizeof(char *));

    char *tok = strtok(content, ",");
    while (tok) {
        // Trim whitespace
        while (*tok && isspace(*tok)) tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && isspace(*end)) *end-- = '\0';

        if (*tok) {
            if (*n_params >= cap) {
                cap *= 2;
                params = realloc(params, cap * sizeof(char *));
            }
            params[(*n_params)++] = strdup(tok);
        }

        tok = strtok(NULL, ",");
    }

    free(content);
    return params;
}

// ============================================================================
// Expression Parsing
// ============================================================================

static AstNode *parse_expr(CalcParser *p) {
    skip_whitespace(p);

    if (at_end(p)) return calc_ast_empty();

    // Integer literal
    if (isdigit(*p->p) || (*p->p == '-' && isdigit(p->p[1]))) {
        return parse_int(p);
    }

    // Array literal: [1,2,3] or [str1,str2,str3]
    if (*p->p == '[') {
        char *content = parse_bracket_content(p);
        if (!content) return NULL;

        int n_elements;
        AstNode **elements = parse_arg_list(p, content, &n_elements);
        free(content);

        // Check if all elements are string literals - if so, create string array
        bool all_strings = n_elements > 0;
        for (int i = 0; i < n_elements && all_strings; i++) {
            if (elements[i]->type != AST_STRING_LIT) {
                all_strings = false;
            }
        }

        if (all_strings) {
            // Convert to string array literal
            char **strings = calloc(n_elements, sizeof(char *));
            for (int i = 0; i < n_elements; i++) {
                strings[i] = strdup(elements[i]->string_value);
                calc_ast_free(elements[i]);
            }
            free(elements);
            return calc_ast_string_array_lit(strings, n_elements);
        }

        return calc_ast_array_lit(elements, n_elements);
    }

    // Map literal: |1->10, 2->20| (new CL syntax)
    if (*p->p == '|') {
        p->p++;
        p->col++;

        // Find matching |
        const char *start = p->p;
        int depth = 1;
        while (*p->p && depth > 0) {
            if (*p->p == '|')
                depth--;
            else if (*p->p == '\n') {
                p->line++;
                p->col = 0;
            }
            if (depth > 0) {
                p->p++;
                p->col++;
            }
        }

        size_t len = p->p - start;
        char *content = malloc(len + 1);
        memcpy(content, start, len);
        content[len] = '\0';

        if (*p->p == '|') {
            p->p++;
            p->col++;
        }

        // Parse key->value pairs
        int cap = 8;
        AstNode **keys = calloc(cap, sizeof(AstNode *));
        AstNode **values = calloc(cap, sizeof(AstNode *));
        int n_pairs = 0;

        if (len > 0) {
            char *pair_str = strtok(content, ",");
            while (pair_str) {
                // Find ->
                char *arrow = strstr(pair_str, "->");
                if (arrow) {
                    *arrow = '\0';
                    char *key_str = pair_str;
                    char *val_str = arrow + 2;

                    // Trim whitespace
                    while (*key_str && isspace(*key_str)) key_str++;
                    while (*val_str && isspace(*val_str)) val_str++;

                    CalcParser key_p, val_p;
                    calc_parser_init(&key_p, key_str);
                    calc_parser_init(&val_p, val_str);

                    if (n_pairs >= cap) {
                        cap *= 2;
                        keys = realloc(keys, cap * sizeof(AstNode *));
                        values = realloc(values, cap * sizeof(AstNode *));
                    }
                    keys[n_pairs] = parse_expr(&key_p);
                    values[n_pairs] = parse_expr(&val_p);
                    n_pairs++;
                }
                pair_str = strtok(NULL, ",");
            }
        }

        free(content);
        return calc_ast_map_lit(keys, values, n_pairs);
    }

    // Variable access: ${name} or ${*name} or ${item${i}} (new CL syntax)
    if (*p->p == '$' && p->p[1] == '{') {
        p->p += 2;
        p->col += 2;

        bool is_deref = false;
        if (*p->p == '*') {
            is_deref = true;
            p->p++;
            p->col++;
        }

        // Collect parts: strings and ${expr} chunks
        // This handles both simple ${name} and concatenated ${item${i}}
        int cap = 4;
        int n_parts = 0;
        AstNode **parts = calloc(cap, sizeof(AstNode *));

        while (*p->p && *p->p != '}') {
            if (*p->p == '$' && p->p[1] == '{') {
                // Nested expression: ${...}
                AstNode *inner = parse_expr(p); // This consumes ${...}
                if (n_parts >= cap) {
                    cap *= 2;
                    parts = realloc(parts, cap * sizeof(AstNode *));
                }
                parts[n_parts++] = inner;
            } else {
                // Literal string until next ${ or }
                const char *start = p->p;
                while (*p->p && *p->p != '}' && !(*p->p == '$' && p->p[1] == '{')) {
                    p->p++;
                    p->col++;
                }
                size_t len = p->p - start;
                char *str = malloc(len + 1);
                memcpy(str, start, len);
                str[len] = '\0';

                if (n_parts >= cap) {
                    cap *= 2;
                    parts = realloc(parts, cap * sizeof(AstNode *));
                }
                parts[n_parts++] = calc_ast_string_lit(str);
                free(str);
            }
        }

        if (*p->p == '}') {
            p->p++;
            p->col++;
        }

        // Simplify: if only one part and it's a string literal, return simple var_access
        if (n_parts == 1 && parts[0]->type == AST_STRING_LIT) {
            char *name = strdup(parts[0]->string_value);
            calc_ast_free(parts[0]);
            free(parts);
            AstNode *result = calc_ast_var_access(name, NULL, is_deref);
            free(name);
            return result;
        }

        // Simplify: if only one part and it's dynamic (AST_VAR_ACCESS), return var_access with dynamic_name
        if (n_parts == 1 && parts[0]->type != AST_STRING_LIT) {
            AstNode *dynamic_name = parts[0];
            free(parts);
            return calc_ast_var_access(NULL, dynamic_name, is_deref);
        }

        // Multiple parts: create AST_VAR_CONCAT wrapped in AST_VAR_ACCESS
        // AST_VAR_CONCAT produces the computed string, AST_VAR_ACCESS does the lookup
        AstNode *concat = calc_ast_var_concat(parts, n_parts, false); // concat itself has no deref
        return calc_ast_var_access(NULL, concat, is_deref);
    }

    // Command starting with backslash
    if (*p->p == '\\') {
        p->p++;
        p->col++;

        // Binary arithmetic operations
        if (match(p, "add{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\add requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_ADD, args[0], args[1]);
        }
        if (match(p, "sub{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\sub requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_SUB, args[0], args[1]);
        }
        if (match(p, "mul{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\mul requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_MUL, args[0], args[1]);
        }
        if (match(p, "div{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\div requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_DIV, args[0], args[1]);
        }
        if (match(p, "mod{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\mod requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_MOD, args[0], args[1]);
        }
        if (match(p, "max{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\max requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_MAX, args[0], args[1]);
        }
        if (match(p, "min{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\min requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_MIN, args[0], args[1]);
        }

        // Comparison operations
        if (match(p, "eq{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\eq requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_EQ, args[0], args[1]);
        }
        if (match(p, "ne{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\ne requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_NE, args[0], args[1]);
        }
        if (match(p, "lt{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\lt requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_LT, args[0], args[1]);
        }
        if (match(p, "gt{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\gt requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_GT, args[0], args[1]);
        }
        if (match(p, "le{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\le requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_LE, args[0], args[1]);
        }
        if (match(p, "ge{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\ge requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_GE, args[0], args[1]);
        }

        // Logical operations
        if (match(p, "and{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\and requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_AND, args[0], args[1]);
        }
        if (match(p, "or{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\or requires 2 arguments");
                return NULL;
            }
            return calc_ast_binary(AST_OR, args[0], args[1]);
        }
        if (match(p, "not{")) {
            p->p--;
            char *content = parse_brace_content(p);
            CalcParser sub;
            calc_parser_init(&sub, content);
            AstNode *operand = parse_expr(&sub);
            free(content);
            propagate_error(p, &sub);
            if (p->had_error) {
                calc_ast_free(operand);
                return NULL;
            }
            return calc_ast_unary(AST_NOT, operand);
        }
        if (match(p, "rand{")) {
            p->p--;
            char *content = parse_brace_content(p);
            CalcParser sub;
            calc_parser_init(&sub, content);
            AstNode *operand = parse_expr(&sub);
            free(content);
            propagate_error(p, &sub);
            if (p->had_error) {
                calc_ast_free(operand);
                return NULL;
            }
            return calc_ast_unary(AST_RAND, operand);
        }

        // \valueof<name> - BANNED: use ${name} instead
        if (match(p, "valueof<")) {
            parser_error(p, "\\valueof is deprecated. Use ${name} instead");
            return NULL;
        }

        // \recall<name> - BANNED: use ${name} for values or \invoke<name>[args] for calls
        if (match(p, "recall<")) {
            parser_error(p, "\\recall is deprecated. Use ${name} for values or "
                            "\\invoke<name>[args] for function calls");
            return NULL;
        }

        // \ref<name>
        if (match(p, "ref<")) {
            p->p--;
            char *name = parse_angle_name(p);
            return calc_ast_ref(name);
        }

        // \let<name>{value} or \let<name[]>{[...]} or \let<name:type>{value}
        if (match(p, "let<")) {
            p->p--;
            char *name_content = parse_angle_name(p);
            bool is_array = false;
            CLType cl_type = CL_TYPE_INFERRED;
            char *actual_name = name_content;

            // Check for type annotation: name:type
            char *colon = strchr(name_content, ':');
            if (colon) {
                *colon = '\0';
                actual_name = name_content;
                char *type_str = colon + 1;

                // Parse type
                if (strcmp(type_str, "int") == 0) {
                    cl_type = CL_TYPE_INT;
                } else if (strcmp(type_str, "string") == 0) {
                    cl_type = CL_TYPE_STRING;
                } else if (strcmp(type_str, "int[]") == 0) {
                    cl_type = CL_TYPE_INT_ARRAY;
                    is_array = true;
                } else if (strcmp(type_str, "string[]") == 0) {
                    cl_type = CL_TYPE_STRING_ARRAY;
                    is_array = true;
                } else if (strcmp(type_str, "map") == 0) {
                    cl_type = CL_TYPE_MAP;
                } else if (strcmp(type_str, "address") == 0) {
                    cl_type = CL_TYPE_ADDRESS;
                }
            } else {
                // Check for [] suffix (legacy syntax)
                size_t len = strlen(name_content);
                if (len >= 2 && name_content[len - 2] == '[' && name_content[len - 1] == ']') {
                    is_array = true;
                    name_content[len - 2] = '\0';
                }
            }

            char *value_content = parse_brace_content(p);
            CalcParser sub;
            calc_parser_init(&sub, value_content);
            AstNode *value = parse_expr(&sub);
            free(value_content);
            propagate_error(p, &sub);
            if (p->had_error) {
                calc_ast_free(value);
                free(name_content);
                return NULL;
            }

            AstNode *result;
            if (cl_type != CL_TYPE_INFERRED) {
                result = calc_ast_let_typed(actual_name, cl_type, value);
            } else {
                result = calc_ast_let(actual_name, is_array, value);
            }
            free(name_content);
            return result;
        }

        // \assign<name> - BANNED: use \let<name>{value} for reassignment
        if (match(p, "assign<")) {
            parser_error(p, "\\assign is deprecated. Use \\let<name>{value} for reassignment");
            return NULL;
        }

        // \inc<name> or \inc<*name> or \inc<${expr}>
        if (match(p, "inc<")) {
            p->p--;
            char *name_content = parse_angle_name(p);

            bool is_deref = false;
            char *actual_name = name_content;
            if (name_content[0] == '*') {
                is_deref = true;
                actual_name = name_content + 1;
            }

            AstNode *target = NULL;
            char *static_name = NULL;
            if (strncmp(actual_name, "${", 2) == 0) {
                // Dynamic: parse the ${...} expression
                CalcParser name_parser;
                calc_parser_init(&name_parser, actual_name);
                AstNode *parsed = parse_expr(&name_parser);
                // For inc/dec, we want the NAME string, not the looked-up VALUE
                // If it's AST_VAR_ACCESS with dynamic_name, extract the dynamic_name
                if (parsed->type == AST_VAR_ACCESS && parsed->var_access.dynamic_name) {
                    target = parsed->var_access.dynamic_name;
                    parsed->var_access.dynamic_name = NULL; // Prevent double-free
                    calc_ast_free(parsed);
                } else {
                    // Simple ${name} or ${${expr}} - target is the parsed expression
                    target = parsed;
                }
            } else {
                static_name = strdup(actual_name);
            }
            free(name_content);

            AstNode *result = calc_ast_inc_dyn(static_name, target, is_deref);
            free(static_name);
            return result;
        }

        // \dec<name> or \dec<*name> or \dec<${expr}>
        if (match(p, "dec<")) {
            p->p--;
            char *name_content = parse_angle_name(p);

            bool is_deref = false;
            char *actual_name = name_content;
            if (name_content[0] == '*') {
                is_deref = true;
                actual_name = name_content + 1;
            }

            AstNode *target = NULL;
            char *static_name = NULL;
            if (strncmp(actual_name, "${", 2) == 0) {
                // Dynamic: parse the ${...} expression
                CalcParser name_parser;
                calc_parser_init(&name_parser, actual_name);
                AstNode *parsed = parse_expr(&name_parser);
                // For inc/dec, we want the NAME string, not the looked-up VALUE
                // If it's AST_VAR_ACCESS with dynamic_name, extract the dynamic_name
                if (parsed->type == AST_VAR_ACCESS && parsed->var_access.dynamic_name) {
                    target = parsed->var_access.dynamic_name;
                    parsed->var_access.dynamic_name = NULL; // Prevent double-free
                    calc_ast_free(parsed);
                } else {
                    // Simple ${name} or ${${expr}} - target is the parsed expression
                    target = parsed;
                }
            } else {
                static_name = strdup(actual_name);
            }
            free(name_content);

            AstNode *result = calc_ast_dec_dyn(static_name, target, is_deref);
            free(static_name);
            return result;
        }

        // \setelement<name> - BANNED: use \set<name>[index]{value} instead
        if (match(p, "setelement<")) {
            parser_error(p, "\\setelement is deprecated. Use \\set<name>[index]{value} instead");
            return NULL;
        }

        // \len<name> or \len<*name> (unified length: arrays, maps, heap)
        if (match(p, "len<")) {
            p->p--;
            char *name_content = parse_angle_name(p);

            bool is_deref = false;
            char *actual_name = name_content;
            if (name_content[0] == '*') {
                is_deref = true;
                actual_name = name_content + 1;
            }

            // Construct target expression
            AstNode *target;
            if (strncmp(actual_name, "${", 2) == 0) {
                CalcParser name_parser;
                calc_parser_init(&name_parser, actual_name);
                target = parse_expr(&name_parser);
            } else {
                target = calc_ast_var_access(strdup(actual_name), NULL, is_deref);
            }
            free(name_content);

            return calc_ast_len(target);
        }

        // \copyarray<dest>{source_name_expr}
        if (match(p, "copyarray<")) {
            p->p--;
            char *dest_name = parse_angle_name(p);
            char *source_content = parse_brace_content(p);
            CalcParser source_parser;
            calc_parser_init(&source_parser, source_content);
            AstNode *source = parse_expr(&source_parser);
            free(source_content);
            AstNode *result = calc_ast_copyarray(dest_name, source);
            free(dest_name);
            return result;
        }

        // \cursor{row,col} - emit cursor positioning
        if (match(p, "cursor{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\cursor requires 2 arguments");
                return NULL;
            }
            return calc_ast_cursor(args[0], args[1]);
        }

        // \ansi{codes} - emit ANSI escape sequence
        if (match(p, "ansi{")) {
            p->p--;
            char *content = parse_brace_content(p);
            // Content is literal string like "97;40", not an expression
            AstNode *node = calc_ast_ansi(content);
            free(content);
            return node;
        }

        // \emit{expr} - emit expression value as text
        if (match(p, "emit{")) {
            p->p--;
            char *content = parse_brace_content(p);
            CalcParser sub;
            calc_parser_init(&sub, content);
            AstNode *expr = parse_expr(&sub);
            free(content);
            return calc_ast_emit(expr);
        }

        // \mem_load{addr,idx} - load from heap
        if (match(p, "mem_load{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 2) {
                parser_error(p, "\\mem_load requires 2 arguments");
                return NULL;
            }
            return calc_ast_mem_load(args[0], args[1]);
        }

        // \mem_store{addr,idx,val} - store to heap
        if (match(p, "mem_store{")) {
            p->p--;
            char *content = parse_brace_content(p);
            int n;
            AstNode **args = parse_arg_list(p, content, &n);
            free(content);
            if (n != 3) {
                parser_error(p, "\\mem_store requires 3 arguments");
                return NULL;
            }
            return calc_ast_mem_store(args[0], args[1], args[2]);
        }

        // \mem_len{addr} - get heap block length
        if (match(p, "mem_len{")) {
            p->p--;
            char *content = parse_brace_content(p);
            CalcParser sub;
            calc_parser_init(&sub, content);
            AstNode *addr = parse_expr(&sub);
            free(content);
            return calc_ast_mem_len(addr);
        }

        // \mem_alloc{count} - allocate heap block
        if (match(p, "mem_alloc{")) {
            p->p--;
            char *content = parse_brace_content(p);
            CalcParser sub;
            calc_parser_init(&sub, content);
            AstNode *count = parse_expr(&sub);
            free(content);
            return calc_ast_mem_alloc(count);
        }

        // \map_new{} - create empty map
        if (match(p, "map_new{")) {
            p->p--;
            char *content = parse_brace_content(p);
            free(content); // Should be empty
            return calc_ast_map_new();
        }

        // \map_get{...} - BANNED: use \at<name>[key] instead
        if (match(p, "map_get{")) {
            parser_error(
                p, "\\map_get{} is deprecated. Use \\at<name>[key] or \\at<*addr>[key] instead");
            return NULL;
        }

        // \map_set{...} - BANNED: use \set<name>[key]{val} instead
        if (match(p, "map_set{")) {
            parser_error(p, "\\map_set{} is deprecated. Use \\set<name>[key]{val} or "
                            "\\set<*addr>[key]{val} instead");
            return NULL;
        }

        // \map_has<name>[key] or \map_has<*name>[key] (new CL syntax)
        if (match(p, "map_has<")) {
            p->p--;
            char *name_content = parse_angle_name(p);

            bool is_deref = false;
            char *actual_name = name_content;
            if (name_content[0] == '*') {
                is_deref = true;
                actual_name = name_content + 1;
            }

            // Construct address expression
            AstNode *addr;
            if (strncmp(actual_name, "${", 2) == 0) {
                CalcParser name_parser;
                calc_parser_init(&name_parser, actual_name);
                addr = parse_expr(&name_parser);
            } else {
                addr = calc_ast_var_access(strdup(actual_name), NULL, is_deref);
            }
            free(name_content);

            // Parse [key]
            char *key_content = parse_bracket_content(p);
            CalcParser key_parser;
            calc_parser_init(&key_parser, key_content);
            AstNode *key = parse_expr(&key_parser);
            free(key_content);

            return calc_ast_map_has(addr, key);
        }

        // \map_has{...} - BANNED: use \map_has<name>[key] instead
        if (match(p, "map_has{")) {
            parser_error(p, "\\map_has{} is deprecated. Use \\map_has<name>[key] or "
                            "\\map_has<*addr>[key] instead");
            return NULL;
        }

        // \map_del<name>[key] or \map_del<*name>[key] (new CL syntax)
        if (match(p, "map_del<")) {
            p->p--;
            char *name_content = parse_angle_name(p);

            bool is_deref = false;
            char *actual_name = name_content;
            if (name_content[0] == '*') {
                is_deref = true;
                actual_name = name_content + 1;
            }

            // Construct address expression
            AstNode *addr;
            if (strncmp(actual_name, "${", 2) == 0) {
                CalcParser name_parser;
                calc_parser_init(&name_parser, actual_name);
                addr = parse_expr(&name_parser);
            } else {
                addr = calc_ast_var_access(strdup(actual_name), NULL, is_deref);
            }
            free(name_content);

            // Parse [key]
            char *key_content = parse_bracket_content(p);
            CalcParser key_parser;
            calc_parser_init(&key_parser, key_content);
            AstNode *key = parse_expr(&key_parser);
            free(key_content);

            return calc_ast_map_del(addr, key);
        }

        // \map_del{...} - BANNED: use \map_del<name>[key] instead
        if (match(p, "map_del{")) {
            parser_error(p, "\\map_del{} is deprecated. Use \\map_del<name>[key] or "
                            "\\map_del<*addr>[key] instead");
            return NULL;
        }

        // \map_len{...} - BANNED: use \len<name> instead
        if (match(p, "map_len{")) {
            parser_error(p, "\\map_len{} is deprecated. Use \\len<name> or \\len<*addr> instead");
            return NULL;
        }

        // \map_keys<name> or \map_keys<*name> (new CL syntax)
        if (match(p, "map_keys<")) {
            p->p--;
            char *name_content = parse_angle_name(p);

            bool is_deref = false;
            char *actual_name = name_content;
            if (name_content[0] == '*') {
                is_deref = true;
                actual_name = name_content + 1;
            }

            // Construct address expression
            AstNode *addr;
            if (strncmp(actual_name, "${", 2) == 0) {
                CalcParser name_parser;
                calc_parser_init(&name_parser, actual_name);
                addr = parse_expr(&name_parser);
            } else {
                addr = calc_ast_var_access(strdup(actual_name), NULL, is_deref);
            }
            free(name_content);

            return calc_ast_map_keys(addr);
        }

        // \map_keys{...} - BANNED: use \map_keys<name> instead
        if (match(p, "map_keys{")) {
            parser_error(
                p, "\\map_keys{} is deprecated. Use \\map_keys<name> or \\map_keys<*addr> instead");
            return NULL;
        }

        // \return{value}
        if (match(p, "return{")) {
            p->p--;
            char *content = parse_brace_content(p);
            CalcParser sub;
            calc_parser_init(&sub, content);
            AstNode *value = parse_expr(&sub);
            free(content);
            propagate_error(p, &sub);
            if (p->had_error) {
                calc_ast_free(value);
                return NULL;
            }
            return calc_ast_return(value);
        }

        // \if{cond}{then}\else{else}
        if (match(p, "if{")) {
            p->p--;
            char *cond_content = parse_brace_content(p);
            CalcParser cond_parser;
            calc_parser_init(&cond_parser, cond_content);
            AstNode *cond = parse_expr(&cond_parser);
            free(cond_content);

            char *then_content = parse_brace_content(p);
            CalcParser then_parser;
            calc_parser_init(&then_parser, then_content);
            AstNode *then_branch = calc_parse(&then_parser);
            free(then_content);
            propagate_error(p, &then_parser);
            if (p->had_error) {
                calc_ast_free(cond);
                calc_ast_free(then_branch);
                return NULL;
            }

            AstNode *else_branch = NULL;
            skip_whitespace(p);
            if (match(p, "\\else{")) {
                p->p--;
                char *else_content = parse_brace_content(p);
                CalcParser else_parser;
                calc_parser_init(&else_parser, else_content);
                else_branch = calc_parse(&else_parser);
                free(else_content);
                propagate_error(p, &else_parser);
                if (p->had_error) {
                    calc_ast_free(cond);
                    calc_ast_free(then_branch);
                    calc_ast_free(else_branch);
                    return NULL;
                }
            }

            return calc_ast_if(cond, then_branch, else_branch);
        }

        // \begin{loop}...\end{loop}
        if (match(p, "begin{loop}")) {
            // Find matching \end{loop}
            const char *body_start = p->p;
            int depth = 1;
            while (*p->p && depth > 0) {
                if (strncmp(p->p, "\\begin{loop}", 12) == 0) {
                    depth++;
                    p->p += 12;
                } else if (strncmp(p->p, "\\end{loop}", 10) == 0) {
                    depth--;
                    if (depth == 0) break;
                    p->p += 10;
                } else {
                    p->p++;
                }
            }

            size_t body_len = p->p - body_start;
            char *body_content = malloc(body_len + 1);
            memcpy(body_content, body_start, body_len);
            body_content[body_len] = '\0';

            if (match(p, "\\end{loop}")) {
                // Good
            }

            CalcParser body_parser;
            calc_parser_init(&body_parser, body_content);
            AstNode *body = calc_parse(&body_parser);
            free(body_content);
            propagate_error(p, &body_parser);
            if (p->had_error) {
                calc_ast_free(body);
                return NULL;
            }

            return calc_ast_loop(body);
        }

        // \exit_when{cond}
        if (match(p, "exit_when{")) {
            p->p--;
            char *content = parse_brace_content(p);
            CalcParser sub;
            calc_parser_init(&sub, content);
            AstNode *cond = parse_expr(&sub);
            free(content);
            propagate_error(p, &sub);
            if (p->had_error) {
                calc_ast_free(cond);
                return NULL;
            }
            return calc_ast_exit_when(cond);
        }

        // \lambda<name>[params]{body} or \lambda<name>[params]#{body}
        if (match(p, "lambda<")) {
            p->p--;
            char *name = parse_angle_name(p);

            int n_params = 0;
            char **params = NULL;
            if (*p->p == '[') {
                params = parse_param_list(p, &n_params);
            }

            bool is_computational = false;
            if (*p->p == '#') {
                is_computational = true;
                p->p++;
            }

            char *body_content = parse_brace_content(p);
            CalcParser body_parser;
            calc_parser_init(&body_parser, body_content);
            AstNode *body = calc_parse(&body_parser);
            free(body_content);
            propagate_error(p, &body_parser);
            if (p->had_error) {
                free(name);
                for (int i = 0; i < n_params; i++) free(params[i]);
                free(params);
                calc_ast_free(body);
                return NULL;
            }

            AstNode *result = calc_ast_lambda(name, params, n_params, body, is_computational);
            free(name);
            return result;
        }

        // ============================================================================
        // New CL Syntax Commands
        // ============================================================================

        // \invoke<name>[args] or \invoke<*name>[args] (new CL syntax)
        if (match(p, "invoke<")) {
            p->p--;
            char *name_content = parse_angle_name(p);

            bool is_deref = false;
            char *actual_name = name_content;

            // Check for dereference prefix
            if (name_content[0] == '*') {
                is_deref = true;
                actual_name = name_content + 1;
            }

            // Check if name is a dynamic expression (${...})
            AstNode *target = NULL;
            char *static_name = NULL;
            if (strncmp(actual_name, "${", 2) == 0) {
                // Dynamic: parse the ${...} expression
                CalcParser name_parser;
                calc_parser_init(&name_parser, actual_name);
                target = parse_expr(&name_parser);
            } else {
                static_name = strdup(actual_name);
            }
            free(name_content);

            // Parse [args] - optional for thunks
            AstNode **args = NULL;
            int n_args = 0;
            if (*p->p == '[') {
                char *args_content = parse_bracket_content(p);
                args = parse_arg_list(p, args_content, &n_args);
                free(args_content);
            }

            AstNode *result = calc_ast_invoke(static_name, target, args, n_args, is_deref);
            free(static_name);
            return result;
        }

        // \at<name>[key] or \at<*name>[key] (new CL syntax - collection read)
        if (match(p, "at<")) {
            p->p--;
            char *name_content = parse_angle_name(p);

            bool is_deref = false;
            char *actual_name = name_content;

            if (name_content[0] == '*') {
                is_deref = true;
                actual_name = name_content + 1;
            }

            AstNode *target = NULL;
            char *static_name = NULL;
            if (strncmp(actual_name, "${", 2) == 0) {
                CalcParser name_parser;
                calc_parser_init(&name_parser, actual_name);
                target = parse_expr(&name_parser);
            } else {
                static_name = strdup(actual_name);
            }
            free(name_content);

            // Parse [key]
            char *key_content = parse_bracket_content(p);
            CalcParser key_parser;
            calc_parser_init(&key_parser, key_content);
            AstNode *key = parse_expr(&key_parser);
            free(key_content);

            AstNode *result = calc_ast_collection_get(static_name, target, key, is_deref);
            free(static_name);
            return result;
        }

        // \set<name>[key]{val} or \set<*name>[key]{val} (new CL syntax - collection write)
        if (match(p, "set<")) {
            p->p--;
            char *name_content = parse_angle_name(p);

            bool is_deref = false;
            char *actual_name = name_content;

            if (name_content[0] == '*') {
                is_deref = true;
                actual_name = name_content + 1;
            }

            AstNode *target = NULL;
            char *static_name = NULL;
            if (strncmp(actual_name, "${", 2) == 0) {
                CalcParser name_parser;
                calc_parser_init(&name_parser, actual_name);
                target = parse_expr(&name_parser);
            } else {
                static_name = strdup(actual_name);
            }
            free(name_content);

            // Parse [key]
            char *key_content = parse_bracket_content(p);
            CalcParser key_parser;
            calc_parser_init(&key_parser, key_content);
            AstNode *key = parse_expr(&key_parser);
            free(key_content);

            // Parse {value}
            char *val_content = parse_brace_content(p);
            CalcParser val_parser;
            calc_parser_init(&val_parser, val_content);
            AstNode *value = parse_expr(&val_parser);
            free(val_content);

            AstNode *result = calc_ast_collection_set(static_name, target, key, value, is_deref);
            free(static_name);
            return result;
        }

        // \addressof<name> (new CL syntax - get address of collection)
        if (match(p, "addressof<")) {
            p->p--;
            char *name = parse_angle_name(p);
            AstNode *result = calc_ast_addressof(name);
            free(name);
            return result;
        }

        // \begin<arr>[i,v]{enumerate}...\end{enumerate} (new CL syntax)
        if (match(p, "begin<")) {
            p->p--;
            char *arr_content = parse_angle_name(p);

            // Check for * prefix (dereference)
            char *actual_arr = arr_content;
            AstNode *array_expr = NULL;
            char *array_name = NULL;

            if (arr_content[0] == '*') {
                actual_arr = arr_content + 1;
            }

            // Check if it's a dynamic expression
            if (strncmp(actual_arr, "${", 2) == 0 || actual_arr[0] == '\\') {
                CalcParser arr_parser;
                calc_parser_init(&arr_parser, actual_arr);
                array_expr = parse_expr(&arr_parser);
            } else {
                array_name = strdup(actual_arr);
            }
            free(arr_content);

            // Parse [idx_var, val_var]
            char *vars_content = parse_bracket_content(p);
            char *idx_var = NULL;
            char *val_var = NULL;

            // Split on comma
            char *comma = strchr(vars_content, ',');
            if (comma) {
                *comma = '\0';
                idx_var = strdup(vars_content);
                // Trim whitespace
                while (*idx_var && isspace(*idx_var)) idx_var++;
                char *val_start = comma + 1;
                while (*val_start && isspace(*val_start)) val_start++;
                val_var = strdup(val_start);
                // Trim trailing whitespace
                char *end = val_var + strlen(val_var) - 1;
                while (end > val_var && isspace(*end)) *end-- = '\0';
            }
            free(vars_content);

            // Expect {enumerate}
            if (*p->p != '{') {
                parser_error(p, "Expected '{enumerate}' after enumerate variables");
                return NULL;
            }
            char *type_content = parse_brace_content(p);
            if (strcmp(type_content, "enumerate") != 0) {
                free(type_content);
                parser_error(p, "Expected 'enumerate' block type");
                return NULL;
            }
            free(type_content);

            // Parse body until \end{enumerate}
            const char *body_start = p->p;
            int depth = 1;
            while (*p->p && depth > 0) {
                if (strncmp(p->p, "\\begin<", 7) == 0) {
                    depth++;
                    p->p += 7;
                } else if (strncmp(p->p, "\\end{enumerate}", 15) == 0) {
                    depth--;
                    if (depth == 0) break;
                    p->p += 15;
                } else {
                    p->p++;
                }
            }

            size_t body_len = p->p - body_start;
            char *body_content = malloc(body_len + 1);
            memcpy(body_content, body_start, body_len);
            body_content[body_len] = '\0';

            if (match(p, "\\end{enumerate}")) {
                // Good
            }

            CalcParser body_parser;
            calc_parser_init(&body_parser, body_content);
            AstNode *body = calc_parse(&body_parser);
            free(body_content);
            propagate_error(p, &body_parser);
            if (p->had_error) {
                free(array_name);
                calc_ast_free(array_expr);
                free(idx_var);
                free(val_var);
                calc_ast_free(body);
                return NULL;
            }

            AstNode *result = calc_ast_enumerate(array_name, array_expr, idx_var, val_var, body);
            free(array_name);
            free(idx_var);
            free(val_var);
            return result;
        }

        // Unknown command - treat rest as string
        parser_error(p, "Unknown command");
        return NULL;
    }

    // If nothing matched, collect text until next command or end
    const char *start = p->p;
    while (*p->p && *p->p != '\\' && *p->p != '}' && *p->p != ']') {
        p->p++;
    }

    if (p->p > start) {
        size_t len = p->p - start;
        char *text = malloc(len + 1);
        memcpy(text, start, len);
        text[len] = '\0';

        // Try to parse as integer
        char *endptr;
        long val = strtol(text, &endptr, 10);
        if (*endptr == '\0') {
            free(text);
            return calc_ast_int_lit(val);
        }

        // Otherwise it's a string
        return calc_ast_string_lit(text);
    }

    return calc_ast_empty();
}

// Parse a sequence of expressions
AstNode *calc_parse(CalcParser *p) {
    int cap = 8;
    int n = 0;
    AstNode **stmts = calloc(cap, sizeof(AstNode *));

    while (!at_end(p) && !p->had_error) {
        skip_whitespace(p);
        if (at_end(p)) break;

        AstNode *expr = parse_expr(p);
        if (expr && expr->type != AST_EMPTY) {
            if (n >= cap) {
                cap *= 2;
                stmts = realloc(stmts, cap * sizeof(AstNode *));
            }
            stmts[n++] = expr;
        } else if (expr && expr->type == AST_EMPTY) {
            calc_ast_free(expr);
        }

        skip_whitespace(p);
    }

    if (n == 0) {
        free(stmts);
        return calc_ast_empty();
    }

    return calc_ast_seq(stmts, n);
}

AstNode *calc_parse_expr(CalcParser *p) {
    return parse_expr(p);
}

// ============================================================================
// Debug Printing
// ============================================================================

static const char *ast_type_name(AstNodeType type) {
    switch (type) {
    case AST_INT_LIT: return "INT";
    case AST_STRING_LIT: return "STRING";
    case AST_VALUEOF: return "VALUEOF";
    case AST_RECALL: return "RECALL";
    case AST_REF: return "REF";
    case AST_LET: return "LET";
    case AST_ASSIGN: return "ASSIGN";
    case AST_INC: return "INC";
    case AST_DEC: return "DEC";
    case AST_SETELEMENT: return "SETELEMENT";
    case AST_ADD: return "ADD";
    case AST_SUB: return "SUB";
    case AST_MUL: return "MUL";
    case AST_DIV: return "DIV";
    case AST_MOD: return "MOD";
    case AST_NEG: return "NEG";
    case AST_RAND: return "RAND";
    case AST_MAX: return "MAX";
    case AST_MIN: return "MIN";
    case AST_EQ: return "EQ";
    case AST_NE: return "NE";
    case AST_LT: return "LT";
    case AST_GT: return "GT";
    case AST_LE: return "LE";
    case AST_GE: return "GE";
    case AST_AND: return "AND";
    case AST_OR: return "OR";
    case AST_NOT: return "NOT";
    case AST_IF: return "IF";
    case AST_LOOP: return "LOOP";
    case AST_EXIT_WHEN: return "EXIT_WHEN";
    case AST_RETURN: return "RETURN";
    case AST_ARRAY_LIT: return "ARRAY";
    case AST_STRING_ARRAY_LIT: return "STRING_ARRAY";
    case AST_LEN: return "LEN";
    case AST_COPYARRAY: return "COPYARRAY";
    case AST_MEM_LOAD: return "MEM_LOAD";
    case AST_MEM_STORE: return "MEM_STORE";
    case AST_MEM_LEN: return "MEM_LEN";
    case AST_MEM_ALLOC: return "MEM_ALLOC";
    case AST_MAP_NEW: return "MAP_NEW";
    case AST_MAP_GET: return "MAP_GET";
    case AST_MAP_SET: return "MAP_SET";
    case AST_MAP_HAS: return "MAP_HAS";
    case AST_MAP_DEL: return "MAP_DEL";
    case AST_MAP_LEN: return "MAP_LEN";
    case AST_MAP_KEYS: return "MAP_KEYS";
    case AST_CURSOR: return "CURSOR";
    case AST_ANSI: return "ANSI";
    case AST_EMIT: return "EMIT";
    case AST_LAMBDA: return "LAMBDA";
    case AST_SEQ: return "SEQ";
    case AST_EMPTY: return "EMPTY";
    // New CL syntax nodes
    case AST_VAR_ACCESS: return "VAR_ACCESS";
    case AST_VAR_CONCAT: return "VAR_CONCAT";
    case AST_INVOKE: return "INVOKE";
    case AST_COLLECTION_GET: return "COLLECTION_GET";
    case AST_COLLECTION_SET: return "COLLECTION_SET";
    case AST_ADDRESSOF: return "ADDRESSOF";
    case AST_ENUMERATE: return "ENUMERATE";
    case AST_MAP_LIT: return "MAP_LIT";
    default: return "UNKNOWN";
    }
}

void calc_ast_print(AstNode *node, int indent) {
    if (!node) return;

    for (int i = 0; i < indent; i++) fprintf(stderr, "  ");
    fprintf(stderr, "%s", ast_type_name(node->type));

    switch (node->type) {
    case AST_INT_LIT: fprintf(stderr, "(%lld)\n", (long long)node->int_value); break;

    case AST_STRING_LIT: fprintf(stderr, "(\"%s\")\n", node->string_value); break;

    case AST_VALUEOF:
    case AST_RECALL:
    case AST_REF:
        fprintf(stderr, "<%s>\n", node->var.name);
        if (node->var.index) {
            calc_ast_print(node->var.index, indent + 1);
        }
        break;

    case AST_INC:
    case AST_DEC:
        if (node->collection.name) {
            fprintf(stderr, "<%s%s>\n", node->collection.is_deref ? "*" : "",
                    node->collection.name);
        } else {
            fprintf(stderr, "<%sdynamic>\n", node->collection.is_deref ? "*" : "");
            if (node->collection.target) {
                calc_ast_print(node->collection.target, indent + 1);
            }
        }
        break;

    case AST_LEN:
        fprintf(stderr, "\n");
        calc_ast_print(node->unary.operand, indent + 1);
        break;

    case AST_LET:
    case AST_ASSIGN:
        fprintf(stderr, "<%s%s>\n", node->binding.name, node->binding.is_array ? "[]" : "");
        calc_ast_print(node->binding.value, indent + 1);
        break;

    case AST_SETELEMENT:
        fprintf(stderr, "<%s>\n", node->setelement.array_name);
        calc_ast_print(node->setelement.index, indent + 1);
        calc_ast_print(node->setelement.value, indent + 1);
        break;

    case AST_ADD:
    case AST_SUB:
    case AST_MUL:
    case AST_DIV:
    case AST_MOD:
    case AST_MAX:
    case AST_MIN:
    case AST_EQ:
    case AST_NE:
    case AST_LT:
    case AST_GT:
    case AST_LE:
    case AST_GE:
    case AST_AND:
    case AST_OR:
    case AST_CURSOR:
        fprintf(stderr, "\n");
        calc_ast_print(node->binary.left, indent + 1);
        calc_ast_print(node->binary.right, indent + 1);
        break;

    case AST_NEG:
    case AST_RAND:
    case AST_NOT:
    case AST_EMIT:
        fprintf(stderr, "\n");
        calc_ast_print(node->unary.operand, indent + 1);
        break;

    case AST_ANSI: fprintf(stderr, "(\"%s\")\n", node->string_value); break;

    case AST_IF:
        fprintf(stderr, "\n");
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "COND:\n");
        calc_ast_print(node->if_node.condition, indent + 2);
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "THEN:\n");
        calc_ast_print(node->if_node.then_branch, indent + 2);
        if (node->if_node.else_branch) {
            for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
            fprintf(stderr, "ELSE:\n");
            calc_ast_print(node->if_node.else_branch, indent + 2);
        }
        break;

    case AST_LOOP:
        fprintf(stderr, "\n");
        calc_ast_print(node->loop.body, indent + 1);
        break;

    case AST_EXIT_WHEN:
    case AST_RETURN:
        fprintf(stderr, "\n");
        calc_ast_print(node->control.value, indent + 1);
        break;

    case AST_ARRAY_LIT:
        fprintf(stderr, "[%d elements]\n", node->array.n_elements);
        for (int i = 0; i < node->array.n_elements; i++) {
            calc_ast_print(node->array.elements[i], indent + 1);
        }
        break;

    case AST_STRING_ARRAY_LIT:
        fprintf(stderr, "[%d strings]\n", node->string_array.n_strings);
        for (int i = 0; i < node->string_array.n_strings; i++) {
            for (int j = 0; j < indent + 1; j++) fprintf(stderr, "  ");
            fprintf(stderr, "\"%s\"\n", node->string_array.strings[i]);
        }
        break;

    case AST_MEM_LOAD:
        fprintf(stderr, "\n");
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "ADDR:\n");
        calc_ast_print(node->mem.addr, indent + 2);
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "IDX:\n");
        calc_ast_print(node->mem.index, indent + 2);
        break;

    case AST_MEM_STORE:
        fprintf(stderr, "\n");
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "ADDR:\n");
        calc_ast_print(node->mem.addr, indent + 2);
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "IDX:\n");
        calc_ast_print(node->mem.index, indent + 2);
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "VAL:\n");
        calc_ast_print(node->mem.value, indent + 2);
        break;

    case AST_MEM_LEN:
    case AST_MEM_ALLOC:
        fprintf(stderr, "\n");
        calc_ast_print(node->mem.addr, indent + 1);
        break;

    case AST_MAP_NEW: fprintf(stderr, "\n"); break;

    case AST_MAP_GET:
    case AST_MAP_HAS:
    case AST_MAP_DEL:
        fprintf(stderr, "\n");
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "ADDR:\n");
        calc_ast_print(node->map.addr, indent + 2);
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "KEY:\n");
        calc_ast_print(node->map.key, indent + 2);
        break;

    case AST_MAP_SET:
        fprintf(stderr, "\n");
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "ADDR:\n");
        calc_ast_print(node->map.addr, indent + 2);
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "KEY:\n");
        calc_ast_print(node->map.key, indent + 2);
        for (int i = 0; i < indent + 1; i++) fprintf(stderr, "  ");
        fprintf(stderr, "VAL:\n");
        calc_ast_print(node->map.value, indent + 2);
        break;

    case AST_MAP_LEN:
    case AST_MAP_KEYS:
        fprintf(stderr, "\n");
        calc_ast_print(node->map.addr, indent + 1);
        break;

    case AST_LAMBDA:
        fprintf(stderr, "<%s>[", node->lambda.name ? node->lambda.name : "anon");
        for (int i = 0; i < node->lambda.n_params; i++) {
            if (i > 0) fprintf(stderr, ",");
            fprintf(stderr, "%s", node->lambda.params[i]);
        }
        fprintf(stderr, "]%s\n", node->lambda.is_computational ? "#" : "");
        calc_ast_print(node->lambda.body, indent + 1);
        break;

    case AST_SEQ:
        fprintf(stderr, " (%d stmts)\n", node->seq.n_stmts);
        for (int i = 0; i < node->seq.n_stmts; i++) {
            calc_ast_print(node->seq.stmts[i], indent + 1);
        }
        break;

    case AST_EMPTY: fprintf(stderr, "\n"); break;

    default: fprintf(stderr, "\n"); break;
    }
}

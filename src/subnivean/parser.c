// parser.c - Subnivean parser implementation
//
// Recursive descent parser for Hyades computational expressions.

#include "parser.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Forward Declarations
// ============================================================================

static AstNode *parse_statement(Parser *p);
static AstNode *parse_expression(Parser *p);
static AstNode *parse_block_until(Parser *p, TokenType end_type);
static AstNode *parse_primary(Parser *p);

// ============================================================================
// Parser Utilities
// ============================================================================

static void error(Parser *p, const char *format, ...) {
    if (p->panic_mode) return; // Don't cascade errors

    p->had_error = true;
    p->panic_mode = true;

    va_list args;
    va_start(args, format);
    int len = snprintf(p->error_msg, sizeof(p->error_msg), "[%d:%d] Error: ", p->current.line,
                       p->current.col);
    vsnprintf(p->error_msg + len, sizeof(p->error_msg) - len, format, args);
    va_end(args);

    fprintf(stderr, "%s\n", p->error_msg);
}

static void advance(Parser *p) {
    p->previous = p->current;
    p->current = lexer_next(&p->lexer);

    // Skip error tokens
    while (p->current.type == TOK_ERROR) {
        error(p, "%.*s", p->current.length, p->current.start);
        p->current = lexer_next(&p->lexer);
    }
}

static bool check(Parser *p, TokenType type) {
    return p->current.type == type;
}

static bool match(Parser *p, TokenType type) {
    if (!check(p, type)) return false;
    advance(p);
    return true;
}

static void consume(Parser *p, TokenType type, const char *message) {
    if (p->current.type == type) {
        advance(p);
        return;
    }
    error(p, "%s (got %s)", message, token_type_name(p->current.type));
}

// Extract text from a TEXT token
static char *extract_text(Token *tok) {
    char *text = malloc(tok->length + 1);
    if (text) {
        memcpy(text, tok->start, tok->length);
        text[tok->length] = '\0';
    }
    return text;
}

// Parse a name from <name> syntax
static char *parse_angle_name(Parser *p) {
    consume(p, TOK_LANGLE, "Expected '<' before name");
    if (p->had_error) return NULL;

    if (!check(p, TOK_TEXT) && !check(p, TOK_INT)) {
        error(p, "Expected name after '<'");
        return NULL;
    }

    char *name = extract_text(&p->current);
    advance(p);

    consume(p, TOK_RANGLE, "Expected '>' after name");
    return name;
}

// Parse braced content as a block: {statements}
static AstNode *parse_braced_block(Parser *p) {
    consume(p, TOK_LBRACE, "Expected '{'");
    if (p->had_error) return NULL;

    AstNode *block = parse_block_until(p, TOK_RBRACE);

    consume(p, TOK_RBRACE, "Expected '}'");
    return block;
}

// Parse braced content as a single expression: {expr}
static AstNode *parse_braced_expression(Parser *p) {
    consume(p, TOK_LBRACE, "Expected '{'");
    if (p->had_error) return NULL;

    AstNode *expr = parse_expression(p);

    consume(p, TOK_RBRACE, "Expected '}'");
    return expr;
}

// ============================================================================
// Expression Parsing
// ============================================================================

// Parse binary operator: \op{left, right}
static AstNode *parse_binary_op(Parser *p, AstType ast_type) {
    int line = p->previous.line;
    int col = p->previous.col;

    consume(p, TOK_LBRACE, "Expected '{' after operator");
    if (p->had_error) return NULL;

    AstNode *left = parse_expression(p);
    if (!left) return NULL;

    consume(p, TOK_COMMA, "Expected ',' between operands");
    if (p->had_error) {
        ast_free(left);
        return NULL;
    }

    AstNode *right = parse_expression(p);
    if (!right) {
        ast_free(left);
        return NULL;
    }

    consume(p, TOK_RBRACE, "Expected '}' after operands");
    if (p->had_error) {
        ast_free(left);
        ast_free(right);
        return NULL;
    }

    p->nodes_created++;
    return ast_binary(ast_type, left, right, line, col);
}

// Parse unary operator: \op{operand}
static AstNode *parse_unary_op(Parser *p, AstType ast_type) {
    int line = p->previous.line;
    int col = p->previous.col;

    consume(p, TOK_LBRACE, "Expected '{' after operator");
    if (p->had_error) return NULL;

    AstNode *operand = parse_expression(p);
    if (!operand) return NULL;

    consume(p, TOK_RBRACE, "Expected '}' after operand");
    if (p->had_error) {
        ast_free(operand);
        return NULL;
    }

    p->nodes_created++;
    return ast_unary(ast_type, operand, line, col);
}

// Parse \valueof<name> or \valueof<name>[index]
static AstNode *parse_valueof(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    char *name = parse_angle_name(p);
    if (!name) return NULL;

    // Check for array index
    if (match(p, TOK_LBRACKET)) {
        AstNode *index = parse_expression(p);
        consume(p, TOK_RBRACKET, "Expected ']' after index");
        if (p->had_error || !index) {
            free(name);
            ast_free(index);
            return NULL;
        }

        AstNode *node = ast_new(AST_ARRAY_GET, line, col);
        node->array_get.name = name;
        node->array_get.index = index;
        p->nodes_created++;
        return node;
    }

    // Simple variable reference
    AstNode *node = ast_new(AST_VAR, line, col);
    node->var.name = name;
    p->nodes_created++;
    return node;
}

// Parse \recall<name> or \recall<name>[args]
static AstNode *parse_recall(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    char *name = parse_angle_name(p);
    if (!name) return NULL;

    // Check for arguments
    AstNode **args = NULL;
    int n_args = 0;

    if (match(p, TOK_LBRACKET)) {
        // Parse argument list
        int args_cap = 4;
        args = calloc(args_cap, sizeof(AstNode *));

        if (!check(p, TOK_RBRACKET)) {
            do {
                AstNode *arg = parse_expression(p);
                if (!arg) {
                    // Cleanup
                    for (int i = 0; i < n_args; i++) ast_free(args[i]);
                    free(args);
                    free(name);
                    return NULL;
                }

                if (n_args >= args_cap) {
                    args_cap *= 2;
                    args = realloc(args, args_cap * sizeof(AstNode *));
                }
                args[n_args++] = arg;
            } while (match(p, TOK_COMMA));
        }

        consume(p, TOK_RBRACKET, "Expected ']' after arguments");
        if (p->had_error) {
            for (int i = 0; i < n_args; i++) ast_free(args[i]);
            free(args);
            free(name);
            return NULL;
        }
    }

    p->nodes_created++;
    return ast_call(name, args, n_args, line, col);
}

// Parse \len<name>
static AstNode *parse_len(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    char *name = parse_angle_name(p);
    if (!name) return NULL;

    AstNode *node = ast_new(AST_ARRAY_LEN, line, col);
    node->array_op.name = name;
    p->nodes_created++;
    return node;
}

// Parse \ref<name>
static AstNode *parse_ref(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    char *name = parse_angle_name(p);
    if (!name) return NULL;

    AstNode *node = ast_new(AST_REF, line, col);
    node->var.name = name;
    p->nodes_created++;
    return node;
}

// Parse \pop<name> or \peek<name>
static AstNode *parse_pop_peek(Parser *p, AstType type) {
    int line = p->previous.line;
    int col = p->previous.col;

    char *name = parse_angle_name(p);
    if (!name) return NULL;

    AstNode *node = ast_new(type, line, col);
    node->array_op.name = name;
    p->nodes_created++;
    return node;
}

// Primary expression
static AstNode *parse_primary(Parser *p) {
    int line = p->current.line;
    int col = p->current.col;

    // Integer literal
    if (match(p, TOK_INT)) {
        p->nodes_created++;
        return ast_int(p->previous.int_value, line, col);
    }

    // Parenthesized expression (or sub-block)
    if (match(p, TOK_LBRACE)) {
        AstNode *expr = parse_expression(p);
        consume(p, TOK_RBRACE, "Expected '}' after expression");
        return expr;
    }

    // Commands that return values
    if (match(p, TOK_CMD_VALUEOF)) return parse_valueof(p);
    if (match(p, TOK_CMD_RECALL)) return parse_recall(p);
    if (match(p, TOK_CMD_LEN)) return parse_len(p);
    if (match(p, TOK_CMD_REF)) return parse_ref(p);
    if (match(p, TOK_CMD_POP)) return parse_pop_peek(p, AST_ARRAY_POP);
    if (match(p, TOK_CMD_PEEK)) return parse_pop_peek(p, AST_ARRAY_PEEK);

    // Arithmetic
    if (match(p, TOK_CMD_ADD)) return parse_binary_op(p, AST_ADD);
    if (match(p, TOK_CMD_SUB)) return parse_binary_op(p, AST_SUB);
    if (match(p, TOK_CMD_MUL)) return parse_binary_op(p, AST_MUL);
    if (match(p, TOK_CMD_DIV)) return parse_binary_op(p, AST_DIV);
    if (match(p, TOK_CMD_MOD)) return parse_binary_op(p, AST_MOD);

    // Comparison
    if (match(p, TOK_CMD_EQ)) return parse_binary_op(p, AST_EQ);
    if (match(p, TOK_CMD_NE)) return parse_binary_op(p, AST_NE);
    if (match(p, TOK_CMD_LT)) return parse_binary_op(p, AST_LT);
    if (match(p, TOK_CMD_GT)) return parse_binary_op(p, AST_GT);
    if (match(p, TOK_CMD_LE)) return parse_binary_op(p, AST_LE);
    if (match(p, TOK_CMD_GE)) return parse_binary_op(p, AST_GE);

    // Logic
    if (match(p, TOK_CMD_AND)) return parse_binary_op(p, AST_AND);
    if (match(p, TOK_CMD_OR)) return parse_binary_op(p, AST_OR);
    if (match(p, TOK_CMD_NOT)) return parse_unary_op(p, AST_NOT);

    // Text (passthrough)
    if (match(p, TOK_TEXT)) {
        p->nodes_created++;
        return ast_text_splice(p->previous.start, p->previous.length, line, col);
    }

    // Unknown command (passthrough to text interpreter)
    if (match(p, TOK_UNKNOWN_CMD)) {
        // Capture the whole command including any following braces
        const char *start = p->previous.start;
        int length = p->previous.length;

        // TODO: Could capture following {content} as well for full passthrough
        p->nodes_created++;
        return ast_text_splice(start, length, line, col);
    }

    error(p, "Expected expression, got %s", token_type_name(p->current.type));
    return NULL;
}

// Expression (for now, just primary - Hyades uses prefix notation)
static AstNode *parse_expression(Parser *p) {
    return parse_primary(p);
}

// ============================================================================
// Statement Parsing
// ============================================================================

// Parse \let<name>{value} or \let<name[]>{[...]}
static AstNode *parse_let(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    consume(p, TOK_LANGLE, "Expected '<' after \\let");
    if (p->had_error) return NULL;

    if (!check(p, TOK_TEXT)) {
        error(p, "Expected variable name after '<'");
        return NULL;
    }

    char *name = extract_text(&p->current);
    advance(p);

    // Check for array declaration: name[]
    bool is_array = false;
    if (match(p, TOK_LBRACKET)) {
        consume(p, TOK_RBRACKET, "Expected ']' for array declaration");
        is_array = true;
    }

    consume(p, TOK_RANGLE, "Expected '>' after name");
    if (p->had_error) {
        free(name);
        return NULL;
    }

    consume(p, TOK_LBRACE, "Expected '{' for value");
    if (p->had_error) {
        free(name);
        return NULL;
    }

    if (is_array) {
        // Parse array initializer [a, b, c]
        consume(p, TOK_LBRACKET, "Expected '[' for array initializer");
        if (p->had_error) {
            free(name);
            return NULL;
        }

        // Parse elements
        int cap = 8;
        AstNode **elements = calloc(cap, sizeof(AstNode *));
        int n_elements = 0;

        if (!check(p, TOK_RBRACKET)) {
            do {
                AstNode *elem = parse_expression(p);
                if (!elem) {
                    for (int i = 0; i < n_elements; i++) ast_free(elements[i]);
                    free(elements);
                    free(name);
                    return NULL;
                }
                if (n_elements >= cap) {
                    cap *= 2;
                    elements = realloc(elements, cap * sizeof(AstNode *));
                }
                elements[n_elements++] = elem;
            } while (match(p, TOK_COMMA));
        }

        consume(p, TOK_RBRACKET, "Expected ']' after array elements");
        consume(p, TOK_RBRACE, "Expected '}' after array initializer");

        if (p->had_error) {
            for (int i = 0; i < n_elements; i++) ast_free(elements[i]);
            free(elements);
            free(name);
            return NULL;
        }

        // Create array literal node
        AstNode *array_lit = ast_new(AST_ARRAY_LITERAL, line, col);
        array_lit->array_lit.elements = elements;
        array_lit->array_lit.n_elements = n_elements;

        // Create let_array node
        AstNode *node = ast_new(AST_LET_ARRAY, line, col);
        node->let_array.name = name;
        node->let_array.init = array_lit;
        p->nodes_created += 2;
        return node;
    } else {
        // Simple value
        AstNode *value = parse_expression(p);
        consume(p, TOK_RBRACE, "Expected '}' after value");

        if (p->had_error || !value) {
            free(name);
            ast_free(value);
            return NULL;
        }

        p->nodes_created++;
        return ast_let(name, value, line, col);
    }
}

// Parse \assign<name>{value}
static AstNode *parse_assign(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    char *name = parse_angle_name(p);
    if (!name) return NULL;

    AstNode *value = parse_braced_expression(p);
    if (!value) {
        free(name);
        return NULL;
    }

    p->nodes_created++;
    return ast_assign(name, value, line, col);
}

// Parse \inc<name> or \dec<name>
static AstNode *parse_inc_dec(Parser *p, bool is_inc) {
    int line = p->previous.line;
    int col = p->previous.col;

    char *name = parse_angle_name(p);
    if (!name) return NULL;

    p->nodes_created++;
    return is_inc ? ast_inc(name, line, col) : ast_dec(name, line, col);
}

// Parse \if{cond}{then}\else{else}
static AstNode *parse_if(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    // Condition
    AstNode *cond = parse_braced_expression(p);
    if (!cond) return NULL;

    // Then branch
    AstNode *then_branch = parse_braced_block(p);
    if (!then_branch) {
        ast_free(cond);
        return NULL;
    }

    // Optional else branch
    AstNode *else_branch = NULL;
    if (match(p, TOK_CMD_ELSE)) {
        else_branch = parse_braced_block(p);
        if (!else_branch) {
            ast_free(cond);
            ast_free(then_branch);
            return NULL;
        }
    }

    p->nodes_created++;
    return ast_if(cond, then_branch, else_branch, line, col);
}

// Parse \begin{loop}...\end{loop}
static AstNode *parse_loop(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    // Expect {loop}
    consume(p, TOK_LBRACE, "Expected '{' after \\begin");
    if (p->had_error) return NULL;

    if (!check(p, TOK_TEXT) || strncmp(p->current.start, "loop", 4) != 0) {
        error(p, "Expected 'loop' after \\begin{");
        return NULL;
    }
    advance(p); // Skip "loop"

    consume(p, TOK_RBRACE, "Expected '}' after 'loop'");
    if (p->had_error) return NULL;

    // Parse body until \end{loop}
    AstNode *body = ast_block(line, col);
    p->nodes_created++;

    while (!check(p, TOK_EOF)) {
        // Check for \end{loop}
        if (check(p, TOK_CMD_END)) {
            advance(p);
            consume(p, TOK_LBRACE, "Expected '{' after \\end");
            if (check(p, TOK_TEXT) && strncmp(p->current.start, "loop", 4) == 0) {
                advance(p);
                consume(p, TOK_RBRACE, "Expected '}' after 'loop'");
                break;
            }
            error(p, "Expected 'loop' after \\end{");
            ast_free(body);
            return NULL;
        }

        AstNode *stmt = parse_statement(p);
        if (stmt) {
            ast_block_append(body, stmt);
        }

        if (p->panic_mode) {
            // Try to recover
            p->panic_mode = false;
        }
    }

    p->nodes_created++;
    return ast_loop(body, line, col);
}

// Parse \exit_when{condition}
static AstNode *parse_exit_when(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    AstNode *cond = parse_braced_expression(p);
    if (!cond) return NULL;

    p->nodes_created++;
    return ast_exit_when(cond, line, col);
}

// Parse \lambda<name>[params]{body} or \lambda<name>[params]#{body}
static AstNode *parse_lambda(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    // Name
    char *name = parse_angle_name(p);
    if (!name) return NULL;

    // Parameters (optional)
    char **params = NULL;
    int n_params = 0;

    if (match(p, TOK_LBRACKET)) {
        int params_cap = 4;
        params = calloc(params_cap, sizeof(char *));

        if (!check(p, TOK_RBRACKET)) {
            do {
                if (!check(p, TOK_TEXT)) {
                    error(p, "Expected parameter name");
                    for (int i = 0; i < n_params; i++) free(params[i]);
                    free(params);
                    free(name);
                    return NULL;
                }

                if (n_params >= params_cap) {
                    params_cap *= 2;
                    params = realloc(params, params_cap * sizeof(char *));
                }
                params[n_params++] = extract_text(&p->current);
                advance(p);
            } while (match(p, TOK_COMMA));
        }

        consume(p, TOK_RBRACKET, "Expected ']' after parameters");
        if (p->had_error) {
            for (int i = 0; i < n_params; i++) free(params[i]);
            free(params);
            free(name);
            return NULL;
        }
    }

    // Check for computational mode: #{
    bool is_computational = match(p, TOK_HASH);

    // Body
    AstNode *body = parse_braced_block(p);
    if (!body) {
        for (int i = 0; i < n_params; i++) free(params[i]);
        free(params);
        free(name);
        return NULL;
    }

    p->nodes_created++;
    return ast_lambda(name, params, n_params, body, is_computational, line, col);
}

// Parse \return{value}
static AstNode *parse_return(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    AstNode *value = parse_braced_expression(p);
    // value can be NULL for empty return

    p->nodes_created++;
    return ast_return(value, line, col);
}

// Parse \push<name>{value}
static AstNode *parse_push(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    char *name = parse_angle_name(p);
    if (!name) return NULL;

    AstNode *value = parse_braced_expression(p);
    if (!value) {
        free(name);
        return NULL;
    }

    AstNode *node = ast_new(AST_ARRAY_PUSH, line, col);
    node->array_push.name = name;
    node->array_push.value = value;
    p->nodes_created++;
    return node;
}

// Parse \setelement<name>[index]{value}
static AstNode *parse_setelement(Parser *p) {
    int line = p->previous.line;
    int col = p->previous.col;

    char *name = parse_angle_name(p);
    if (!name) return NULL;

    consume(p, TOK_LBRACKET, "Expected '[' for index");
    if (p->had_error) {
        free(name);
        return NULL;
    }

    AstNode *index = parse_expression(p);
    if (!index) {
        free(name);
        return NULL;
    }

    consume(p, TOK_RBRACKET, "Expected ']' after index");
    if (p->had_error) {
        free(name);
        ast_free(index);
        return NULL;
    }

    AstNode *value = parse_braced_expression(p);
    if (!value) {
        free(name);
        ast_free(index);
        return NULL;
    }

    AstNode *node = ast_new(AST_ARRAY_SET, line, col);
    node->array_set.name = name;
    node->array_set.index = index;
    node->array_set.value = value;
    p->nodes_created++;
    return node;
}

// Parse a single statement
static AstNode *parse_statement(Parser *p) {
    // Variable statements
    if (match(p, TOK_CMD_LET)) return parse_let(p);
    if (match(p, TOK_CMD_ASSIGN)) return parse_assign(p);
    if (match(p, TOK_CMD_INC)) return parse_inc_dec(p, true);
    if (match(p, TOK_CMD_DEC)) return parse_inc_dec(p, false);

    // Control flow
    if (match(p, TOK_CMD_IF)) return parse_if(p);
    if (match(p, TOK_CMD_BEGIN)) return parse_loop(p);
    if (match(p, TOK_CMD_EXIT_WHEN)) return parse_exit_when(p);

    // Functions
    if (match(p, TOK_CMD_LAMBDA)) return parse_lambda(p);
    if (match(p, TOK_CMD_RETURN)) return parse_return(p);

    // Arrays
    if (match(p, TOK_CMD_PUSH)) return parse_push(p);
    if (match(p, TOK_CMD_SETELEMENT)) return parse_setelement(p);

    // Expression statement (or passthrough)
    AstNode *expr = parse_expression(p);
    if (expr) {
        // Wrap in expression statement if needed
        if (expr->type != AST_TEXT_SPLICE) {
            AstNode *stmt = ast_new(AST_EXPR_STMT, expr->line, expr->col);
            stmt->expr_stmt.expr = expr;
            p->nodes_created++;
            return stmt;
        }
        return expr;
    }

    return NULL;
}

// Parse statements until end token
static AstNode *parse_block_until(Parser *p, TokenType end_type) {
    AstNode *block = ast_block(p->current.line, p->current.col);
    p->nodes_created++;

    while (!check(p, end_type) && !check(p, TOK_EOF)) {
        AstNode *stmt = parse_statement(p);
        if (stmt) {
            ast_block_append(block, stmt);
        }

        if (p->panic_mode) {
            // Skip tokens until we find a sync point
            p->panic_mode = false;
            while (!check(p, end_type) && !check(p, TOK_EOF)) {
                if (check(p, TOK_CMD_LET) || check(p, TOK_CMD_IF) || check(p, TOK_CMD_BEGIN) ||
                    check(p, TOK_CMD_LAMBDA)) {
                    break; // Found a statement start
                }
                advance(p);
            }
        }
    }

    return block;
}

// ============================================================================
// Public Interface
// ============================================================================

void parser_init(Parser *p, const char *source) {
    lexer_init(&p->lexer, source);
    p->had_error = false;
    p->panic_mode = false;
    p->error_msg[0] = '\0';
    p->nodes_created = 0;

    // Prime the parser with first token
    advance(p);
}

void parser_free(Parser *p) {
    // Nothing to free currently - lexer doesn't own memory
    (void)p;
}

AstNode *parser_parse(Parser *p) {
    AstNode *program = parse_block_until(p, TOK_EOF);

    if (p->had_error) {
        ast_free(program);
        return NULL;
    }

    return program;
}

AstNode *parser_parse_statement(Parser *p) {
    return parse_statement(p);
}

AstNode *parser_parse_expression(Parser *p) {
    return parse_expression(p);
}

bool parser_had_error(Parser *p) {
    return p->had_error;
}

const char *parser_error_msg(Parser *p) {
    return p->error_msg;
}

int parser_nodes_created(Parser *p) {
    return p->nodes_created;
}

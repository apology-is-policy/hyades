// ast.h - Subnivean Abstract Syntax Tree
//
// AST node definitions for the Hyades computational language subset.

#ifndef SUBNIVEAN_AST_H
#define SUBNIVEAN_AST_H

#include <stdbool.h>
#include <stdint.h>

// Forward declaration
typedef struct AstNode AstNode;

typedef enum {
    // ========================================================================
    // Literals
    // ========================================================================
    AST_INT,    // Integer literal
    AST_STRING, // String/text literal
    AST_VAR,    // Variable reference: \valueof<name>

    // ========================================================================
    // Arithmetic Expressions
    // ========================================================================
    AST_ADD, // \add{a,b}
    AST_SUB, // \sub{a,b}
    AST_MUL, // \mul{a,b}
    AST_DIV, // \div{a,b}
    AST_MOD, // \mod{a,b}
    AST_NEG, // Unary negation (from negative literals)

    // ========================================================================
    // Comparison Expressions
    // ========================================================================
    AST_EQ, // \eq{a,b}
    AST_NE, // \ne{a,b}
    AST_LT, // \lt{a,b}
    AST_GT, // \gt{a,b}
    AST_LE, // \le{a,b}
    AST_GE, // \ge{a,b}

    // ========================================================================
    // Logic Expressions
    // ========================================================================
    AST_AND, // \and{a,b}
    AST_OR,  // \or{a,b}
    AST_NOT, // \not{a}

    // ========================================================================
    // Variable Statements
    // ========================================================================
    AST_LET,       // \let<name>{value}
    AST_LET_ARRAY, // \let<name[]>{[...]}
    AST_ASSIGN,    // \assign<name>{value}
    AST_INC,       // \inc<name>
    AST_DEC,       // \dec<name>

    // ========================================================================
    // Control Flow
    // ========================================================================
    AST_IF,        // \if{cond}{then}\else{else}
    AST_LOOP,      // \begin{loop}...\end{loop}
    AST_EXIT_WHEN, // \exit_when{cond}
    AST_ENUMERATE, // \begin<arr>[i,v]{enumerate}...\end{enumerate}

    // ========================================================================
    // Functions
    // ========================================================================
    AST_LAMBDA, // \lambda<name>[params]{body}
    AST_CALL,   // \recall<name>[args]
    AST_RETURN, // \return{value}
    AST_REF,    // \ref<name> (reference to variable name)

    // ========================================================================
    // Arrays
    // ========================================================================
    AST_ARRAY_LITERAL, // [a, b, c]
    AST_ARRAY_GET,     // \recall<arr>[idx] or \valueof<arr>[idx]
    AST_ARRAY_SET,     // \setelement<arr>[idx]{val}
    AST_ARRAY_LEN,     // \len<arr>
    AST_ARRAY_PUSH,    // \push<arr>{val}
    AST_ARRAY_POP,     // \pop<arr>
    AST_ARRAY_PEEK,    // \peek<arr>

    // ========================================================================
    // Compound
    // ========================================================================
    AST_BLOCK,     // Sequence of statements
    AST_EXPR_STMT, // Expression used as statement

    // ========================================================================
    // Interop
    // ========================================================================
    AST_TEXT_SPLICE, // Raw text to pass through to output

    AST_COUNT // Number of node types
} AstType;

// AST node structure using tagged union
struct AstNode {
    AstType type;
    int line; // Source line (for error reporting)
    int col;  // Source column

    union {
        // AST_INT
        int32_t int_val;

        // AST_STRING, AST_TEXT_SPLICE
        struct {
            char *text;
            int length;
        } string;

        // AST_VAR, AST_INC, AST_DEC, AST_REF
        struct {
            char *name;
        } var;

        // AST_LET, AST_ASSIGN
        struct {
            char *name;
            AstNode *value;
        } let;

        // AST_LET_ARRAY
        struct {
            char *name;
            AstNode *init; // AST_ARRAY_LITERAL
        } let_array;

        // Binary operations (AST_ADD, AST_EQ, AST_AND, etc.)
        struct {
            AstNode *left;
            AstNode *right;
        } binary;

        // Unary operations (AST_NOT, AST_NEG)
        struct {
            AstNode *operand;
        } unary;

        // AST_IF
        struct {
            AstNode *condition;
            AstNode *then_branch;
            AstNode *else_branch; // May be NULL
        } if_stmt;

        // AST_LOOP
        struct {
            AstNode *body; // AST_BLOCK
        } loop;

        // AST_EXIT_WHEN
        struct {
            AstNode *condition;
        } exit_when;

        // AST_ENUMERATE
        struct {
            char *array_name;  // Name of array to iterate
            char *index_var;   // Index variable name
            char *element_var; // Element variable name
            AstNode *body;     // AST_BLOCK
        } enumerate;

        // AST_LAMBDA
        struct {
            char *name;
            char **params;
            int n_params;
            AstNode *body;         // AST_BLOCK
            bool is_computational; // true for #{...}, false for {...}
        } lambda;

        // AST_CALL
        struct {
            char *name;
            AstNode **args;
            int n_args;
        } call;

        // AST_RETURN
        struct {
            AstNode *value; // May be NULL for bare \return
        } ret;

        // AST_ARRAY_LITERAL
        struct {
            AstNode **elements;
            int n_elements;
        } array_lit;

        // AST_ARRAY_GET
        struct {
            char *name;
            AstNode *index;
        } array_get;

        // AST_ARRAY_SET
        struct {
            char *name;
            AstNode *index;
            AstNode *value;
        } array_set;

        // AST_ARRAY_LEN, AST_ARRAY_POP, AST_ARRAY_PEEK
        struct {
            char *name;
        } array_op;

        // AST_ARRAY_PUSH
        struct {
            char *name;
            AstNode *value;
        } array_push;

        // AST_BLOCK
        struct {
            AstNode **stmts;
            int n_stmts;
            int capacity;
        } block;

        // AST_EXPR_STMT
        struct {
            AstNode *expr;
        } expr_stmt;
    };
};

// ============================================================================
// AST Construction
// ============================================================================

// Allocate a new AST node
AstNode *ast_new(AstType type, int line, int col);

// Convenience constructors
AstNode *ast_int(int32_t value, int line, int col);
AstNode *ast_string(const char *text, int length, int line, int col);
AstNode *ast_var(const char *name, int line, int col);

AstNode *ast_binary(AstType type, AstNode *left, AstNode *right, int line, int col);
AstNode *ast_unary(AstType type, AstNode *operand, int line, int col);

AstNode *ast_let(const char *name, AstNode *value, int line, int col);
AstNode *ast_assign(const char *name, AstNode *value, int line, int col);
AstNode *ast_inc(const char *name, int line, int col);
AstNode *ast_dec(const char *name, int line, int col);

AstNode *ast_if(AstNode *cond, AstNode *then_b, AstNode *else_b, int line, int col);
AstNode *ast_loop(AstNode *body, int line, int col);
AstNode *ast_exit_when(AstNode *cond, int line, int col);

AstNode *ast_lambda(const char *name, char **params, int n_params, AstNode *body,
                    bool is_computational, int line, int col);
AstNode *ast_call(const char *name, AstNode **args, int n_args, int line, int col);
AstNode *ast_return(AstNode *value, int line, int col);

AstNode *ast_block(int line, int col);
void ast_block_append(AstNode *block, AstNode *stmt);

AstNode *ast_text_splice(const char *text, int length, int line, int col);

// ============================================================================
// AST Utilities
// ============================================================================

// Free an AST node and all children
void ast_free(AstNode *node);

// Print AST for debugging (to stderr)
void ast_print(const AstNode *node, int indent);

// Get human-readable name for AST type
const char *ast_type_name(AstType type);

// Deep copy an AST node
AstNode *ast_clone(const AstNode *node);

#endif // SUBNIVEAN_AST_H

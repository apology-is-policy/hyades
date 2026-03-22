// calc_ast.h - AST for Hyades Computational Expressions
//
// This defines the AST nodes for parsing Hyades calc expressions
// that appear in computational lambdas (#{...}).

#ifndef CALC_AST_H
#define CALC_AST_H

#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// Type System for New CL Syntax
// ============================================================================

typedef enum {
    CL_TYPE_INFERRED,     // No type annotation (reassignment)
    CL_TYPE_INT,          // int - 64-bit signed integer
    CL_TYPE_STRING,       // string - UTF-8 immutable
    CL_TYPE_INT_ARRAY,    // int[] - mutable integer array
    CL_TYPE_STRING_ARRAY, // string[] - mutable string array
    CL_TYPE_MAP,          // map - Robin Hood hash map
    CL_TYPE_ADDRESS,      // address - handle/reference to collection
} CLType;

// ============================================================================
// AST Node Types
// ============================================================================

typedef enum {
    // Literals
    AST_INT_LIT,    // Integer literal: 42
    AST_STRING_LIT, // String literal (from content)

    // Variables (legacy syntax)
    AST_VALUEOF, // \valueof<name> or \valueof<name>[idx]
    AST_RECALL,  // \recall<name> or \recall<name>[args...]
    AST_REF,     // \ref<name> - reference to array/lambda

    // Variables (new CL syntax)
    AST_VAR_ACCESS,     // ${name} - unified variable access
    AST_VAR_CONCAT,     // ${prefix${i}suffix} - concatenated variable name
    AST_INVOKE,         // \invoke<name>[args] - function call
    AST_COLLECTION_GET, // \at<name>[key] - read collection element
    AST_COLLECTION_SET, // \set<name>[key]{val} - write collection element
    AST_ADDRESSOF,      // \addressof<name> - get address of named collection

    // Binding
    AST_LET,        // \let<name>{value} or \let<name:type>{value}
    AST_ASSIGN,     // \assign<name>{value} - immutable value
    AST_INC,        // \inc<name>
    AST_DEC,        // \dec<name>
    AST_SETELEMENT, // \setelement<arr>[idx]{value} (legacy)

    // Arithmetic
    AST_ADD,  // \add{a,b}
    AST_SUB,  // \sub{a,b}
    AST_MUL,  // \mul{a,b}
    AST_DIV,  // \div{a,b}
    AST_MOD,  // \mod{a,b}
    AST_NEG,  // \neg{a} (unary minus)
    AST_RAND, // \rand{max} - random int [0, max)
    AST_MAX,  // \max{a,b}
    AST_MIN,  // \min{a,b}

    // Comparison
    AST_EQ, // \eq{a,b}
    AST_NE, // \ne{a,b}
    AST_LT, // \lt{a,b}
    AST_GT, // \gt{a,b}
    AST_LE, // \le{a,b}
    AST_GE, // \ge{a,b}

    // Logic
    AST_AND, // \and{a,b}
    AST_OR,  // \or{a,b}
    AST_NOT, // \not{a}

    // Control Flow
    AST_IF,        // \if{cond}{then}\else{else}
    AST_LOOP,      // \begin{loop}...\exit_when{cond}...\end{loop}
    AST_EXIT_WHEN, // \exit_when{cond}
    AST_RETURN,    // \return{value}
    AST_ENUMERATE, // \begin<arr>[i,v]{enumerate}...\end{enumerate}

    // Arrays
    AST_ARRAY_LIT,        // [1,2,3]
    AST_STRING_ARRAY_LIT, // [str1,str2,str3] - string array for persistent store
    AST_MAP_LIT,          // |1->10, 2->20| (new syntax)
    AST_LEN,              // \len<arr> or \len<*addr> (unified)
    AST_COPYARRAY,        // \copyarray<dest>{source_name_expr}

    // Heap Memory (persistent store)
    AST_MEM_LOAD,  // \mem_load{addr,idx}
    AST_MEM_STORE, // \mem_store{addr,idx,val}
    AST_MEM_LEN,   // \mem_len{addr}
    AST_MEM_ALLOC, // \mem_alloc{count}

    // Maps (Robin Hood hash tables in persistent store)
    AST_MAP_NEW,  // \map_new{} - create empty map, returns addr
    AST_MAP_GET,  // \map_get{addr,key} - get value (0 if missing)
    AST_MAP_SET,  // \map_set{addr,key,val} - set key-value pair
    AST_MAP_HAS,  // \map_has{addr,key} - check if key exists (0/1)
    AST_MAP_DEL,  // \map_del{addr,key} - delete key (returns 0/1)
    AST_MAP_LEN,  // \map_len{addr} - number of entries
    AST_MAP_KEYS, // \map_keys{addr} - returns array addr of keys

    // Output (for rendering lambdas)
    AST_CURSOR, // \cursor{row,col} - emit cursor positioning
    AST_ANSI,   // \ansi{codes} - emit ANSI escape sequence
    AST_EMIT,   // \emit{expr} - emit value as text

    // Lambda
    AST_LAMBDA, // \lambda<name>[params]{body}

    // Sequence
    AST_SEQ,   // Sequence of expressions
    AST_EMPTY, // Empty/no-op
} AstNodeType;

// ============================================================================
// AST Node
// ============================================================================

typedef struct AstNode AstNode;

struct AstNode {
    AstNodeType type;

    union {
        // AST_INT_LIT
        int64_t int_value;

        // AST_STRING_LIT
        char *string_value;

        // AST_VALUEOF, AST_RECALL, AST_REF, AST_LEN, AST_INC, AST_DEC
        struct {
            char *name;
            AstNode *index; // NULL if no index
        } var;

        // AST_VAR_ACCESS (${name} new syntax)
        struct {
            char *name;            // Static name (NULL if dynamic)
            AstNode *dynamic_name; // Dynamic name expression (NULL if static)
            bool is_deref;         // true if *name (dereference)
        } var_access;

        // AST_VAR_CONCAT (${prefix${i}suffix} - concatenated variable name)
        struct {
            AstNode **parts; // Array of string literals and expressions
            int n_parts;
            bool is_deref; // true if *prefix (dereference result)
        } var_concat;

        // AST_INVOKE (\invoke<name>[args])
        struct {
            char *name;      // Direct name (NULL if dynamic)
            AstNode *target; // Dynamic target expression (NULL if direct)
            AstNode **args;
            int n_args;
            bool is_deref; // true if *name (dereference)
        } invoke;

        // AST_COLLECTION_GET (\at<name>[key]) and AST_COLLECTION_SET (\set<name>[key]{val})
        struct {
            char *name;      // Direct name (NULL if dynamic)
            AstNode *target; // Dynamic target expression (NULL if direct)
            AstNode *key;    // Index/key expression
            AstNode *value;  // Value for set (NULL for get)
            bool is_deref;   // true if *name (dereference)
        } collection;

        // AST_ADDRESSOF (\addressof<name>)
        struct {
            char *name; // Collection name
        } addressof;

        // AST_ENUMERATE (\begin<arr>[i,v]{enumerate}...\end{enumerate})
        struct {
            char *array_name;    // Static array name (NULL if dynamic)
            AstNode *array_expr; // Dynamic array expression (NULL if static)
            char *idx_var;       // Name of index variable
            char *val_var;       // Name of value variable
            AstNode *body;       // Loop body
        } enumerate;

        // AST_LET, AST_ASSIGN
        struct {
            char *name;
            bool is_array;  // true if \let<name[]>
            CLType cl_type; // Type annotation (CL_TYPE_INFERRED if none)
            AstNode *value;
        } binding;

        // AST_SETELEMENT
        struct {
            char *array_name; // Direct name (NULL if indirect)
            AstNode *target;  // Indirect target expression (NULL if direct)
            AstNode *index;
            AstNode *value;
        } setelement;

        // AST_COPYARRAY
        struct {
            char *dest_name; // Name of the local array to create
            AstNode *source; // Expression evaluating to external array name
        } copyarray;

        // AST_MEM_LOAD, AST_MEM_STORE, AST_MEM_LEN, AST_MEM_ALLOC
        struct {
            AstNode *addr;  // Heap address (integer)
            AstNode *index; // Index for load/store (NULL for len/alloc)
            AstNode *value; // Value for store (NULL for load/len/alloc)
        } mem;

        // AST_MAP_NEW, AST_MAP_GET, AST_MAP_SET, AST_MAP_HAS, AST_MAP_DEL, AST_MAP_LEN, AST_MAP_KEYS
        struct {
            AstNode *addr;  // Map address (integer)
            AstNode *key;   // Key for get/set/has/del (NULL for new/len/keys)
            AstNode *value; // Value for set (NULL for others)
        } map;

        // Binary operations: AST_ADD, AST_SUB, etc.
        struct {
            AstNode *left;
            AstNode *right;
        } binary;

        // Unary operations: AST_NEG, AST_NOT
        struct {
            AstNode *operand;
        } unary;

        // AST_IF
        struct {
            AstNode *condition;
            AstNode *then_branch;
            AstNode *else_branch; // NULL if no else
        } if_node;

        // AST_LOOP
        struct {
            AstNode *body; // Contains AST_EXIT_WHEN nodes
        } loop;

        // AST_EXIT_WHEN, AST_RETURN
        struct {
            AstNode *value;
        } control;

        // AST_RECALL with arguments (function call)
        struct {
            char *name;      // Direct name (NULL if indirect)
            AstNode *target; // Indirect target expression (NULL if direct)
            AstNode **args;
            int n_args;
        } call;

        // AST_ARRAY_LIT
        struct {
            AstNode **elements;
            int n_elements;
        } array;

        // AST_STRING_ARRAY_LIT
        struct {
            char **strings;
            int n_strings;
        } string_array;

        // AST_MAP_LIT (|1->10, 2->20| new syntax)
        struct {
            AstNode **keys;
            AstNode **values;
            int n_pairs;
        } map_lit;

        // AST_LAMBDA
        struct {
            char *name;
            char **params;
            CLType *param_types; // Type for each parameter (NULL if no types)
            int n_params;
            AstNode *body;
            bool is_computational;
        } lambda;

        // AST_SEQ
        struct {
            AstNode **stmts;
            int n_stmts;
        } seq;
    };

    // Source location for error messages
    int line;
    int col;
};

// ============================================================================
// AST Construction
// ============================================================================

AstNode *calc_ast_int_lit(int64_t value);
AstNode *calc_ast_string_lit(const char *value);
AstNode *calc_ast_valueof(const char *name, AstNode *index);
AstNode *calc_ast_recall(const char *name, AstNode *index);
AstNode *calc_ast_recall_indirect(AstNode *target, AstNode *index);
AstNode *calc_ast_recall_call(const char *name, AstNode **args, int n_args);
AstNode *calc_ast_recall_call_indirect(AstNode *target, AstNode **args, int n_args);
AstNode *calc_ast_ref(const char *name);
AstNode *calc_ast_let(const char *name, bool is_array, AstNode *value);
AstNode *calc_ast_assign(const char *name, bool is_array, AstNode *value);
AstNode *calc_ast_inc(const char *name);
AstNode *calc_ast_inc_dyn(const char *name, AstNode *target, bool is_deref);
AstNode *calc_ast_dec(const char *name);
AstNode *calc_ast_dec_dyn(const char *name, AstNode *target, bool is_deref);
AstNode *calc_ast_setelement(const char *name, AstNode *index, AstNode *value);
AstNode *calc_ast_binary(AstNodeType type, AstNode *left, AstNode *right);
AstNode *calc_ast_unary(AstNodeType type, AstNode *operand);
AstNode *calc_ast_if(AstNode *cond, AstNode *then_branch, AstNode *else_branch);
AstNode *calc_ast_loop(AstNode *body);
AstNode *calc_ast_exit_when(AstNode *cond);
AstNode *calc_ast_return(AstNode *value);
AstNode *calc_ast_array_lit(AstNode **elements, int n_elements);
AstNode *calc_ast_string_array_lit(char **strings, int n_strings);
AstNode *calc_ast_len(AstNode *target); // unified \len for arrays, maps, heap
AstNode *calc_ast_copyarray(const char *dest_name, AstNode *source);
AstNode *calc_ast_lambda(const char *name, char **params, int n_params, AstNode *body,
                         bool is_computational);
AstNode *calc_ast_seq(AstNode **stmts, int n_stmts);
AstNode *calc_ast_empty(void);

// Heap memory operations
AstNode *calc_ast_mem_load(AstNode *addr, AstNode *index);
AstNode *calc_ast_mem_store(AstNode *addr, AstNode *index, AstNode *value);
AstNode *calc_ast_mem_len(AstNode *addr);
AstNode *calc_ast_mem_alloc(AstNode *count);

// Map operations
AstNode *calc_ast_map_new(void);
AstNode *calc_ast_map_get(AstNode *addr, AstNode *key);
AstNode *calc_ast_map_set(AstNode *addr, AstNode *key, AstNode *value);
AstNode *calc_ast_map_has(AstNode *addr, AstNode *key);
AstNode *calc_ast_map_del(AstNode *addr, AstNode *key);
AstNode *calc_ast_map_len(AstNode *addr);
AstNode *calc_ast_map_keys(AstNode *addr);

// Output operations
AstNode *calc_ast_cursor(AstNode *row, AstNode *col);
AstNode *calc_ast_ansi(const char *codes); // pre-builds "\x1b[codes m"
AstNode *calc_ast_emit(AstNode *expr);

// New CL syntax operations
AstNode *calc_ast_var_access(const char *name, AstNode *dynamic_name, bool is_deref);
AstNode *calc_ast_var_concat(AstNode **parts, int n_parts, bool is_deref);
AstNode *calc_ast_invoke(const char *name, AstNode *target, AstNode **args, int n_args,
                         bool is_deref);
AstNode *calc_ast_collection_get(const char *name, AstNode *target, AstNode *key, bool is_deref);
AstNode *calc_ast_collection_set(const char *name, AstNode *target, AstNode *key, AstNode *value,
                                 bool is_deref);
AstNode *calc_ast_addressof(const char *name);
AstNode *calc_ast_enumerate(const char *array_name, AstNode *array_expr, const char *idx_var,
                            const char *val_var, AstNode *body);
AstNode *calc_ast_let_typed(const char *name, CLType cl_type, AstNode *value);
AstNode *calc_ast_lambda_typed(const char *name, char **params, CLType *param_types, int n_params,
                               AstNode *body, bool is_computational);
AstNode *calc_ast_map_lit(AstNode **keys, AstNode **values, int n_pairs);

void calc_ast_free(AstNode *node);
void calc_ast_print(AstNode *node, int indent);

#endif // CALC_AST_H

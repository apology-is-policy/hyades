// delimiter_stack.h - Track and validate matching delimiters
#pragma once

#include "../utils/error.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Types of delimiters we track
typedef enum {
    DELIM_BRACE,          // { }
    DELIM_BRACKET,        // [ ]
    DELIM_PAREN,          // ( )
    DELIM_DOLLAR,         // $ $ (inline math)
    DELIM_DOUBLE_DOLLAR,  // $$ $$ (display math)
    DELIM_BEGIN_END,      // \begin{X} \end{X}
    DELIM_LEFT_RIGHT,     // \left \right
    DELIM_IF_ELSE,        // \if \else
    DELIM_ANGLE,          // < > for calc commands like \let<name>
    DELIM_CASSILDA_LABEL, // @label ... @end
    DELIM_CASSILDA_BLOCK  // #before_each/#after_each ... #end
} DelimiterType;

// A single delimiter entry on the stack
typedef struct {
    DelimiterType type;
    char *open_text; // The actual opening text (e.g., "\\begin{vbox}")
    char *env_name;  // Environment name for begin/end (e.g., "vbox")
    int open_line;   // 1-based line where delimiter opened
    int open_col;    // 1-based column where delimiter opened
    int open_pos;    // Byte offset where delimiter opened
} DelimiterEntry;

// Stack of delimiter entries
typedef struct {
    DelimiterEntry *stack;
    int depth;
    int capacity;
} DelimiterStack;

// ============================================================================
// Lifecycle
// ============================================================================

// Create a new delimiter stack
DelimiterStack *delimiter_stack_new(void);

// Free a delimiter stack
void delimiter_stack_free(DelimiterStack *ds);

// ============================================================================
// Stack operations
// ============================================================================

// Push an opening delimiter onto the stack
// Returns true on success, false on allocation failure
bool delimiter_stack_push(DelimiterStack *ds, DelimiterType type, const char *open_text,
                          const char *env_name, int line, int col, int pos);

// Pop a closing delimiter and validate it matches
// Returns true if matched, false if mismatched or stack empty
// If mismatched, adds error to error_list
bool delimiter_stack_pop(DelimiterStack *ds, DelimiterType type, const char *close_text,
                         const char *env_name, int line, int col, int pos,
                         ParseErrorList *error_list);

// Peek at the top of the stack without popping
// Returns NULL if stack is empty
const DelimiterEntry *delimiter_stack_peek(const DelimiterStack *ds);

// Check if stack is empty
bool delimiter_stack_is_empty(const DelimiterStack *ds);

// Get current depth
int delimiter_stack_depth(const DelimiterStack *ds);

// ============================================================================
// Validation
// ============================================================================

// Check for any unclosed delimiters and report errors
// Call this at end of parsing
void delimiter_stack_check_unclosed(DelimiterStack *ds, ParseErrorList *error_list);

// Get the expected closing delimiter for the top of stack
// Returns NULL if stack is empty
const char *delimiter_stack_expected_close(const DelimiterStack *ds);

// Check if a delimiter type matches what's on top of stack
bool delimiter_stack_matches_top(const DelimiterStack *ds, DelimiterType type,
                                 const char *env_name);

// ============================================================================
// Utility
// ============================================================================

// Get delimiter type name as string
const char *delimiter_type_name(DelimiterType type);

// Get the matching close delimiter for an open delimiter
const char *delimiter_get_close(DelimiterType type, const char *env_name);

// Parse a begin/end and extract environment name
// Returns true if valid, sets env_name (caller must free)
bool delimiter_parse_begin(const char *text, char **env_name);
bool delimiter_parse_end(const char *text, char **env_name);

#ifdef __cplusplus
}
#endif

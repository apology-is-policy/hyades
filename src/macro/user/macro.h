// macro.h - User-defined macro system for Hyades
//
// Syntax:
//   \macro<\name[opt1=default1, opt2]{req1}{req2}>{body with ${arg} refs}
//
// Calling:
//   \name[opt1:value1, opt2:value2]{req1}{req2}
//   \name[value]        (single optional arg, name can be omitted)
//   \name{req}          (no optional args)

#ifndef MACRO_H
#define MACRO_H

#include <stdbool.h>

// ============================================================================
// Data Structures
// ============================================================================

// A single macro argument definition
typedef struct {
    char *name;          // Argument name
    char *default_value; // Default value (NULL if required, "" if optional with no default)
    bool is_optional;    // true for [...], false for {...}
} MacroArg;

// A defined macro
typedef struct {
    char *name;     // Macro name (without backslash)
    MacroArg *args; // Array of argument definitions
    int n_args;     // Number of arguments
    int n_optional; // Number of optional arguments
    int n_required; // Number of required arguments
    char *body;     // Macro body (with ${arg} placeholders)
    // LSP position tracking (1-based, 0 means unknown)
    int def_line;     // Line where macro is defined
    int def_col;      // Column where macro is defined
    int def_end_line; // End line of macro definition
    int def_end_col;  // End column of macro definition
} Macro;

// Macro registry (stores all defined macros)
typedef struct MacroRegistry {
    Macro *macros;
    int n_macros;
    int capacity;
} MacroRegistry;

// ============================================================================
// Registry Management
// ============================================================================

// Create a new macro registry
MacroRegistry *macro_registry_new(void);

// Free a macro registry and all its macros
void macro_registry_free(MacroRegistry *reg);

// Add a macro to the registry (takes ownership of macro fields)
bool macro_registry_add(MacroRegistry *reg, Macro *macro);

// Find a macro by name (returns NULL if not found)
Macro *macro_registry_find(MacroRegistry *reg, const char *name);

// ============================================================================
// Macro Parsing
// ============================================================================

// Parse a \macro<...>{...} definition
// Returns the parsed Macro, or NULL on error
// Sets *end_pos to position after the definition
// On error, sets error_msg
Macro *macro_parse_definition(const char *input, int *end_pos, char *error_msg, int error_size);

// ============================================================================
// Macro Expansion
// ============================================================================

// Check if input starts with a call to a known macro
// Returns the macro if found, NULL otherwise
// Sets *name_end to position after the macro name
Macro *macro_match_call(MacroRegistry *reg, const char *input, int *name_end);

// Expand a macro call
// Input should start with \macroname
// Returns newly allocated expanded string, or NULL on error
// Sets *end_pos to position after the macro call in input
char *macro_expand_call(Macro *macro, const char *input, int *end_pos, char *error_msg,
                        int error_size);

// ============================================================================
// Macro Hygiene
// ============================================================================

// Apply hygiene with ID 0 to top-level (non-macro) code
// This allows top-level lowercase variables to work with \ref
char *macro_apply_toplevel_hygiene(const char *input);

// ============================================================================
// Full Document Processing
// ============================================================================

// Process a document: extract macro definitions and expand all macro calls
// Returns newly allocated string with macros expanded
// The registry is populated with any \macro<...>{...} definitions found
char *macro_process_document(const char *input, MacroRegistry *reg, char *error_msg,
                             int error_size);

// Expand all macro calls in input (assumes definitions already in registry)
// Iterates until no more expansions occur
char *macro_expand_all(const char *input, MacroRegistry *reg, char *error_msg, int error_size);

#endif // MACRO_H

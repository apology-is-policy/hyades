// macro_expand.h - Macro expansion for Hyades documents
//
// Expands system macros (\table, \quad, etc.), escape sequences,
// user-defined macros, and Julia computations.
#ifndef MACRO_EXPAND_H
#define MACRO_EXPAND_H

#include "document/source_map.h"
#include "document/symbol_table.h"
#include "utils/error.h"
#include <stdbool.h>

// ============================================================================
// Macro Expansion
// ============================================================================

// Expand all macros in input until no more expansions occur
//
// Handles:
// - System macros: \table, \quad, \qquad, \hskip, \thinspace
// - Escape sequences: \{, \}, \#, \_, \&, \textbackslash, etc.
// - User-defined macros: \macro<n>{...} definitions and invocations
// - Julia computation: \julia{code}, \julia<n>{code}, \call[n]{args}
// - Arithmetic: \add{a,b}, \sub{a,b}, \mul{a,b}, \div{a,b}, \mod{a,b}
// - Comparisons: \eq{a,b}, \ne{a,b}, \gt{a,b}, \lt{a,b}, \ge{a,b}, \le{a,b}
// - Counters: \let<name>{value}, \inc<name>, \dec<name>, \valueof<name>
// - Conditionals: \if{cond}{true}\else{false}
// - Built-ins: \width
//
// Parameters:
//   input      - Input text
//   width      - Document width (for \width command)
//   error_msg  - Buffer for error message
//   error_size - Size of error buffer
//
// Returns:
//   Newly allocated string with all macros expanded, or NULL on error.
//   Caller must free().
//
char *expand_all_macros(const char *input, int width, char *error_msg, int error_size);

// LSP-aware macro expansion
//
// Same as expand_all_macros but collects errors into a ParseErrorList
// and populates a LspSymbolTable with macro definitions.
//
// Parameters:
//   input      - Input text
//   width      - Document width
//   errors     - Error list to populate (may be NULL)
//   source_map - Source map for position tracking (may be NULL)
//   symbols    - Symbol table to populate with macro definitions (may be NULL)
//
// Returns:
//   Newly allocated string with all macros expanded, or NULL on error.
//   Caller must free().
//
char *expand_all_macros_lsp(const char *input, int width, ParseErrorList *errors,
                            SourceMap *source_map, LspSymbolTable *symbols);

// Keep the macro registry alive across multiple expand_all_macros calls
// Call with true at start of document parsing, false at end
void macro_registry_keep_alive(bool enable);

#endif // MACRO_EXPAND_H

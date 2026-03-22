// document.h - Public API for Hyades document processing
//
// Parses Hyades source documents into a BoxLayout tree that can be rendered.

#ifndef DOCUMENT_H
#define DOCUMENT_H

#include "compositor/compositor.h" // For CompOptions
#include "document/symbol_table.h"
#include "layout/layout_types.h"
#include "utils/error.h"
#include "utils/parse_options.h"

// ============================================================================
// Document Parsing
// ============================================================================

// Parse a Hyades document into a BoxLayout tree
//
// The document is parsed as a vertical box (vbox) containing:
// - Text paragraphs (BOX_TYPE_CONTENT)
// - Display math (BOX_TYPE_DISPLAY_MATH)
// - Commands (BOX_TYPE_COMMAND)
// - Nested layouts (BOX_TYPE_HBOX, BOX_TYPE_VBOX)
// - Vertical spacing
//
// Parameters:
//   input - Hyades source document (null-terminated string)
//   width - Target width in columns
//   err   - Error output (may be NULL)
//
// Returns:
//   Newly allocated BoxLayout tree, or NULL on error.
//   Caller must free with box_layout_free().
//
BoxLayout *parse_document_as_vbox(const char *input, int width, ParseError *err);

// Parse document with LSP options for error collection, symbol tracking, etc.
//
// This extended version supports:
// - Collecting multiple errors (via ParseErrorList)
// - Symbol table population for go-to-definition, hover, etc.
// - Delimiter validation for unclosed/mismatched delimiters
//
// Parameters:
//   input   - Hyades source document
//   width   - Target width in columns
//   errors  - Error list to populate (may be NULL)
//   symbols - Symbol table to populate (may be NULL)
//   opts    - Parse options controlling features (may be NULL for defaults)
//
BoxLayout *parse_document_as_vbox_lsp(const char *input, int width, ParseErrorList *errors,
                                      LspSymbolTable *symbols, const HyadesParseOptions *opts);

// ============================================================================
// Verbatim Content Access
// ============================================================================

// Get stored verbatim content by index
// Returns NULL if index is invalid
// Used by compositor to render \verb placeholders
const char *verbatim_store_get(int index);

// Clear all stored verbatim content
void verbatim_store_clear(void);

// Protect verbatim content in input string
// Replaces \verb#...# sequences with @@VERB_N@@ placeholders
// Returns newly allocated string (caller must free)
// When the verbatim store is locked, appends to existing entries
char *protect_verbatim(const char *input);

// ============================================================================
// Document Composition (Full Pipeline)
// ============================================================================

// Compose a Hyades document to rendered output
//
// This is the main entry point that:
// 1. Parses the document into a BoxLayout tree
// 2. Resolves width inheritance
// 3. Renders the tree to a Box
// 4. Converts to string output
//
// Parameters:
//   input - Hyades source document
//   opt   - Composition options (may be modified by document commands)
//   err   - Error output (may be NULL)
//
// Returns:
//   Newly allocated string with rendered output, or NULL on error.
//   Caller must free().
//
char *compose_document(const char *input, const CompOptions *opt, ParseError *err);

// ============================================================================
// Continuous Mode (for animations/games)
// ============================================================================

// Set continuous mode flag and exit flag pointer
// When enabled, \main{body} loops with terminal cursor control
void macro_set_continuous_mode(bool enable, volatile int *exit_flag);

// Check if continuous mode is enabled
bool macro_is_continuous_mode(void);

// Get pointer to exit flag (for signal handlers)
volatile int *macro_get_exit_flag(void);

#endif // DOCUMENT_H

// doc_commands.h - Document command definitions for Hyades
//
// This provides a single source of truth for all document-level commands
// like \setunicode, \setwidth, etc. Both the document parser and compositor
// reference this to avoid duplicating command lists.

#ifndef DOC_COMMANDS_H
#define DOC_COMMANDS_H

#include <stdbool.h>

// ============================================================================
// Command Categories
// ============================================================================

typedef enum {
    CMD_CAT_RENDER,      // Rendering options (unicode, mathitalic, etc.)
    CMD_CAT_LAYOUT,      // Layout options (width, hyphenation, etc.)
    CMD_CAT_SPACING,     // Spacing commands (parskip, mathabove, etc.)
    CMD_CAT_LINEBREAK,   // Line breaking algorithm options
    CMD_CAT_SYMBOL,      // Symbol customization (setsym, setmode)
    CMD_CAT_VERTICAL,    // Vertical spacing (vskip, smallskip, etc.)
    CMD_CAT_DIAGNOSTICS, // Diagnostic logging (diagnostics, printdiagnostics, showdiag)
} DocCommandCategory;

// ============================================================================
// Command Definitions
// ============================================================================

typedef struct {
    const char *name; // Command name (without backslash)
    DocCommandCategory category;
    int min_args;            // Minimum required arguments
    int max_args;            // Maximum arguments (-1 for unlimited)
    const char *description; // Human-readable description
} DocCommandDef;

// ============================================================================
// Command Registry Access
// ============================================================================

// Check if a command name is a document command
// name should NOT include the backslash
bool is_document_command(const char *name);

// Get command definition by name (returns NULL if not found)
const DocCommandDef *get_document_command(const char *name);

// Check if command is in a specific category
bool is_command_category(const char *name, DocCommandCategory category);

// ============================================================================
// Command Lists by Category
// ============================================================================

// Check if command is a rendering option
static inline bool is_render_command(const char *name) {
    return is_command_category(name, CMD_CAT_RENDER);
}

// Check if command is a layout option
static inline bool is_layout_command(const char *name) {
    return is_command_category(name, CMD_CAT_LAYOUT);
}

// Check if command is a spacing option
static inline bool is_spacing_command(const char *name) {
    return is_command_category(name, CMD_CAT_SPACING);
}

// Check if command is a line breaking option
static inline bool is_linebreak_command(const char *name) {
    return is_command_category(name, CMD_CAT_LINEBREAK);
}

// Check if command is a symbol customization
static inline bool is_symbol_command(const char *name) {
    return is_command_category(name, CMD_CAT_SYMBOL);
}

// Check if command is a vertical spacing command
static inline bool is_vertical_spacing_command(const char *name) {
    return is_command_category(name, CMD_CAT_VERTICAL);
}

// ============================================================================
// All Document Commands (for iteration)
// ============================================================================

// Get the full command table
// Returns pointer to static array, terminated by entry with name == NULL
const DocCommandDef *get_all_document_commands(void);

// Get count of all document commands
int get_document_command_count(void);

#endif // DOC_COMMANDS_H

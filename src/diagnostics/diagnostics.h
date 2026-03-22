// diagnostics.h - Diagnostic logging system for Hyades
// Logs macro expansion, layout parsing, and rendering steps to memory

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdbool.h>

// Diagnostic categories (bitmask)
typedef enum {
    DIAG_NONE = 0,
    DIAG_MACROS = 1,      // User macro expansion
    DIAG_SYSTEM = 2,      // System macros (\table, \aligned, \cases, etc.)
    DIAG_LAYOUT = 4,      // Box layout parsing (vrule, hrule, hbox, vbox)
    DIAG_MATH = 8,        // Math rendering
    DIAG_CALC = 16,       // Calc commands (\measure, counters, etc.)
    DIAG_MERGE = 32,      // Box merging and baseline alignment
    DIAG_EXPANSION = 64,  // Expansion diagnostics
    DIAG_SUBNIVEAN = 128, // Subnivean VM execution (compiled computational lambdas)
    DIAG_ALL = 255
} DiagCategory;

// Enable diagnostic logging for specified categories
// Multiple categories can be OR'd together: DIAG_MACROS | DIAG_LAYOUT
void diag_enable(DiagCategory cats);

// Disable all diagnostic logging and clear accumulated logs
void diag_disable(void);

// Check if a specific category is enabled
bool diag_is_enabled(DiagCategory cat);

// Get currently enabled categories
DiagCategory diag_get_enabled(void);

// Log a diagnostic message
// cat: category for this message
// indent: nesting level (0 = top level, 1 = one level in, etc.)
// fmt: printf-style format string
void diag_log(DiagCategory cat, int indent, const char *fmt, ...);

// Log a result/continuation line (uses arrow prefix)
// cat: category for this message
// indent: nesting level
// fmt: printf-style format string
void diag_result(DiagCategory cat, int indent, const char *fmt, ...);

// Clear all accumulated log entries
void diag_clear(void);

// Get accumulated diagnostic output as a string
// Caller must free the returned string
char *diag_get_output(void);

// Get number of log entries
int diag_entry_count(void);

// Category name helpers
const char *diag_category_name(DiagCategory cat);
DiagCategory diag_parse_categories(const char *str);

#endif // DIAGNOSTICS_H

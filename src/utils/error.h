// error.h - Error types for Hyades API
#pragma once

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Parse Error
// ============================================================================

typedef enum {
    PARSE_OK = 0,
    PARSE_ERR_SYNTAX,
    PARSE_ERR_MATH_SYNTAX,
    PARSE_ERR_UNSUPPORTED,
    PARSE_ERR_OOM,
    PARSE_ERR_INTERNAL,
    PARSE_ERR_NOT_FOUND,
    PARSE_ERR_IO,
    PARSE_ERR_UNCLOSED_DELIMITER,
    PARSE_ERR_MISMATCHED_DELIMITER,
    PARSE_ERR_UNDEFINED_REFERENCE,
    PARSE_ERR_INVALID_ARGUMENT,
    PARSE_ERR_ARITY_MISMATCH,
    PARSE_ERR_UNKNOWN_COMMAND,
    PARSE_ERR_OTHER
} ParseErrorCode;

// Error severity levels for LSP diagnostics
typedef enum {
    PARSE_SEVERITY_ERROR = 1,
    PARSE_SEVERITY_WARNING = 2,
    PARSE_SEVERITY_INFO = 3,
    PARSE_SEVERITY_HINT = 4
} ParseErrorSeverity;

typedef struct {
    ParseErrorCode code;         // PARSE_OK on success
    ParseErrorSeverity severity; // Error severity level
    int row;                     // 1-based; 0 if unknown
    int col;                     // 1-based; 0 if unknown
    int end_row;                 // 1-based end line (for error span)
    int end_col;                 // 1-based end column (for error span)
    char message[256];           // nul-terminated; empty if PARSE_OK
    char source[64];             // Source of error (e.g., "document", "math", "calc")
} ParseError;

// ============================================================================
// Parse Error List (for accumulating multiple errors)
// ============================================================================

#define PARSE_ERROR_LIST_DEFAULT_MAX 100

typedef struct {
    ParseError *errors; // Array of errors
    int n_errors;       // Number of errors collected
    int capacity;       // Allocated capacity
    int max_errors;     // Maximum errors to collect (0 = unlimited)
    int error_count;    // Total errors (may exceed max_errors)
    int warning_count;  // Total warnings
} ParseErrorList;

// Create a new error list
ParseErrorList *parse_error_list_new(void);

// Create with custom max errors
ParseErrorList *parse_error_list_new_with_max(int max_errors);

// Free an error list
void parse_error_list_free(ParseErrorList *list);

// Add an error to the list
void parse_error_list_add(ParseErrorList *list, ParseErrorCode code, int row, int col, int end_row,
                          int end_col, const char *source, const char *fmt, ...);

// Add an error with severity
void parse_error_list_add_with_severity(ParseErrorList *list, ParseErrorCode code,
                                        ParseErrorSeverity severity, int row, int col, int end_row,
                                        int end_col, const char *source, const char *fmt, ...);

// Add a warning
void parse_error_list_add_warning(ParseErrorList *list, ParseErrorCode code, int row, int col,
                                  int end_row, int end_col, const char *source, const char *fmt,
                                  ...);

// Add from an existing ParseError
void parse_error_list_add_error(ParseErrorList *list, const ParseError *err);

// Get error at index
const ParseError *parse_error_list_get(const ParseErrorList *list, int index);

// Check if any errors occurred
int parse_error_list_has_errors(const ParseErrorList *list);

// Get total error count (including those not stored)
int parse_error_list_total_errors(const ParseErrorList *list);

// Clear all errors
void parse_error_list_clear(ParseErrorList *list);

// Copy first error to a single ParseError struct (for compatibility)
void parse_error_list_first(const ParseErrorList *list, ParseError *out);

// ============================================================================
// Composition Error
// ============================================================================

typedef enum { COMP_OK = 0, COMP_ERR_INPUT, COMP_ERR_OOM, COMP_ERR_INTERNAL } CompErrorCode;

typedef struct {
    CompErrorCode code;
    int row, col; // optional; 0 if N/A
    char message[256];
} CompError;

// ============================================================================
// Error Helpers
// ============================================================================

static inline void parse_error_init(ParseError *err) {
    if (err) {
        err->code = PARSE_OK;
        err->severity = PARSE_SEVERITY_ERROR;
        err->row = 0;
        err->col = 0;
        err->end_row = 0;
        err->end_col = 0;
        err->message[0] = '\0';
        err->source[0] = '\0';
    }
}

static inline int parse_error_occurred(const ParseError *err) {
    return err && err->code != PARSE_OK;
}

// Set an error with formatted message
static inline void parse_error_set(ParseError *err, ParseErrorCode code, int row, int col,
                                   const char *fmt, ...) {
    if (!err) return;
    err->code = code;
    err->severity = PARSE_SEVERITY_ERROR;
    err->row = row;
    err->col = col;
    err->end_row = row;
    err->end_col = col;

    va_list args;
    va_start(args, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, args);
    va_end(args);
}

// Set error with span
static inline void parse_error_set_span(ParseError *err, ParseErrorCode code, int row, int col,
                                        int end_row, int end_col, const char *fmt, ...) {
    if (!err) return;
    err->code = code;
    err->severity = PARSE_SEVERITY_ERROR;
    err->row = row;
    err->col = col;
    err->end_row = end_row;
    err->end_col = end_col;

    va_list args;
    va_start(args, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, args);
    va_end(args);
}

// Get error code name as string
static inline const char *parse_error_code_name(ParseErrorCode code) {
    switch (code) {
    case PARSE_OK: return "OK";
    case PARSE_ERR_SYNTAX: return "SYNTAX";
    case PARSE_ERR_MATH_SYNTAX: return "MATH_SYNTAX";
    case PARSE_ERR_UNSUPPORTED: return "UNSUPPORTED";
    case PARSE_ERR_OOM: return "OUT_OF_MEMORY";
    case PARSE_ERR_INTERNAL: return "INTERNAL";
    case PARSE_ERR_NOT_FOUND: return "NOT_FOUND";
    case PARSE_ERR_IO: return "IO";
    case PARSE_ERR_UNCLOSED_DELIMITER: return "UNCLOSED_DELIMITER";
    case PARSE_ERR_MISMATCHED_DELIMITER: return "MISMATCHED_DELIMITER";
    case PARSE_ERR_UNDEFINED_REFERENCE: return "UNDEFINED_REFERENCE";
    case PARSE_ERR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case PARSE_ERR_ARITY_MISMATCH: return "ARITY_MISMATCH";
    case PARSE_ERR_UNKNOWN_COMMAND: return "UNKNOWN_COMMAND";
    case PARSE_ERR_OTHER: return "OTHER";
    default: return "UNKNOWN";
    }
}

#ifdef __cplusplus
} // extern "C"
#endif

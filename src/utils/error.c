// error.c - Implementation of error list functions
#include "error.h"

#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 16

// ============================================================================
// Lifecycle
// ============================================================================

ParseErrorList *parse_error_list_new(void) {
    return parse_error_list_new_with_max(PARSE_ERROR_LIST_DEFAULT_MAX);
}

ParseErrorList *parse_error_list_new_with_max(int max_errors) {
    ParseErrorList *list = calloc(1, sizeof(ParseErrorList));
    if (!list) return NULL;

    list->errors = calloc(INITIAL_CAPACITY, sizeof(ParseError));
    if (!list->errors) {
        free(list);
        return NULL;
    }

    list->n_errors = 0;
    list->capacity = INITIAL_CAPACITY;
    list->max_errors = max_errors;
    list->error_count = 0;
    list->warning_count = 0;

    return list;
}

void parse_error_list_free(ParseErrorList *list) {
    if (!list) return;
    free(list->errors);
    free(list);
}

// ============================================================================
// Adding errors
// ============================================================================

static int ensure_capacity(ParseErrorList *list) {
    if (list->n_errors < list->capacity) return 1;

    int old_cap = list->capacity;
    int new_cap = list->capacity * 2;
    ParseError *new_errors = realloc(list->errors, new_cap * sizeof(ParseError));
    if (!new_errors) return 0;

    // Zero the new portion
    memset(new_errors + old_cap, 0, (new_cap - old_cap) * sizeof(ParseError));

    list->errors = new_errors;
    list->capacity = new_cap;
    return 1;
}

static void add_error_internal(ParseErrorList *list, ParseErrorCode code,
                               ParseErrorSeverity severity, int row, int col, int end_row,
                               int end_col, const char *source, const char *fmt, va_list args) {
    if (!list) return;

    // Track counts
    if (severity == PARSE_SEVERITY_WARNING) {
        list->warning_count++;
    } else if (severity == PARSE_SEVERITY_ERROR) {
        list->error_count++;
    }

    // Check if we should store this error
    if (list->max_errors > 0 && list->n_errors >= list->max_errors) {
        return; // At capacity, just count it
    }

    if (!ensure_capacity(list)) return;

    ParseError *err = &list->errors[list->n_errors++];
    err->code = code;
    err->severity = severity;
    err->row = row;
    err->col = col;
    err->end_row = end_row > 0 ? end_row : row;
    err->end_col = end_col > 0 ? end_col : col;

    if (source) {
        strncpy(err->source, source, sizeof(err->source) - 1);
        err->source[sizeof(err->source) - 1] = '\0';
    } else {
        err->source[0] = '\0';
    }

    vsnprintf(err->message, sizeof(err->message), fmt, args);
}

void parse_error_list_add(ParseErrorList *list, ParseErrorCode code, int row, int col, int end_row,
                          int end_col, const char *source, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    add_error_internal(list, code, PARSE_SEVERITY_ERROR, row, col, end_row, end_col, source, fmt,
                       args);
    va_end(args);
}

void parse_error_list_add_with_severity(ParseErrorList *list, ParseErrorCode code,
                                        ParseErrorSeverity severity, int row, int col, int end_row,
                                        int end_col, const char *source, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    add_error_internal(list, code, severity, row, col, end_row, end_col, source, fmt, args);
    va_end(args);
}

void parse_error_list_add_warning(ParseErrorList *list, ParseErrorCode code, int row, int col,
                                  int end_row, int end_col, const char *source, const char *fmt,
                                  ...) {
    va_list args;
    va_start(args, fmt);
    add_error_internal(list, code, PARSE_SEVERITY_WARNING, row, col, end_row, end_col, source, fmt,
                       args);
    va_end(args);
}

void parse_error_list_add_error(ParseErrorList *list, const ParseError *err) {
    if (!list || !err || err->code == PARSE_OK) return;

    // Track counts
    if (err->severity == PARSE_SEVERITY_WARNING) {
        list->warning_count++;
    } else {
        list->error_count++;
    }

    // Check if we should store this error
    if (list->max_errors > 0 && list->n_errors >= list->max_errors) {
        return;
    }

    if (!ensure_capacity(list)) return;

    list->errors[list->n_errors++] = *err;
}

// ============================================================================
// Querying errors
// ============================================================================

const ParseError *parse_error_list_get(const ParseErrorList *list, int index) {
    if (!list || index < 0 || index >= list->n_errors) return NULL;
    return &list->errors[index];
}

int parse_error_list_has_errors(const ParseErrorList *list) {
    return list && list->error_count > 0;
}

int parse_error_list_total_errors(const ParseErrorList *list) {
    return list ? list->error_count : 0;
}

void parse_error_list_clear(ParseErrorList *list) {
    if (!list) return;
    list->n_errors = 0;
    list->error_count = 0;
    list->warning_count = 0;
}

void parse_error_list_first(const ParseErrorList *list, ParseError *out) {
    if (!out) return;

    parse_error_init(out);

    if (list && list->n_errors > 0) {
        *out = list->errors[0];
    }
}

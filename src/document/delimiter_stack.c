// delimiter_stack.c - Implementation of delimiter tracking
#include "delimiter_stack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 32

// ============================================================================
// Lifecycle
// ============================================================================

DelimiterStack *delimiter_stack_new(void) {
    DelimiterStack *ds = calloc(1, sizeof(DelimiterStack));
    if (!ds) return NULL;

    ds->stack = calloc(INITIAL_CAPACITY, sizeof(DelimiterEntry));
    if (!ds->stack) {
        free(ds);
        return NULL;
    }

    ds->depth = 0;
    ds->capacity = INITIAL_CAPACITY;

    return ds;
}

static void free_entry(DelimiterEntry *entry) {
    if (entry) {
        free(entry->open_text);
        free(entry->env_name);
        entry->open_text = NULL;
        entry->env_name = NULL;
    }
}

void delimiter_stack_free(DelimiterStack *ds) {
    if (!ds) return;

    for (int i = 0; i < ds->depth; i++) {
        free_entry(&ds->stack[i]);
    }

    free(ds->stack);
    free(ds);
}

// ============================================================================
// Stack operations
// ============================================================================

static int ensure_capacity(DelimiterStack *ds) {
    if (ds->depth < ds->capacity) return 1;

    int old_cap = ds->capacity;
    int new_cap = ds->capacity * 2;
    DelimiterEntry *new_stack = realloc(ds->stack, new_cap * sizeof(DelimiterEntry));
    if (!new_stack) return 0;

    // Zero the new portion
    memset(new_stack + old_cap, 0, (new_cap - old_cap) * sizeof(DelimiterEntry));

    ds->stack = new_stack;
    ds->capacity = new_cap;
    return 1;
}

bool delimiter_stack_push(DelimiterStack *ds, DelimiterType type, const char *open_text,
                          const char *env_name, int line, int col, int pos) {
    if (!ds || !ensure_capacity(ds)) return false;

    DelimiterEntry *entry = &ds->stack[ds->depth++];
    entry->type = type;
    entry->open_text = open_text ? strdup(open_text) : NULL;
    entry->env_name = env_name ? strdup(env_name) : NULL;
    entry->open_line = line;
    entry->open_col = col;
    entry->open_pos = pos;

    return true;
}

// Check if a delimiter type is "structural" (document-level)
// Structural delimiters can "see through" content delimiters when closing
static bool is_structural_delimiter(DelimiterType type) {
    switch (type) {
    case DELIM_CASSILDA_LABEL: // @label/@end
    case DELIM_CASSILDA_BLOCK: // #before_each/#end
    case DELIM_DOUBLE_DOLLAR:  // $$
    case DELIM_BEGIN_END:      // \begin/\end
        return true;
    default: return false;
    }
}

bool delimiter_stack_pop(DelimiterStack *ds, DelimiterType type, const char *close_text,
                         const char *env_name, int line, int col, int pos,
                         ParseErrorList *error_list) {
    (void)pos; // Unused for now

    if (!ds || ds->depth == 0) {
        // Stack empty - unexpected closing delimiter
        if (error_list) {
            parse_error_list_add(error_list, PARSE_ERR_MISMATCHED_DELIMITER, line, col, line, col,
                                 "document",
                                 "Unexpected closing delimiter '%s' with no matching opener",
                                 close_text ? close_text : delimiter_type_name(type));
        }
        return false;
    }

    DelimiterEntry *top = &ds->stack[ds->depth - 1];

    // If types match directly, great
    if (top->type == type) {
        // For begin/end, also check environment name
        if (type == DELIM_BEGIN_END) {
            if ((top->env_name == NULL) != (env_name == NULL) ||
                (top->env_name && env_name && strcmp(top->env_name, env_name) != 0)) {
                if (error_list) {
                    parse_error_list_add(error_list, PARSE_ERR_MISMATCHED_DELIMITER, line, col,
                                         line, col, "document",
                                         "Mismatched environment: \\begin{%s} at line %d:%d "
                                         "closed with \\end{%s}",
                                         top->env_name ? top->env_name : "?", top->open_line,
                                         top->open_col, env_name ? env_name : "?");
                }
                return false;
            }
        }

        // Match successful - pop the entry
        free_entry(top);
        ds->depth--;
        return true;
    }

    // Types don't match. If we're closing a structural delimiter, try to
    // clean up any unclosed content delimiters and find the matching opener.
    if (is_structural_delimiter(type)) {
        // Look through the stack for a matching opener
        for (int i = ds->depth - 1; i >= 0; i--) {
            DelimiterEntry *entry = &ds->stack[i];

            bool matches = (entry->type == type);
            if (matches && type == DELIM_BEGIN_END) {
                // Also check environment name
                if ((entry->env_name == NULL) != (env_name == NULL) ||
                    (entry->env_name && env_name && strcmp(entry->env_name, env_name) != 0)) {
                    matches = false;
                }
            }

            if (matches) {
                // Found it! Report all unclosed delimiters between here and there
                for (int j = ds->depth - 1; j > i; j--) {
                    DelimiterEntry *unclosed = &ds->stack[j];
                    if (error_list) {
                        parse_error_list_add(
                            error_list, PARSE_ERR_UNCLOSED_DELIMITER, unclosed->open_line,
                            unclosed->open_col, unclosed->open_line, unclosed->open_col, "document",
                            "Unclosed '%s' (expected '%s' before '%s')",
                            unclosed->open_text ? unclosed->open_text
                                                : delimiter_type_name(unclosed->type),
                            delimiter_get_close(unclosed->type, unclosed->env_name),
                            close_text ? close_text : delimiter_type_name(type));
                    }
                    free_entry(unclosed);
                }

                // Pop everything including the match
                free_entry(entry);
                ds->depth = i;
                return true;
            }
        }
    }

    // No match found - report error at the closing delimiter
    if (error_list) {
        parse_error_list_add(
            error_list, PARSE_ERR_MISMATCHED_DELIMITER, line, col, line, col, "document",
            "Mismatched delimiter: expected '%s' to close '%s' at line %d:%d, "
            "but found '%s'",
            delimiter_get_close(top->type, top->env_name),
            top->open_text ? top->open_text : delimiter_type_name(top->type), top->open_line,
            top->open_col, close_text ? close_text : delimiter_type_name(type));
    }
    return false;
}

const DelimiterEntry *delimiter_stack_peek(const DelimiterStack *ds) {
    if (!ds || ds->depth == 0) return NULL;
    return &ds->stack[ds->depth - 1];
}

bool delimiter_stack_is_empty(const DelimiterStack *ds) {
    return !ds || ds->depth == 0;
}

int delimiter_stack_depth(const DelimiterStack *ds) {
    return ds ? ds->depth : 0;
}

// ============================================================================
// Validation
// ============================================================================

void delimiter_stack_check_unclosed(DelimiterStack *ds, ParseErrorList *error_list) {
    if (!ds || !error_list) return;

    // Report all unclosed delimiters
    for (int i = ds->depth - 1; i >= 0; i--) {
        DelimiterEntry *entry = &ds->stack[i];
        parse_error_list_add(error_list, PARSE_ERR_UNCLOSED_DELIMITER, entry->open_line,
                             entry->open_col, entry->open_line, entry->open_col, "document",
                             "Unclosed delimiter '%s' (expected '%s')",
                             entry->open_text ? entry->open_text : delimiter_type_name(entry->type),
                             delimiter_get_close(entry->type, entry->env_name));
    }
}

const char *delimiter_stack_expected_close(const DelimiterStack *ds) {
    const DelimiterEntry *top = delimiter_stack_peek(ds);
    if (!top) return NULL;
    return delimiter_get_close(top->type, top->env_name);
}

bool delimiter_stack_matches_top(const DelimiterStack *ds, DelimiterType type,
                                 const char *env_name) {
    const DelimiterEntry *top = delimiter_stack_peek(ds);
    if (!top) return false;

    if (top->type != type) return false;

    if (type == DELIM_BEGIN_END) {
        if ((top->env_name == NULL) != (env_name == NULL)) return false;
        if (top->env_name && env_name && strcmp(top->env_name, env_name) != 0) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// Utility
// ============================================================================

const char *delimiter_type_name(DelimiterType type) {
    switch (type) {
    case DELIM_BRACE: return "{";
    case DELIM_BRACKET: return "[";
    case DELIM_PAREN: return "(";
    case DELIM_DOLLAR: return "$";
    case DELIM_DOUBLE_DOLLAR: return "$$";
    case DELIM_BEGIN_END: return "\\begin";
    case DELIM_LEFT_RIGHT: return "\\left";
    case DELIM_IF_ELSE: return "\\if";
    case DELIM_ANGLE: return "<";
    case DELIM_CASSILDA_LABEL: return "@label";
    case DELIM_CASSILDA_BLOCK: return "#block";
    default: return "?";
    }
}

const char *delimiter_get_close(DelimiterType type, const char *env_name) {
    static char buffer[128];

    switch (type) {
    case DELIM_BRACE: return "}";
    case DELIM_BRACKET: return "]";
    case DELIM_PAREN: return ")";
    case DELIM_DOLLAR: return "$";
    case DELIM_DOUBLE_DOLLAR: return "$$";
    case DELIM_BEGIN_END:
        if (env_name) {
            snprintf(buffer, sizeof(buffer), "\\end{%s}", env_name);
            return buffer;
        }
        return "\\end{...}";
    case DELIM_LEFT_RIGHT: return "\\right";
    case DELIM_IF_ELSE: return "\\fi or \\else";
    case DELIM_ANGLE: return ">";
    case DELIM_CASSILDA_LABEL: return "@end";
    case DELIM_CASSILDA_BLOCK: return "#end";
    default: return "?";
    }
}

bool delimiter_parse_begin(const char *text, char **env_name) {
    if (!text || !env_name) return false;

    // Look for \begin{name}
    const char *start = strstr(text, "\\begin{");
    if (!start) return false;

    start += 7; // Skip past \begin{

    const char *end = strchr(start, '}');
    if (!end) return false;

    int len = (int)(end - start);
    if (len <= 0 || len > 64) return false;

    *env_name = malloc(len + 1);
    if (!*env_name) return false;

    strncpy(*env_name, start, len);
    (*env_name)[len] = '\0';

    return true;
}

bool delimiter_parse_end(const char *text, char **env_name) {
    if (!text || !env_name) return false;

    // Look for \end{name}
    const char *start = strstr(text, "\\end{");
    if (!start) return false;

    start += 5; // Skip past \end{

    const char *end = strchr(start, '}');
    if (!end) return false;

    int len = (int)(end - start);
    if (len <= 0 || len > 64) return false;

    *env_name = malloc(len + 1);
    if (!*env_name) return false;

    strncpy(*env_name, start, len);
    (*env_name)[len] = '\0';

    return true;
}

// parser_recovery.c - Implementation of error recovery helpers
#include "parser_recovery.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Recovery Context Management
// ============================================================================

void recovery_context_init(RecoveryContext *ctx, ParseErrorList *errors) {
    if (!ctx) return;

    ctx->errors = errors;
    ctx->recovery_depth = 0;
    ctx->max_recovery_attempts = 10;
    ctx->current_attempts = 0;
    ctx->in_recovery = false;
    ctx->last_sync = SYNC_NONE;
}

bool recovery_should_try(RecoveryContext *ctx) {
    if (!ctx) return false;
    return ctx->current_attempts < ctx->max_recovery_attempts;
}

void recovery_enter(RecoveryContext *ctx) {
    if (!ctx) return;
    ctx->in_recovery = true;
    ctx->current_attempts++;
}

void recovery_exit(RecoveryContext *ctx) {
    if (!ctx) return;
    ctx->in_recovery = false;
}

bool recovery_in_progress(RecoveryContext *ctx) {
    return ctx && ctx->in_recovery;
}

// ============================================================================
// Synchronization
// ============================================================================

static bool is_blank_line(const char *p) {
    // Skip to start of line
    while (*p && *p != '\n') p--;
    if (*p == '\n') p++;

    // Check if line is blank (only whitespace until newline or EOF)
    while (*p && *p != '\n') {
        if (!isspace(*p)) return false;
        p++;
    }
    return true;
}

static bool starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

bool recovery_is_sync_point(const char *source, SyncTokenType type) {
    if (!source || !*source) return type == SYNC_EOF;

    switch (type) {
    case SYNC_BRACE_CLOSE: return *source == '}';
    case SYNC_BRACKET_CLOSE: return *source == ']';
    case SYNC_PAREN_CLOSE: return *source == ')';
    case SYNC_DOLLAR: return *source == '$' && (source[1] != '$');
    case SYNC_DOUBLE_DOLLAR: return source[0] == '$' && source[1] == '$';
    case SYNC_BEGIN: return starts_with(source, "\\begin");
    case SYNC_END: return starts_with(source, "\\end");
    case SYNC_PARAGRAPH: return *source == '\n' && is_blank_line(source + 1);
    case SYNC_NEWLINE: return *source == '\n';
    case SYNC_COMMAND: return *source == '\\' && isalpha(source[1]);
    case SYNC_EOF: return *source == '\0';
    default: return false;
    }
}

SyncTokenType recovery_sync_to(const char **source, const char *end, SyncTokenType *allowed,
                               int n_allowed) {
    if (!source || !*source || !allowed || n_allowed <= 0) {
        return SYNC_NONE;
    }

    const char *p = *source;

    while (p < end && *p) {
        // Check each allowed sync type
        for (int i = 0; i < n_allowed; i++) {
            if (recovery_is_sync_point(p, allowed[i])) {
                *source = p;
                return allowed[i];
            }
        }
        p++;
    }

    *source = p;
    return SYNC_EOF;
}

const char *recovery_sync_name(SyncTokenType type) {
    switch (type) {
    case SYNC_NONE: return "none";
    case SYNC_BRACE_CLOSE: return "}";
    case SYNC_BRACKET_CLOSE: return "]";
    case SYNC_PAREN_CLOSE: return ")";
    case SYNC_DOLLAR: return "$";
    case SYNC_DOUBLE_DOLLAR: return "$$";
    case SYNC_BEGIN: return "\\begin";
    case SYNC_END: return "\\end";
    case SYNC_PARAGRAPH: return "paragraph break";
    case SYNC_NEWLINE: return "newline";
    case SYNC_COMMAND: return "command";
    case SYNC_EOF: return "end of file";
    default: return "unknown";
    }
}

// ============================================================================
// Error Recovery Strategies
// ============================================================================

const char *recovery_skip_to_close_brace(const char *source, const char *end) {
    if (!source) return NULL;

    int depth = 1;
    const char *p = source;

    while (p < end && *p && depth > 0) {
        if (*p == '{')
            depth++;
        else if (*p == '}')
            depth--;

        if (depth > 0) p++;
    }

    return (depth == 0) ? p + 1 : NULL;
}

const char *recovery_skip_to_close_bracket(const char *source, const char *end) {
    if (!source) return NULL;

    int depth = 1;
    const char *p = source;

    while (p < end && *p && depth > 0) {
        if (*p == '[')
            depth++;
        else if (*p == ']')
            depth--;

        if (depth > 0) p++;
    }

    return (depth == 0) ? p + 1 : NULL;
}

const char *recovery_skip_to_close_paren(const char *source, const char *end) {
    if (!source) return NULL;

    int depth = 1;
    const char *p = source;

    while (p < end && *p && depth > 0) {
        if (*p == '(')
            depth++;
        else if (*p == ')')
            depth--;

        if (depth > 0) p++;
    }

    return (depth == 0) ? p + 1 : NULL;
}

const char *recovery_skip_to_end_inline_math(const char *source, const char *end) {
    if (!source) return NULL;

    const char *p = source;

    while (p < end && *p) {
        // Skip escaped dollar
        if (*p == '\\' && p[1] == '$') {
            p += 2;
            continue;
        }

        // Found closing $
        if (*p == '$' && (p == source || p[-1] != '$') && (p[1] != '$')) {
            return p + 1;
        }

        p++;
    }

    return NULL;
}

const char *recovery_skip_to_end_display_math(const char *source, const char *end) {
    if (!source) return NULL;

    const char *p = source;

    while (p < end - 1 && *p) {
        if (p[0] == '$' && p[1] == '$') {
            return p + 2;
        }
        p++;
    }

    return NULL;
}

const char *recovery_skip_to_paragraph_end(const char *source, const char *end) {
    if (!source) return NULL;

    const char *p = source;

    while (p < end && *p) {
        // Look for blank line (two newlines with only whitespace between)
        if (*p == '\n') {
            const char *q = p + 1;

            // Skip whitespace on next line
            while (q < end && (*q == ' ' || *q == '\t')) q++;

            // Another newline means paragraph break
            if (q < end && *q == '\n') {
                return q + 1;
            }
        }
        p++;
    }

    return end;
}

const char *recovery_skip_to_end_env(const char *source, const char *end, const char *env_name) {
    if (!source || !env_name) return NULL;

    // Build the expected \end{name} string
    char target[128];
    snprintf(target, sizeof(target), "\\end{%s}", env_name);
    int target_len = (int)strlen(target);

    const char *p = source;
    int depth = 1; // Start at depth 1 (we're inside the environment)

    while (p < end - target_len && *p) {
        // Check for nested \begin{same}
        if (starts_with(p, "\\begin{")) {
            const char *name_start = p + 7;
            const char *name_end = strchr(name_start, '}');
            if (name_end) {
                int name_len = (int)(name_end - name_start);
                if (name_len == (int)strlen(env_name) &&
                    strncmp(name_start, env_name, name_len) == 0) {
                    depth++;
                }
            }
        }

        // Check for \end{same}
        if (starts_with(p, target)) {
            depth--;
            if (depth == 0) {
                return p + target_len;
            }
        }

        p++;
    }

    return NULL;
}

// ============================================================================
// Balanced Delimiter Tracking
// ============================================================================

void delimiter_balance_init(DelimiterBalance *db) {
    if (!db) return;

    db->brace_depth = 0;
    db->bracket_depth = 0;
    db->paren_depth = 0;
    db->dollar_depth = 0;
    db->env_depth = 0;
}

bool delimiter_balance_update(DelimiterBalance *db, char c) {
    if (!db) return false;

    switch (c) {
    case '{': db->brace_depth++; break;
    case '}': db->brace_depth--; break;
    case '[': db->bracket_depth++; break;
    case ']': db->bracket_depth--; break;
    case '(': db->paren_depth++; break;
    case ')': db->paren_depth--; break;
    case '$': db->dollar_depth = db->dollar_depth ? 0 : 1; break;
    default: break;
    }

    return delimiter_balance_is_balanced(db);
}

void delimiter_balance_update_cmd(DelimiterBalance *db, const char *cmd, const char *env_name) {
    (void)env_name; // May be used for stricter matching

    if (!db || !cmd) return;

    if (strcmp(cmd, "\\begin") == 0) {
        db->env_depth++;
    } else if (strcmp(cmd, "\\end") == 0) {
        db->env_depth--;
    }
}

bool delimiter_balance_is_balanced(const DelimiterBalance *db) {
    if (!db) return true;

    return db->brace_depth == 0 && db->bracket_depth == 0 && db->paren_depth == 0 &&
           db->dollar_depth == 0 && db->env_depth == 0;
}

const char *delimiter_balance_first_unbalanced(const DelimiterBalance *db) {
    if (!db) return NULL;

    if (db->brace_depth > 0) return "{";
    if (db->brace_depth < 0) return "}";
    if (db->bracket_depth > 0) return "[";
    if (db->bracket_depth < 0) return "]";
    if (db->paren_depth > 0) return "(";
    if (db->paren_depth < 0) return ")";
    if (db->dollar_depth != 0) return "$";
    if (db->env_depth > 0) return "\\begin";
    if (db->env_depth < 0) return "\\end";

    return NULL;
}

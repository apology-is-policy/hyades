// diagnostics.c - Diagnostic logging system implementation

#include "diagnostics.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Internal State
// ============================================================================

#define DIAG_MAX_ENTRIES 10000
#define DIAG_MAX_LINE_LEN 256
#define DIAG_TRUNCATE_AT 60

typedef struct {
    DiagCategory cat;
    int indent;
    bool is_result; // true = continuation/result line with arrow
    char message[DIAG_MAX_LINE_LEN];
} DiagEntry;

static DiagCategory g_enabled_cats = DIAG_NONE;
static DiagEntry *g_entries = NULL;
static int g_entry_count = 0;
static int g_entry_capacity = 0;
static int g_sequence = 0; // Running sequence number

// ============================================================================
// String Buffer Helper
// ============================================================================

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} StrBuf;

static void strbuf_init(StrBuf *sb) {
    sb->capacity = 1024;
    sb->data = malloc(sb->capacity);
    sb->data[0] = '\0';
    sb->len = 0;
}

static void strbuf_free(StrBuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->capacity = 0;
}

static void strbuf_ensure(StrBuf *sb, size_t additional) {
    if (sb->len + additional + 1 > sb->capacity) {
        while (sb->len + additional + 1 > sb->capacity) {
            sb->capacity *= 2;
        }
        sb->data = realloc(sb->data, sb->capacity);
    }
}

static void strbuf_append(StrBuf *sb, const char *str) {
    size_t len = strlen(str);
    strbuf_ensure(sb, len);
    memcpy(sb->data + sb->len, str, len);
    sb->len += len;
    sb->data[sb->len] = '\0';
}

static void strbuf_appendf(StrBuf *sb, const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    strbuf_append(sb, buf);
}

static char *strbuf_detach(StrBuf *sb) {
    char *result = sb->data;
    sb->data = NULL;
    sb->len = 0;
    sb->capacity = 0;
    return result;
}

// ============================================================================
// Core API
// ============================================================================

void diag_enable(DiagCategory cats) {
    g_enabled_cats = cats;
    // Don't clear on enable - allow accumulation
}

void diag_disable(void) {
    g_enabled_cats = DIAG_NONE;
    diag_clear();
}

bool diag_is_enabled(DiagCategory cat) {
    return (g_enabled_cats & cat) != 0;
}

DiagCategory diag_get_enabled(void) {
    return g_enabled_cats;
}

static void ensure_capacity(void) {
    if (g_entry_count >= g_entry_capacity) {
        int new_cap = g_entry_capacity == 0 ? 64 : g_entry_capacity * 2;
        if (new_cap > DIAG_MAX_ENTRIES) new_cap = DIAG_MAX_ENTRIES;
        g_entries = realloc(g_entries, new_cap * sizeof(DiagEntry));
        g_entry_capacity = new_cap;
    }
}

// Escape newlines and truncate long strings
static void format_message(char *dest, size_t dest_size, const char *src) {
    size_t di = 0;
    size_t si = 0;
    size_t src_len = strlen(src);
    bool truncated = false;

    while (si < src_len && di < dest_size - 4) { // Leave room for "..."
        if (di >= DIAG_TRUNCATE_AT && !truncated) {
            // Check if we're near the end anyway
            if (src_len - si > 10) {
                truncated = true;
                break;
            }
        }

        char c = src[si++];
        if (c == '\n') {
            if (di + 2 < dest_size - 4) {
                dest[di++] = '\\';
                dest[di++] = 'n';
            } else
                break;
        } else if (c == '\t') {
            if (di + 2 < dest_size - 4) {
                dest[di++] = '\\';
                dest[di++] = 't';
            } else
                break;
        } else if (c == '\r') {
            // Skip carriage returns
        } else {
            dest[di++] = c;
        }
    }

    if (truncated) {
        dest[di++] = '.';
        dest[di++] = '.';
        dest[di++] = '.';
    }
    dest[di] = '\0';
}

static void log_internal(DiagCategory cat, int indent, bool is_result, const char *fmt,
                         va_list args) {
    if (!(g_enabled_cats & cat)) return;
    if (g_entry_count >= DIAG_MAX_ENTRIES) return;

    ensure_capacity();

    DiagEntry *entry = &g_entries[g_entry_count++];
    entry->cat = cat;
    entry->indent = indent;
    entry->is_result = is_result;

    // Format the message
    char raw_msg[DIAG_MAX_LINE_LEN * 2];
    vsnprintf(raw_msg, sizeof(raw_msg), fmt, args);
    format_message(entry->message, sizeof(entry->message), raw_msg);

    g_sequence++;
}

void diag_log(DiagCategory cat, int indent, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_internal(cat, indent, false, fmt, args);
    va_end(args);
}

void diag_result(DiagCategory cat, int indent, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_internal(cat, indent, true, fmt, args);
    va_end(args);
}

void diag_clear(void) {
    g_entry_count = 0;
    g_sequence = 0;
}

int diag_entry_count(void) {
    return g_entry_count;
}

// ============================================================================
// Output Formatting
// ============================================================================

const char *diag_category_name(DiagCategory cat) {
    switch (cat) {
    case DIAG_MACROS: return "MACRO";
    case DIAG_SYSTEM: return "SYSTEM";
    case DIAG_LAYOUT: return "LAYOUT";
    case DIAG_MATH: return "MATH";
    case DIAG_CALC: return "CALC";
    case DIAG_MERGE: return "MERGE";
    case DIAG_EXPANSION: return "EXPANSION";
    case DIAG_SUBNIVEAN: return "SUBNIVEAN";
    default: return "?";
    }
}

char *diag_get_output(void) {
    if (g_entry_count == 0) {
        return strdup("(no diagnostic entries)\n");
    }

    StrBuf sb;
    strbuf_init(&sb);

    strbuf_appendf(&sb, "=== Diagnostics (%d entries) ===\n", g_entry_count);

    for (int i = 0; i < g_entry_count; i++) {
        DiagEntry *e = &g_entries[i];

        if (e->cat || DIAG_EXPANSION) {
            strbuf_appendf(&sb, "%s\n", e->message);
            continue;
        }

        // Sequence number
        strbuf_appendf(&sb, "[%d] ", i + 1);

        // Indentation
        for (int j = 0; j < e->indent; j++) {
            strbuf_append(&sb, "  ");
        }

        if (e->is_result) {
            // Result line with arrow
            strbuf_appendf(&sb, "-> %s\n", e->message);
        } else {
            // Category label and message
            strbuf_appendf(&sb, "%s: %s\n", diag_category_name(e->cat), e->message);
        }
    }

    strbuf_append(&sb, "=== End Diagnostics ===\n");

    return strbuf_detach(&sb);
}

// ============================================================================
// Category Parsing
// ============================================================================

DiagCategory diag_parse_categories(const char *str) {
    if (!str || !*str) return DIAG_NONE;

    // Handle special cases
    if (strcmp(str, "all") == 0 || strcmp(str, "true") == 0) {
        return DIAG_ALL;
    }
    if (strcmp(str, "off") == 0 || strcmp(str, "false") == 0 || strcmp(str, "none") == 0) {
        return DIAG_NONE;
    }

    DiagCategory result = DIAG_NONE;

    // Parse comma-separated list
    const char *p = str;
    while (*p) {
        // Skip whitespace and commas
        while (*p == ' ' || *p == ',' || *p == '\t') p++;
        if (!*p) break;

        // Find end of token
        const char *start = p;
        while (*p && *p != ',' && *p != ' ' && *p != '\t') p++;
        size_t len = p - start;

        // Match category
        if (len == 6 && strncmp(start, "macros", 6) == 0) {
            result |= DIAG_MACROS;
        } else if (len == 6 && strncmp(start, "system", 6) == 0) {
            result |= DIAG_SYSTEM;
        } else if (len == 6 && strncmp(start, "layout", 6) == 0) {
            result |= DIAG_LAYOUT;
        } else if (len == 4 && strncmp(start, "math", 4) == 0) {
            result |= DIAG_MATH;
        } else if (len == 4 && strncmp(start, "calc", 4) == 0) {
            result |= DIAG_CALC;
        } else if (len == 5 && strncmp(start, "merge", 5) == 0) {
            result |= DIAG_MERGE;
        } else if (len == 3 && strncmp(start, "all", 3) == 0) {
            result |= DIAG_ALL;
        } else if (len == 9 && strncmp(start, "expansion", 9) == 0) {
            result |= DIAG_EXPANSION;
        } else if (len == 9 && strncmp(start, "subnivean", 9) == 0) {
            result |= DIAG_SUBNIVEAN;
        }
        // Unknown categories are silently ignored
    }

    return result;
}

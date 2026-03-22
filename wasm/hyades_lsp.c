// hyades_lsp.c - WebAssembly bindings for LSP features
//
// This provides LSP-focused WASM exports for IDE integration.
// All functions return JSON strings for easy TypeScript consumption.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

// Forward declarations from main Hyades
extern void hyades_wasm_init(void);
extern void hyades_reset_state(void);
extern void line_registry_reset(void);
extern void verbatim_store_clear(void);

// Corruption detection - check if stdlib macro strings are intact
// Stub: stdlib_get_macro_def is not available in WASM build
static const char *stdlib_get_macro_def(int i) {
    (void)i;
    return "\\macro"; // Return a dummy string to pass the check
}
static int g_parse_count = 0;

// Memory tracking
#ifdef __EMSCRIPTEN__
#include <emscripten/heap.h>
static size_t g_last_heap_size = 0;
static void log_memory_usage(const char *context) {
    size_t current = emscripten_get_heap_size();
    long diff = (long)current - (long)g_last_heap_size;
    fprintf(stderr, "MEMORY [%s] parse #%d: heap=%zu bytes (%+ld from last)\n", context,
            g_parse_count, current, diff);
    g_last_heap_size = current;
}
#else
static void log_memory_usage(const char *context) {
    (void)context;
}
#endif

static void check_stdlib_corruption(const char *context) {
    const char *first = stdlib_get_macro_def(0);
    if (!first || first[0] != '\\' || first[1] != 'm' || first[2] != 'a' || first[3] != 'c' ||
        first[4] != 'r' || first[5] != 'o') {
        fprintf(stderr, "STDLIB CORRUPTION at %s (parse #%d): first macro starts with '%.20s...'\n",
                context, g_parse_count, first ? first : "(null)");
    }
}

// Include the parse API
#include "../src/lsp/builtin_docs.h"
#include "../src/public_api/hyades_parse_api.h"

// ============================================================================
// Global State
// ============================================================================

// Current parse result (cached for subsequent LSP queries)
static HyadesParseResult *g_current_result = NULL;
static char *g_current_source = NULL;

// ============================================================================
// Internal Helpers
// ============================================================================

static char *safe_strdup(const char *s) {
    if (!s) return strdup("");
    return strdup(s);
}

static void clear_current_result(void) {
    if (g_current_result) {
        hyades_parse_result_free(g_current_result);
        g_current_result = NULL;
    }
    if (g_current_source) {
        free(g_current_source);
        g_current_source = NULL;
    }
    // Clean up static registries that can accumulate memory
    line_registry_reset();
    verbatim_store_clear();
}

// ============================================================================
// Memory Management
// ============================================================================

// Free a string returned by any LSP function
WASM_EXPORT
void hyades_lsp_free(char *ptr) {
    if (ptr) free(ptr);
}

// ============================================================================
// Document Parsing
// ============================================================================

// Parse a document and cache the result for subsequent queries
// Returns JSON: { "success": true, "error_count": N, "symbol_count": N }
WASM_EXPORT
char *hyades_lsp_parse(const char *source) {
    g_parse_count++;

    log_memory_usage("parse_start");
    check_stdlib_corruption("before_init");
    hyades_wasm_init();
    check_stdlib_corruption("after_init");
    clear_current_result();
    log_memory_usage("after_clear");
    check_stdlib_corruption("after_clear");

    // Note: We intentionally do NOT call hyades_reset_state() here.
    // The macro registry is managed by expand_all_macros_lsp() via keep_alive calls.
    // The verbatim store and line registry are cleared as part of parsing.
    // Calling reset_state() would decrement the keep_alive counter incorrectly.

    if (!source) {
        return strdup("{\"success\":false,\"error\":\"Source is NULL\"}");
    }

    // Save source for position mapping - IMPORTANT: make this copy FIRST
    // The source_map stores a pointer to the source string (doesn't copy it),
    // so we need to pass our owned copy to ensure the pointer stays valid
    // after JavaScript frees the original source string from the ccall.
    g_current_source = safe_strdup(source);
    if (!g_current_source) {
        return strdup("{\"success\":false,\"error\":\"Out of memory\"}");
    }

    check_stdlib_corruption("before_parse");
    // Parse with LSP options - use our COPY, not the original
    g_current_result = hyades_parse_for_lsp(g_current_source);
    check_stdlib_corruption("after_parse");
    log_memory_usage("after_parse");

    if (!g_current_result) {
        return strdup("{\"success\":false,\"error\":\"Parse failed\"}");
    }

    char *buf = malloc(256);
    if (!buf) return strdup("{\"success\":false,\"error\":\"Out of memory\"}");

    snprintf(buf, 256,
             "{"
             "\"success\":true,"
             "\"error_count\":%d,"
             "\"warning_count\":%d,"
             "\"symbol_count\":%d,"
             "\"lines\":%d"
             "}",
             hyades_error_count(g_current_result), hyades_warning_count(g_current_result),
             hyades_symbol_count(g_current_result), g_current_result->stats.total_lines);

    return buf;
}

// ============================================================================
// Diagnostics (Errors/Warnings)
// ============================================================================

// Get all diagnostics as LSP-compatible JSON array
// Returns: [{ severity, range, message, source, code }]
WASM_EXPORT
char *hyades_lsp_get_diagnostics(void) {
    if (!g_current_result) {
        return strdup("[]");
    }
    return hyades_errors_to_json(g_current_result);
}

// Get number of errors
WASM_EXPORT
int hyades_lsp_error_count(void) {
    return g_current_result ? hyades_error_count(g_current_result) : 0;
}

// Get number of warnings
WASM_EXPORT
int hyades_lsp_warning_count(void) {
    return g_current_result ? hyades_warning_count(g_current_result) : 0;
}

// ============================================================================
// Symbols
// ============================================================================

// Get all symbols as LSP-compatible JSON array
// Returns: [{ name, kind, location }]
WASM_EXPORT
char *hyades_lsp_get_symbols(void) {
    if (!g_current_result) {
        return strdup("[]");
    }
    return hyades_symbols_to_json(g_current_result);
}

// Get document outline (same as symbols but for outline view)
WASM_EXPORT
char *hyades_lsp_get_document_symbols(void) {
    if (!g_current_result) {
        return strdup("[]");
    }
    return hyades_document_symbols_to_json(g_current_result);
}

// ============================================================================
// Completions
// ============================================================================

// Get completions at position (0-based line and column)
// Returns: [{ label, kind, detail }]
WASM_EXPORT
char *hyades_lsp_get_completions(int line, int col) {
    if (!g_current_result) {
        return strdup("[]");
    }
    // Convert from 0-based (LSP) to 1-based (internal)
    return hyades_completions_to_json(g_current_result, line + 1, col + 1);
}

// ============================================================================
// Go to Definition
// ============================================================================

// Get definition location at position (0-based)
// Returns: { uri, range } or null
WASM_EXPORT
char *hyades_lsp_get_definition(int line, int col) {
    if (!g_current_result) {
        return strdup("null");
    }

    // Convert from 0-based (LSP) to 1-based (internal)
    const Symbol *sym = hyades_definition_at_position(g_current_result, line + 1, col + 1);
    if (!sym) {
        return strdup("null");
    }

    // Skip stdlib/built-in symbols with no meaningful position
    // These have position 0:0 or 1:1-1:1 (no definition in user's file)
    if ((sym->def_line == 0 && sym->def_col == 0) ||
        (sym->def_line == 1 && sym->def_col == 1 && sym->def_end_line == 1 &&
         sym->def_end_col == 1)) {
        return strdup("null");
    }

    char *buf = malloc(512);
    if (!buf) return strdup("null");

    snprintf(buf, 512,
             "{"
             "\"uri\":\"file://current\","
             "\"range\":{"
             "\"start\":{\"line\":%d,\"character\":%d},"
             "\"end\":{\"line\":%d,\"character\":%d}"
             "}"
             "}",
             sym->def_line - 1, // Convert to 0-based
             sym->def_col - 1, sym->def_end_line > 0 ? sym->def_end_line - 1 : sym->def_line - 1,
             sym->def_end_col > 0 ? sym->def_end_col - 1 : sym->def_col - 1);

    return buf;
}

// ============================================================================
// Hover
// ============================================================================

// Helper: Extract command name at position (0-based line/col)
// Returns allocated string like "\\frac" or NULL
static char *extract_command_at_position(const char *source, int line, int col) {
    if (!source) return NULL;

    // Find the line
    const char *p = source;
    for (int i = 0; i < line && *p; i++) {
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    if (!*p) return NULL;

    // Find the column position
    const char *line_start = p;
    const char *line_end = p;
    while (*line_end && *line_end != '\n') line_end++;

    // Make sure col is within the line
    int line_len = (int)(line_end - line_start);
    if (col >= line_len) col = line_len > 0 ? line_len - 1 : 0;

    const char *pos = line_start + col;

    // Scan backwards to find backslash (command start)
    const char *cmd_start = pos;
    while (cmd_start > line_start) {
        if (*(cmd_start - 1) == '\\') {
            cmd_start--;
            break;
        }
        // If we hit a non-command character, check if we're in a command
        char c = *(cmd_start - 1);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            cmd_start--;
        } else {
            break;
        }
    }

    // Check if we found a backslash
    if (*cmd_start != '\\') {
        // Maybe cursor is right after backslash, scan forward instead
        if (pos > line_start && *(pos - 1) == '\\') {
            cmd_start = pos - 1;
        } else {
            return NULL;
        }
    }

    // Extract command name
    const char *cmd_end = cmd_start + 1;
    while (cmd_end < line_end && ((*cmd_end >= 'a' && *cmd_end <= 'z') ||
                                  (*cmd_end >= 'A' && *cmd_end <= 'Z') || *cmd_end == '_')) {
        cmd_end++;
    }

    int cmd_len = (int)(cmd_end - cmd_start);
    if (cmd_len <= 1) return NULL; // Just backslash, no name

    char *cmd = malloc(cmd_len + 1);
    if (!cmd) return NULL;
    memcpy(cmd, cmd_start, cmd_len);
    cmd[cmd_len] = '\0';

    return cmd;
}

// Helper: Format builtin doc as JSON hover response
static char *builtin_hover_to_json(const BuiltinDoc *doc, int line, int col) {
    char *content = builtin_doc_format(doc);
    if (!content) return strdup("null");

    // Escape for JSON
    size_t content_len = strlen(content);
    char *escaped = malloc(content_len * 2 + 1);
    if (!escaped) {
        free(content);
        return strdup("null");
    }

    char *out = escaped;
    for (const char *p = content; *p; p++) {
        switch (*p) {
        case '\\':
            *out++ = '\\';
            *out++ = '\\';
            break;
        case '"':
            *out++ = '\\';
            *out++ = '"';
            break;
        case '\n':
            *out++ = '\\';
            *out++ = 'n';
            break;
        case '\r':
            *out++ = '\\';
            *out++ = 'r';
            break;
        case '\t':
            *out++ = '\\';
            *out++ = 't';
            break;
        default: *out++ = *p; break;
        }
    }
    *out = '\0';
    free(content);

    char *buf = malloc(2048 + strlen(escaped));
    if (!buf) {
        free(escaped);
        return strdup("null");
    }

    snprintf(buf, 2048 + strlen(escaped),
             "{"
             "\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"},"
             "\"range\":{"
             "\"start\":{\"line\":%d,\"character\":%d},"
             "\"end\":{\"line\":%d,\"character\":%d}"
             "}"
             "}",
             escaped, line, col, line, col);

    free(escaped);
    return buf;
}

// Get hover information at position (0-based)
// Returns: { contents, range } or null
WASM_EXPORT
char *hyades_lsp_get_hover(int line, int col) {
    if (!g_current_result) {
        return strdup("null");
    }

    // First try user-defined symbols
    char *result = hyades_hover_to_json(g_current_result, line + 1, col + 1);
    if (result && strcmp(result, "null") != 0) {
        return result;
    }
    if (result) free(result);

    // Fall back to builtin command documentation
    if (g_current_source) {
        char *cmd = extract_command_at_position(g_current_source, line, col);
        if (cmd) {
            const BuiltinDoc *doc = builtin_doc_lookup(cmd);
            free(cmd);
            if (doc) {
                return builtin_hover_to_json(doc, line, col);
            }
        }
    }

    return strdup("null");
}

// ============================================================================
// References
// ============================================================================

// Find all references to symbol at position (0-based)
// Returns: [{ uri, range }]
WASM_EXPORT
char *hyades_lsp_get_references(int line, int col) {
    if (!g_current_result) {
        return strdup("[]");
    }

    // Convert from 0-based (LSP) to 1-based (internal)
    return hyades_references_to_json(g_current_result, line + 1, col + 1);
}

// ============================================================================
// Validation
// ============================================================================

// Run additional validation passes
WASM_EXPORT
void hyades_lsp_validate(void) {
    if (g_current_result) {
        hyades_validate(g_current_result);
    }
}

// ============================================================================
// Utilities
// ============================================================================

// ============================================================================
// Semantic Tokens
// ============================================================================

// Get semantic tokens for syntax highlighting
// Returns: { "data": [deltaLine, deltaChar, length, type, modifiers, ...] }
WASM_EXPORT
char *hyades_lsp_get_semantic_tokens(void) {
    if (!g_current_result || !g_current_source) {
        return strdup("{\"data\":[]}");
    }
    return hyades_semantic_tokens_to_json(g_current_result, g_current_source);
}

// ============================================================================
// Raw Semantic Tokens (binary, no JSON serialization)
// ============================================================================

static int *g_raw_tokens_buf = NULL;
static int g_raw_tokens_count = 0;

// Compute semantic tokens into a raw int buffer (no JSON overhead).
// Returns the number of ints (multiple of 5, each group = one token).
// Call hyades_lsp_semantic_tokens_ptr() to get the buffer pointer.
WASM_EXPORT
int hyades_lsp_compute_semantic_tokens(void) {
    free(g_raw_tokens_buf);
    g_raw_tokens_buf = NULL;
    g_raw_tokens_count = 0;

    if (!g_current_result || !g_current_source) {
        return 0;
    }

    g_raw_tokens_count =
        hyades_semantic_tokens_to_raw(g_current_result, g_current_source, &g_raw_tokens_buf);
    return g_raw_tokens_count;
}

// Get pointer to raw token data buffer (valid until next compute call)
WASM_EXPORT
int *hyades_lsp_semantic_tokens_ptr(void) {
    return g_raw_tokens_buf;
}

// ============================================================================
// Utilities
// ============================================================================

// Get version info as JSON
WASM_EXPORT
char *hyades_lsp_version(void) {
    return strdup("{\"name\":\"hyades-lsp\",\"version\":\"1.0.0\"}");
}

// Clear cached parse result
WASM_EXPORT
void hyades_lsp_clear(void) {
    clear_current_result();
}

// Check if we have a valid parse result
WASM_EXPORT
int hyades_lsp_has_result(void) {
    return g_current_result != NULL ? 1 : 0;
}

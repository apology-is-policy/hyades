// document_parser.c - Document parsing for Hyades
//
// Parses Hyades source documents into a BoxLayout tree structure.
// Handles:
// - Display math ($$...$$)
// - Document commands (\setunicode, etc.)
// - Box layouts (\begin{vbox}, \begin{hbox})
// - Vertical spacing (\vskip, \smallskip, etc.)
// - Paragraph breaks (blank lines)
// - Input file inclusion (\input{file})
// - Macro expansion
// - Julia computation (\julia, \call)

#include "calc.h"
#include "compositor/compositor.h"
#include "diagnostics/diagnostics.h"
#include "doc_commands.h"
#include "document.h"
#include "layout/layout.h"
#include "layout/layout_types.h"
#include "macro_expand.h"
#include "utils/strbuf.h"

// LSP infrastructure
#include "document/delimiter_stack.h"
#include "document/source_map.h"
#include "document/symbol_table.h"
#include "utils/error.h"
#include "utils/parse_options.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External declarations for functions defined elsewhere
extern BoxLayout *parse_box_layout(const char *input, int *end_pos);
extern BoxLayout *parse_box_layout_with_width(const char *input, int *end_pos, int target_width);
extern BoxLayout *parse_hrule_command(const char *input, int *pos);
extern BoxLayout *parse_vrule_command(const char *input, int *pos);
extern CompOptions default_options(void);

// ============================================================================
// Defensive Limits (prevent hangs from memory corruption)
// ============================================================================

// Maximum characters to scan in string parsing loops.
// If we scan more than this, something is wrong (corruption or missing terminator).
#define MAX_SCAN_CHARS (500 * 1024 * 1024) // 500MB - effectively unlimited

// ============================================================================
// Forward Declarations
// ============================================================================

static char *expand_input_commands(const char *input, int *recursion_depth);
static char *strip_tex_comments(const char *in);
static void check_delimiters_pre_expansion(const char *source, ParseErrorList *errors);
static void parse_command_args(const char *input, int *pos, char ***args_out, int *n_args);
static BoxLayout *parse_intersect_rules_command(const char *input, int *end_pos, int width);
static void insert_implicit_spacing(BoxLayout *root, const CompOptions *opt);

// LSP-aware parsing context
typedef struct {
    DelimiterStack *delim_stack;
    SourceMap *source_map;
    LspSymbolTable *symbols;
    ParseErrorList *errors;
    const char *original_source;
    int current_line;
    int current_col;
    // Ownership flags - if true, we own and should free on cleanup
    bool owns_errors;
    bool owns_symbols;
} LspParseContext;

static void lsp_ctx_init(LspParseContext *ctx, const char *source, const HyadesParseOptions *opts);
static void lsp_ctx_free(LspParseContext *ctx);
static void lsp_ctx_update_position(LspParseContext *ctx, const char *start, const char *current);
static void lsp_ctx_push_delimiter(LspParseContext *ctx, DelimiterType type, const char *text,
                                   const char *env_name, int pos);
static bool lsp_ctx_pop_delimiter(LspParseContext *ctx, DelimiterType type, const char *text,
                                  const char *env_name, int pos);

// ============================================================================
// LSP Context Helpers
// ============================================================================

static void lsp_ctx_init(LspParseContext *ctx, const char *source, const HyadesParseOptions *opts) {
    ctx->original_source = source;
    ctx->current_line = 1;
    ctx->current_col = 1;
    ctx->owns_errors = false;
    ctx->owns_symbols = false;

    if (opts && opts->build_symbol_table) {
        ctx->delim_stack = delimiter_stack_new();
        ctx->source_map = source_map_new_with_source(source);
        ctx->symbols = lsp_symbol_table_new();
        ctx->errors = parse_error_list_new_with_max(opts->max_errors);
        ctx->owns_errors = true;
        ctx->owns_symbols = true;
    } else {
        ctx->delim_stack = NULL;
        ctx->source_map = NULL;
        ctx->symbols = NULL;
        ctx->errors = NULL;
    }
}

static void lsp_ctx_free(LspParseContext *ctx) {
    if (ctx->delim_stack) delimiter_stack_free(ctx->delim_stack);
    if (ctx->source_map) source_map_free(ctx->source_map);
    // Only free errors/symbols if we own them (not externally provided)
    if (ctx->owns_symbols && ctx->symbols) lsp_symbol_table_free(ctx->symbols);
    if (ctx->owns_errors && ctx->errors) parse_error_list_free(ctx->errors);

    ctx->delim_stack = NULL;
    ctx->source_map = NULL;
    ctx->symbols = NULL;
    ctx->errors = NULL;
}

static void lsp_ctx_update_position(LspParseContext *ctx, const char *start, const char *current) {
    if (!ctx || !start || !current) return;

    // Compute line and column from byte offset
    ctx->current_line = 1;
    ctx->current_col = 1;
    for (const char *p = start; p < current; p++) {
        if (*p == '\n') {
            ctx->current_line++;
            ctx->current_col = 1;
        } else {
            ctx->current_col++;
        }
    }
}

static void lsp_ctx_push_delimiter(LspParseContext *ctx, DelimiterType type, const char *text,
                                   const char *env_name, int pos) {
    if (!ctx || !ctx->delim_stack) return;
    delimiter_stack_push(ctx->delim_stack, type, text, env_name, ctx->current_line,
                         ctx->current_col, pos);
}

static bool lsp_ctx_pop_delimiter(LspParseContext *ctx, DelimiterType type, const char *text,
                                  const char *env_name, int pos) {
    if (!ctx || !ctx->delim_stack) return true; // No validation if no stack
    return delimiter_stack_pop(ctx->delim_stack, type, text, env_name, ctx->current_line,
                               ctx->current_col, pos, ctx->errors);
}

// ============================================================================
// \intersect_rules{...} Parsing
// ============================================================================

static BoxLayout *parse_intersect_rules_command(const char *input, int *end_pos, int width) {
    const char *p = input;

    if (strncmp(p, "\\intersect_rules", 16) != 0) return NULL;
    p += 16;

    while (*p == ' ' || *p == '\t' || *p == '\n') p++;

    if (*p != '{') return NULL;
    p++;

    // Find matching '}'
    const char *content_start = p;
    int brace_depth = 1;
    while (*p && brace_depth > 0) {
        if (*p == '{')
            brace_depth++;
        else if (*p == '}')
            brace_depth--;
        if (brace_depth > 0) p++;
    }

    if (brace_depth != 0) return NULL;

    size_t content_len = p - content_start;
    char *content = malloc(content_len + 1);
    memcpy(content, content_start, content_len);
    content[content_len] = '\0';

    p++;
    *end_pos = (int)(p - input);

    // Create wrapper
    BoxLayout *wrapper = box_layout_new(BOX_TYPE_INTERSECT_RULES, -1);

    // Parse inner content
    int inner_pos = 0;
    BoxLayout *inner = parse_box_layout_with_width(content, &inner_pos, width);
    free(content);

    if (!inner) {
        box_layout_free(wrapper);
        return NULL;
    }

    box_layout_add_child(wrapper, inner);
    return wrapper;
}

// ============================================================================
// \lineroutine<routine>{content} Parsing
// ============================================================================

static BoxLayout *parse_lineroutine_command(const char *input, int *end_pos, int width) {
    const char *p = input;

    if (strncmp(p, "\\lineroutine", 12) != 0) return NULL;
    p += 12;

    while (*p == ' ' || *p == '\t') p++;

    // Expect '<' for meta argument
    if (*p != '<') return NULL;
    p++;

    // Parse routine name (up to '>')
    const char *name_start = p;
    while (*p && *p != '>') p++;
    if (*p != '>') return NULL;

    size_t name_len = p - name_start;
    char *routine_name = malloc(name_len + 1);
    memcpy(routine_name, name_start, name_len);
    routine_name[name_len] = '\0';

    p++; // Skip '>'

    while (*p == ' ' || *p == '\t' || *p == '\n') p++;

    // Expect '{' for content
    if (*p != '{') {
        free(routine_name);
        return NULL;
    }
    p++;

    // Find matching '}'
    const char *content_start = p;
    int brace_depth = 1;
    while (*p && brace_depth > 0) {
        if (*p == '{')
            brace_depth++;
        else if (*p == '}')
            brace_depth--;
        if (brace_depth > 0) p++;
    }

    if (brace_depth != 0) {
        free(routine_name);
        return NULL;
    }

    size_t content_len = p - content_start;
    char *content = malloc(content_len + 1);
    memcpy(content, content_start, content_len);
    content[content_len] = '\0';

    p++; // Skip '}'
    *end_pos = (int)(p - input);

    // Create LINEROUTINE node
    BoxLayout *node = box_layout_new(BOX_TYPE_LINEROUTINE, -1);
    node->routine_name = routine_name;

    // Parse the content as a document (it will be rendered and then line-processed)
    ParseError err = {0};
    BoxLayout *inner = parse_document_as_vbox(content, width > 0 ? width : 80, &err);
    free(content);

    if (inner) {
        box_layout_add_child(node, inner);
    }

    return node;
}

// ============================================================================
// File I/O Helpers
// ============================================================================

static void strip_cr(char *s) {
    char *w = s;
    for (const char *r = s; *r; r++) {
        if (*r != '\r') *w++ = *r;
    }
    *w = '\0';
}

static char *read_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(content, 1, size, f);
    content[read_size] = '\0';
    fclose(f);

    strip_cr(content);
    return content;
}

// ============================================================================
// Input Expansion (\input{file})
// ============================================================================

static char *expand_input_commands(const char *input, int *recursion_depth) {
    if (!input) return NULL;
    if (*recursion_depth > 10) return strdup(input); // Prevent infinite recursion

    // Check if input contains \input{
    if (!strstr(input, "\\input{")) return strdup(input);

    // Build expanded string
    StrBuf result;
    strbuf_init_with_capacity(&result, strlen(input) * 2);

    const char *p = input;
    while (*p) {
        if (strncmp(p, "\\input{", 7) == 0) {
            // Found \input{filename}
            p += 7;
            const char *filename_start = p;

            // Find closing }
            while (*p && *p != '}') p++;
            if (*p != '}') break;

            size_t filename_len = p - filename_start;
            char *filename = malloc(filename_len + 1);
            memcpy(filename, filename_start, filename_len);
            filename[filename_len] = '\0';

            // Read file
            char *file_content = read_file(filename);
            if (file_content) {
                // Recursively expand the file content
                (*recursion_depth)++;
                char *expanded_content = expand_input_commands(file_content, recursion_depth);
                (*recursion_depth)--;
                free(file_content);

                if (expanded_content) {
                    strbuf_append(&result, expanded_content);
                    free(expanded_content);
                }
            }

            free(filename);
            p++; // Skip }
        } else {
            // Regular character
            strbuf_putc(&result, *p++);
        }
    }

    return strbuf_detach(&result);
}

// ============================================================================
// Verbatim Protection
// ============================================================================

// Global storage for verbatim contents
// Thread-safety note: This is not thread-safe, but matches the rest of the codebase
#define MAX_VERBATIM_BLOCKS 256

static struct {
    char *contents[MAX_VERBATIM_BLOCKS];
    int n;
    bool locked; // If true, don't clear during subsequent parses
} verbatim_store = {0};

void verbatim_store_clear(void) {
    verbatim_store.locked = false;
    for (int i = 0; i < verbatim_store.n; i++) {
        free(verbatim_store.contents[i]);
        verbatim_store.contents[i] = NULL;
    }
    verbatim_store.n = 0;
}

const char *verbatim_store_get(int index) {
    if (index >= 0 && index < verbatim_store.n) {
        const char *content = verbatim_store.contents[index];
        if (!content) return NULL; // Safety check
        if (diag_is_enabled(DIAG_SYSTEM)) {
            int lines = 1;
            for (const char *p = content; *p; p++) {
                if (*p == '\n') lines++;
            }
            diag_log(DIAG_SYSTEM, 1, "VERBATIM: retrieving @@VERB_%d@@ (%zu chars, %d lines)",
                     index, strlen(content), lines);
        }
        return content;
    }
    if (diag_is_enabled(DIAG_SYSTEM)) {
        diag_log(DIAG_SYSTEM, 1, "VERBATIM: @@VERB_%d@@ NOT FOUND (store has %d entries)", index,
                 verbatim_store.n);
    }
    return NULL;
}

// Protect \verb|...| sequences by replacing them with placeholders
// The delimiter is any character immediately following \verb
// Example: \verb|code| or \verb+code+ or \verb!code!
char *protect_verbatim(const char *input) {
    if (!input) return NULL;

    // If store is locked, we still need to scan for NEW \verb sequences
    // (e.g., from macro expansion) but don't clear existing entries
    bool was_locked = verbatim_store.locked;
    if (!was_locked) {
        // Clear any previous verbatim contents (only on first call)
        verbatim_store_clear();
    } else {
        if (diag_is_enabled(DIAG_SYSTEM)) {
            diag_log(DIAG_SYSTEM, 0,
                     "VERBATIM: store locked, scanning for new sequences (preserving %d existing "
                     "entries)",
                     verbatim_store.n);
        }
    }

    if (diag_is_enabled(DIAG_SYSTEM)) {
        diag_log(DIAG_SYSTEM, 0, "VERBATIM: scanning for \\verb sequences");
    }

    size_t len = strlen(input);
    // Allocate generous buffer (placeholders are shorter than \verb|...|)
    char *out = malloc(len * 2 + 1);
    if (!out) return NULL;

    const char *src = input;
    char *dst = out;

    while (*src) {
        // Check for \verb
        if (strncmp(src, "\\verb", 5) == 0 && !isalpha((unsigned char)src[5])) {
            char delim = src[5];
            if (delim && delim != ' ' && delim != '\t' && delim != '\n') {
                // Find closing delimiter
                const char *content_start = src + 6;
                const char *content_end = strchr(content_start, delim);

                if (content_end && verbatim_store.n < MAX_VERBATIM_BLOCKS) {
                    // Store the verbatim content, trimming leading/trailing newlines
                    // (common when delimiter is on separate line: \verb|\n...\n|)
                    const char *trim_start = content_start;
                    const char *trim_end = content_end;

                    // Trim leading newline
                    if (trim_start < trim_end && *trim_start == '\n') {
                        trim_start++;
                    }

                    // Trim trailing newline
                    if (trim_start < trim_end && *(trim_end - 1) == '\n') {
                        trim_end--;
                    }

                    size_t content_len = trim_end - trim_start;
                    char *content = malloc(content_len + 1);
                    if (content) {
                        memcpy(content, trim_start, content_len);
                        content[content_len] = '\0';
                        verbatim_store.contents[verbatim_store.n] = content;

                        if (diag_is_enabled(DIAG_SYSTEM)) {
                            // Count newlines to report line count
                            int newlines = 0;
                            for (size_t i = 0; i < content_len; i++) {
                                if (content[i] == '\n') newlines++;
                            }
                            diag_log(DIAG_SYSTEM, 1,
                                     "found \\verb%c...%c (%zu chars, %d lines) -> @@VERB_%d@@",
                                     delim, delim, content_len, newlines + 1, verbatim_store.n);
                        }

                        // Write placeholder (use @@ to avoid backslash-underscore expansion)
                        dst += sprintf(dst, "@@VERB_%d@@", verbatim_store.n);
                        verbatim_store.n++;

                        src = content_end + 1; // Skip past closing delimiter
                        continue;
                    }
                }
            }
        }

        *dst++ = *src++;
    }
    *dst = '\0';

    // Lock the store if we stored any verbatim content
    // This prevents clearing during rendering-phase parses
    if (verbatim_store.n > 0) {
        verbatim_store.locked = true;
        if (diag_is_enabled(DIAG_SYSTEM)) {
            if (was_locked) {
                diag_log(DIAG_SYSTEM, 0, "VERBATIM: now have %d entries total", verbatim_store.n);
            } else {
                diag_log(DIAG_SYSTEM, 0, "VERBATIM: stored %d entries, store locked",
                         verbatim_store.n);
            }
        }
    } else if (diag_is_enabled(DIAG_SYSTEM)) {
        diag_log(DIAG_SYSTEM, 0, "VERBATIM: no \\verb sequences found");
    }

    return out;
}

// ============================================================================
// Comment Stripping
// ============================================================================

static char *strip_tex_comments(const char *in) {
    if (!in) return NULL;

    size_t len = strlen(in);
    char *out = malloc(len + 1);
    if (!out) return NULL;

    const char *src = in;
    char *dst = out;

    while (*src) {
        // Check for escaped \%
        if (*src == '\\' && *(src + 1) == '%') {
            *dst++ = *src++; // Copy backslash
            *dst++ = *src++; // Copy percent
            continue;
        }

        // Check for comment
        if (*src == '%') {
            // Skip comment until newline
            while (*src && *src != '\n') {
                src++;
            }
            // Also skip the newline itself (LaTeX behavior: % consumes the newline)
            // This is critical for lambda bodies where % is used as line continuation
            if (*src == '\n') {
                src++;
            }
            // Also skip leading whitespace on the next line (LaTeX behavior)
            while (*src == ' ' || *src == '\t') {
                src++;
            }
            continue;
        }

        // Copy regular character
        *dst++ = *src++;
    }

    *dst = '\0';
    return out;
}

// ============================================================================
// Pre-expansion delimiter checking (for accurate line numbers)
// ============================================================================

// Check if a line contains NBSP (U+00A0, UTF-8: C2 A0)
// These are rendered output lines that should be skipped
static bool line_contains_nbsp(const char *line_start, const char *line_end) {
    for (const char *p = line_start; p < line_end; p++) {
        if (p[0] == (char)0xC2 && p + 1 < line_end && p[1] == (char)0xA0) {
            return true;
        }
    }
    return false;
}

// Check for basic delimiter errors before macro expansion.
// This allows us to report errors at their original source positions.
// Helper: Extract environment name from \begin{name} or \end{name}
// Also handles Hyades special forms: \begin<arr>[vars]{enumerate}, \begin[opts]{env}
// Returns malloc'd string or NULL. Caller must free.
static char *extract_env_name(const char *p, int *consumed) {
    // p points at \begin or \end
    const char *start = p;
    if (strncmp(p, "\\begin", 6) == 0) {
        p += 6;
    } else if (strncmp(p, "\\end", 4) == 0) {
        p += 4;
    } else {
        return NULL;
    }

    // Skip optional <array_name> for enumerate syntax
    if (*p == '<') {
        int angle_depth = 1;
        p++;
        while (*p && angle_depth > 0) {
            if (*p == '<')
                angle_depth++;
            else if (*p == '>')
                angle_depth--;
            else if (*p == '\n')
                return NULL; // Malformed
            p++;
        }
    }

    // Skip optional [...]
    if (*p == '[') {
        int bracket_depth = 1;
        p++;
        while (*p && bracket_depth > 0) {
            if (*p == '[')
                bracket_depth++;
            else if (*p == ']')
                bracket_depth--;
            else if (*p == '\n')
                return NULL; // Malformed
            p++;
        }
    }

    // Must have {name}
    if (*p != '{') return NULL;
    p++;
    const char *name_start = p;
    while (*p && *p != '}' && *p != '\n') p++;
    if (*p != '}') return NULL;

    int name_len = (int)(p - name_start);
    if (name_len == 0 || name_len > 64) return NULL;

    char *name = malloc(name_len + 1);
    if (!name) return NULL;
    memcpy(name, name_start, name_len);
    name[name_len] = '\0';

    if (consumed) *consumed = (int)(p + 1 - start);
    return name;
}

// Skips: NBSP lines (rendered output), \verb|...|, \begin{verbatim}...\end{verbatim}
// Tracks: $$, $, \begin{X}...\end{X}
static void check_delimiters_pre_expansion(const char *source, ParseErrorList *errors) {
    if (!source || !errors) return;

    // Use delimiter stack for proper nesting validation
    DelimiterStack *ds = delimiter_stack_new();
    if (!ds) return;

    int line = 1;
    int col = 1;
    const char *p = source;

    while (*p) {
        // Find end of current line
        const char *line_start = p;
        const char *line_end = p;
        while (*line_end && *line_end != '\n') line_end++;

        // Check if this line contains NBSP (rendered output) - skip it entirely
        if (line_contains_nbsp(line_start, line_end)) {
            p = line_end;
            if (*p == '\n') {
                p++;
                line++;
                col = 1;
            }
            continue;
        }

        // Check for Cassilda directives at start of line
        // @label name - push
        if (line_start[0] == '@' && strncmp(line_start, "@label", 6) == 0 &&
            (line_start[6] == ' ' || line_start[6] == '\t')) {
            // Extract label name for error messages
            const char *name_start = line_start + 7;
            while (*name_start == ' ' || *name_start == '\t') name_start++;
            const char *name_end = name_start;
            while (name_end < line_end && *name_end != ' ' && *name_end != '\t') name_end++;
            char label_name[64] = "@label";
            int name_len = (int)(name_end - name_start);
            if (name_len > 0 && name_len < 50) {
                label_name[6] = ' ';
                memcpy(label_name + 7, name_start, name_len);
                label_name[7 + name_len] = '\0';
            }
            delimiter_stack_push(ds, DELIM_CASSILDA_LABEL, label_name, NULL, line, 1,
                                 (int)(line_start - source));
            p = line_end;
            if (*p == '\n') {
                p++;
                line++;
                col = 1;
            }
            continue;
        }

        // @end - pop label
        if (line_start[0] == '@' && strncmp(line_start, "@end", 4) == 0 &&
            (line_start[4] == '\n' || line_start[4] == '\0' || line_start[4] == ' ')) {
            delimiter_stack_pop(ds, DELIM_CASSILDA_LABEL, "@end", NULL, line, 1,
                                (int)(line_start - source), errors);
            p = line_end;
            if (*p == '\n') {
                p++;
                line++;
                col = 1;
            }
            continue;
        }

        // #before_each or #after_each - push block
        if (line_start[0] == '#' && (strncmp(line_start, "#before_each", 12) == 0 ||
                                     strncmp(line_start, "#after_each", 11) == 0)) {
            const char *block_name = (line_start[1] == 'b') ? "#before_each" : "#after_each";
            delimiter_stack_push(ds, DELIM_CASSILDA_BLOCK, block_name, NULL, line, 1,
                                 (int)(line_start - source));
            p = line_end;
            if (*p == '\n') {
                p++;
                line++;
                col = 1;
            }
            continue;
        }

        // #end - pop block
        if (line_start[0] == '#' && strncmp(line_start, "#end", 4) == 0 &&
            (line_start[4] == '\n' || line_start[4] == '\0' || line_start[4] == ' ')) {
            delimiter_stack_pop(ds, DELIM_CASSILDA_BLOCK, "#end", NULL, line, 1,
                                (int)(line_start - source), errors);
            p = line_end;
            if (*p == '\n') {
                p++;
                line++;
                col = 1;
            }
            continue;
        }

        // Process this line character by character (stop before newline)
        while (p < line_end) {
            // Skip % comments (rest of line)
            if (*p == '%') {
                // Check for escaped \%
                if (p > source && *(p - 1) == '\\') {
                    p++;
                    col++;
                    continue;
                }
                // Skip to end of line
                p = line_end;
                break;
            }

            // Skip \verb|...| constructs (can span multiple lines)
            if (p[0] == '\\' && strncmp(p, "\\verb", 5) == 0) {
                p += 5;
                col += 5;
                if (*p && !isalpha((unsigned char)*p)) {
                    char delim = *p;
                    p++;
                    col++;
                    while (*p && *p != delim) {
                        if (*p == '\n') {
                            line++;
                            col = 1;
                            p++;
                        } else {
                            p++;
                            col++;
                        }
                    }
                    if (*p == delim) {
                        p++;
                        col++;
                    }
                }
                line_end = p;
                while (*line_end && *line_end != '\n') line_end++;
                continue;
            }

            // Skip \begin{verbatim}...\end{verbatim}
            if (p[0] == '\\' && strncmp(p, "\\begin{verbatim}", 16) == 0) {
                p += 16;
                col += 16;
                while (*p) {
                    if (*p == '\n') {
                        line++;
                        col = 1;
                        p++;
                        continue;
                    }
                    if (strncmp(p, "\\end{verbatim}", 14) == 0) {
                        p += 14;
                        col += 14;
                        break;
                    }
                    p++;
                    col++;
                }
                continue;
            }

            // Skip \begin{lstlisting}...\end{lstlisting}
            if (p[0] == '\\' && strncmp(p, "\\begin{lstlisting}", 18) == 0) {
                p += 18;
                col += 18;
                while (*p) {
                    if (*p == '\n') {
                        line++;
                        col = 1;
                        p++;
                        continue;
                    }
                    if (strncmp(p, "\\end{lstlisting}", 16) == 0) {
                        p += 16;
                        col += 16;
                        break;
                    }
                    p++;
                    col++;
                }
                continue;
            }

            // Check for \begin - push environment
            // Handle: \begin{env}, \begin[opts]{env}, \begin<arr>[vars]{enumerate}
            if (p[0] == '\\' && strncmp(p, "\\begin", 6) == 0 &&
                (p[6] == '{' || p[6] == '[' || p[6] == '<')) {
                int consumed = 0;
                char *env_name = extract_env_name(p, &consumed);
                if (env_name) {
                    delimiter_stack_push(ds, DELIM_BEGIN_END, "\\begin", env_name, line, col,
                                         (int)(p - source));
                    free(env_name);
                    p += consumed;
                    col += consumed;
                    continue;
                }
            }

            // Check for \end - pop environment
            if (p[0] == '\\' && strncmp(p, "\\end", 4) == 0 && (p[4] == '{' || p[4] == '[')) {
                int consumed = 0;
                char *env_name = extract_env_name(p, &consumed);
                if (env_name) {
                    delimiter_stack_pop(ds, DELIM_BEGIN_END, "\\end", env_name, line, col,
                                        (int)(p - source), errors);
                    free(env_name);
                    p += consumed;
                    col += consumed;
                    continue;
                }
            }

            // Check for \left - push delimiter with what follows
            // BUT skip this if we're inside angle brackets (e.g., \macro<\left{...}>)
            // because \left/\right there are macro names, not delimiters
            const DelimiterEntry *top_entry = delimiter_stack_peek(ds);
            bool inside_angle = top_entry && top_entry->type == DELIM_ANGLE;

            if (!inside_angle && p[0] == '\\' && strncmp(p, "\\left", 5) == 0 &&
                !isalpha((unsigned char)p[5])) {
                // Build the full \left<delim> string for error messages
                char left_str[16] = "\\left";
                int len = 5;
                p += 5;
                col += 5;
                // Capture the delimiter character(s)
                if (*p == '\\') {
                    // Handle \left\{ or \left\langle etc.
                    left_str[len++] = *p;
                    p++;
                    col++;
                    // Check if it's a single escaped char like \{ or a command like \langle
                    if (*p == '{' || *p == '}' || *p == '|') {
                        // Single escaped char: \{, \}, \|
                        left_str[len++] = *p;
                        p++;
                        col++;
                    } else {
                        // Command like \langle, \lfloor, etc.
                        while (p < line_end && isalpha((unsigned char)*p) && len < 14) {
                            left_str[len++] = *p;
                            p++;
                            col++;
                        }
                    }
                } else if (*p && *p != '\n') {
                    // Single char delimiter: \left( or \left.
                    left_str[len++] = *p;
                    p++;
                    col++;
                }
                left_str[len] = '\0';
                delimiter_stack_push(ds, DELIM_LEFT_RIGHT, left_str, NULL, line, col - len,
                                     (int)(p - len - source));
                continue;
            }

            // Check for \right - pop and validate
            // Skip if inside angle brackets (e.g., \macro<\right{...}>)
            if (!inside_angle && p[0] == '\\' && strncmp(p, "\\right", 6) == 0 &&
                !isalpha((unsigned char)p[6])) {
                char right_str[16] = "\\right";
                int len = 6;
                int start_col = col;
                p += 6;
                col += 6;
                // Capture the delimiter
                if (*p == '\\') {
                    // Handle \right\} or \right\rangle etc.
                    right_str[len++] = *p;
                    p++;
                    col++;
                    if (*p == '{' || *p == '}' || *p == '|') {
                        right_str[len++] = *p;
                        p++;
                        col++;
                    } else {
                        while (p < line_end && isalpha((unsigned char)*p) && len < 14) {
                            right_str[len++] = *p;
                            p++;
                            col++;
                        }
                    }
                } else if (*p && *p != '\n') {
                    right_str[len++] = *p;
                    p++;
                    col++;
                }
                right_str[len] = '\0';
                delimiter_stack_pop(ds, DELIM_LEFT_RIGHT, right_str, NULL, line, start_col,
                                    (int)(p - len - source), errors);
                continue;
            }

            // Check for calc command angle brackets: \let<, \lambda<, \macro<, etc.
            if (p[0] == '\\' && isalpha((unsigned char)p[1])) {
                const char *cmd_start = p;
                p++;
                col++; // skip backslash
                while (p < line_end && (isalpha((unsigned char)*p) || *p == '_')) {
                    p++;
                    col++;
                }
                if (*p == '<') {
                    // This is a calc command with angle brackets
                    char cmd_str[64];
                    int cmd_len = (int)(p - cmd_start);
                    if (cmd_len < 60) {
                        memcpy(cmd_str, cmd_start, cmd_len);
                        cmd_str[cmd_len] = '<';
                        cmd_str[cmd_len + 1] = '\0';
                    } else {
                        strcpy(cmd_str, "\\...<");
                    }
                    delimiter_stack_push(ds, DELIM_ANGLE, cmd_str, NULL, line, col,
                                         (int)(p - source));
                    p++;
                    col++; // skip <
                    continue;
                }
                // Not a calc command, continue from where we are
                continue;
            }

            // Check for closing angle bracket >
            if (*p == '>') {
                const DelimiterEntry *top = delimiter_stack_peek(ds);
                if (top && top->type == DELIM_ANGLE) {
                    delimiter_stack_pop(ds, DELIM_ANGLE, ">", NULL, line, col, (int)(p - source),
                                        errors);
                }
                p++;
                col++;
                continue;
            }

            // Skip other escaped characters
            if (*p == '\\' && p + 1 < line_end) {
                p += 2;
                col += 2;
                continue;
            }

            // Skip ${name} macro parameter syntax (not math)
            // Must track brace depth to handle nested ${}: e.g. ${item${i}}
            if (p[0] == '$' && p + 1 < line_end && p[1] == '{') {
                p += 2;
                col += 2;
                int brace_depth = 1;
                while (p < line_end && brace_depth > 0) {
                    if (*p == '{') {
                        brace_depth++;
                    } else if (*p == '}') {
                        brace_depth--;
                        if (brace_depth == 0) break;
                    }
                    p++;
                    col++;
                }
                if (p < line_end && *p == '}') {
                    p++;
                    col++;
                }
                continue;
            }

            // Check for display math $$
            if (p[0] == '$' && p + 1 <= line_end && p[1] == '$') {
                const DelimiterEntry *top = delimiter_stack_peek(ds);
                if (top && top->type == DELIM_DOUBLE_DOLLAR) {
                    // Closing $$
                    delimiter_stack_pop(ds, DELIM_DOUBLE_DOLLAR, "$$", NULL, line, col,
                                        (int)(p - source), errors);
                } else {
                    // Opening $$
                    delimiter_stack_push(ds, DELIM_DOUBLE_DOLLAR, "$$", NULL, line, col,
                                         (int)(p - source));
                }
                p += 2;
                col += 2;
                continue;
            }

            // Check for inline math $ (but not if we're in display math)
            if (*p == '$') {
                const DelimiterEntry *top = delimiter_stack_peek(ds);
                bool in_display = (top && top->type == DELIM_DOUBLE_DOLLAR);
                if (!in_display) {
                    if (top && top->type == DELIM_DOLLAR) {
                        // Closing $
                        delimiter_stack_pop(ds, DELIM_DOLLAR, "$", NULL, line, col,
                                            (int)(p - source), errors);
                    } else {
                        // Opening $
                        delimiter_stack_push(ds, DELIM_DOLLAR, "$", NULL, line, col,
                                             (int)(p - source));
                    }
                }
                p++;
                col++;
                continue;
            }

            // Check for opening brace {
            if (*p == '{') {
                delimiter_stack_push(ds, DELIM_BRACE, "{", NULL, line, col, (int)(p - source));
                p++;
                col++;
                continue;
            }

            // Check for closing brace }
            if (*p == '}') {
                delimiter_stack_pop(ds, DELIM_BRACE, "}", NULL, line, col, (int)(p - source),
                                    errors);
                p++;
                col++;
                continue;
            }

            p++;
            col++;
        }

        // Handle newline
        if (*p == '\n') {
            p++;
            line++;
            col = 1;
        }
    }

    // Report all unclosed delimiters
    delimiter_stack_check_unclosed(ds, errors);
    delimiter_stack_free(ds);
}

// ============================================================================
// Command Argument Parsing
// ============================================================================

static void parse_command_args(const char *input, int *pos, char ***args_out, int *n_args) {
    *args_out = NULL;
    *n_args = 0;

    // Skip whitespace
    while (input[*pos] == ' ' || input[*pos] == '\t') (*pos)++;

    // Parse {...} arguments
    int capacity = 4;
    char **args = malloc(capacity * sizeof(char *));
    int count = 0;

    while (input[*pos] == '{') {
        (*pos)++; // Skip '{'

        const char *arg_start = input + *pos;
        int depth = 1;

        while (input[*pos] && depth > 0) {
            if (input[*pos] == '{')
                depth++;
            else if (input[*pos] == '}')
                depth--;
            if (depth > 0) (*pos)++;
        }

        size_t arg_len = (input + *pos) - arg_start;

        if (count >= capacity) {
            capacity *= 2;
            args = realloc(args, capacity * sizeof(char *));
        }

        args[count] = malloc(arg_len + 1);
        memcpy(args[count], arg_start, arg_len);
        args[count][arg_len] = '\0';
        count++;

        if (input[*pos] == '}') (*pos)++; // Skip '}'

        // Skip whitespace between arguments
        while (input[*pos] == ' ' || input[*pos] == '\t') (*pos)++;
    }

    *args_out = args;
    *n_args = count;
}

// ============================================================================
// Element Type Classification (for spacing)
// ============================================================================

typedef enum {
    ELEM_NONE,
    ELEM_PROSE,
    ELEM_DISPLAY_MATH,
    ELEM_BOX_LAYOUT,
    ELEM_EXPLICIT_SKIP,
    ELEM_COMMAND,
    ELEM_OTHER
} ElementType;

static ElementType classify_element(BoxLayout *child) {
    if (!child) return ELEM_NONE;

    switch (child->type) {
    case BOX_TYPE_CONTENT:
        // Check if it's an explicit skip (preformatted newlines)
        if (child->preformatted && child->content) {
            bool only_newlines = true;
            for (const char *p = child->content; *p; p++) {
                if (*p != '\n') {
                    only_newlines = false;
                    break;
                }
            }
            if (only_newlines && child->content[0] != '\0') {
                return ELEM_EXPLICIT_SKIP;
            }
        }
        return ELEM_PROSE;

    case BOX_TYPE_DISPLAY_MATH: return ELEM_DISPLAY_MATH;

    case BOX_TYPE_COMMAND: return ELEM_COMMAND;

    case BOX_TYPE_ANSI: return ELEM_COMMAND; // Transparent to spacing

    case BOX_TYPE_HBOX:
    case BOX_TYPE_VBOX:
    case BOX_TYPE_INTERSECT_RULES:
    case BOX_TYPE_LINEROUTINE: return ELEM_BOX_LAYOUT;

    case BOX_TYPE_LINE_BREAK:
        // Line break resets spacing chain (no parskip added around it)
        return ELEM_EXPLICIT_SKIP;

    default: return ELEM_OTHER;
    }
}

// ============================================================================
// Implicit Spacing
// ============================================================================

static BoxLayout *create_implicit_skip(int lines) {
    if (lines <= 0) return NULL;

    BoxLayout *skip = box_layout_new(BOX_TYPE_CONTENT, -1);
    skip->preformatted = true;

    char *content = malloc(lines + 1);
    for (int i = 0; i < lines; i++) {
        content[i] = '\n';
    }
    content[lines] = '\0';

    box_layout_set_content(skip, content);
    free(content);

    return skip;
}

static void insert_implicit_spacing(BoxLayout *root, const CompOptions *opt) {
    if (!root || root->type != BOX_TYPE_VBOX || root->n_children == 0) return;

    // Get spacing values
    int parskip = opt ? opt->parskip : 1;
    int math_above = opt ? opt->math_above_skip : 1;
    int math_below = opt ? opt->math_below_skip : 1;

    // Build new children array with spacing inserted
    int max_new_children = root->n_children * 2 + 1;
    BoxLayout **new_children = malloc(max_new_children * sizeof(BoxLayout *));
    int new_count = 0;

    ElementType prev_type = ELEM_NONE;

    for (int i = 0; i < root->n_children; i++) {
        BoxLayout *child = root->children[i];
        ElementType curr_type = classify_element(child);

        // Commands don't affect spacing
        if (curr_type == ELEM_COMMAND) {
            new_children[new_count++] = child;
            continue;
        }

        // Explicit skip resets the chain
        if (curr_type == ELEM_EXPLICIT_SKIP) {
            new_children[new_count++] = child;
            prev_type = ELEM_EXPLICIT_SKIP;
            continue;
        }

        // Calculate implicit spacing needed
        int implicit_skip = 0;

        if (prev_type != ELEM_NONE && prev_type != ELEM_EXPLICIT_SKIP) {
            if (prev_type == ELEM_PROSE && curr_type == ELEM_PROSE) {
                implicit_skip = parskip;
            } else if (prev_type == ELEM_PROSE && curr_type == ELEM_DISPLAY_MATH) {
                implicit_skip = math_above;
            } else if (prev_type == ELEM_DISPLAY_MATH && curr_type == ELEM_PROSE) {
                implicit_skip = math_below;
            } else if (prev_type == ELEM_DISPLAY_MATH && curr_type == ELEM_DISPLAY_MATH) {
                implicit_skip = (math_below > math_above) ? math_below : math_above;
            } else if ((prev_type == ELEM_BOX_LAYOUT && curr_type == ELEM_PROSE) ||
                       (prev_type == ELEM_PROSE && curr_type == ELEM_BOX_LAYOUT)) {
                implicit_skip = parskip;
            } else if (prev_type == ELEM_BOX_LAYOUT && curr_type == ELEM_DISPLAY_MATH) {
                implicit_skip = math_above;
            } else if (prev_type == ELEM_DISPLAY_MATH && curr_type == ELEM_BOX_LAYOUT) {
                implicit_skip = math_below;
            }
        }

        // Insert implicit spacing if needed
        if (implicit_skip > 0) {
            BoxLayout *skip_node = create_implicit_skip(implicit_skip);
            if (skip_node) {
                new_children[new_count++] = skip_node;
            }
        }

        new_children[new_count++] = child;
        prev_type = curr_type;
    }

    // Replace children array
    free(root->children);
    root->children = new_children;
    root->n_children = new_count;
    root->children_capacity = max_new_children;
}

// ============================================================================
// Inline Math Detection Helper
// ============================================================================

// Check if position p is inside inline math ($...$) by counting unescaped single $ signs
static bool is_inside_inline_math(const char *start, const char *p) {
    int dollar_count = 0;
    const char *q = start;

    while (q < p) {
        // Skip escaped \$
        if (*q == '\\' && q + 1 < p && q[1] == '$') {
            q += 2;
            continue;
        }
        // Skip display math $$
        if (*q == '$' && q + 1 < p && q[1] == '$') {
            // Find closing $$
            const char *close = strstr(q + 2, "$$");
            if (close && close < p) {
                q = close + 2;
                continue;
            } else {
                // Unclosed display math - position is inside it, not inline math
                return false;
            }
        }
        // Count single $
        if (*q == '$') {
            dollar_count++;
        }
        q++;
    }

    return (dollar_count % 2) == 1;
}

// ============================================================================
// Text Flushing Helper
// ============================================================================

// Check if text at position starts with an ANSI marker (\x01A:codes\x01)
// Returns the length of the marker if found, 0 otherwise
static int check_ansi_marker(const char *p, char **codes_out) {
    if (p[0] != '\x01' || p[1] != 'A' || p[2] != ':') return 0;
    const char *end = strchr(p + 3, '\x01');
    if (!end) return 0;
    if (codes_out) {
        size_t len = end - (p + 3);
        *codes_out = malloc(len + 1);
        memcpy(*codes_out, p + 3, len);
        (*codes_out)[len] = '\0';
    }
    return (int)(end + 1 - p);
}

// Check if text at position starts with a lineroutine marker (__lr_N__)
// Returns the length of the marker if found, 0 otherwise
static int check_lineinsert_marker(const char *p, char **id_out) {
    if (strncmp(p, "__lr_", 5) != 0) return 0;

    const char *num_start = p + 5;
    const char *num_end = num_start;
    while (*num_end >= '0' && *num_end <= '9') {
        num_end++;
    }

    if (num_end == num_start) return 0;           // No digits
    if (strncmp(num_end, "__", 2) != 0) return 0; // No closing __

    // Found a valid marker
    int marker_len = (num_end + 2) - p;
    if (id_out) {
        *id_out = malloc(marker_len + 1);
        memcpy(*id_out, p, marker_len);
        (*id_out)[marker_len] = '\0';
    }
    return marker_len;
}

// Flush accumulated text as CONTENT child(ren), splitting on lineroutine markers
// If markers are present, wraps the sequence in an HBOX for horizontal merging.
// Leading/trailing ANSI markers are stripped and added as bare ANSI children to
// root so they become vbox-level style (persisting across paragraphs) and the
// remaining text goes through the compositor (enabling inline math, wrapping, etc).
static void flush_text(BoxLayout *root, const char *start, const char *end) {
    if (!start || end <= start) return;

    // Trim trailing whitespace
    size_t len = end - start;
    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t' || start[len - 1] == '\n')) {
        len--;
    }

    if (len == 0) return;

    const char *text_end = start + len;

    // === Strip leading ANSI markers → bare children of root ===
    // These become vbox-level ANSI nodes, picked up by render_vbox for style tracking.
    const char *p = start;
    while (p < text_end) {
        char *ansi_codes = NULL;
        int ansi_len = check_ansi_marker(p, &ansi_codes);
        if (ansi_len > 0) {
            BoxLayout *ansi = box_layout_new(BOX_TYPE_ANSI, WIDTH_INTRINSIC);
            ansi->ansi_codes = ansi_codes;
            box_layout_add_child(root, ansi);
            p += ansi_len;
        } else {
            break;
        }
    }
    start = p;

    // Trim leading whitespace exposed by stripping ANSI markers
    while (start < text_end && (*start == ' ' || *start == '\t' || *start == '\n')) {
        start++;
    }

    // === Strip trailing ANSI markers ===
    // Collect codes to add after the content node(s).
    char *trailing_codes[16];
    int n_trailing = 0;

    while (text_end > start) {
        // An ANSI marker ends with \x01 — check if text ends with one
        if (*(text_end - 1) != '\x01') break;

        // Scan backwards for the opening \x01
        const char *q = text_end - 2;
        while (q >= start && *q != '\x01') q--;
        if (q < start) break;

        // Validate that this is a proper ANSI marker starting at q
        char *codes = NULL;
        int mlen = check_ansi_marker(q, &codes);
        if (mlen > 0 && q + mlen == text_end && n_trailing < 16) {
            trailing_codes[n_trailing++] = codes;
            text_end = q;
        } else {
            free(codes);
            break;
        }
    }

    // Re-trim whitespace between stripped markers and content (including newlines)
    len = text_end - start;
    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t' || start[len - 1] == '\n')) {
        len--;
    }
    text_end = start + len;

    // Process the remaining middle text [start, text_end)
    if (text_end > start) {
        // First pass: check if there are any markers (lineinsert or ANSI)
        bool has_marker = false;
        for (const char *s = start; s < text_end;) {
            int marker_len = check_lineinsert_marker(s, NULL);
            if (marker_len > 0) {
                has_marker = true;
                break;
            }
            marker_len = check_ansi_marker(s, NULL);
            if (marker_len > 0) {
                has_marker = true;
                break;
            }
            s++;
        }

        // If no markers, just create a single CONTENT node
        if (!has_marker) {
            size_t text_len = text_end - start;
            BoxLayout *text_child = box_layout_new(BOX_TYPE_CONTENT, -1);
            char *text = malloc(text_len + 1);
            memcpy(text, start, text_len);
            text[text_len] = '\0';
            box_layout_set_content(text_child, text);
            box_layout_add_child(root, text_child);
            free(text);
        } else {
            // Has markers - create an HBOX to hold mixed CONTENT + LINEINSERT children
            // Use WIDTH_INTRINSIC for children so they render to natural size
            BoxLayout *hbox = box_layout_new(BOX_TYPE_HBOX, -1);

            const char *hp = start;
            const char *text_start = start;

            while (hp < text_end) {
                // Check for lineinsert marker (__lr_N__)
                char *marker_id = NULL;
                int marker_len = check_lineinsert_marker(hp, &marker_id);

                if (marker_len > 0) {
                    // Flush any text before the marker as CONTENT child
                    if (hp > text_start) {
                        size_t text_len = hp - text_start;
                        BoxLayout *text_child = box_layout_new(BOX_TYPE_CONTENT, WIDTH_INTRINSIC);
                        text_child->preformatted = true; // Render without compositor padding
                        char *text = malloc(text_len + 1);
                        memcpy(text, text_start, text_len);
                        text[text_len] = '\0';
                        box_layout_set_content(text_child, text);
                        box_layout_add_child(hbox, text_child);
                        free(text);
                    }

                    // Create LINEINSERT node for the marker (intrinsic width)
                    BoxLayout *insert = box_layout_new(BOX_TYPE_LINEINSERT, WIDTH_INTRINSIC);
                    insert->lineinsert_id = marker_id;
                    box_layout_add_child(hbox, insert);

                    // Continue after the marker
                    hp += marker_len;
                    text_start = hp;
                    continue;
                }

                // Check for ANSI marker (\x01A:codes\x01)
                char *ansi_codes = NULL;
                int ansi_len = check_ansi_marker(hp, &ansi_codes);

                if (ansi_len > 0) {
                    // Flush any text before the marker as CONTENT child
                    if (hp > text_start) {
                        size_t text_len = hp - text_start;
                        BoxLayout *text_child = box_layout_new(BOX_TYPE_CONTENT, WIDTH_INTRINSIC);
                        text_child->preformatted = true;
                        char *text = malloc(text_len + 1);
                        memcpy(text, text_start, text_len);
                        text[text_len] = '\0';
                        box_layout_set_content(text_child, text);
                        box_layout_add_child(hbox, text_child);
                        free(text);
                    }

                    // Create ANSI node (zero-width, rendered as escape sequence)
                    BoxLayout *ansi = box_layout_new(BOX_TYPE_ANSI, WIDTH_INTRINSIC);
                    ansi->ansi_codes = ansi_codes;
                    box_layout_add_child(hbox, ansi);

                    // Continue after the marker
                    hp += ansi_len;
                    text_start = hp;
                    continue;
                }

                hp++;
            }

            // Flush any remaining text after the last marker
            if (text_start < text_end) {
                size_t text_len = text_end - text_start;
                BoxLayout *text_child = box_layout_new(BOX_TYPE_CONTENT, WIDTH_INTRINSIC);
                text_child->preformatted = true; // Render without compositor padding
                char *text = malloc(text_len + 1);
                memcpy(text, text_start, text_len);
                text[text_len] = '\0';
                box_layout_set_content(text_child, text);
                box_layout_add_child(hbox, text_child);
                free(text);
            }

            // Add the hbox as a single child of root
            box_layout_add_child(root, hbox);
        }
    }

    // === Add trailing ANSI markers as bare children (reverse order → original order) ===
    for (int i = n_trailing - 1; i >= 0; i--) {
        BoxLayout *ansi = box_layout_new(BOX_TYPE_ANSI, WIDTH_INTRINSIC);
        ansi->ansi_codes = trailing_codes[i];
        box_layout_add_child(root, ansi);
    }
}

// ============================================================================
// Main Document Parser
// ============================================================================

// Track nesting level for parse_document_as_vbox to prevent clearing verbatim store
static int g_parse_nesting = 0;

BoxLayout *parse_document_as_vbox(const char *input, int width, ParseError *err) {
    g_parse_nesting++;

    // Note: macro_registry_keep_alive is called from hyades_api.c compose_document_with_map
    // to keep macros available during the rendering phase for \lineroutine

    // Step 1: Expand \input commands
    int recursion = 0;
    char *after_input = expand_input_commands(input, &recursion);
    if (!after_input) {
        g_parse_nesting--;
        if (err) {
            err->code = PARSE_ERR_OOM;
            snprintf(err->message, sizeof(err->message), "Out of memory");
        }
        return NULL;
    }

    // Step 2: Protect verbatim content (must be before comment stripping)
    // Only run verbatim protection at the top level to avoid clearing the store
    char *after_verbatim;
    if (g_parse_nesting == 1) {
        after_verbatim = protect_verbatim(after_input);
    } else {
        // In nested calls, skip verbatim protection to preserve the outer store
        after_verbatim = strdup(after_input);
    }
    free(after_input);
    if (!after_verbatim) {
        g_parse_nesting--;
        if (err) {
            err->code = PARSE_ERR_OOM;
            snprintf(err->message, sizeof(err->message), "Out of memory");
        }
        return NULL;
    }

    // Step 3: Strip TeX comments
    char *no_comments = strip_tex_comments(after_verbatim);
    free(after_verbatim);
    if (!no_comments) {
        g_parse_nesting--;
        if (err) {
            err->code = PARSE_ERR_OOM;
            snprintf(err->message, sizeof(err->message), "Out of memory");
        }
        return NULL;
    }

    // Step 4: Expand all macros (pass width for \width command)
    char macro_error[256] = {0};
    char *expanded = expand_all_macros(no_comments, width, macro_error, sizeof(macro_error));
    free(no_comments);

    if (!expanded) {
        g_parse_nesting--;
        if (err) {
            err->code = PARSE_ERR_OTHER;
            snprintf(err->message, sizeof(err->message), "Macro expansion failed: %s", macro_error);
        }
        return NULL;
    }

    // Step 5: Protect any \verb sequences that came from macro expansion
    // Macros can generate \verb#...# content that wasn't in the original document
    char *after_verb = protect_verbatim(expanded);
    free(expanded);
    if (!after_verb) {
        g_parse_nesting--;
        if (err) {
            err->code = PARSE_ERR_OOM;
            snprintf(err->message, sizeof(err->message), "Out of memory");
        }
        return NULL;
    }
    expanded = after_verb;

    // Create root vbox with inherited width (-1)
    // This allows \setwidth to dynamically change document width via opt->width
    // The actual width is passed to box_layout_resolve_widths which sets computed_width
    BoxLayout *root = box_layout_new(BOX_TYPE_VBOX, -1);

    const char *p = expanded;
    const char *text_start = NULL;
    size_t chars_scanned = 0;

    while (*p) {
        // Defensive: prevent infinite loop from memory corruption
        if (++chars_scanned > MAX_SCAN_CHARS) {
            fprintf(stderr, "parse_document_as_vbox: scan limit exceeded (corruption?)\n");
            g_parse_nesting--;
            if (err) {
                err->code = PARSE_ERR_OTHER;
                snprintf(err->message, sizeof(err->message), "Scan limit exceeded");
            }
            box_layout_free(root);
            free(expanded);
            return NULL;
        }

        // Skip whitespace
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        // Check if we reached end of input after skipping whitespace
        if (!*p) break;

        // ========== Display Math $$...$$ ==========
        if (p[0] == '$' && p[1] == '$') {
            flush_text(root, text_start, p);
            text_start = NULL;

            const char *math_start = p + 2;
            const char *math_end = strstr(math_start, "$$");

            if (!math_end) {
                g_parse_nesting--;
                if (err) {
                    err->code = PARSE_ERR_MATH_SYNTAX;
                    snprintf(err->message, sizeof(err->message), "Unclosed display math $$");
                }
                box_layout_free(root);
                free(expanded);
                return NULL;
            }

            size_t math_len = math_end - math_start;
            BoxLayout *math_child = box_layout_new(BOX_TYPE_DISPLAY_MATH, -1);
            math_child->h_align = ALIGN_CENTER;

            char *math = malloc(math_len + 1);
            memcpy(math, math_start, math_len);
            math[math_len] = '\0';
            box_layout_set_math(math_child, math);
            box_layout_add_child(root, math_child);
            free(math);

            p = math_end + 2;
            if (*p == '\n') p++;
            continue;
        }

        // ========== Box Layouts \begin{vbox|hbox} ==========
        if (*p == '\\' && strncmp(p, "\\begin", 6) == 0) {
            const char *check = p + 6;
            while (*check == ' ' || *check == '\t') check++;

            bool is_box =
                (*check == '[') || (*check == '{' && (strncmp(check + 1, "vbox", 4) == 0 ||
                                                      strncmp(check + 1, "hbox", 4) == 0));

            if (is_box) {
                flush_text(root, text_start, p);
                text_start = NULL;

                int end_pos = 0;
                BoxLayout *nested = parse_box_layout_with_width(p, &end_pos, width);
                if (nested) {
                    box_layout_add_child(root, nested);
                    p += end_pos;
                    continue;
                }
            }
        }

        // ========== \intersect_rules{...} ==========
        if (*p == '\\' && strncmp(p, "\\intersect_rules", 16) == 0) {
            flush_text(root, text_start, p);
            text_start = NULL;

            int end_pos = 0;
            BoxLayout *ir = parse_intersect_rules_command(p, &end_pos, width);
            if (ir) {
                box_layout_add_child(root, ir);
                p += end_pos;
                continue;
            }
        }

        // ========== \lineroutine<routine>{content} ==========
        if (*p == '\\' && strncmp(p, "\\lineroutine", 12) == 0) {
            flush_text(root, text_start, p);
            text_start = NULL;

            int end_pos = 0;
            BoxLayout *lr = parse_lineroutine_command(p, &end_pos, width);
            if (lr) {
                box_layout_add_child(root, lr);
                p += end_pos;
                continue;
            }
        }

        // ========== Commands ==========
        if (*p == '\\') {
            bool valid_position =
                (p == expanded || p[-1] == '\n' || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '}');

            const char *cmd_start = p + 1;
            const char *cmd_end = cmd_start;

            while (isalpha((unsigned char)*cmd_end) || isdigit((unsigned char)*cmd_end) ||
                   *cmd_end == '_') {
                cmd_end++;
            }

            if (cmd_end > cmd_start) {
                size_t cmd_len = cmd_end - cmd_start;
                char cmd_name[64];
                if (cmd_len < sizeof(cmd_name)) {
                    memcpy(cmd_name, cmd_start, cmd_len);
                    cmd_name[cmd_len] = '\0';

                    // \verbatim{...}
                    if (strcmp(cmd_name, "verbatim") == 0) {
                        const char *brace = cmd_end;
                        while (*brace == ' ' || *brace == '\t') brace++;

                        if (*brace == '{') {
                            const char *content_start = brace + 1;
                            const char *content_end = content_start;
                            int depth = 1;
                            while (*content_end && depth > 0) {
                                if (*content_end == '{')
                                    depth++;
                                else if (*content_end == '}')
                                    depth--;
                                if (depth > 0) content_end++;
                            }

                            if (depth == 0) {
                                flush_text(root, text_start, p);
                                text_start = NULL;

                                size_t content_len = content_end - content_start;
                                if (content_len > 0) {
                                    BoxLayout *verbatim = box_layout_new(BOX_TYPE_CONTENT, -1);
                                    verbatim->preformatted = true;
                                    char *vtext = malloc(content_len + 1);
                                    memcpy(vtext, content_start, content_len);
                                    vtext[content_len] = '\0';
                                    box_layout_set_content(verbatim, vtext);
                                    box_layout_add_child(root, verbatim);
                                    free(vtext);
                                }

                                p = content_end + 1;
                                continue;
                            }
                        }
                    }

                    // Vertical spacing commands (create content with newlines)
                    if (strcmp(cmd_name, "vskip") == 0 || strcmp(cmd_name, "smallskip") == 0 ||
                        strcmp(cmd_name, "medskip") == 0 || strcmp(cmd_name, "bigskip") == 0) {

                        flush_text(root, text_start, p);
                        text_start = NULL;

                        int arg_pos = (int)(cmd_end - expanded);
                        char **args = NULL;
                        int n_args = 0;
                        parse_command_args(expanded, &arg_pos, &args, &n_args);

                        int amount = 1;
                        if (strcmp(cmd_name, "vskip") == 0 && n_args >= 1) {
                            amount = atoi(args[0]);
                            if (amount < 0) amount = 0;
                            if (amount > 100) amount = 100;
                        } else if (strcmp(cmd_name, "medskip") == 0) {
                            amount = 2;
                        } else if (strcmp(cmd_name, "bigskip") == 0) {
                            amount = 3;
                        }

                        if (amount > 0) {
                            char *spacing = malloc(amount + 1);
                            for (int i = 0; i < amount; i++) spacing[i] = '\n';
                            spacing[amount] = '\0';

                            BoxLayout *skip = box_layout_new(BOX_TYPE_CONTENT, -1);
                            skip->preformatted = true;
                            box_layout_set_content(skip, spacing);
                            box_layout_add_child(root, skip);
                            free(spacing);
                        }

                        for (int i = 0; i < n_args; i++) free(args[i]);
                        free(args);

                        p = expanded + arg_pos;
                        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                        continue;
                    }

                    // \hrule[width]{left}{fill}{right} - horizontal rule
                    if (strcmp(cmd_name, "hrule") == 0) {
                        flush_text(root, text_start, p);
                        text_start = NULL;

                        int hrule_pos = (int)(p - expanded);
                        BoxLayout *hrule = parse_hrule_command(expanded, &hrule_pos);
                        if (hrule) {
                            box_layout_add_child(root, hrule);
                            p = expanded + hrule_pos;
                            while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                            continue;
                        }
                        // If parsing failed, fall through to treat as text
                    }

                    // \vrule[width][height]{top}{fill}{bottom} - vertical rule
                    if (strcmp(cmd_name, "vrule") == 0) {
                        flush_text(root, text_start, p);
                        text_start = NULL;

                        int vrule_pos = (int)(p - expanded);
                        BoxLayout *vrule = parse_vrule_command(expanded, &vrule_pos);
                        if (vrule) {
                            box_layout_add_child(root, vrule);
                            p = expanded + vrule_pos;
                            while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                            continue;
                        }
                        // If parsing failed, fall through to treat as text
                    }

                    // Document commands
                    if (valid_position && is_document_command(cmd_name)) {
                        flush_text(root, text_start, p);
                        text_start = NULL;

                        int arg_pos = (int)(cmd_end - expanded);
                        char **args = NULL;
                        int n_args = 0;
                        parse_command_args(expanded, &arg_pos, &args, &n_args);

                        BoxLayout *cmd = box_layout_new(BOX_TYPE_COMMAND, -1);
                        box_layout_set_command(cmd, cmd_name, args, n_args);
                        box_layout_add_child(root, cmd);

                        for (int i = 0; i < n_args; i++) free(args[i]);
                        free(args);

                        p = expanded + arg_pos;
                        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                        continue;
                    }
                }
            }
        }

        // ========== Line Break \\ ==========
        // Creates a line break (like <br> in HTML) - no vertical space added.
        // This differs from \vskip which adds vertical space.
        // IMPORTANT: Don't treat \\ as line break if inside inline math ($...$)
        if (p[0] == '\\' && p[1] == '\\' && !is_inside_inline_math(expanded, p)) {
            flush_text(root, text_start, p);
            text_start = NULL;

            // LINE_BREAK node: resets spacing chain but renders to nothing
            BoxLayout *br = box_layout_new(BOX_TYPE_LINE_BREAK, -1);
            box_layout_add_child(root, br);

            p += 2;
            while (*p == ' ' || *p == '\t' || *p == '\n') p++;
            continue;
        }

        // ========== Paragraph Break (blank line) ==========
        if (*p == '\n') {
            const char *q = p + 1;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '\n') {
                // Found blank line - create paragraph break
                flush_text(root, text_start, p);
                text_start = NULL;
                p = q + 1;
                continue;
            }
        }

        // ========== Regular Text ==========
        // Only advance and accumulate text if not at end of string
        if (*p) {
            if (!text_start) text_start = p;
            p++;
        }
    }

    // Flush remaining text
    flush_text(root, text_start, p);

    free(expanded);

    // Insert implicit vertical spacing
    CompOptions default_opt = default_options();
    insert_implicit_spacing(root, &default_opt);

    // Resolve width inheritance
    box_layout_resolve_widths(root, width);

    g_parse_nesting--;
    return root;
}

// ============================================================================
// LSP Helpers
// ============================================================================

// Strip rendered output lines (lines containing NBSP) from input.
// Any line containing NBSP (U+00A0, encoded as C2 A0 in UTF-8) is treated as
// already-rendered content and should not be re-parsed through macro expansion.
// Line count is preserved by replacing content with empty lines.
static char *strip_rendered_output(const char *input) {
    if (!input) return NULL;

    size_t len = strlen(input);
    char *result = malloc(len + 1);
    if (!result) return NULL;

    const char *src = input;
    char *dst = result;

    while (*src) {
        // Scan the line for NBSP (C2 A0)
        const char *line_end = src;
        bool has_nbsp = false;
        while (*line_end && *line_end != '\n') {
            if (line_end[0] == (char)0xC2 && line_end[1] == (char)0xA0) {
                has_nbsp = true;
            }
            line_end++;
        }

        if (has_nbsp) {
            // Skip content but preserve the newline
            src = line_end;
            if (*src == '\n') {
                *dst++ = '\n';
                src++;
            }
        } else {
            // Copy line as-is
            while (src < line_end) {
                *dst++ = *src++;
            }
            if (*src == '\n') {
                *dst++ = *src++;
            }
        }
    }

    *dst = '\0';
    return result;
}

// ============================================================================
// LSP-Aware Document Parser
// ============================================================================

// Parse document with LSP options for error collection, symbol tracking, etc.
BoxLayout *parse_document_as_vbox_lsp(const char *input, int width, ParseErrorList *errors,
                                      LspSymbolTable *symbols, const HyadesParseOptions *opts) {
    if (!input) return NULL;

    // Create LSP context
    LspParseContext lsp_ctx;
    lsp_ctx_init(&lsp_ctx, input, opts);

    // Override with externally provided structures if given
    if (errors) {
        if (lsp_ctx.owns_errors && lsp_ctx.errors) parse_error_list_free(lsp_ctx.errors);
        lsp_ctx.errors = errors;
        lsp_ctx.owns_errors = false; // External, don't free on cleanup
    }
    if (symbols) {
        if (lsp_ctx.owns_symbols && lsp_ctx.symbols) lsp_symbol_table_free(lsp_ctx.symbols);
        lsp_ctx.symbols = symbols;
        lsp_ctx.owns_symbols = false; // External, don't free on cleanup
    }

    g_parse_nesting++;

    // Step 1: Expand \input commands
    int recursion = 0;
    char *after_input = expand_input_commands(input, &recursion);
    if (!after_input) {
        g_parse_nesting--;
        if (lsp_ctx.errors) {
            parse_error_list_add(lsp_ctx.errors, PARSE_ERR_OOM, 0, 0, 0, 0, "document",
                                 "Out of memory during input expansion");
        }
        lsp_ctx_free(&lsp_ctx);
        return NULL;
    }

    // Step 2: Protect verbatim content
    char *after_verbatim;
    if (g_parse_nesting == 1) {
        after_verbatim = protect_verbatim(after_input);
    } else {
        after_verbatim = strdup(after_input);
    }
    free(after_input);
    if (!after_verbatim) {
        g_parse_nesting--;
        if (lsp_ctx.errors) {
            parse_error_list_add(lsp_ctx.errors, PARSE_ERR_OOM, 0, 0, 0, 0, "document",
                                 "Out of memory during verbatim protection");
        }
        lsp_ctx_free(&lsp_ctx);
        return NULL;
    }

    // Step 3: Strip TeX comments
    char *no_comments = strip_tex_comments(after_verbatim);
    free(after_verbatim);
    if (!no_comments) {
        g_parse_nesting--;
        if (lsp_ctx.errors) {
            parse_error_list_add(lsp_ctx.errors, PARSE_ERR_OOM, 0, 0, 0, 0, "document",
                                 "Out of memory during comment stripping");
        }
        lsp_ctx_free(&lsp_ctx);
        return NULL;
    }

    // Step 3.5: Check for basic delimiter errors on ORIGINAL source
    // This ensures we report errors at their original source positions.
    // The check skips NBSP lines (rendered output) internally.
    if (lsp_ctx.errors) {
        check_delimiters_pre_expansion(input, lsp_ctx.errors);
    }

    // Step 3.6: Strip rendered output lines (lines containing NBSP)
    // These are already-rendered @cassilda output and should not be re-parsed
    char *no_rendered = strip_rendered_output(no_comments);
    free(no_comments);
    if (!no_rendered) {
        g_parse_nesting--;
        if (lsp_ctx.errors) {
            parse_error_list_add(lsp_ctx.errors, PARSE_ERR_OOM, 0, 0, 0, 0, "document",
                                 "Out of memory during rendered output stripping");
        }
        lsp_ctx_free(&lsp_ctx);
        return NULL;
    }

    // Step 4: Expand all macros (using LSP-aware version)
    char *expanded = expand_all_macros_lsp(no_rendered, width, lsp_ctx.errors, lsp_ctx.source_map,
                                           lsp_ctx.symbols);
    free(no_rendered);

    if (!expanded) {
        g_parse_nesting--;
        // Error already added by expand_all_macros_lsp
        lsp_ctx_free(&lsp_ctx);
        return NULL;
    }

    // Step 5: Protect any \verb sequences that came from macro expansion
    char *after_verb = protect_verbatim(expanded);
    free(expanded);
    if (!after_verb) {
        g_parse_nesting--;
        if (lsp_ctx.errors) {
            parse_error_list_add(lsp_ctx.errors, PARSE_ERR_OOM, 0, 0, 0, 0, "document",
                                 "Out of memory during verbatim protection");
        }
        lsp_ctx_free(&lsp_ctx);
        return NULL;
    }
    expanded = after_verb;

    // Create root vbox
    BoxLayout *root = box_layout_new(BOX_TYPE_VBOX, -1);

    const char *p = expanded;
    const char *text_start = NULL;

    while (*p) {
        // Update LSP position tracking
        lsp_ctx_update_position(&lsp_ctx, expanded, p);

        // Skip whitespace
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        // Check if we reached end of input after skipping whitespace
        if (!*p) break;

        // ========== Display Math $$...$$ ==========
        if (p[0] == '$' && p[1] == '$') {
            flush_text(root, text_start, p);
            text_start = NULL;

            // Note: $$ delimiter tracking done by pre-expansion check for accurate positions

            const char *math_start = p + 2;
            const char *math_end = strstr(math_start, "$$");

            if (!math_end) {
                // Error already reported by pre-expansion check with accurate position
                // Just handle recovery here
                if (opts && opts->continue_on_error) {
                    // Continue anyway, treating rest as text
                    p += 2;
                    if (!text_start) text_start = p;
                    continue;
                }
                g_parse_nesting--;
                box_layout_free(root);
                free(expanded);
                lsp_ctx_free(&lsp_ctx);
                return NULL;
            }

            size_t math_len = math_end - math_start;
            BoxLayout *math_child = box_layout_new(BOX_TYPE_DISPLAY_MATH, -1);
            math_child->h_align = ALIGN_CENTER;

            char *math = malloc(math_len + 1);
            memcpy(math, math_start, math_len);
            math[math_len] = '\0';
            box_layout_set_math(math_child, math);
            box_layout_add_child(root, math_child);
            free(math);

            p = math_end + 2;
            if (*p == '\n') p++;
            continue;
        }

        // ========== Box Layouts \begin{vbox|hbox} ==========
        if (*p == '\\' && strncmp(p, "\\begin", 6) == 0) {
            const char *check = p + 6;
            while (*check == ' ' || *check == '\t') check++;

            bool is_box =
                (*check == '[') || (*check == '{' && (strncmp(check + 1, "vbox", 4) == 0 ||
                                                      strncmp(check + 1, "hbox", 4) == 0));

            if (is_box) {
                flush_text(root, text_start, p);
                text_start = NULL;

                // Track \begin delimiter - extract env name from {hbox} or {vbox}
                // Need to skip past [...] if present
                char *env_name = NULL;
                const char *env_scan = check;
                if (*env_scan == '[') {
                    // Skip past [...]
                    int bracket_depth = 1;
                    env_scan++;
                    while (*env_scan && bracket_depth > 0) {
                        if (*env_scan == '[')
                            bracket_depth++;
                        else if (*env_scan == ']')
                            bracket_depth--;
                        env_scan++;
                    }
                    // Skip whitespace after ]
                    while (*env_scan == ' ' || *env_scan == '\t') env_scan++;
                }
                if (*env_scan == '{') {
                    const char *env_start = env_scan + 1;
                    const char *env_end = env_start;
                    while (*env_end && *env_end != '}') env_end++;
                    size_t env_len = env_end - env_start;
                    env_name = malloc(env_len + 1);
                    memcpy(env_name, env_start, env_len);
                    env_name[env_len] = '\0';

                    lsp_ctx_push_delimiter(&lsp_ctx, DELIM_BEGIN_END, "\\begin", env_name,
                                           (int)(p - expanded));
                }

                int end_pos = 0;
                BoxLayout *nested = parse_box_layout_with_width(p, &end_pos, width);
                if (nested) {
                    box_layout_add_child(root, nested);
                    p += end_pos;

                    // Pop delimiter (the \end was found)
                    if (env_name) {
                        lsp_ctx_update_position(&lsp_ctx, expanded, p);
                        lsp_ctx_pop_delimiter(&lsp_ctx, DELIM_BEGIN_END, "\\end", env_name,
                                              (int)(p - expanded));
                    }
                    if (env_name) free(env_name);
                    continue;
                }

                // Parsing failed - skip past \begin{env} to avoid infinite loop
                if (env_name) free(env_name);
                p += 6; // Skip "\begin"
                while (*p && *p != '}') p++;
                if (*p == '}') p++;
                continue;
            }
        }

        // ========== \intersect_rules{...} ==========
        if (*p == '\\' && strncmp(p, "\\intersect_rules", 16) == 0) {
            flush_text(root, text_start, p);
            text_start = NULL;

            int end_pos = 0;
            BoxLayout *ir = parse_intersect_rules_command(p, &end_pos, width);
            if (ir) {
                box_layout_add_child(root, ir);
                p += end_pos;
                continue;
            }
        }

        // ========== \lineroutine<routine>{content} ==========
        if (*p == '\\' && strncmp(p, "\\lineroutine", 12) == 0) {
            flush_text(root, text_start, p);
            text_start = NULL;

            int end_pos = 0;
            BoxLayout *lr = parse_lineroutine_command(p, &end_pos, width);
            if (lr) {
                box_layout_add_child(root, lr);
                p += end_pos;
                continue;
            }
        }

        // ========== Commands ==========
        if (*p == '\\') {
            bool valid_position =
                (p == expanded || p[-1] == '\n' || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '}');

            const char *cmd_start = p + 1;
            const char *cmd_end = cmd_start;

            while (isalpha((unsigned char)*cmd_end) || isdigit((unsigned char)*cmd_end) ||
                   *cmd_end == '_') {
                cmd_end++;
            }

            if (cmd_end > cmd_start) {
                size_t cmd_len = cmd_end - cmd_start;
                char cmd_name[64];
                if (cmd_len < sizeof(cmd_name)) {
                    memcpy(cmd_name, cmd_start, cmd_len);
                    cmd_name[cmd_len] = '\0';

                    // Vertical spacing commands
                    if (strcmp(cmd_name, "vskip") == 0 || strcmp(cmd_name, "smallskip") == 0 ||
                        strcmp(cmd_name, "medskip") == 0 || strcmp(cmd_name, "bigskip") == 0) {

                        flush_text(root, text_start, p);
                        text_start = NULL;

                        int arg_pos = (int)(cmd_end - expanded);
                        char **args = NULL;
                        int n_args = 0;
                        parse_command_args(expanded, &arg_pos, &args, &n_args);

                        int amount = 1;
                        if (strcmp(cmd_name, "vskip") == 0 && n_args >= 1) {
                            amount = atoi(args[0]);
                            if (amount < 0) amount = 0;
                            if (amount > 100) amount = 100;
                        } else if (strcmp(cmd_name, "medskip") == 0) {
                            amount = 2;
                        } else if (strcmp(cmd_name, "bigskip") == 0) {
                            amount = 3;
                        }

                        if (amount > 0) {
                            char *spacing = malloc(amount + 1);
                            for (int i = 0; i < amount; i++) spacing[i] = '\n';
                            spacing[amount] = '\0';

                            BoxLayout *skip = box_layout_new(BOX_TYPE_CONTENT, -1);
                            skip->preformatted = true;
                            box_layout_set_content(skip, spacing);
                            box_layout_add_child(root, skip);
                            free(spacing);
                        }

                        for (int i = 0; i < n_args; i++) free(args[i]);
                        free(args);

                        p = expanded + arg_pos;
                        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                        continue;
                    }

                    // \hrule
                    if (strcmp(cmd_name, "hrule") == 0) {
                        flush_text(root, text_start, p);
                        text_start = NULL;

                        int hrule_pos = (int)(p - expanded);
                        BoxLayout *hrule = parse_hrule_command(expanded, &hrule_pos);
                        if (hrule) {
                            box_layout_add_child(root, hrule);
                            p = expanded + hrule_pos;
                            while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                            continue;
                        }
                    }

                    // \vrule
                    if (strcmp(cmd_name, "vrule") == 0) {
                        flush_text(root, text_start, p);
                        text_start = NULL;

                        int vrule_pos = (int)(p - expanded);
                        BoxLayout *vrule = parse_vrule_command(expanded, &vrule_pos);
                        if (vrule) {
                            box_layout_add_child(root, vrule);
                            p = expanded + vrule_pos;
                            while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                            continue;
                        }
                    }

                    // Document commands
                    if (valid_position && is_document_command(cmd_name)) {
                        flush_text(root, text_start, p);
                        text_start = NULL;

                        int arg_pos = (int)(cmd_end - expanded);
                        char **args = NULL;
                        int n_args = 0;
                        parse_command_args(expanded, &arg_pos, &args, &n_args);

                        BoxLayout *cmd = box_layout_new(BOX_TYPE_COMMAND, -1);
                        box_layout_set_command(cmd, cmd_name, args, n_args);
                        box_layout_add_child(root, cmd);

                        for (int i = 0; i < n_args; i++) free(args[i]);
                        free(args);

                        p = expanded + arg_pos;
                        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
                        continue;
                    }
                }
            }
        }

        // ========== Line Break \\ ==========
        if (p[0] == '\\' && p[1] == '\\' && !is_inside_inline_math(expanded, p)) {
            flush_text(root, text_start, p);
            text_start = NULL;

            BoxLayout *br = box_layout_new(BOX_TYPE_LINE_BREAK, -1);
            box_layout_add_child(root, br);

            p += 2;
            while (*p == ' ' || *p == '\t' || *p == '\n') p++;
            continue;
        }

        // ========== Paragraph Break (blank line) ==========
        if (*p == '\n') {
            const char *q = p + 1;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '\n') {
                flush_text(root, text_start, p);
                text_start = NULL;
                p = q + 1;
                continue;
            }
        }

        // ========== Regular Text ==========
        if (*p) {
            if (!text_start) text_start = p;
            p++;
        }
    }

    // Flush remaining text
    flush_text(root, text_start, p);

    // Check for unclosed delimiters
    if (lsp_ctx.delim_stack && opts && opts->validate_delimiters) {
        delimiter_stack_check_unclosed(lsp_ctx.delim_stack, lsp_ctx.errors);
    }

    free(expanded);

    // Insert implicit vertical spacing
    CompOptions default_opt = default_options();
    insert_implicit_spacing(root, &default_opt);

    // Resolve width inheritance
    box_layout_resolve_widths(root, width);

    // Clean up LSP context (but don't free externally provided structures)
    if (errors == NULL && lsp_ctx.errors) {
        parse_error_list_free(lsp_ctx.errors);
        lsp_ctx.errors = NULL;
    }
    if (symbols == NULL && lsp_ctx.symbols) {
        lsp_symbol_table_free(lsp_ctx.symbols);
        lsp_ctx.symbols = NULL;
    }
    if (lsp_ctx.delim_stack) delimiter_stack_free(lsp_ctx.delim_stack);
    if (lsp_ctx.source_map) source_map_free(lsp_ctx.source_map);

    g_parse_nesting--;
    return root;
}

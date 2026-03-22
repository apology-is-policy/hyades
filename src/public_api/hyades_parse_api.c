// hyades_parse_api.c - Implementation of unified parse API
#include "hyades_parse_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../document/document.h"
#include "../layout/layout.h"
#include "../lsp/builtin_docs.h"

// ============================================================================
// Internal Helpers
// ============================================================================

// Escape a string for JSON output
// Returns a newly allocated string that must be freed
static char *json_escape_string(const char *s) {
    if (!s) return strdup("");

    // Count how many characters need escaping
    size_t len = strlen(s);
    size_t extra = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '\\' || *p == '"' || *p == '\n' || *p == '\r' || *p == '\t') {
            extra++;
        }
    }

    // Allocate escaped string
    char *escaped = malloc(len + extra + 1);
    if (!escaped) return strdup("");

    char *out = escaped;
    for (const char *p = s; *p; p++) {
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

    return escaped;
}

static int count_lines(const char *source) {
    if (!source) return 0;
    int lines = 1;
    for (const char *p = source; *p; p++) {
        if (*p == '\n') lines++;
    }
    return lines;
}

static HyadesParseResult *alloc_result(void) {
    HyadesParseResult *result = calloc(1, sizeof(HyadesParseResult));
    if (!result) return NULL;

    result->errors = parse_error_list_new();
    result->symbols = lsp_symbol_table_new();
    result->source_map = source_map_new();

    if (!result->errors || !result->symbols || !result->source_map) {
        hyades_parse_result_free(result);
        return NULL;
    }

    return result;
}

// ============================================================================
// Main Parse API
// ============================================================================

HyadesParseResult *hyades_parse(const char *source) {
    HyadesParseOptions opts = hyades_parse_options_default();
    return hyades_parse_with_options(source, &opts);
}

HyadesParseResult *hyades_parse_with_options(const char *source, const HyadesParseOptions *opts) {
    if (!source) return NULL;

    HyadesParseResult *result = alloc_result();
    if (!result) return NULL;

    clock_t start = clock();

    // Store options
    if (opts) {
        result->options = *opts;
    } else {
        result->options = hyades_parse_options_default();
    }

    // Collect stats
    result->stats.total_chars = (int)strlen(source);
    result->stats.total_lines = count_lines(source);

    // Set up source map with original source
    if (result->options.build_source_map) {
        source_map_free(result->source_map);
        result->source_map = source_map_new_with_source(source);
    }

    // Parse the document using the LSP-aware parser
    // This will populate errors and symbols directly
    result->layout =
        parse_document_as_vbox_lsp(source, 80, result->errors, result->symbols, &result->options);

    // Calculate parse time
    clock_t end = clock();
    result->stats.parse_time_ms = (int)((end - start) * 1000 / CLOCKS_PER_SEC);

    return result;
}

HyadesParseResult *hyades_parse_for_lsp(const char *source) {
    HyadesParseOptions opts = hyades_parse_options_lsp();
    return hyades_parse_with_options(source, &opts);
}

void hyades_parse_result_free(HyadesParseResult *result) {
    if (!result) return;

    if (result->layout) {
        box_layout_free(result->layout);
    }
    parse_error_list_free(result->errors);
    lsp_symbol_table_free(result->symbols);
    source_map_free(result->source_map);

    free(result);
}

// ============================================================================
// Error Access
// ============================================================================

bool hyades_has_errors(const HyadesParseResult *result) {
    return result && parse_error_list_has_errors(result->errors);
}

int hyades_error_count(const HyadesParseResult *result) {
    return result ? parse_error_list_total_errors(result->errors) : 0;
}

int hyades_warning_count(const HyadesParseResult *result) {
    return result && result->errors ? result->errors->warning_count : 0;
}

const ParseError *hyades_error_at(const HyadesParseResult *result, int index) {
    if (!result) return NULL;
    return parse_error_list_get(result->errors, index);
}

char *hyades_errors_to_json(const HyadesParseResult *result) {
    if (!result || !result->errors) {
        return strdup("[]");
    }

    // Estimate buffer size
    int n = result->errors->n_errors;
    int buf_size = 128 + n * 512;
    char *buf = malloc(buf_size);
    if (!buf) return NULL;

    char *p = buf;
    int remaining = buf_size;

    p += snprintf(p, remaining, "[");
    remaining = buf_size - (int)(p - buf);

    for (int i = 0; i < n; i++) {
        const ParseError *err = &result->errors->errors[i];

        if (i > 0) {
            p += snprintf(p, remaining, ",");
            remaining = buf_size - (int)(p - buf);
        }

        // Escape strings that may contain special characters
        char *escaped_msg = json_escape_string(err->message);
        char *escaped_src = json_escape_string(err->source[0] ? err->source : "hyades");

        p += snprintf(p, remaining,
                      "{"
                      "\"severity\":%d,"
                      "\"range\":{"
                      "\"start\":{\"line\":%d,\"character\":%d},"
                      "\"end\":{\"line\":%d,\"character\":%d}"
                      "},"
                      "\"message\":\"%s\","
                      "\"source\":\"%s\","
                      "\"code\":\"%s\""
                      "}",
                      err->severity,
                      err->row > 0 ? err->row - 1 : 0, // LSP uses 0-based
                      err->col > 0 ? err->col - 1 : 0,
                      err->end_row > 0 ? err->end_row - 1 : (err->row > 0 ? err->row - 1 : 0),
                      err->end_col > 0 ? err->end_col - 1 : (err->col > 0 ? err->col - 1 : 0),
                      escaped_msg, escaped_src, parse_error_code_name(err->code));
        remaining = buf_size - (int)(p - buf);

        free(escaped_msg);
        free(escaped_src);
    }

    snprintf(p, remaining, "]");

    return buf;
}

// ============================================================================
// Symbol Access
// ============================================================================

int hyades_symbol_count(const HyadesParseResult *result) {
    return result ? lsp_symbol_table_count(result->symbols) : 0;
}

const Symbol *hyades_symbol_at(const HyadesParseResult *result, int index) {
    return result ? lsp_symbol_table_get(result->symbols, index) : NULL;
}

const Symbol *hyades_symbol_find(const HyadesParseResult *result, const char *name) {
    return result ? lsp_symbol_table_find(result->symbols, name) : NULL;
}

const Symbol *hyades_symbol_at_position(const HyadesParseResult *result, int line, int col) {
    return result ? lsp_symbol_table_at_position(result->symbols, line, col) : NULL;
}

const Symbol *hyades_definition_at_position(const HyadesParseResult *result, int line, int col) {
    if (!result || !result->symbols) return NULL;

    // First, find if there's a reference at this position
    const SymbolReference *ref = lsp_symbol_table_reference_at_position(result->symbols, line, col);
    if (ref) {
        // Look up the definition
        return lsp_symbol_table_find(result->symbols, ref->name);
    }

    // Maybe we're directly on the definition
    return lsp_symbol_table_at_position(result->symbols, line, col);
}

char *hyades_symbols_to_json(const HyadesParseResult *result) {
    if (!result || !result->symbols) {
        return strdup("[]");
    }

    int n = lsp_symbol_table_count(result->symbols);
    int buf_size = 128 + n * 512;
    char *buf = malloc(buf_size);
    if (!buf) return NULL;

    char *p = buf;
    int remaining = buf_size;

    p += snprintf(p, remaining, "[");
    remaining = buf_size - (int)(p - buf);

    for (int i = 0; i < n; i++) {
        const Symbol *sym = lsp_symbol_table_get(result->symbols, i);
        if (!sym) continue;

        if (i > 0) {
            p += snprintf(p, remaining, ",");
            remaining = buf_size - (int)(p - buf);
        }

        // Escape symbol name (may contain backslashes)
        char *escaped_name = json_escape_string(sym->name);

        p += snprintf(p, remaining,
                      "{"
                      "\"name\":\"%s\","
                      "\"kind\":%d,"
                      "\"location\":{"
                      "\"range\":{"
                      "\"start\":{\"line\":%d,\"character\":%d},"
                      "\"end\":{\"line\":%d,\"character\":%d}"
                      "}"
                      "}"
                      "}",
                      escaped_name, symbol_kind_to_lsp(sym->kind),
                      sym->def_line > 0 ? sym->def_line - 1 : 0,
                      sym->def_col > 0 ? sym->def_col - 1 : 0,
                      sym->def_end_line > 0 ? sym->def_end_line - 1 : 0,
                      sym->def_end_col > 0 ? sym->def_end_col - 1 : 0);
        remaining = buf_size - (int)(p - buf);

        free(escaped_name);
    }

    snprintf(p, remaining, "]");

    return buf;
}

// ============================================================================
// Completion Support
// ============================================================================

int hyades_get_completions(const HyadesParseResult *result, int line, int col,
                           HyadesCompletion *out_completions, int max_completions) {
    (void)line;
    (void)col;

    if (!result || !out_completions || max_completions <= 0) return 0;

    int count = 0;

    // Add symbols from symbol table
    int n_symbols = lsp_symbol_table_count(result->symbols);
    for (int i = 0; i < n_symbols && count < max_completions; i++) {
        const Symbol *sym = lsp_symbol_table_get(result->symbols, i);
        if (!sym) continue;

        out_completions[count].label = sym->name;
        out_completions[count].insert_text = sym->name;
        out_completions[count].detail = symbol_detail(sym);
        out_completions[count].documentation = sym->doc_comment;
        out_completions[count].kind = symbol_kind_to_lsp(sym->kind);
        out_completions[count].is_snippet = false;
        count++;
    }

    return count;
}

char *hyades_completions_to_json(const HyadesParseResult *result, int line, int col) {
    if (!result) return strdup("[]");

    HyadesCompletion completions[256];
    int n = hyades_get_completions(result, line, col, completions, 256);

    int buf_size = 128 + n * 512;
    char *buf = malloc(buf_size);
    if (!buf) return NULL;

    char *p = buf;
    int remaining = buf_size;

    p += snprintf(p, remaining, "[");
    remaining = buf_size - (int)(p - buf);

    for (int i = 0; i < n; i++) {
        if (i > 0) {
            p += snprintf(p, remaining, ",");
            remaining = buf_size - (int)(p - buf);
        }

        // Escape strings that may contain special characters
        char *escaped_label = json_escape_string(completions[i].label);
        char *escaped_detail = json_escape_string(completions[i].detail);

        p += snprintf(p, remaining,
                      "{"
                      "\"label\":\"%s\","
                      "\"kind\":%d,"
                      "\"detail\":\"%s\""
                      "}",
                      escaped_label, completions[i].kind, escaped_detail);
        remaining = buf_size - (int)(p - buf);

        free(escaped_label);
        free(escaped_detail);
    }

    snprintf(p, remaining, "]");

    return buf;
}

// ============================================================================
// Position Mapping
// ============================================================================

bool hyades_map_position(const HyadesParseResult *result, int trans_line, int trans_col,
                         int *orig_line, int *orig_col) {
    if (!result || !result->source_map) return false;

    // Convert line:col to byte offset for source map lookup
    // (simplified - assumes 1:1 mapping for now)
    return source_map_lookup(result->source_map, trans_line, orig_line, orig_col);
}

bool hyades_map_position_reverse(const HyadesParseResult *result, int orig_line, int orig_col,
                                 int *trans_line, int *trans_col) {
    // For now, assume identity mapping
    if (trans_line) *trans_line = orig_line;
    if (trans_col) *trans_col = orig_col;
    return result != NULL;
}

// ============================================================================
// Hover Information
// ============================================================================

HyadesHoverResult *hyades_get_hover(const HyadesParseResult *result, int line, int col) {
    if (!result) return NULL;

    // Find symbol at position (checks references first, then definitions)
    const Symbol *sym = hyades_definition_at_position(result, line, col);
    if (!sym) return NULL;

    HyadesHoverResult *hover = calloc(1, sizeof(HyadesHoverResult));
    if (!hover) return NULL;

    // Build markdown content
    char *content = malloc(1024);
    if (!content) {
        free(hover);
        return NULL;
    }

    int len = snprintf(content, 1024, "**%s** *(%s)*\n\n", sym->name, symbol_kind_name(sym->kind));

    if (sym->signature) {
        len += snprintf(content + len, 1024 - len, "Parameters: `%s`\n\n", sym->signature);
    }

    if (sym->doc_comment) {
        len += snprintf(content + len, 1024 - len, "%s\n", sym->doc_comment);
    }

    if (sym->body_preview) {
        snprintf(content + len, 1024 - len, "\n```\n%s\n```\n", sym->body_preview);
    }

    hover->contents = content;
    hover->start_line = sym->def_line;
    hover->start_col = sym->def_col;
    hover->end_line = sym->def_end_line;
    hover->end_col = sym->def_end_col;

    return hover;
}

void hyades_hover_free(HyadesHoverResult *hover) {
    if (!hover) return;
    free((char *)hover->contents);
    free(hover);
}

char *hyades_hover_to_json(const HyadesParseResult *result, int line, int col) {
    HyadesHoverResult *hover = hyades_get_hover(result, line, col);
    if (!hover) return strdup("null");

    // Escape contents for JSON (may contain newlines, quotes, backslashes)
    char *escaped_contents = json_escape_string(hover->contents);

    int buf_size = 2048 + (int)strlen(escaped_contents);
    char *buf = malloc(buf_size);
    if (!buf) {
        free(escaped_contents);
        hyades_hover_free(hover);
        return NULL;
    }

    snprintf(buf, buf_size,
             "{"
             "\"contents\":{\"kind\":\"markdown\",\"value\":\"%s\"},"
             "\"range\":{"
             "\"start\":{\"line\":%d,\"character\":%d},"
             "\"end\":{\"line\":%d,\"character\":%d}"
             "}"
             "}",
             escaped_contents, hover->start_line > 0 ? hover->start_line - 1 : 0,
             hover->start_col > 0 ? hover->start_col - 1 : 0,
             hover->end_line > 0 ? hover->end_line - 1 : 0,
             hover->end_col > 0 ? hover->end_col - 1 : 0);

    free(escaped_contents);
    hyades_hover_free(hover);
    return buf;
}

// ============================================================================
// References
// ============================================================================

int hyades_find_references(const HyadesParseResult *result, int line, int col,
                           const SymbolReference **out_refs) {
    if (!result || !result->symbols) {
        if (out_refs) *out_refs = NULL;
        return 0;
    }

    // Find symbol at position
    const SymbolReference *ref = lsp_symbol_table_reference_at_position(result->symbols, line, col);
    if (!ref) {
        if (out_refs) *out_refs = NULL;
        return 0;
    }

    // Find all references to that symbol
    return lsp_symbol_table_find_references(result->symbols, ref->name, out_refs);
}

char *hyades_references_to_json(const HyadesParseResult *result, int line, int col) {
    const SymbolReference *refs;
    int n = hyades_find_references(result, line, col, &refs);

    if (n == 0) return strdup("[]");

    // Need to iterate through all references manually since we only got first
    // This is a simplification - proper implementation would return array
    int buf_size = 128 + n * 256;
    char *buf = malloc(buf_size);
    if (!buf) return NULL;

    char *p = buf;
    int remaining = buf_size;

    p += snprintf(p, remaining, "[");
    remaining = buf_size - (int)(p - buf);

    // For now just output first reference
    if (refs) {
        snprintf(p, remaining,
                 "{"
                 "\"uri\":\"file://current\","
                 "\"range\":{"
                 "\"start\":{\"line\":%d,\"character\":%d},"
                 "\"end\":{\"line\":%d,\"character\":%d}"
                 "}"
                 "}",
                 refs->line > 0 ? refs->line - 1 : 0, refs->col > 0 ? refs->col - 1 : 0,
                 refs->end_line > 0 ? refs->end_line - 1 : 0,
                 refs->end_col > 0 ? refs->end_col - 1 : 0);
    }

    strcat(buf, "]");

    return buf;
}

// ============================================================================
// Document Symbols
// ============================================================================

char *hyades_document_symbols_to_json(const HyadesParseResult *result) {
    return hyades_symbols_to_json(result);
}

// ============================================================================
// Validation
// ============================================================================

void hyades_validate(HyadesParseResult *result) {
    if (!result) return;

    hyades_validate_delimiters(result);
    hyades_validate_references(result);
    hyades_validate_arity(result);
}

void hyades_validate_references(HyadesParseResult *result) {
    // TODO: Walk through all references and check they have definitions
    (void)result;
}

void hyades_validate_delimiters(HyadesParseResult *result) {
    // TODO: Already done during parsing via delimiter stack
    (void)result;
}

void hyades_validate_arity(HyadesParseResult *result) {
    // TODO: Check macro call argument counts match definitions
    (void)result;
}

// ============================================================================
// Semantic Tokens
// ============================================================================

// UTF-8 to UTF-16 helpers for LSP position encoding
// LSP uses UTF-16 code units for character positions

// Advance pointer over one UTF-8 character, incrementing col by UTF-16 units
static void advance_char(const char **pp, int *col) {
    unsigned char c = (unsigned char)**pp;
    if (c < 0x80) {
        *pp += 1;
        *col += 1;
    } else if (c < 0xE0) {
        *pp += 2;
        *col += 1;
    } else if (c < 0xF0) {
        *pp += 3;
        *col += 1;
    } else {
        *pp += 4;
        *col += 2;
    } // surrogate pair
}

// Compute UTF-16 length of a UTF-8 byte span
static int utf16_len(const char *start, const char *end) {
    int len = 0;
    const char *p = start;
    while (p < end) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x80) {
            p += 1;
            len += 1;
        } else if (c < 0xE0) {
            p += 2;
            len += 1;
        } else if (c < 0xF0) {
            p += 3;
            len += 1;
        } else {
            p += 4;
            len += 2;
        }
    }
    return len;
}

// Dynamic array for semantic token data
typedef struct {
    int *data;
    int count;
    int capacity;
    int prev_line;
    int prev_col;
} TokenData;

static TokenData *token_data_new(void) {
    TokenData *td = calloc(1, sizeof(TokenData));
    if (!td) return NULL;
    td->capacity = 1024;
    td->data = malloc(td->capacity * sizeof(int));
    if (!td->data) {
        free(td);
        return NULL;
    }
    td->prev_line = 0;
    td->prev_col = 0;
    return td;
}

static void token_data_free(TokenData *td) {
    if (td) {
        free(td->data);
        free(td);
    }
}

static void token_data_add(TokenData *td, int line, int col, int len, SemanticTokenType type,
                           int modifiers) {
    if (td->count + 5 > td->capacity) {
        td->capacity *= 2;
        td->data = realloc(td->data, td->capacity * sizeof(int));
        if (!td->data) return;
    }

    // Delta encoding (0-based lines)
    int delta_line = line - td->prev_line;
    int delta_col = (delta_line == 0) ? (col - td->prev_col) : col;

    td->data[td->count++] = delta_line;
    td->data[td->count++] = delta_col;
    td->data[td->count++] = len;
    td->data[td->count++] = (int)type;
    td->data[td->count++] = modifiers;

    td->prev_line = line;
    td->prev_col = col;
}

// Check if a command is a known built-in
static bool is_builtin_command(const char *cmd, int len) {
    // Common math/layout built-ins
    static const char *builtins[] = {
        // Math functions
        "frac", "dfrac", "tfrac", "sqrt", "root", "sum", "prod", "int", "lim", "sin", "cos", "tan",
        "log", "ln", "exp", "alpha", "beta", "gamma", "delta", "epsilon", "theta", "lambda", "mu",
        "pi", "sigma", "tau", "phi", "omega", "infty", "left", "right", "big", "Big", "bigg",
        "Bigg", "text", "textbf", "textit", "mathrm", "mathbf", "mathit", "mathcal", "hbox", "vbox",
        "mbox", "hskip", "vskip", "quad", "qquad", "over", "atop", "choose", "brace", "brack",
        "overline", "underline", "hat", "tilde", "vec", "dot", "ddot", "overbrace", "underbrace",
        "hspace", "vspace", "smallskip", "medskip", "bigskip", "center", "raggedright",
        "raggedleft", "item", "enumerate", "itemize", "rule", "hrule", "vrule", "phantom",
        "hphantom", "vphantom", "stackrel", "overset", "underset", "binom", "tbinom", "dbinom",
        "cases", "matrix", "pmatrix", "bmatrix", "vmatrix",
        // Calc/Lambda commands (without angle brackets)
        "if", "else", "return", "exit_when", "add", "sub", "mul", "div", "mod", "neg", "abs",
        "floor", "ceil", "round", "max", "min", "rand", "eq", "neq", "ne", "lt", "gt", "leq", "le",
        "geq", "ge", "and", "or", "not", "streq", "strneq", "strlen", "substr", "strcat",
        "startswith", "endswith", "contains",
        // Output commands (no angle brackets)
        "emit", "cursor", "ansi",
        // Memory commands (brace syntax)
        "mem_alloc", "mem_load", "mem_store", NULL};

    for (int i = 0; builtins[i]; i++) {
        if ((int)strlen(builtins[i]) == len && strncmp(cmd, builtins[i], len) == 0) {
            return true;
        }
    }
    return false;
}

// Internal: compute semantic tokens into a TokenData structure
static TokenData *compute_semantic_tokens(const HyadesParseResult *result, const char *source) {
    TokenData *td = token_data_new();
    if (!td) return NULL;

    int line = 0; // 0-based for LSP
    int col = 0;
    const char *p = source;
    const char *line_start = source;
    int math_depth = 0; // Track nested math mode ($ or $$)

    while (*p) {
        // Track line/col
        if (*p == '\n') {
            line++;
            col = 0;
            p++;
            line_start = p;
            continue;
        }

        // Skip lines containing NBSP (rendered Cassilda output)
        // NBSP in UTF-8 is 0xC2 0xA0
        if (p == line_start) {
            // Check if this line contains NBSP
            const char *scan = p;
            bool has_nbsp = false;
            while (*scan && *scan != '\n') {
                if ((unsigned char)scan[0] == 0xC2 && (unsigned char)scan[1] == 0xA0) {
                    has_nbsp = true;
                    break;
                }
                scan++;
            }
            if (has_nbsp) {
                // Skip entire line
                while (*p && *p != '\n') {
                    p++;
                } // col doesn't matter, line is skipped
                continue;
            }
        }

        // Skip whitespace
        if (*p == ' ' || *p == '\t') {
            p++;
            col++;
            continue;
        }

        // Comment: % to end of line
        if (*p == '%' && (p == source || *(p - 1) != '\\')) {
            const char *start = p;
            int start_col = col;
            while (*p && *p != '\n') {
                advance_char(&p, &col);
            }
            token_data_add(td, line, start_col, utf16_len(start, p), SEM_TOK_COMMENT, 0);
            continue;
        }

        // Cassilda directives
        if (p == line_start || (p > source && *(p - 1) == '\n')) {
            // @label
            if (strncmp(p, "@label", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
                token_data_add(td, line, col, 6, SEM_TOK_KEYWORD, 0);
                p += 6;
                col += 6;
                // Skip whitespace then highlight label name as variable
                while (*p == ' ' || *p == '\t') {
                    p++;
                    col++;
                }
                const char *name_start = p;
                int name_col = col;
                while (*p && *p != '\n' && *p != ' ' && *p != '\t') {
                    advance_char(&p, &col);
                }
                if (p > name_start) {
                    token_data_add(td, line, name_col, utf16_len(name_start, p), SEM_TOK_VARIABLE,
                                   SEM_MOD_DECLARATION);
                }
                continue;
            }
            // @end
            if (strncmp(p, "@end", 4) == 0 && (p[4] == '\n' || p[4] == '\0' || p[4] == ' ')) {
                token_data_add(td, line, col, 4, SEM_TOK_KEYWORD, 0);
                p += 4;
                col += 4;
                continue;
            }
            // @cassilda:
            if (strncmp(p, "@cassilda:", 10) == 0) {
                token_data_add(td, line, col, 10, SEM_TOK_KEYWORD, 0);
                p += 10;
                col += 10;
                // Label reference after
                while (*p == ' ' || *p == '\t') {
                    p++;
                    col++;
                }
                const char *ref_start = p;
                int ref_col = col;
                while (*p && *p != '\n' && *p != ',') {
                    advance_char(&p, &col);
                }
                if (p > ref_start) {
                    token_data_add(td, line, ref_col, utf16_len(ref_start, p), SEM_TOK_VARIABLE, 0);
                }
                continue;
            }
            // #before_each, #after_each, #end
            if (*p == '#') {
                if (strncmp(p, "#before_each", 12) == 0) {
                    token_data_add(td, line, col, 12, SEM_TOK_KEYWORD, 0);
                    p += 12;
                    col += 12;
                    continue;
                }
                if (strncmp(p, "#after_each", 11) == 0) {
                    token_data_add(td, line, col, 11, SEM_TOK_KEYWORD, 0);
                    p += 11;
                    col += 11;
                    continue;
                }
                if (strncmp(p, "#end", 4) == 0) {
                    token_data_add(td, line, col, 4, SEM_TOK_KEYWORD, 0);
                    p += 4;
                    col += 4;
                    continue;
                }
                if (strncmp(p, "#source_prefix", 14) == 0 ||
                    strncmp(p, "#target_prefix", 14) == 0) {
                    token_data_add(td, line, col, 14, SEM_TOK_KEYWORD, 0);
                    p += 14;
                    col += 14;
                    continue;
                }
            }
        }

        // Variable access / macro parameter ${name}, ${*name}, ${item${i}}
        if (p[0] == '$' && p[1] == '{') {
            const char *start = p;
            int start_col = col;
            p += 2;
            col += 2;
            // Handle nested ${...} by tracking brace depth
            int brace_depth = 1;
            while (*p && brace_depth > 0 && *p != '\n') {
                if (*p == '$' && p[1] == '{') {
                    brace_depth++;
                    p += 2;
                    col += 2;
                } else if (*p == '{') {
                    brace_depth++;
                    p++;
                    col++;
                } else if (*p == '}') {
                    brace_depth--;
                    if (brace_depth > 0) {
                        p++;
                        col++;
                    }
                } else {
                    advance_char(&p, &col);
                }
            }
            if (*p == '}') {
                p++;
                col++;
            }
            // Use VARIABLE for ${} access (consistent with CL semantics)
            // Check if it's a dereference pattern ${*name}
            bool is_deref = (start[2] == '*');
            token_data_add(td, line, start_col, utf16_len(start, p), SEM_TOK_VARIABLE,
                           is_deref ? SEM_MOD_READONLY : 0);
            continue;
        }

        // Display math $$
        if (p[0] == '$' && p[1] == '$') {
            token_data_add(td, line, col, 2, SEM_TOK_KEYWORD, 0);
            // Toggle math mode (display math uses depth 2 to distinguish)
            if (math_depth == 2)
                math_depth = 0;
            else if (math_depth == 0)
                math_depth = 2;
            p += 2;
            col += 2;
            continue;
        }

        // Inline math $
        if (*p == '$') {
            token_data_add(td, line, col, 1, SEM_TOK_KEYWORD, 0);
            // Toggle math mode
            if (math_depth == 1)
                math_depth = 0;
            else if (math_depth == 0)
                math_depth = 1;
            p++;
            col++;
            continue;
        }

        // Type annotations :int, :string, :int[], :string[], :map, :address
        // These appear inside angle brackets like \let<name:int>
        if (*p == ':' && p[1] && (p[1] >= 'a' && p[1] <= 'z')) {
            const char *start = p;
            int start_col = col;
            p++;
            col++; // skip ':'
            // Collect type name
            while (*p && ((*p >= 'a' && *p <= 'z') || *p == '[' || *p == ']')) {
                p++;
                col++;
            }
            token_data_add(td, line, start_col, (int)(p - start), SEM_TOK_TYPE, 0);
            continue;
        }

        // Dereference marker * in angle brackets (like <*name>)
        if (*p == '*' && col > 0) {
            // Check if we're likely inside angle brackets (after < or after a command)
            token_data_add(td, line, col, 1, SEM_TOK_OPERATOR, 0);
            p++;
            col++;
            continue;
        }

        // Map literal delimiters | and arrow ->
        if (*p == '|') {
            token_data_add(td, line, col, 1, SEM_TOK_OPERATOR, 0);
            p++;
            col++;
            continue;
        }
        if (p[0] == '-' && p[1] == '>') {
            token_data_add(td, line, col, 2, SEM_TOK_OPERATOR, 0);
            p += 2;
            col += 2;
            continue;
        }

        // Angle brackets for type parameters (closing >)
        // The opening < is handled by command-specific code above
        // Use CLASS for light blue color distinct from pink keyword
        if (*p == '>') {
            token_data_add(td, line, col, 1, SEM_TOK_CLASS, 0);
            p++;
            col++;
            continue;
        }

        // Subscript/superscript operators in math mode
        if (math_depth > 0 && (*p == '_' || *p == '^')) {
            token_data_add(td, line, col, 1, SEM_TOK_OPERATOR, 0);
            p++;
            col++;
            continue;
        }

        // Backslash commands
        if (*p == '\\') {
            int cmd_col = col;
            p++;
            col++;

            // \begin or \end - keyword
            if (strncmp(p, "begin", 5) == 0 && (p[5] == '{' || p[5] == '[' || p[5] == '<')) {
                token_data_add(td, line, cmd_col, 6, SEM_TOK_KEYWORD, 0);
                p += 5;
                col += 5;
                continue;
            }
            if (strncmp(p, "end", 3) == 0 && (p[3] == '{' || p[3] == '[')) {
                token_data_add(td, line, cmd_col, 4, SEM_TOK_KEYWORD, 0);
                p += 3;
                col += 3;
                continue;
            }

            // \verb<delim>...<delim> - verbatim content as string
            if (strncmp(p, "verb", 4) == 0 && p[4] != 'a') { // not \verbatim
                token_data_add(td, line, cmd_col, 5, SEM_TOK_KEYWORD, 0);
                p += 4;
                col += 4;
                if (*p && *p != '\n') {
                    char delim = *p;
                    p++;
                    col++; // skip opening delimiter
                    const char *content_start = p;
                    int content_col = col;
                    // Find closing delimiter (can span lines)
                    while (*p && *p != delim) {
                        if (*p == '\n') {
                            line++;
                            col = 0;
                            p++;
                        } else {
                            advance_char(&p, &col);
                        }
                    }
                    if (p > content_start) {
                        // For multi-line, just highlight first line portion
                        // (LSP tokens are per-line anyway, this is simplified)
                        token_data_add(td, line, content_col, utf16_len(content_start, p),
                                       SEM_TOK_STRING, 0);
                    }
                    if (*p == delim) {
                        p++;
                        col++;
                    } // skip closing delimiter
                }
                continue;
            }

            // \verbatim{...} - verbatim content as string
            if (strncmp(p, "verbatim", 8) == 0 && p[8] == '{') {
                token_data_add(td, line, cmd_col, 9, SEM_TOK_KEYWORD, 0);
                p += 9;
                col += 9; // includes the {
                const char *content_start = p;
                int content_col = col;
                int content_start_line = line;
                int brace_depth = 1;
                // Find matching closing brace
                while (*p && brace_depth > 0) {
                    if (*p == '{')
                        brace_depth++;
                    else if (*p == '}')
                        brace_depth--;
                    if (brace_depth > 0) {
                        if (*p == '\n') {
                            line++;
                            col = 0;
                            p++;
                        } else {
                            advance_char(&p, &col);
                        }
                    }
                }
                // Highlight content as string (simplified - just the extent)
                if (p > content_start) {
                    token_data_add(td, content_start_line, content_col, utf16_len(content_start, p),
                                   SEM_TOK_STRING, 0);
                }
                if (*p == '}') {
                    p++;
                    col++;
                }
                continue;
            }

            // Commands with angle brackets: \macro<, \lambda<, \let<, \recall<, \valueof<, etc.
            // Highlight command name as keyword, then < as operator
            // The content and closing > will be handled by the main loop
            if ((strncmp(p, "macro<", 6) == 0) || (strncmp(p, "lambda<", 7) == 0) ||
                (strncmp(p, "let<", 4) == 0) || (strncmp(p, "assign<", 7) == 0) ||
                (strncmp(p, "recall<", 7) == 0) || (strncmp(p, "valueof<", 8) == 0) ||
                (strncmp(p, "inc<", 4) == 0) || (strncmp(p, "dec<", 4) == 0) ||
                (strncmp(p, "push<", 5) == 0) || (strncmp(p, "pop<", 4) == 0) ||
                (strncmp(p, "peek<", 5) == 0) || (strncmp(p, "enqueue<", 8) == 0) ||
                (strncmp(p, "dequeue<", 8) == 0) || (strncmp(p, "len<", 4) == 0) ||
                (strncmp(p, "split<", 6) == 0) || (strncmp(p, "setelement<", 11) == 0) ||
                (strncmp(p, "measure<", 8) == 0) || (strncmp(p, "measureref<", 11) == 0) ||
                (strncmp(p, "ref<", 4) == 0) || (strncmp(p, "lineroutine<", 12) == 0) ||
                // New CL syntax commands
                (strncmp(p, "invoke<", 7) == 0) || (strncmp(p, "at<", 3) == 0) ||
                (strncmp(p, "set<", 4) == 0) || (strncmp(p, "addressof<", 10) == 0) ||
                (strncmp(p, "map_has<", 8) == 0) || (strncmp(p, "map_del<", 8) == 0) ||
                (strncmp(p, "map_keys<", 9) == 0)) {

                // Find the < position
                const char *angle = p;
                while (*angle && *angle != '<') angle++;

                // Highlight command name (without <) as keyword
                int cmd_len = (int)(angle - p) + 1; // +1 for backslash
                token_data_add(td, line, cmd_col, cmd_len, SEM_TOK_KEYWORD, 0);
                p = angle;
                col = cmd_col + cmd_len;

                // Highlight < as class (light blue, distinct from pink keyword)
                token_data_add(td, line, col, 1, SEM_TOK_CLASS, 0);
                p++;
                col++;

                // Let main loop continue to parse content inside <>
                continue;
            }

            // Collect command name
            // In math mode, _ and ^ are subscript/superscript operators, not part of command names
            const char *name_start = p;
            while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                          (math_depth == 0 && *p == '_') || (*p >= '0' && *p <= '9'))) {
                p++;
                col++;
            }
            int name_len = (int)(p - name_start);

            if (name_len == 0) {
                // Escaped character like \{ \} \% etc
                if (*p) {
                    advance_char(&p, &col);
                }
                continue;
            }

            int total_len = name_len + 1; // +1 for backslash

            // Check if it's a user macro (from symbol table)
            bool is_user_macro = false;
            if (result && result->symbols) {
                // Build name with backslash for lookup
                char lookup_name[128];
                lookup_name[0] = '\\';
                int copy_len = name_len < 126 ? name_len : 126;
                memcpy(lookup_name + 1, name_start, copy_len);
                lookup_name[copy_len + 1] = '\0';

                const Symbol *sym = lsp_symbol_table_find(result->symbols, lookup_name);
                if (sym && (sym->kind == SYMKIND_MACRO || sym->kind == SYMKIND_LAMBDA)) {
                    is_user_macro = true;
                }
            }

            if (is_user_macro) {
                token_data_add(td, line, cmd_col, total_len, SEM_TOK_MACRO, 0);
            } else if (is_builtin_command(name_start, name_len)) {
                token_data_add(td, line, cmd_col, total_len, SEM_TOK_FUNCTION,
                               SEM_MOD_DEFAULT_LIBRARY);
            } else {
                // Unknown command - treat as function
                token_data_add(td, line, cmd_col, total_len, SEM_TOK_FUNCTION, 0);
            }
            continue;
        }

        // Numbers
        if (*p >= '0' && *p <= '9') {
            const char *num_start = p;
            int num_col = col;
            while (*p && ((*p >= '0' && *p <= '9') || *p == '.')) {
                p++;
                col++;
            }
            token_data_add(td, line, num_col, (int)(p - num_start), SEM_TOK_NUMBER, 0);
            continue;
        }

        // Brackets: {}, [], ()
        if (*p == '{' || *p == '}' || *p == '[' || *p == ']' || *p == '(' || *p == ')') {
            token_data_add(td, line, col, 1, SEM_TOK_CLASS, 0);
            p++;
            col++;
            continue;
        }

        // Default: skip character (advance by full UTF-8 sequence)
        advance_char(&p, &col);
    }

    return td;
}

char *hyades_semantic_tokens_to_json(const HyadesParseResult *result, const char *source) {
    if (!source) return strdup("{\"data\":[]}");

    TokenData *td = compute_semantic_tokens(result, source);
    if (!td) return strdup("{\"data\":[]}");

    // Build JSON output
    int buf_size = 32 + td->count * 12;
    char *buf = malloc(buf_size);
    if (!buf) {
        token_data_free(td);
        return strdup("{\"data\":[]}");
    }

    char *out = buf;
    int remaining = buf_size;
    out += snprintf(out, remaining, "{\"data\":[");
    remaining = buf_size - (int)(out - buf);

    for (int i = 0; i < td->count; i++) {
        if (i > 0) {
            out += snprintf(out, remaining, ",");
            remaining = buf_size - (int)(out - buf);
        }
        out += snprintf(out, remaining, "%d", td->data[i]);
        remaining = buf_size - (int)(out - buf);
    }

    snprintf(out, remaining, "]}");

    token_data_free(td);
    return buf;
}

int hyades_semantic_tokens_to_raw(const HyadesParseResult *result, const char *source,
                                  int **out_data) {
    *out_data = NULL;
    if (!source) return 0;

    TokenData *td = compute_semantic_tokens(result, source);
    if (!td) return 0;

    int count = td->count;
    // Transfer ownership of data array to caller
    *out_data = td->data;
    td->data = NULL; // prevent token_data_free from freeing it
    token_data_free(td);
    return count;
}

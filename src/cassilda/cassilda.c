// cassilda.c - Cassilda document processor for Hyades

#include "cassilda.h"
#include "compositor/compositor.h"
#include "document/document.h" // For verbatim_store_clear
#include "label_library.h"
#include "public_api/hyades_api.h"
#include "utils/error.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NBSP character (U+00A0) as UTF-8: 0xC2 0xA0
#define NBSP "\xC2\xA0"
#define NBSP_LEN 2

// ============================================================================
// String Buffer (for building output)
// ============================================================================

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} StringBuf;

static void strbuf_init(StringBuf *sb) {
    sb->capacity = 1024;
    sb->data = malloc(sb->capacity);
    sb->data[0] = '\0';
    sb->len = 0;
}

static void strbuf_free(StringBuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->capacity = 0;
}

static void strbuf_ensure(StringBuf *sb, size_t additional) {
    if (sb->len + additional + 1 > sb->capacity) {
        while (sb->len + additional + 1 > sb->capacity) {
            sb->capacity *= 2;
        }
        sb->data = realloc(sb->data, sb->capacity);
    }
}

static void strbuf_append(StringBuf *sb, const char *str) {
    size_t len = strlen(str);
    strbuf_ensure(sb, len);
    memcpy(sb->data + sb->len, str, len + 1);
    sb->len += len;
}

static void strbuf_append_n(StringBuf *sb, const char *str, size_t n) {
    strbuf_ensure(sb, n);
    memcpy(sb->data + sb->len, str, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void strbuf_append_char(StringBuf *sb, char c) {
    strbuf_ensure(sb, 1);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

static char *strbuf_detach(StringBuf *sb) {
    char *result = sb->data;
    sb->data = NULL;
    sb->len = 0;
    sb->capacity = 0;
    return result;
}

// ============================================================================
// Configuration
// ============================================================================

void cassilda_config_init(CassildaConfig *config) {
    config->source_prefix = strdup("");
    config->target_prefix = strdup("");
    config->before_each = strdup("");
    config->after_each = strdup("");
    config->default_width = 80;
}

void cassilda_config_free(CassildaConfig *config) {
    free(config->source_prefix);
    free(config->target_prefix);
    free(config->before_each);
    free(config->after_each);
    config->source_prefix = NULL;
    config->target_prefix = NULL;
    config->before_each = NULL;
    config->after_each = NULL;
}

// ============================================================================
// Document Memory Management
// ============================================================================

static CassildaDocument *doc_new(void) {
    CassildaDocument *doc = malloc(sizeof(CassildaDocument));
    cassilda_config_init(&doc->config);

    doc->segments = NULL;
    doc->n_segments = 0;
    doc->segments_capacity = 0;

    doc->references = NULL;
    doc->n_references = 0;
    doc->references_capacity = 0;

    doc->lines = NULL;
    doc->n_lines = 0;
    doc->lines_capacity = 0;

    doc->library_ctx = NULL;
    doc->filename = NULL;

    return doc;
}

static void doc_add_segment(CassildaDocument *doc, const char *name, const char *content) {
    if (doc->n_segments >= doc->segments_capacity) {
        doc->segments_capacity = doc->segments_capacity == 0 ? 8 : doc->segments_capacity * 2;
        doc->segments = realloc(doc->segments, doc->segments_capacity * sizeof(CassildaSegment));
    }
    doc->segments[doc->n_segments].name = strdup(name);
    doc->segments[doc->n_segments].content = strdup(content);
    doc->n_segments++;
}

static void doc_add_reference(CassildaDocument *doc, int line_number, char **labels, int n_labels) {
    if (doc->n_references >= doc->references_capacity) {
        doc->references_capacity = doc->references_capacity == 0 ? 8 : doc->references_capacity * 2;
        doc->references =
            realloc(doc->references, doc->references_capacity * sizeof(CassildaReference));
    }
    doc->references[doc->n_references].line_number = line_number;
    doc->references[doc->n_references].labels = labels;
    doc->references[doc->n_references].n_labels = n_labels;
    doc->n_references++;
}

static void doc_add_line(CassildaDocument *doc, const char *line) {
    if (doc->n_lines >= doc->lines_capacity) {
        doc->lines_capacity = doc->lines_capacity == 0 ? 64 : doc->lines_capacity * 2;
        doc->lines = realloc(doc->lines, doc->lines_capacity * sizeof(char *));
    }
    doc->lines[doc->n_lines++] = strdup(line);
}

void cassilda_free(CassildaDocument *doc) {
    if (!doc) return;

    cassilda_config_free(&doc->config);

    for (int i = 0; i < doc->n_segments; i++) {
        free(doc->segments[i].name);
        free(doc->segments[i].content);
    }
    free(doc->segments);

    for (int i = 0; i < doc->n_references; i++) {
        for (int j = 0; j < doc->references[i].n_labels; j++) {
            free(doc->references[i].labels[j]);
        }
        free(doc->references[i].labels);
    }
    free(doc->references);

    for (int i = 0; i < doc->n_lines; i++) {
        free(doc->lines[i]);
    }
    free(doc->lines);

    free(doc->filename);

    free(doc);
}

// ============================================================================
// Parsing Helpers
// ============================================================================

// Skip whitespace (not newlines)
static void skip_ws(const char **p) {
    while (**p == ' ' || **p == '\t') (*p)++;
}

// Parse a quoted string, returns allocated string
// Handles "..." with basic escape sequences
static char *parse_quoted_string(const char **p) {
    skip_ws(p);
    if (**p != '"') return NULL;
    (*p)++; // Skip opening quote

    StringBuf sb;
    strbuf_init(&sb);

    while (**p && **p != '"') {
        if (**p == '\\' && (*p)[1]) {
            (*p)++;
            switch (**p) {
            case 'n': strbuf_append_char(&sb, '\n'); break;
            case 't': strbuf_append_char(&sb, '\t'); break;
            case '\\': strbuf_append_char(&sb, '\\'); break;
            case '"': strbuf_append_char(&sb, '"'); break;
            case '0':
                // Handle \033 for ANSI escapes
                if ((*p)[1] == '3' && (*p)[2] == '3') {
                    strbuf_append_char(&sb, '\033');
                    (*p) += 2;
                } else {
                    strbuf_append_char(&sb, **p);
                }
                break;
            default: strbuf_append_char(&sb, **p); break;
            }
        } else {
            strbuf_append_char(&sb, **p);
        }
        (*p)++;
    }

    if (**p == '"') (*p)++; // Skip closing quote

    return strbuf_detach(&sb);
}

// Parse an identifier (alphanumeric + underscore)
static char *parse_identifier(const char **p) {
    skip_ws(p);
    const char *start = *p;

    while (isalnum((unsigned char)**p) || **p == '_' || **p == '-') {
        (*p)++;
    }

    if (*p == start) return NULL;

    size_t len = *p - start;
    char *id = malloc(len + 1);
    memcpy(id, start, len);
    id[len] = '\0';
    return id;
}

// Parse comma-separated list of identifiers
static char **parse_label_list(const char **p, int *n_labels) {
    char **labels = NULL;
    int capacity = 0;
    *n_labels = 0;

    while (1) {
        skip_ws(p);
        char *label = parse_identifier(p);
        if (!label) break;

        if (*n_labels >= capacity) {
            capacity = capacity == 0 ? 4 : capacity * 2;
            labels = realloc(labels, capacity * sizeof(char *));
        }
        labels[(*n_labels)++] = label;

        skip_ws(p);
        if (**p == ',') {
            (*p)++;
        } else {
            break;
        }
    }

    return labels;
}

// Check if line is rendered output by looking for target_prefix + NBSP pattern
// We render as: target_prefix + NBSP + content
// So we detect by checking for exactly that pattern
static bool line_is_rendered_output(const char *line, const char *target_prefix) {
    if (!target_prefix || !target_prefix[0]) {
        // No target_prefix - just check if line starts with NBSP
        return strncmp(line, NBSP, NBSP_LEN) == 0;
    }

    // Check for target_prefix + NBSP at start of line
    size_t prefix_len = strlen(target_prefix);
    if (strncmp(line, target_prefix, prefix_len) != 0) {
        return false;
    }

    // Check for NBSP immediately after target_prefix
    return strncmp(line + prefix_len, NBSP, NBSP_LEN) == 0;
}

// Check if input contains the #!clear directive
static bool has_clear_directive(const char *input) {
    return strstr(input, "#!clear") != NULL;
}

// Check if line contains NBSP anywhere in the first N characters
// This is used for cleaning when we don't know the exact target_prefix
static bool line_contains_nbsp(const char *line, size_t max_chars) {
    size_t len = strlen(line);
    size_t check_len = len < max_chars ? len : max_chars;

    for (size_t i = 0; i + NBSP_LEN <= check_len; i++) {
        if (strncmp(line + i, NBSP, NBSP_LEN) == 0) {
            return true;
        }
    }
    return false;
}

// Clean rendered output: remove all lines containing NBSP
// This implements both the #!clear directive and the clean command
char *cassilda_clean(const char *input, const char *target_prefix_hint) {
    if (!input) return NULL;

    StringBuf result;
    strbuf_init(&result);

    const char *p = input;

    while (*p) {
        // Find end of line
        const char *line_start = p;
        while (*p && *p != '\n') p++;

        size_t line_len = p - line_start;

        // Extract line for checking
        char *line = malloc(line_len + 1);
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';

        // Skip the #!clear directive line itself
        bool is_clear_directive = (strstr(line, "#!clear") != NULL);

        // Check if this is rendered output (to skip)
        // If we have a target_prefix_hint, use exact matching
        // Otherwise, check for NBSP anywhere in the first 20 chars
        bool is_rendered;
        if (target_prefix_hint && target_prefix_hint[0]) {
            is_rendered = line_is_rendered_output(line, target_prefix_hint);
        } else {
            is_rendered = line_contains_nbsp(line, 20);
        }

        // Keep lines that are not rendered output and not the #!clear directive
        if (!is_rendered && !is_clear_directive) {
            strbuf_append_n(&result, line_start, line_len);
            if (*p == '\n') {
                strbuf_append_char(&result, '\n');
            }
        }

        free(line);

        if (*p == '\n') p++;
    }

    return strbuf_detach(&result);
}

// Try to parse #source_prefix from anywhere on the line
// Returns the value if found, NULL otherwise
static char *try_parse_source_prefix_directive(const char *line) {
    // Look for #source_prefix anywhere on the line
    const char *p = strstr(line, "#source_prefix");
    if (!p) return NULL;

    p += 13; // Skip "#source_prefix"
    skip_ws(&p);

    return parse_quoted_string(&p);
}

// Try to parse @cassilda: from anywhere on the line (auto-detect prefix)
// This allows @cassilda: directives to work regardless of source_prefix setting
// Returns true if found and parsed, false otherwise
static bool try_parse_cassilda_reference(const char *line, CassildaDocument *doc) {
    // Look for @cassilda: anywhere on the line
    const char *p = strstr(line, "@cassilda:");
    if (!p) return false;

    p += 10; // Skip "@cassilda:"

    int n_labels = 0;
    char **labels = parse_label_list(&p, &n_labels);

    if (n_labels > 0) {
        doc_add_reference(doc, doc->n_lines, labels, n_labels);
        return true;
    }

    return false;
}

// Get default comment character for file extension
static const char *get_default_source_prefix(const char *filename) {
    if (!filename) return "";

    const char *ext = strrchr(filename, '.');
    if (!ext) return "";

    // C-style comments
    if (strcmp(ext, ".c") == 0 || strcmp(ext, ".cpp") == 0 || strcmp(ext, ".cc") == 0 ||
        strcmp(ext, ".cxx") == 0 || strcmp(ext, ".h") == 0 || strcmp(ext, ".hpp") == 0 ||
        strcmp(ext, ".hh") == 0 || strcmp(ext, ".hxx") == 0 || strcmp(ext, ".js") == 0 ||
        strcmp(ext, ".ts") == 0 || strcmp(ext, ".jsx") == 0 || strcmp(ext, ".tsx") == 0 ||
        strcmp(ext, ".java") == 0 || strcmp(ext, ".cs") == 0 || strcmp(ext, ".go") == 0 ||
        strcmp(ext, ".rs") == 0 || strcmp(ext, ".swift") == 0 || strcmp(ext, ".kt") == 0) {
        return "//";
    }

    // Hash comments
    if (strcmp(ext, ".py") == 0 || strcmp(ext, ".sh") == 0 || strcmp(ext, ".bash") == 0 ||
        strcmp(ext, ".rb") == 0 || strcmp(ext, ".pl") == 0 || strcmp(ext, ".pm") == 0 ||
        strcmp(ext, ".yaml") == 0 || strcmp(ext, ".yml") == 0 || strcmp(ext, ".toml") == 0 ||
        strcmp(ext, ".conf") == 0 || strcmp(ext, ".r") == 0 || strcmp(ext, ".R") == 0) {
        return "#";
    }

    // SQL comments
    if (strcmp(ext, ".sql") == 0) {
        return "--";
    }

    // Lua comments
    if (strcmp(ext, ".lua") == 0) {
        return "--";
    }

    // VB/VBA comments
    if (strcmp(ext, ".vb") == 0 || strcmp(ext, ".vba") == 0 || strcmp(ext, ".bas") == 0) {
        return "'";
    }

    // MATLAB/Octave
    if (strcmp(ext, ".m") == 0) {
        return "%";
    }

    // Lisp-style
    if (strcmp(ext, ".lisp") == 0 || strcmp(ext, ".cl") == 0 || strcmp(ext, ".el") == 0 ||
        strcmp(ext, ".scm") == 0) {
        return ";";
    }

    return "";
}

// ============================================================================
// Parser
// ============================================================================

CassildaDocument *cassilda_parse(const char *input, const char *filename,
                                 const char *target_prefix_hint, char *error_msg, int error_size) {
    CassildaDocument *doc = doc_new();

    // Store filename for extension-based config lookup
    if (filename) {
        doc->filename = strdup(filename);
    }

    // If target_prefix hint is provided (from config), use it for rendered output detection
    if (target_prefix_hint && target_prefix_hint[0]) {
        free(doc->config.target_prefix);
        doc->config.target_prefix = strdup(target_prefix_hint);
    }

    // Pre-scan: Check if input has #target_prefix directive (needed for rendered output detection)
    // This must be done before parsing since rendered output lines may appear before the directive
    const char *tp_pos = strstr(input, "#target_prefix");
    if (!tp_pos) tp_pos = strstr(input, "#output_prefix"); // Legacy name
    if (tp_pos) {
        const char *p = tp_pos + (strncmp(tp_pos, "#target_prefix", 14) == 0 ? 14 : 14);
        skip_ws(&p);
        char *value = parse_quoted_string(&p);
        if (value) {
            free(doc->config.target_prefix);
            doc->config.target_prefix = value;
        }
    }

    // Pre-scan: Check if input has #source_prefix directive
    // If not, infer from filename extension
    if (strstr(input, "#source_prefix") == NULL && filename) {
        const char *default_cc = get_default_source_prefix(filename);
        if (default_cc) {
            free(doc->config.source_prefix);
            doc->config.source_prefix = strdup(default_cc);
        }
    }

    // Split input into lines
    const char *p = input;
    const char *line_start = p;
    int line_number = 0;

    // State for parsing
    bool in_segment = false;
    char *current_segment_name = NULL;
    StringBuf segment_content;
    strbuf_init(&segment_content);

    bool in_before_each = false;
    bool in_after_each = false;
    StringBuf block_content;
    strbuf_init(&block_content);

    while (1) {
        // Find end of line
        const char *line_end = p;
        while (*line_end && *line_end != '\n') line_end++;

        // Extract line
        size_t line_len = line_end - line_start;
        char *line = malloc(line_len + 1);
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';

        // Check if this is rendered output (to skip)
        bool is_rendered = line_is_rendered_output(line, doc->config.target_prefix);

        if (!is_rendered) {
            // First, check for #source_prefix anywhere on this line
            // This must be done BEFORE stripping, to handle cases like: // #source_prefix "//"
            char *new_source_prefix = try_parse_source_prefix_directive(line);
            if (new_source_prefix) {
                free(doc->config.source_prefix);
                doc->config.source_prefix = new_source_prefix;
                doc_add_line(doc, line);
                free(line);
                if (!*line_end) break;
                p = line_end + 1;
                line_start = p;
                if (*line_start == '\0') break;
                line_number++;
                continue;
            }

            // Auto-detect @cassilda: references anywhere on the line
            // This allows @cassilda: to work regardless of source_prefix setting
            // (source_prefix is for when the file is used as a SOURCE/library,
            // but @cassilda: is a TARGET directive for rendering content)
            if (try_parse_cassilda_reference(line, doc)) {
                doc_add_line(doc, line);
                free(line);
                if (!*line_end) break;
                p = line_end + 1;
                line_start = p;
                if (*line_start == '\0') break;
                line_number++;
                continue;
            }

            // Set up two pointers:
            // - content: preserves spaces, for segment/block content
            // - lp: skips leading spaces, for directive detection
            const char *content = line;
            const char *lp = line;

            // Strip comment char if present
            size_t comment_len = strlen(doc->config.source_prefix);
            if (comment_len > 0 && strncmp(line, doc->config.source_prefix, comment_len) == 0) {
                content += comment_len; // Skip comment char
                lp += comment_len;
                // Skip ONE space after comment char
                if (*content == ' ') {
                    content++;
                    lp++;
                }
            }

            // For directive detection, skip leading whitespace
            skip_ws(&lp);

            // Check for # directives
            if (*lp == '#') {
                lp++;
                char *directive = parse_identifier(&lp);

                if (directive) {
                    // Close any open block
                    if (in_before_each) {
                        free(doc->config.before_each);
                        doc->config.before_each = strbuf_detach(&block_content);
                        strbuf_init(&block_content);
                        in_before_each = false;

                        // Extract \setwidth from before_each to set default_width for parsing
                        // This allows rules and line breaking to use the correct width
                        if (doc->config.before_each) {
                            const char *setwidth_pos =
                                strstr(doc->config.before_each, "\\setwidth");
                            if (setwidth_pos) {
                                const char *width_p = setwidth_pos + 9; // Skip "\\setwidth"
                                while (*width_p == ' ' || *width_p == '\t') width_p++;
                                if (*width_p == '{') {
                                    width_p++;
                                    if (isdigit(*width_p)) {
                                        doc->config.default_width = atoi(width_p);
                                    }
                                }
                            }
                        }
                    }
                    if (in_after_each) {
                        free(doc->config.after_each);
                        doc->config.after_each = strbuf_detach(&block_content);
                        strbuf_init(&block_content);
                        in_after_each = false;
                    }

                    if (strcmp(directive, "source_prefix") == 0 ||
                        strcmp(directive, "comment_char") == 0) { // comment_char is legacy
                        char *value = parse_quoted_string(&lp);
                        if (value) {
                            free(doc->config.source_prefix);
                            doc->config.source_prefix = value;
                        }
                    } else if (strcmp(directive, "target_prefix") == 0 ||
                               strcmp(directive, "output_prefix") == 0) { // output_prefix is legacy
                        char *value = parse_quoted_string(&lp);
                        if (value) {
                            free(doc->config.target_prefix);
                            doc->config.target_prefix = value;
                        }
                    } else if (strcmp(directive, "before_each") == 0) {
                        in_before_each = true;
                    } else if (strcmp(directive, "after_each") == 0) {
                        in_after_each = true;
                    } else if (strcmp(directive, "end") == 0) {
                        // Explicit end, already handled by closing blocks above
                    }

                    free(directive);
                }

                // Add directive line to output
                doc_add_line(doc, line);
            }
            // Check for @ segment/reference
            else if (*lp == '@') {
                lp++;
                char *name = parse_identifier(&lp);

                if (name) {
                    skip_ws(&lp);

                    if (strcmp(name, "label") == 0) {
                        // Start segment definition
                        if (in_segment) {
                            // Close previous segment
                            doc_add_segment(doc, current_segment_name, segment_content.data);
                            free(current_segment_name);
                            strbuf_free(&segment_content);
                            strbuf_init(&segment_content);
                        }

                        char *label_name = parse_identifier(&lp);
                        if (label_name) {
                            current_segment_name = label_name;
                            in_segment = true;
                        }
                        doc_add_line(doc, line);
                    } else if (strcmp(name, "end") == 0) {
                        // End segment
                        if (in_segment) {
                            doc_add_segment(doc, current_segment_name, segment_content.data);
                            free(current_segment_name);
                            current_segment_name = NULL;
                            strbuf_free(&segment_content);
                            strbuf_init(&segment_content);
                            in_segment = false;
                        }
                        doc_add_line(doc, line);
                    } else if (strcmp(name, "cassilda") == 0 && *lp == ':') {
                        // Reference
                        lp++; // Skip ':'
                        int n_labels = 0;
                        char **labels = parse_label_list(&lp, &n_labels);

                        if (n_labels > 0) {
                            doc_add_reference(doc, doc->n_lines, labels, n_labels);
                        }
                        doc_add_line(doc, line);
                    } else {
                        // Unknown @ directive, treat as content
                        if (in_segment) {
                            strbuf_append(&segment_content, content);
                            strbuf_append_char(&segment_content, '\n');
                        }
                        doc_add_line(doc, line);
                    }

                    free(name);
                } else {
                    // Bare @, treat as content
                    if (in_segment) {
                        strbuf_append(&segment_content, content);
                        strbuf_append_char(&segment_content, '\n');
                    }
                    doc_add_line(doc, line);
                }
            }
            // Regular content
            else {
                if (in_before_each || in_after_each) {
                    strbuf_append(&block_content, content);
                    strbuf_append_char(&block_content, '\n');
                } else if (in_segment) {
                    strbuf_append(&segment_content, content);
                    strbuf_append_char(&segment_content, '\n');
                }
                doc_add_line(doc, line);
            }
        }
        // Skip rendered output lines (they will be regenerated)

        free(line);

        if (!*line_end) break;
        p = line_end + 1;
        line_start = p;

        // Don't process empty trailing line
        if (*line_start == '\0') break;

        line_number++;
    }

    // Close any open blocks
    if (in_before_each) {
        free(doc->config.before_each);
        doc->config.before_each = strbuf_detach(&block_content);
    } else if (in_after_each) {
        free(doc->config.after_each);
        doc->config.after_each = strbuf_detach(&block_content);
    }
    strbuf_free(&block_content);

    // Close any open segment
    if (in_segment) {
        doc_add_segment(doc, current_segment_name, segment_content.data);
        free(current_segment_name);
    }
    strbuf_free(&segment_content);

    return doc;
}

// ============================================================================
// Segment Lookup and Rendering
// ============================================================================

CassildaSegment *cassilda_find_segment(CassildaDocument *doc, const char *name) {
    for (int i = 0; i < doc->n_segments; i++) {
        if (strcmp(doc->segments[i].name, name) == 0) {
            return &doc->segments[i];
        }
    }
    return NULL;
}

char *cassilda_render_segment(CassildaDocument *doc, const char *name, char *error_msg,
                              int error_size) {
    CassildaSegment *seg = cassilda_find_segment(doc, name);

    // If not found locally, try library lookup
    char *library_content = NULL;
    char *library_before_each = NULL;
    char *library_after_each = NULL;
    if (!seg && doc->library_ctx) {
        LibraryContext *lib_ctx = (LibraryContext *)doc->library_ctx;
        const char *source_file = NULL;
        library_content = library_lookup_label(lib_ctx, name, &source_file, &library_before_each,
                                               &library_after_each, error_msg, error_size);

        if (!library_content) {
            // Not in library either - provide helpful error with suggestions
            int n_suggestions;
            char **suggestions = library_suggest_labels(lib_ctx, name, &n_suggestions);

            if (n_suggestions > 0) {
                snprintf(error_msg, error_size, "Label '%s' not found. Did you mean: %s?", name,
                         suggestions[0]);
                library_free_suggestions(suggestions, n_suggestions);
            } else {
                snprintf(error_msg, error_size, "Label '%s' not found in file or library", name);
            }
            return NULL;
        }
        // library_content now contains the label content, will use it below
    }

    if (!seg && !library_content) {
        snprintf(error_msg, error_size, "Segment not found: %s", name);
        return NULL;
    }

    // Build full Hyades source: before_each + content + after_each
    // Priority: library before_each, then document before_each
    StringBuf source;
    strbuf_init(&source);

    // 1. Library's before_each (if from library)
    if (library_before_each && library_before_each[0]) {
        strbuf_append(&source, library_before_each);
        if (source.data[source.len - 1] != '\n') {
            strbuf_append_char(&source, '\n');
        }
    }

    // 2. Document's before_each
    if (doc->config.before_each && doc->config.before_each[0]) {
        strbuf_append(&source, doc->config.before_each);
        if (source.data[source.len - 1] != '\n') {
            strbuf_append_char(&source, '\n');
        }
    }

    // 3. Content
    const char *content_to_render = library_content ? library_content : seg->content;
    strbuf_append(&source, content_to_render);

    // 4. Document's after_each
    if (doc->config.after_each && doc->config.after_each[0]) {
        if (source.len > 0 && source.data[source.len - 1] != '\n') {
            strbuf_append_char(&source, '\n');
        }
        strbuf_append(&source, doc->config.after_each);
    }

    // 5. Library's after_each (if from library)
    if (library_after_each && library_after_each[0]) {
        if (source.len > 0 && source.data[source.len - 1] != '\n') {
            strbuf_append_char(&source, '\n');
        }
        strbuf_append(&source, library_after_each);
    }

    // Clear state from previous segment renders
    // This is critical for verbatim content - the store gets locked and must be cleared
    verbatim_store_clear();

    // Render with Hyades
    CompOptions opt = default_options();
    opt.width = doc->config.default_width;
    ParseError err = {0};

    char *rendered = compose_document(source.data, &opt, &err);
    strbuf_free(&source);

    // Free library resources if we used them
    if (library_content) {
        free(library_content);
    }
    if (library_before_each) {
        free(library_before_each);
    }
    if (library_after_each) {
        free(library_after_each);
    }

    if (!rendered) {
        snprintf(error_msg, error_size, "Hyades render failed for segment '%s': %s", name,
                 err.message[0] ? err.message : "unknown error");
        return NULL;
    }

    return rendered;
}

// ============================================================================
// Document Processing
// ============================================================================

// Format rendered output with target_prefix + NBSP marker
static char *format_rendered_output(const char *rendered, CassildaConfig *config) {
    StringBuf out;
    strbuf_init(&out);

    const char *p = rendered;
    while (*p) {
        // Start of line: target_prefix + NBSP + content
        // The NBSP serves as both a marker and the natural space after the comment prefix
        // This preserves visual alignment (e.g., "// @cassilda:" followed by "// content")
        if (config->target_prefix[0]) {
            strbuf_append(&out, config->target_prefix);
        }
        strbuf_append(&out, NBSP);

        // Copy line content
        while (*p && *p != '\n') {
            strbuf_append_char(&out, *p++);
        }

        strbuf_append_char(&out, '\n');

        if (*p == '\n') {
            p++;
            // Don't create empty line for trailing newline
            if (*p == '\0') break;
        }
    }

    return strbuf_detach(&out);
}

char *cassilda_process(CassildaDocument *doc, char *error_msg, int error_size) {
    StringBuf out;
    strbuf_init(&out);

    int ref_idx = 0;

    for (int i = 0; i < doc->n_lines; i++) {
        // Add newline before line (except first)
        if (i > 0) {
            strbuf_append_char(&out, '\n');
        }

        // Output the line
        strbuf_append(&out, doc->lines[i]);

        // Check if there's a reference at this line
        while (ref_idx < doc->n_references && doc->references[ref_idx].line_number == i) {
            CassildaReference *ref = &doc->references[ref_idx];

            // Render each label
            for (int j = 0; j < ref->n_labels; j++) {
                char *rendered =
                    cassilda_render_segment(doc, ref->labels[j], error_msg, error_size);
                if (!rendered) {
                    strbuf_free(&out);
                    return NULL;
                }

                // Format with comment char and prefixes
                char *formatted = format_rendered_output(rendered, &doc->config);
                free(rendered);

                // Rendered output starts on new line
                strbuf_append_char(&out, '\n');
                // Append formatted but strip its trailing newline
                size_t flen = strlen(formatted);
                if (flen > 0 && formatted[flen - 1] == '\n') {
                    strbuf_append_n(&out, formatted, flen - 1);
                } else {
                    strbuf_append(&out, formatted);
                }
                free(formatted);
            }

            ref_idx++;
        }
    }

    // Add final newline (files should end with newline)
    strbuf_append_char(&out, '\n');

    return strbuf_detach(&out);
}

char *cassilda_process_with_library(CassildaDocument *doc, void *lib_ctx, char *error_msg,
                                    int error_size) {
    doc->library_ctx = lib_ctx;

    // Apply extension-based target_prefix and target_suffix from .cassilda.json
    if (lib_ctx && doc->filename) {
        LibraryContext *ctx = (LibraryContext *)lib_ctx;

        // Look up target_prefix for this file's extension
        const char *ext_prefix = library_lookup_target_prefix(&ctx->config, doc->filename);
        if (ext_prefix) {
            // Override the target_prefix with the extension-based one
            free(doc->config.target_prefix);
            doc->config.target_prefix = strdup(ext_prefix);
        }
    }

    char *result = cassilda_process(doc, error_msg, error_size);
    doc->library_ctx = NULL; // Clear after processing
    return result;
}

char *cassilda_run(const char *input, char *error_msg, int error_size) {
    // Check for #!clear directive - if present, just clean rendered output
    if (has_clear_directive(input)) {
        return cassilda_clean(input, NULL);
    }

    CassildaDocument *doc = cassilda_parse(input, NULL, NULL, error_msg, error_size);
    if (!doc) return NULL;

    char *result = cassilda_process(doc, error_msg, error_size);
    cassilda_free(doc);

    return result;
}

char *cassilda_run_with_library(const char *input, void *lib_ctx, char *error_msg, int error_size) {
    // Check for #!clear directive - if present, just clean rendered output
    if (has_clear_directive(input)) {
        // Try to get target_prefix from library config if available
        const char *target_prefix = NULL;
        // Note: we don't have filename here, so we can't look up target_prefix from config
        // The clean will work with NULL prefix (checks for NBSP anywhere)
        return cassilda_clean(input, target_prefix);
    }

    CassildaDocument *doc = cassilda_parse(input, NULL, NULL, error_msg, error_size);
    if (!doc) return NULL;

    char *result = cassilda_process_with_library(doc, lib_ctx, error_msg, error_size);
    cassilda_free(doc);

    return result;
}

bool cassilda_check(const char *input, char *error_msg, int error_size) {
    char *processed = cassilda_run(input, error_msg, error_size);
    if (!processed) return false;

    bool match = (strcmp(input, processed) == 0);
    free(processed);

    return match;
}

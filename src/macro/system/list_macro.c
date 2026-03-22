// list_macro.c - List macro system for Hyades
// Expands \list{...} syntax with Markdown-style list items

#include "list_macro.h"
#include "diagnostics/diagnostics.h"
#include "utils/strbuf.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Data Structures
// ============================================================================

// List item representation
typedef struct {
    char *content;     // Item content (after the bullet point)
    int level;         // Nesting level (0 = top level, 1 = first nest, etc.)
    int indent_column; // Column where the bullet point appears
} ListItem;

// List options
typedef struct {
    int indent;   // Spaces per indentation level (default 2)
    char *point1; // Bullet point for level 0 (default "-")
    char *point2; // Bullet point for level 1 (default "-")
    char *point3; // Bullet point for level 2+ (default "-")
    int ragged;
} ListOptions;

// List structure
typedef struct {
    ListItem *items;
    int n_items;
    int capacity;
    ListOptions options;
} List;

// ============================================================================
// Memory Management
// ============================================================================

static void list_item_free(ListItem *item) {
    if (item) {
        free(item->content);
    }
}

static List *list_new(void) {
    List *list = malloc(sizeof(List));
    list->items = NULL;
    list->n_items = 0;
    list->capacity = 0;

    // Default options
    list->options.indent = 2;
    list->options.point1 = strdup("-");
    list->options.point2 = strdup("-");
    list->options.point3 = strdup("-");

    return list;
}

static void list_add_item(List *list, const char *content, int level, int indent_column) {
    if (list->n_items >= list->capacity) {
        list->capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        list->items = realloc(list->items, list->capacity * sizeof(ListItem));
    }

    // Trim leading/trailing whitespace from content
    const char *start = content;
    const char *end = content + strlen(content);

    while (start < end && (*start == ' ' || *start == '\t' || *start == '\n')) {
        start++;
    }
    while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t' || *(end - 1) == '\n')) {
        end--;
    }

    size_t len = end - start;
    char *trimmed = malloc(len + 1);
    if (len > 0) {
        memcpy(trimmed, start, len);
    }
    trimmed[len] = '\0';

    list->items[list->n_items].content = trimmed;
    list->items[list->n_items].level = level;
    list->items[list->n_items].indent_column = indent_column;
    list->n_items++;
}

static void list_free(List *list) {
    if (!list) return;

    for (int i = 0; i < list->n_items; i++) {
        list_item_free(&list->items[i]);
    }
    free(list->items);

    free(list->options.point1);
    free(list->options.point2);
    free(list->options.point3);

    free(list);
}

// ============================================================================
// Parsing Helpers
// ============================================================================

// Skip whitespace (including newlines)
static void skip_ws(const char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') {
        (*p)++;
    }
}

// Skip whitespace (not newlines)
static void skip_ws_inline(const char **p) {
    while (**p == ' ' || **p == '\t') {
        (*p)++;
    }
}

// Parse an identifier (letters, digits, underscore, hyphen)
static char *parse_identifier(const char **p) {
    skip_ws_inline(p);
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

// Parse a number
static int parse_number(const char **p, bool *success) {
    skip_ws_inline(p);
    *success = false;

    if (!isdigit((unsigned char)**p)) return 0;

    int value = 0;
    while (isdigit((unsigned char)**p)) {
        value = value * 10 + (**p - '0');
        (*p)++;
    }

    *success = true;
    return value;
}

// Parse a string value (for bullet points)
static char *parse_string_value(const char **p) {
    skip_ws_inline(p);
    const char *start = *p;

    // Parse until comma, closing bracket, or end
    while (**p && **p != ',' && **p != ']' && **p != ' ' && **p != '\t' && **p != '\n') {
        (*p)++;
    }

    if (*p == start) return NULL;

    size_t len = *p - start;
    char *value = malloc(len + 1);
    memcpy(value, start, len);
    value[len] = '\0';
    return value;
}

// Parse list options [indent=2, point1:-, point2:-, point3:-]
static ListOptions parse_list_options(const char **p) {
    ListOptions opts;
    opts.indent = 2; // default
    opts.point1 = strdup("-");
    opts.point2 = strdup("-");
    opts.point3 = strdup("-");
    opts.ragged = 1;

    skip_ws(p);
    if (**p != '[') return opts;

    (*p)++; // Skip '['

    while (**p && **p != ']') {
        skip_ws(p);
        if (**p == ']') break;

        // Parse key
        char *key = parse_identifier(p);
        if (!key) {
            // Skip to next comma or closing bracket
            while (**p && **p != ',' && **p != ']') (*p)++;
            if (**p == ',') (*p)++;
            continue;
        }

        skip_ws_inline(p);
        if (**p != ':' && **p != '=') {
            free(key);
            while (**p && **p != ',' && **p != ']') (*p)++;
            if (**p == ',') (*p)++;
            continue;
        }
        (*p)++; // Skip ':' or '='

        skip_ws_inline(p);

        // Parse value based on key
        if (strcmp(key, "indent") == 0) {
            bool success;
            int value = parse_number(p, &success);
            if (success) {
                opts.indent = value;
            }
        } else if (strcmp(key, "point1") == 0) {
            char *value = parse_string_value(p);
            if (value) {
                free(opts.point1);
                opts.point1 = value;
            }
        } else if (strcmp(key, "point2") == 0) {
            char *value = parse_string_value(p);
            if (value) {
                free(opts.point2);
                opts.point2 = value;
            }
        } else if (strcmp(key, "point3") == 0) {
            char *value = parse_string_value(p);
            if (value) {
                free(opts.point3);
                opts.point3 = value;
            }
        } else if (strcmp(key, "ragged") == 0) {
            bool success;
            int value = parse_number(p, &success);
            if (success) {
                opts.ragged = value;
            }
        }

        free(key);

        skip_ws(p);
        if (**p == ',') (*p)++;
    }

    if (**p == ']') (*p)++;

    return opts;
}

// Parse braced content {...}
static char *parse_braced_content(const char **p) {
    skip_ws(p);
    if (**p != '{') return NULL;
    (*p)++; // Skip '{'

    const char *start = *p;
    int depth = 1;

    while (**p && depth > 0) {
        if (**p == '{')
            depth++;
        else if (**p == '}')
            depth--;
        if (depth > 0) (*p)++;
    }

    size_t len = *p - start;
    char *content = malloc(len + 1);
    memcpy(content, start, len);
    content[len] = '\0';

    if (**p == '}') (*p)++;

    return content;
}

// ============================================================================
// Content Parsing
// ============================================================================

// Parse list content - detects nesting and supports multi-line items
static List *parse_list_content(const char *content, ListOptions options, char *error_msg,
                                int error_size) {
    List *list = list_new();
    list->options = options;

    // Duplicate options strings since we'll be freeing them
    list->options.point1 = strdup(options.point1);
    list->options.point2 = strdup(options.point2);
    list->options.point3 = strdup(options.point3);

    // First pass: find all bullet positions to determine minimum indentation
    int min_indent = -1;
    int first_bullet_indent = -1;
    int min_indent_rest = -1; // min indent of all bullets except the first
    const char *p = content;

    while (*p) {
        // Skip leading whitespace and count column position
        int column = 0;
        while (*p == ' ' || *p == '\t') {
            if (*p == ' ') {
                column++;
            } else if (*p == '\t') {
                column += 4; // Tab = 4 spaces
            }
            p++;
        }

        // Check if this line has a bullet point
        if (*p == '-' || *p == '*' || *p == '+') {
            if (min_indent == -1 || column < min_indent) {
                min_indent = column;
            }
            if (first_bullet_indent == -1) {
                first_bullet_indent = column;
            } else {
                if (min_indent_rest == -1 || column < min_indent_rest) {
                    min_indent_rest = column;
                }
            }
        }

        // Skip to next line
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }

    // If no bullets found, min_indent is 0
    if (min_indent == -1) min_indent = 0;

    // Detect strip_tex_comments artifact: when \list{%\n    - items}, the %
    // stripping consumes the newline AND leading whitespace of the next line,
    // making the first bullet appear at column 0 while all others are indented.
    // Fix: if first bullet is at 0 and all remaining bullets are at a higher
    // common indent, use that as the baseline instead.
    if (first_bullet_indent == 0 && min_indent_rest > 0) {
        min_indent = 0; // Keep 0 — the first bullet IS at 0 after stripping,
                        // and remaining bullets' relative indent should be
                        // computed from min_indent_rest for correct nesting.
        // Actually: treat the first bullet as if it were at min_indent_rest
        // by using min_indent_rest as the baseline for ALL bullets.
        min_indent = min_indent_rest;
    }

    // Second pass: parse items with normalized indentation
    // Support multi-line content - lines without bullets belong to previous item
    p = content;
    StrBuf current_item_content;
    strbuf_init(&current_item_content);
    int current_level = -1;
    int current_column = -1;
    bool in_item = false;

    while (*p) {
        // Save line start for multi-line detection
        const char *line_start = p;

        // Skip leading whitespace and count column position
        int column = 0;
        while (*p == ' ' || *p == '\t') {
            if (*p == ' ') {
                column++;
            } else if (*p == '\t') {
                column += 4; // Tab = 4 spaces
            }
            p++;
        }

        // Check if this line starts with a bullet point
        if (*p == '-' || *p == '*' || *p == '+') {
            // Save previous item if exists
            if (in_item) {
                char *item_str = strbuf_detach(&current_item_content);
                list_add_item(list, item_str, current_level, current_column);
                free(item_str);
                strbuf_init(&current_item_content);
            }

            // Start new item
            p++; // Skip bullet

            // Skip whitespace after bullet
            while (*p == ' ' || *p == '\t') p++;

            // Determine nesting level based on relative indentation
            int relative_indent = column - min_indent;
            if (relative_indent < 0) relative_indent = 0;
            current_level = relative_indent / options.indent;
            if (current_level > 2) current_level = 2;
            current_column = column;
            in_item = true;

            // Extract content until end of line
            const char *content_start = p;
            while (*p && *p != '\n') p++;

            strbuf_append_n(&current_item_content, content_start, p - content_start);
        } else if (in_item && *p && *p != '\n') {
            // This is a continuation line - belongs to current item
            // Include the full line starting from line_start to preserve indentation
            const char *content_start = line_start;
            while (*p && *p != '\n') p++;

            // Add newline from previous line, then this line's content
            strbuf_putc(&current_item_content, '\n');
            strbuf_append_n(&current_item_content, content_start, p - content_start);
        } else if (in_item && (*p == '\n' || *p == '\0')) {
            // Empty line or end - preserve it as part of content
            if (*p == '\n') {
                strbuf_putc(&current_item_content, '\n');
            }
        } else if (!in_item && *p && *p != '\n') {
            // Non-bullet, non-whitespace content before any item started
            // Skip to end of line to avoid infinite loop
            while (*p && *p != '\n') p++;
        }

        // Move to next line
        if (*p == '\n') p++;
    }

    // Save last item if exists
    if (in_item) {
        char *item_str = strbuf_detach(&current_item_content);
        list_add_item(list, item_str, current_level, current_column);
        free(item_str);
    } else {
        strbuf_free(&current_item_content);
    }

    if (list->n_items == 0) {
        snprintf(error_msg, error_size, "Empty list - no items found");
        list_free(list);
        return NULL;
    }

    return list;
}

// ============================================================================
// Code Generation
// ============================================================================

// USER IMPLEMENTATION HOOK:
// This is where you should implement your custom list rendering logic.
// The function below generates a placeholder implementation that uses
// basic indentation and bullet points. You can replace this with your
// own rendering logic that uses the \indent stdlib macro or any other
// Hyades commands you prefer.
//
// The ListItem structure provides:
// - item->content: The text content of the list item
// - item->level: The nesting level (0, 1, 2)
// - item->indent_column: The column where the bullet appeared
//
// The ListOptions structure provides:
// - list->options.indent: Spaces per indentation level
// - list->options.point1/2/3: Bullet point characters for each level
//
static char *expand_list(const List *list, char *error_msg, int error_size) {
    if (!list || list->n_items == 0) {
        snprintf(error_msg, error_size, "Empty list");
        return NULL;
    }

    StrBuf sb;
    strbuf_init(&sb);

    if (diag_is_enabled(DIAG_SYSTEM)) {
        diag_log(DIAG_SYSTEM, 0, "\\list: %d items, indent=%d", list->n_items,
                 list->options.indent);
    }

    // ========================================================================
    // IMPLEMENTATION
    // ========================================================================
    // Uses hbox layout to ensure wrapped lines align under the content,
    // not under the bullet point. Structure:
    //   \child{
    //     \begin{hbox}
    //       \child[N]{}           % indentation (N = level * indent)
    //       \child[1]{bullet}     % bullet character (1 char wide)
    //       \child[1]{}           % spacing after bullet
    //       \child[auto]{content} % content (auto width for proper wrapping)
    //     \end{hbox}
    //   }
    // ========================================================================

    strbuf_append(&sb, "\\begin{vbox}\n");

    for (int i = 0; i < list->n_items; i++) {
        ListItem *item = &list->items[i];

        // Select bullet point based on level
        const char *bullet;
        switch (item->level) {
        case 0: bullet = list->options.point1; break;
        case 1: bullet = list->options.point2; break;
        default: bullet = list->options.point3; break;
        }

        // Calculate indentation
        int indent_amount = item->level * list->options.indent;

        // Generate list item using hbox layout
        strbuf_append(&sb, "  \\child{\n");
        strbuf_append(&sb, "    \\begin{hbox}\n");

        // Indentation (only if needed)
        if (indent_amount > 0) {
            strbuf_printf(&sb, "      \\child[%d]{}\n", indent_amount);
        }

        // Bullet character (1 character wide)
        strbuf_append(&sb, "      \\child[1]{");
        strbuf_append(&sb, bullet);
        strbuf_append(&sb, "}\n");

        // Spacing after bullet (1 character)
        strbuf_append(&sb, "      \\child[1]{}\n");

        // Content (auto width for proper wrapping)
        strbuf_append(&sb, "      \\child[auto]{");
        if (list->options.ragged) {
            strbuf_append(&sb, "\\raggedright{");
        }
        strbuf_append(&sb, item->content);
        if (list->options.ragged) {
            strbuf_append(&sb, "}");
        }
        strbuf_append(&sb, "}\n");

        strbuf_append(&sb, "    \\end{hbox}\n");
        strbuf_append(&sb, "  }\n");
    }

    strbuf_append(&sb, "\\end{vbox}\n");

    // ========================================================================
    // END IMPLEMENTATION
    // ========================================================================

    return strbuf_detach(&sb);
}

// ============================================================================
// Public API
// ============================================================================

bool is_list_macro(const char *input) {
    if (!input) return false;
    while (*input == ' ' || *input == '\t' || *input == '\n') input++;
    return strncmp(input, "\\list", 5) == 0;
}

char *list_macro_expand(const char *input, int *end_pos, char *error_msg, int error_size) {
    const char *p = input;

    skip_ws(&p);

    if (strncmp(p, "\\list", 5) != 0) {
        snprintf(error_msg, error_size, "Expected \\list");
        return NULL;
    }
    p += 5;

    // Parse optional parameters [indent=2, point1:-, ...]
    ListOptions opts = parse_list_options(&p);

    // Parse content
    char *content = parse_braced_content(&p);
    if (!content) {
        snprintf(error_msg, error_size, "Expected {content} after \\list");
        free(opts.point1);
        free(opts.point2);
        free(opts.point3);
        return NULL;
    }

    if (end_pos) *end_pos = (int)(p - input);

    // Parse into list structure
    List *list = parse_list_content(content, opts, error_msg, error_size);
    free(content);

    // Free options (they're duplicated inside parse_list_content)
    free(opts.point1);
    free(opts.point2);
    free(opts.point3);

    if (!list) return NULL;

    // Generate output
    char *result = expand_list(list, error_msg, error_size);
    list_free(list);

    return result;
}

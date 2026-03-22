// macro.c - User-defined macro system for Hyades

#include "macro.h"
#include "diagnostics/diagnostics.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Macro Hygiene - Unique Identifier Generation
// ============================================================================

// Global counter for generating unique identifiers per macro expansion
// This solves the problem of \measure/\recall name collisions when the same
// macro is called multiple times
static int g_macro_hygiene_counter = 1; // Start at 1; ID 0 is reserved for top-level

// ============================================================================
// String Buffer Helper
// ============================================================================

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} StrBuf;

static void strbuf_init(StrBuf *sb) {
    sb->capacity = 256;
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

static void strbuf_append(StrBuf *sb, const char *str, size_t len) {
    strbuf_ensure(sb, len);
    memcpy(sb->data + sb->len, str, len);
    sb->len += len;
    sb->data[sb->len] = '\0';
}

static void strbuf_append_str(StrBuf *sb, const char *str) {
    strbuf_append(sb, str, strlen(str));
}

static void strbuf_append_char(StrBuf *sb, char c) {
    strbuf_ensure(sb, 1);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

static char *strbuf_detach(StrBuf *sb) {
    char *result = sb->data;
    sb->data = NULL;
    sb->len = 0;
    sb->capacity = 0;
    return result;
}

// ============================================================================
// Compound Value Field Extraction
// ============================================================================
// Extract a field from a compound value like {t:2, b:1, l:2}
// Returns the field value as a newly allocated string, or "0" if not found

static char *extract_compound_field(const char *compound, const char *field) {
    if (!compound || !field) return strdup("0");

    // Skip leading whitespace
    while (*compound == ' ' || *compound == '\t') compound++;

    // Must start with {
    if (*compound != '{') return strdup("0");
    compound++;

    size_t field_len = strlen(field);

    // Parse key:value pairs
    while (*compound && *compound != '}') {
        // Skip whitespace
        while (*compound == ' ' || *compound == '\t' || *compound == '\n') compound++;
        if (*compound == '}' || *compound == '\0') break;

        // Parse key
        const char *key_start = compound;
        while (*compound && *compound != ':' && *compound != ',' && *compound != '}' &&
               *compound != ' ' && *compound != '\t') {
            compound++;
        }
        size_t key_len = compound - key_start;

        // Skip whitespace and colon
        while (*compound == ' ' || *compound == '\t') compound++;
        if (*compound != ':') {
            // No value, skip to next
            while (*compound && *compound != ',' && *compound != '}') compound++;
            if (*compound == ',') compound++;
            continue;
        }
        compound++; // Skip ':'

        // Skip whitespace
        while (*compound == ' ' || *compound == '\t') compound++;

        // Parse value
        const char *val_start = compound;
        while (*compound && *compound != ',' && *compound != '}' && *compound != ' ' &&
               *compound != '\t' && *compound != '\n') {
            compound++;
        }
        size_t val_len = compound - val_start;

        // Check if this is the field we're looking for
        if (key_len == field_len && strncmp(key_start, field, field_len) == 0) {
            char *result = malloc(val_len + 1);
            memcpy(result, val_start, val_len);
            result[val_len] = '\0';
            return result;
        }

        // Skip to next pair
        while (*compound == ' ' || *compound == '\t') compound++;
        if (*compound == ',') compound++;
    }

    // Field not found, return "0"
    return strdup("0");
}

// ============================================================================
// Registry Management
// ============================================================================

MacroRegistry *macro_registry_new(void) {
    MacroRegistry *reg = malloc(sizeof(MacroRegistry));
    reg->macros = NULL;
    reg->n_macros = 0;
    reg->capacity = 0;
    return reg;
}

static void macro_free(Macro *m) {
    if (!m) return;
    free(m->name);
    for (int i = 0; i < m->n_args; i++) {
        free(m->args[i].name);
        free(m->args[i].default_value);
    }
    free(m->args);
    free(m->body);
}

void macro_registry_free(MacroRegistry *reg) {
    if (!reg) return;
    for (int i = 0; i < reg->n_macros; i++) {
        macro_free(&reg->macros[i]);
    }
    free(reg->macros);
    free(reg);
}

bool macro_registry_add(MacroRegistry *reg, Macro *macro) {
    if (!reg || !macro) return false;

    // Check for duplicate
    for (int i = 0; i < reg->n_macros; i++) {
        if (strcmp(reg->macros[i].name, macro->name) == 0) {
            // Replace existing
            macro_free(&reg->macros[i]);
            reg->macros[i] = *macro;
            return true;
        }
    }

    // Add new
    if (reg->n_macros >= reg->capacity) {
        int new_cap = reg->capacity == 0 ? 8 : reg->capacity * 2;
        reg->macros = realloc(reg->macros, new_cap * sizeof(Macro));
        reg->capacity = new_cap;
    }

    reg->macros[reg->n_macros++] = *macro;
    return true;
}

Macro *macro_registry_find(MacroRegistry *reg, const char *name) {
    if (!reg || !name) return NULL;
    for (int i = 0; i < reg->n_macros; i++) {
        if (strcmp(reg->macros[i].name, name) == 0) {
            return &reg->macros[i];
        }
    }
    return NULL;
}

// ============================================================================
// Parsing Helpers
// ============================================================================

static void skip_ws(const char **p) {
    while (1) {
        // Skip whitespace
        while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') {
            (*p)++;
        }
        // Skip % comments (to end of line)
        if (**p == '%') {
            while (**p && **p != '\n') {
                (*p)++;
            }
            continue; // Check for more whitespace/comments after the newline
        }
        break;
    }
}

static char *parse_identifier(const char **p) {
    const char *start = *p;
    while (isalnum((unsigned char)**p) || **p == '_') {
        (*p)++;
    }
    if (*p == start) return NULL;

    size_t len = *p - start;
    char *id = malloc(len + 1);
    memcpy(id, start, len);
    id[len] = '\0';
    return id;
}

// Parse content within braces, handling nested braces
// Starts after the opening brace, returns content, advances past closing brace
static char *parse_brace_content(const char **p, char *error_msg, int error_size) {
    StrBuf content;
    strbuf_init(&content);

    int depth = 1;
    while (**p && depth > 0) {
        if (**p == '%') {
            // TeX-style line comment: skip everything until (and including) the newline
            // This allows formatting macros across multiple lines without adding extra newlines
            (*p)++;
            while (**p && **p != '\n') {
                (*p)++;
            }
            if (**p == '\n') {
                (*p)++; // Skip the newline itself
            }
            // Also skip leading whitespace on the next line (LaTeX behavior)
            while (**p == ' ' || **p == '\t') {
                (*p)++;
            }
        } else if (**p == '\\' && strncmp(*p + 1, "verb", 4) == 0) {
            // \verb block - copy verbatim without counting braces
            // Format: \verb#...# where # is the delimiter character
            strbuf_append(&content, "\\verb", 5);
            (*p) += 5;
            if (**p) {
                char delim = **p;
                strbuf_append_char(&content, delim);
                (*p)++;
                // Copy until closing delimiter
                while (**p && **p != delim) {
                    strbuf_append_char(&content, **p);
                    (*p)++;
                }
                if (**p == delim) {
                    strbuf_append_char(&content, delim);
                    (*p)++;
                }
            }
        } else if (**p == '{') {
            depth++;
            strbuf_append_char(&content, '{');
            (*p)++;
        } else if (**p == '}') {
            depth--;
            if (depth > 0) {
                strbuf_append_char(&content, '}');
            }
            (*p)++;
        } else if (**p == '\\' && (*p)[1] == '{') {
            // Escaped brace
            strbuf_append(&content, "\\{", 2);
            (*p) += 2;
        } else if (**p == '\\' && (*p)[1] == '}') {
            // Escaped brace
            strbuf_append(&content, "\\}", 2);
            (*p) += 2;
        } else if (**p == '\\' && (*p)[1] == '%') {
            // Escaped percent: literal % character
            strbuf_append_char(&content, '%');
            (*p) += 2;
        } else {
            strbuf_append_char(&content, **p);
            (*p)++;
        }
    }

    if (depth != 0) {
        snprintf(error_msg, error_size, "Unclosed brace in macro");
        strbuf_free(&content);
        return NULL;
    }

    return strbuf_detach(&content);
}

// ============================================================================
// Macro Definition Parsing
// ============================================================================

// Parse: \macro<\name[opt1=def1, opt2]{req1}{req2}>{body}
Macro *macro_parse_definition(const char *input, int *end_pos, char *error_msg, int error_size) {
    const char *p = input;

    // Expect \macro
    if (strncmp(p, "\\macro", 6) != 0) {
        snprintf(error_msg, error_size, "Expected \\macro");
        return NULL;
    }
    p += 6;

    skip_ws(&p);

    // Expect <
    if (*p != '<') {
        snprintf(error_msg, error_size, "Expected '<' after \\macro");
        return NULL;
    }
    p++;

    skip_ws(&p);

    // Expect \name
    if (*p != '\\') {
        snprintf(error_msg, error_size, "Expected '\\' for macro name");
        return NULL;
    }
    p++;

    char *name = parse_identifier(&p);
    if (!name) {
        snprintf(error_msg, error_size, "Expected macro name after '\\'");
        return NULL;
    }

    // Parse arguments
    MacroArg *args = NULL;
    int n_args = 0;
    int args_capacity = 0;
    int n_optional = 0;
    int n_required = 0;

    skip_ws(&p);

    // Optional arguments in [...]
    if (*p == '[') {
        p++; // Skip '['

        while (*p && *p != ']') {
            skip_ws(&p);

            if (*p == ']') break;
            if (*p == ',') {
                p++;
                continue;
            }

            // Parse argument name
            char *arg_name = parse_identifier(&p);
            if (!arg_name) {
                snprintf(error_msg, error_size, "Expected argument name in optional list");
                free(name);
                for (int i = 0; i < n_args; i++) {
                    free(args[i].name);
                    free(args[i].default_value);
                }
                free(args);
                return NULL;
            }

            skip_ws(&p);

            // Check for default value
            char *default_val = NULL;
            if (*p == '=') {
                p++; // Skip '='
                skip_ws(&p);

                // Parse default value (up to , or ])
                const char *val_start = p;
                int brace_depth = 0;
                while (*p && !(*p == ',' && brace_depth == 0) && !(*p == ']' && brace_depth == 0)) {
                    if (*p == '{')
                        brace_depth++;
                    else if (*p == '}')
                        brace_depth--;
                    p++;
                }
                size_t val_len = p - val_start;
                // Trim trailing whitespace
                while (val_len > 0 &&
                       (val_start[val_len - 1] == ' ' || val_start[val_len - 1] == '\t')) {
                    val_len--;
                }
                default_val = malloc(val_len + 1);
                memcpy(default_val, val_start, val_len);
                default_val[val_len] = '\0';
            }

            // Add argument
            if (n_args >= args_capacity) {
                args_capacity = args_capacity == 0 ? 4 : args_capacity * 2;
                args = realloc(args, args_capacity * sizeof(MacroArg));
            }
            args[n_args].name = arg_name;
            args[n_args].default_value = default_val; // NULL means no default (but still optional)
            args[n_args].is_optional = true;
            n_args++;
            n_optional++;

            skip_ws(&p);
            if (*p == ',') p++;
        }

        if (*p != ']') {
            snprintf(error_msg, error_size, "Expected ']' to close optional arguments");
            free(name);
            for (int i = 0; i < n_args; i++) {
                free(args[i].name);
                free(args[i].default_value);
            }
            free(args);
            return NULL;
        }
        p++; // Skip ']'
    }

    skip_ws(&p);

    // Required arguments in {...}{...}
    while (*p == '{') {
        p++; // Skip '{'
        skip_ws(&p);

        char *arg_name = parse_identifier(&p);
        if (!arg_name) {
            snprintf(error_msg, error_size, "Expected argument name in required argument");
            free(name);
            for (int i = 0; i < n_args; i++) {
                free(args[i].name);
                free(args[i].default_value);
            }
            free(args);
            return NULL;
        }

        skip_ws(&p);

        if (*p != '}') {
            snprintf(error_msg, error_size, "Expected '}' after required argument name");
            free(arg_name);
            free(name);
            for (int i = 0; i < n_args; i++) {
                free(args[i].name);
                free(args[i].default_value);
            }
            free(args);
            return NULL;
        }
        p++; // Skip '}'

        // Add argument
        if (n_args >= args_capacity) {
            args_capacity = args_capacity == 0 ? 4 : args_capacity * 2;
            args = realloc(args, args_capacity * sizeof(MacroArg));
        }
        args[n_args].name = arg_name;
        args[n_args].default_value = NULL;
        args[n_args].is_optional = false;
        n_args++;
        n_required++;

        skip_ws(&p);
    }

    // Expect >
    if (*p != '>') {
        snprintf(error_msg, error_size, "Expected '>' to close macro signature");
        free(name);
        for (int i = 0; i < n_args; i++) {
            free(args[i].name);
            free(args[i].default_value);
        }
        free(args);
        return NULL;
    }
    p++;

    skip_ws(&p);

    // Expect { for body
    if (*p != '{') {
        snprintf(error_msg, error_size, "Expected '{' for macro body");
        free(name);
        for (int i = 0; i < n_args; i++) {
            free(args[i].name);
            free(args[i].default_value);
        }
        free(args);
        return NULL;
    }
    p++;

    // Parse body
    char *body = parse_brace_content(&p, error_msg, error_size);
    if (!body) {
        free(name);
        for (int i = 0; i < n_args; i++) {
            free(args[i].name);
            free(args[i].default_value);
        }
        free(args);
        return NULL;
    }

    // Create macro (use calloc to zero-initialize position fields)
    Macro *macro = calloc(1, sizeof(Macro));
    if (!macro) {
        free(name);
        free(body);
        for (int i = 0; i < n_args; i++) {
            free(args[i].name);
            free(args[i].default_value);
        }
        free(args);
        return NULL;
    }
    macro->name = name;
    macro->args = args;
    macro->n_args = n_args;
    macro->n_optional = n_optional;
    macro->n_required = n_required;
    macro->body = body;
    // Position fields (def_line, def_col, def_end_line, def_end_col) are 0 from calloc

    *end_pos = (int)(p - input);
    return macro;
}

// ============================================================================
// Macro Expansion
// ============================================================================

Macro *macro_match_call(MacroRegistry *reg, const char *input, int *name_end) {
    if (!reg || !input || *input != '\\') return NULL;

    const char *p = input + 1;
    const char *name_start = p;

    while (isalnum((unsigned char)*p) || *p == '_') {
        p++;
    }

    if (p == name_start) return NULL;

    size_t name_len = p - name_start;

    // Check against all registered macros
    for (int i = 0; i < reg->n_macros; i++) {
        if (strlen(reg->macros[i].name) == name_len &&
            strncmp(reg->macros[i].name, name_start, name_len) == 0) {
            *name_end = (int)(p - input);
            return &reg->macros[i];
        }
    }

    return NULL;
}

// Rewrite calc commands that use named storage to add unique prefix for hygiene
// This prevents name collisions when the same macro is called multiple times
// Handles: \measure<...>, \assign<...>, \recall<...>, \valueof<...>, \let<...>, \inc<...>, \dec<...>, \lambda<...>
//          \push<...>, \pop<...>, \peek<...>, \enqueue<...>, \dequeue<...>, \len<...>, \split<...>, \setelement<...>
//          \map_get<...>, \map_set<...>, \map_has<...>, \map_del<...>, \map_len<...>, \map_keys<...>
//          \at<...>, \set<...>, \invoke<...>, \addressof<...> (new CL syntax)
// Special: \ref<name> returns the hygienized name as a string (for passing locals by reference)
// NOTE: Computational lambda bodies (#{...}) are NOT hygienized - the VM handles scoping properly
static char *add_hygiene_prefix(const char *input, int unique_id) {
    if (!input) return NULL;

    StrBuf result;
    strbuf_init(&result);

    const char *p = input;

    while (*p) {
        // Skip computational lambda bodies (#{...}) - they're processed by the VM
        // which has proper scoping, so hygiene is not needed and would break things
        if (*p == '#' && *(p + 1) == '{') {
            strbuf_append(&result, p, 2); // Copy "#{"
            p += 2;
            int depth = 1;
            while (*p && depth > 0) {
                if (*p == '{')
                    depth++;
                else if (*p == '}')
                    depth--;
                if (depth > 0 || *p == '}') {
                    strbuf_append_char(&result, *p);
                    p++;
                }
            }
            continue;
        }

        // Handle \ref<name> specially - it returns the hygienized name as a string
        // This allows passing local variable names by reference to other macros
        if (strncmp(p, "\\ref<", 5) == 0) {
            p += 5; // skip \ref<

            // Parse the name
            const char *name_start = p;
            while (*p && *p != '>') p++;
            size_t name_len = p - name_start;

            if (name_len > 0) {
                // Check if global (ALL-CAPS) - no prefix needed
                bool is_global = true;
                for (const char *ch = name_start; ch < name_start + name_len; ch++) {
                    if (*ch >= 'a' && *ch <= 'z') {
                        is_global = false;
                        break;
                    }
                }

                // Check if already hygienized
                bool already_hygienized = false;
                if (name_len >= 4 && name_start[0] == '_' && name_start[1] == 'm') {
                    const char *ch = name_start + 2;
                    bool has_digits = false;
                    while (ch < name_start + name_len && *ch >= '0' && *ch <= '9') {
                        has_digits = true;
                        ch++;
                    }
                    if (has_digits && ch < name_start + name_len && *ch == '_') {
                        already_hygienized = true;
                    }
                }

                // Output the hygienized name (just the string, not a command)
                if (!is_global && !already_hygienized) {
                    char prefix[32];
                    snprintf(prefix, sizeof(prefix), "_m%d_", unique_id);
                    strbuf_append_str(&result, prefix);
                }
                strbuf_append(&result, name_start, name_len);
            }

            if (*p == '>') p++; // skip closing >
            continue;
        }

        // Check for commands that use named storage
        bool is_measure = (strncmp(p, "\\measure<", 9) == 0);
        bool is_assign = (strncmp(p, "\\assign<", 8) == 0);
        bool is_recall = (strncmp(p, "\\recall<", 8) == 0);
        bool is_valueof = (strncmp(p, "\\valueof<", 9) == 0);
        bool is_let = (strncmp(p, "\\let<", 5) == 0);
        bool is_inc = (strncmp(p, "\\inc<", 5) == 0);
        bool is_dec = (strncmp(p, "\\dec<", 5) == 0);
        // Array operations
        bool is_push = (strncmp(p, "\\push<", 6) == 0);
        bool is_pop = (strncmp(p, "\\pop<", 5) == 0);
        bool is_peek = (strncmp(p, "\\peek<", 6) == 0);
        bool is_enqueue = (strncmp(p, "\\enqueue<", 9) == 0);
        bool is_dequeue = (strncmp(p, "\\dequeue<", 9) == 0);
        bool is_len = (strncmp(p, "\\len<", 5) == 0);
        bool is_split = (strncmp(p, "\\split<", 7) == 0);
        bool is_setelement = (strncmp(p, "\\setelement<", 12) == 0);
        // Lambda operations
        bool is_lambda = (strncmp(p, "\\lambda<", 8) == 0);
        // Map operations
        bool is_map_get = (strncmp(p, "\\map_get<", 9) == 0);
        bool is_map_set = (strncmp(p, "\\map_set<", 9) == 0);
        bool is_map_has = (strncmp(p, "\\map_has<", 9) == 0);
        bool is_map_del = (strncmp(p, "\\map_del<", 9) == 0);
        bool is_map_len = (strncmp(p, "\\map_len<", 9) == 0);
        bool is_map_keys = (strncmp(p, "\\map_keys<", 10) == 0);
        // New CL syntax: \at<name>, \set<name>, \invoke<name>, \addressof<name>
        bool is_at = (strncmp(p, "\\at<", 4) == 0);
        bool is_set = (strncmp(p, "\\set<", 5) == 0);
        bool is_invoke = (strncmp(p, "\\invoke<", 8) == 0);
        bool is_addressof = (strncmp(p, "\\addressof<", 11) == 0);

        if (is_measure || is_assign || is_recall || is_valueof || is_let || is_inc || is_dec ||
            is_push || is_pop || is_peek || is_enqueue || is_dequeue || is_len || is_split ||
            is_setelement || is_lambda || is_map_get || is_map_set || is_map_has || is_map_del ||
            is_map_len || is_map_keys || is_at || is_set || is_invoke || is_addressof) {
            int cmd_len;
            if (is_setelement)
                cmd_len = 12;
            else if (is_addressof)
                cmd_len = 11;
            else if (is_map_keys)
                cmd_len = 10;
            else if (is_measure || is_valueof || is_enqueue || is_dequeue || is_map_get ||
                     is_map_set || is_map_has || is_map_del || is_map_len)
                cmd_len = 9;
            else if (is_assign || is_recall || is_lambda || is_invoke)
                cmd_len = 8;
            else if (is_split)
                cmd_len = 7;
            else if (is_push || is_peek)
                cmd_len = 6;
            else if (is_set)
                cmd_len = 5;
            else if (is_at)
                cmd_len = 4;
            else
                cmd_len = 5; // \let, \inc, \dec, \pop, \len

            // Copy the command and opening bracket
            strbuf_append(&result, p, cmd_len);
            p += cmd_len;

            // Parse the name list (can be multiple: <name1,name2,name3>)
            // Also handles \ref<name> which expands to the hygienized name
            while (*p && *p != '>') {
                // Skip whitespace
                while (*p == ' ' || *p == '\t') {
                    strbuf_append_char(&result, *p);
                    p++;
                }

                // Check for \ref<name> inside the angle brackets
                if (strncmp(p, "\\ref<", 5) == 0) {
                    p += 5; // skip \ref<
                    const char *ref_name_start = p;
                    while (*p && *p != '>') p++;
                    size_t ref_name_len = p - ref_name_start;

                    if (ref_name_len > 0) {
                        // Check if global (ALL-CAPS)
                        bool ref_is_global = true;
                        for (const char *ch = ref_name_start; ch < ref_name_start + ref_name_len;
                             ch++) {
                            if (*ch >= 'a' && *ch <= 'z') {
                                ref_is_global = false;
                                break;
                            }
                        }

                        // Check if already hygienized
                        bool ref_already_hygienized = false;
                        if (ref_name_len >= 4 && ref_name_start[0] == '_' &&
                            ref_name_start[1] == 'm') {
                            const char *ch = ref_name_start + 2;
                            bool has_digits = false;
                            while (ch < ref_name_start + ref_name_len && *ch >= '0' && *ch <= '9') {
                                has_digits = true;
                                ch++;
                            }
                            if (has_digits && ch < ref_name_start + ref_name_len && *ch == '_') {
                                ref_already_hygienized = true;
                            }
                        }

                        // Output the hygienized name
                        if (!ref_is_global && !ref_already_hygienized) {
                            char prefix[32];
                            snprintf(prefix, sizeof(prefix), "_m%d_", unique_id);
                            strbuf_append_str(&result, prefix);
                        }
                        strbuf_append(&result, ref_name_start, ref_name_len);
                    }

                    if (*p == '>') p++; // skip \ref's closing >
                    continue;
                }

                // Parse one name - may contain embedded commands like \valueof<i>
                // We need to handle: prefix\valueof<inner> -> _m0_prefix\valueof<_m0_inner>
                const char *name_start = p;

                // Check if name contains embedded commands
                bool has_embedded_cmd = false;
                const char *scan = p;
                while (*scan && *scan != ',' && *scan != ' ' && *scan != '\t') {
                    // Stop at > only if not inside an embedded command's <>
                    if (*scan == '\\') {
                        has_embedded_cmd = true;
                        // Skip past the embedded command including its <...>
                        while (*scan && *scan != '<') scan++;
                        if (*scan == '<') {
                            scan++;
                            int depth = 1;
                            while (*scan && depth > 0) {
                                if (*scan == '<')
                                    depth++;
                                else if (*scan == '>')
                                    depth--;
                                scan++;
                            }
                        }
                    } else if (*scan == '>') {
                        break; // End of main name
                    } else {
                        scan++;
                    }
                }
                p = scan;

                if (p > name_start) {
                    if (has_embedded_cmd) {
                        // Parse carefully, hygienizing each part
                        const char *sp = name_start;
                        while (sp < p) {
                            if (*sp == '\\') {
                                // Found embedded command - copy command name
                                const char *cmd_start = sp;
                                while (sp < p && *sp != '<') sp++;
                                strbuf_append(&result, cmd_start, sp - cmd_start);

                                if (*sp == '<') {
                                    strbuf_append_char(&result, '<');
                                    sp++; // Skip '<'

                                    // Parse inner name and hygienize it
                                    const char *inner_start = sp;
                                    int depth = 1;
                                    while (sp < p && depth > 0) {
                                        if (*sp == '<')
                                            depth++;
                                        else if (*sp == '>')
                                            depth--;
                                        if (depth > 0) sp++;
                                    }

                                    // inner_start to sp is the inner name
                                    size_t inner_len = sp - inner_start;
                                    if (inner_len > 0) {
                                        // Check if inner should be hygienized
                                        bool inner_is_global = true;
                                        for (const char *ch = inner_start; ch < sp; ch++) {
                                            if (*ch >= 'a' && *ch <= 'z') {
                                                inner_is_global = false;
                                                break;
                                            }
                                        }

                                        bool inner_already_hyg = false;
                                        if (inner_len >= 4 && inner_start[0] == '_' &&
                                            inner_start[1] == 'm') {
                                            const char *ch = inner_start + 2;
                                            bool has_digits = false;
                                            while (ch < sp && *ch >= '0' && *ch <= '9') {
                                                has_digits = true;
                                                ch++;
                                            }
                                            if (has_digits && ch < sp && *ch == '_') {
                                                inner_already_hyg = true;
                                            }
                                        }

                                        if (!inner_is_global && !inner_already_hyg) {
                                            char prefix[32];
                                            snprintf(prefix, sizeof(prefix), "_m%d_", unique_id);
                                            strbuf_append_str(&result, prefix);
                                        }
                                        strbuf_append(&result, inner_start, inner_len);
                                    }

                                    if (*sp == '>') {
                                        strbuf_append_char(&result, '>');
                                        sp++; // Skip '>'
                                    }
                                }
                            } else {
                                // Regular character - collect until next \ or end
                                const char *part_start = sp;
                                while (sp < p && *sp != '\\') sp++;

                                if (sp > part_start) {
                                    // Check if this prefix should be hygienized
                                    bool prefix_is_global = true;
                                    for (const char *ch = part_start; ch < sp; ch++) {
                                        if (*ch >= 'a' && *ch <= 'z') {
                                            prefix_is_global = false;
                                            break;
                                        }
                                    }

                                    bool prefix_already_hyg = false;
                                    size_t plen = sp - part_start;
                                    if (plen >= 4 && part_start[0] == '_' && part_start[1] == 'm') {
                                        const char *ch = part_start + 2;
                                        bool has_digits = false;
                                        while (ch < sp && *ch >= '0' && *ch <= '9') {
                                            has_digits = true;
                                            ch++;
                                        }
                                        if (has_digits && ch < sp && *ch == '_') {
                                            prefix_already_hyg = true;
                                        }
                                    }

                                    if (!prefix_is_global && !prefix_already_hyg) {
                                        char prefix[32];
                                        snprintf(prefix, sizeof(prefix), "_m%d_", unique_id);
                                        strbuf_append_str(&result, prefix);
                                    }
                                    strbuf_append(&result, part_start, sp - part_start);
                                }
                            }
                        }
                    } else {
                        // Simple name without embedded commands
                        // Check for dereference prefix (*) - e.g., *pos_addr
                        bool has_deref = (*name_start == '*');
                        const char *actual_name_start = has_deref ? name_start + 1 : name_start;

                        // Check if this name should be exempt from hygiene
                        // Convention 1: ALL CAPS names are global/shared across invocations
                        // Convention 2: Names already prefixed with _mN_ should not be re-prefixed

                        bool is_global = true;
                        for (const char *ch = actual_name_start; ch < p; ch++) {
                            if (*ch >= 'a' && *ch <= 'z') {
                                is_global = false;
                                break;
                            }
                        }

                        // Check if already hygienized (starts with _mN_ pattern)
                        bool already_hygienized = false;
                        size_t actual_name_len = p - actual_name_start;
                        if (actual_name_len >= 4 && actual_name_start[0] == '_' &&
                            actual_name_start[1] == 'm') {
                            // Check for _m<digits>_
                            const char *ch = actual_name_start + 2;
                            bool has_digits = false;
                            while (ch < p && *ch >= '0' && *ch <= '9') {
                                has_digits = true;
                                ch++;
                            }
                            if (has_digits && ch < p && *ch == '_') {
                                already_hygienized = true;
                            }
                        }

                        // Output the dereference prefix first if present
                        if (has_deref) {
                            strbuf_append_char(&result, '*');
                        }

                        if (!is_global && !already_hygienized) {
                            // Add unique prefix: _mN_ where N is unique_id
                            char prefix[32];
                            snprintf(prefix, sizeof(prefix), "_m%d_", unique_id);
                            strbuf_append_str(&result, prefix);
                        }

                        // Add the actual name (without leading *)
                        strbuf_append(&result, actual_name_start, p - actual_name_start);
                    }
                }

                // Copy comma or whitespace
                while (*p && (*p == ',' || *p == ' ' || *p == '\t') && *p != '>') {
                    strbuf_append_char(&result, *p);
                    p++;
                }
            }

            // Copy closing >
            if (*p == '>') {
                strbuf_append_char(&result, *p);
                p++;
            }

            // Special handling for \lambda: also hygienize parameter names in [...]
            // Note: We do NOT hygienize \recall<>[args] because args may be literals
            // EXCEPTION: Computational lambdas (#{} body) don't need hygiene - VM handles scoping
            if (is_lambda && *p == '[') {
                // Peek ahead to see if this is a computational lambda
                const char *peek = p;
                int depth = 1;
                peek++; // Skip '['
                while (*peek && depth > 0) {
                    if (*peek == '[')
                        depth++;
                    else if (*peek == ']')
                        depth--;
                    peek++;
                }
                // Skip whitespace after ]
                while (*peek == ' ' || *peek == '\t' || *peek == '\n') peek++;
                // Check if body starts with #{
                bool is_computational = (*peek == '#' && *(peek + 1) == '{');

                if (is_computational) {
                    // Skip hygiene for computational lambda params - just copy as-is
                    while (*p && *p != ']') {
                        strbuf_append_char(&result, *p);
                        p++;
                    }
                    if (*p == ']') {
                        strbuf_append_char(&result, *p);
                        p++;
                    }
                    // The #{...} body will be handled by the skip block at the top of the loop
                    continue;
                }

                strbuf_append_char(&result, *p);
                p++; // Skip '['

                while (*p && *p != ']') {
                    // Skip whitespace
                    while (*p == ' ' || *p == '\t') {
                        strbuf_append_char(&result, *p);
                        p++;
                    }

                    // Parse one name/value
                    const char *item_start = p;
                    int brace_depth = 0;
                    while (*p && (brace_depth > 0 || (*p != ',' && *p != ']'))) {
                        if (*p == '{')
                            brace_depth++;
                        else if (*p == '}')
                            brace_depth--;
                        p++;
                    }

                    if (p > item_start) {
                        // Check for typed parameter syntax: name:type
                        const char *colon = NULL;
                        for (const char *ch = item_start; ch < p; ch++) {
                            if (*ch == ':') {
                                colon = ch;
                                break;
                            }
                        }

                        if (colon) {
                            // Typed parameter: hygienize name, keep :type
                            const char *name_end = colon;

                            // Check if name part is simple identifier
                            bool name_is_simple = true;
                            for (const char *ch = item_start; ch < name_end; ch++) {
                                if (!isalnum((unsigned char)*ch) && *ch != '_') {
                                    name_is_simple = false;
                                    break;
                                }
                            }

                            if (name_is_simple && name_end > item_start) {
                                // Check if global (ALL-CAPS)
                                bool name_is_global = true;
                                for (const char *ch = item_start; ch < name_end; ch++) {
                                    if (*ch >= 'a' && *ch <= 'z') {
                                        name_is_global = false;
                                        break;
                                    }
                                }

                                // Check if already hygienized
                                bool name_already_hygienized = false;
                                size_t name_len = name_end - item_start;
                                if (name_len >= 4 && item_start[0] == '_' && item_start[1] == 'm') {
                                    const char *ch = item_start + 2;
                                    bool has_digits = false;
                                    while (ch < name_end && *ch >= '0' && *ch <= '9') {
                                        has_digits = true;
                                        ch++;
                                    }
                                    if (has_digits && ch < name_end && *ch == '_') {
                                        name_already_hygienized = true;
                                    }
                                }

                                // Add hygiene prefix if needed
                                if (!name_is_global && !name_already_hygienized) {
                                    char prefix[32];
                                    snprintf(prefix, sizeof(prefix), "_m%d_", unique_id);
                                    strbuf_append_str(&result, prefix);
                                }

                                // Add the name
                                strbuf_append(&result, item_start, name_end - item_start);
                                // Add the :type part unchanged
                                strbuf_append(&result, colon, p - colon);
                            } else {
                                // Not a simple name before colon, copy as-is
                                strbuf_append(&result, item_start, p - item_start);
                            }
                        } else {
                            // No colon - original logic for simple identifiers
                            bool is_simple_ident = true;
                            for (const char *ch = item_start; ch < p; ch++) {
                                if (!isalnum((unsigned char)*ch) && *ch != '_') {
                                    is_simple_ident = false;
                                    break;
                                }
                            }

                            if (is_simple_ident) {
                                // Check if global (ALL-CAPS)
                                bool item_is_global = true;
                                for (const char *ch = item_start; ch < p; ch++) {
                                    if (*ch >= 'a' && *ch <= 'z') {
                                        item_is_global = false;
                                        break;
                                    }
                                }

                                // Check if already hygienized
                                bool item_already_hygienized = false;
                                if (p - item_start >= 4 && item_start[0] == '_' &&
                                    item_start[1] == 'm') {
                                    const char *ch = item_start + 2;
                                    bool has_digits = false;
                                    while (ch < p && *ch >= '0' && *ch <= '9') {
                                        has_digits = true;
                                        ch++;
                                    }
                                    if (has_digits && ch < p && *ch == '_') {
                                        item_already_hygienized = true;
                                    }
                                }

                                // Add hygiene prefix if needed
                                if (!item_is_global && !item_already_hygienized) {
                                    char prefix[32];
                                    snprintf(prefix, sizeof(prefix), "_m%d_", unique_id);
                                    strbuf_append_str(&result, prefix);
                                }

                                // Add the item content
                                strbuf_append(&result, item_start, p - item_start);
                            } else {
                                // Complex content (contains commands, braces, etc.)
                                // Recursively hygienize it
                                char *item_str = malloc(p - item_start + 1);
                                memcpy(item_str, item_start, p - item_start);
                                item_str[p - item_start] = '\0';
                                char *hygienized_item = add_hygiene_prefix(item_str, unique_id);
                                free(item_str);
                                if (hygienized_item) {
                                    strbuf_append_str(&result, hygienized_item);
                                    free(hygienized_item);
                                } else {
                                    strbuf_append(&result, item_start, p - item_start);
                                }
                            }
                        }
                    }

                    // Copy comma
                    if (*p == ',') {
                        strbuf_append_char(&result, *p);
                        p++;
                    }
                }

                // Copy closing ]
                if (*p == ']') {
                    strbuf_append_char(&result, *p);
                    p++;
                }
            }
            // Handle \begin<arr>[idx,val]{enumerate} - hygienize the variable names in [idx,val]
        } else if (strncmp(p, "\\begin<", 7) == 0) {
            // Copy \begin<
            strbuf_append(&result, p, 7);
            p += 7;

            // Copy array name (may contain \recall<> etc., recursively hygienize it)
            const char *arr_start = p;
            int arr_depth = 1;
            while (*p && arr_depth > 0) {
                if (*p == '<')
                    arr_depth++;
                else if (*p == '>')
                    arr_depth--;
                if (arr_depth > 0) p++;
            }
            if (p > arr_start) {
                char *arr_str = malloc(p - arr_start + 1);
                memcpy(arr_str, arr_start, p - arr_start);
                arr_str[p - arr_start] = '\0';
                char *hyg_arr = add_hygiene_prefix(arr_str, unique_id);
                free(arr_str);
                strbuf_append_str(&result, hyg_arr ? hyg_arr : "");
                if (hyg_arr) free(hyg_arr);
            }

            // Copy closing >
            if (*p == '>') {
                strbuf_append_char(&result, *p);
                p++;
            }

            // Check for [idx,val] variable declaration
            if (*p == '[') {
                strbuf_append_char(&result, *p);
                p++; // Skip '['

                // Parse comma-separated variable names
                while (*p && *p != ']') {
                    // Skip whitespace
                    while (*p == ' ' || *p == '\t') {
                        strbuf_append_char(&result, *p);
                        p++;
                    }

                    if (*p == ',' || *p == ']') {
                        if (*p == ',') {
                            strbuf_append_char(&result, *p);
                            p++;
                        }
                        continue;
                    }

                    // Parse variable name
                    const char *var_start = p;
                    while (*p && *p != ',' && *p != ']' && *p != ' ' && *p != '\t') p++;

                    if (p > var_start) {
                        size_t var_len = p - var_start;

                        // Check if global (ALL-CAPS)
                        bool var_is_global = true;
                        for (const char *ch = var_start; ch < p; ch++) {
                            if (*ch >= 'a' && *ch <= 'z') {
                                var_is_global = false;
                                break;
                            }
                        }

                        // Check if already hygienized
                        bool var_already_hygienized = false;
                        if (var_len >= 4 && var_start[0] == '_' && var_start[1] == 'm') {
                            const char *ch = var_start + 2;
                            bool has_digits = false;
                            while (ch < p && *ch >= '0' && *ch <= '9') {
                                has_digits = true;
                                ch++;
                            }
                            if (has_digits && ch < p && *ch == '_') {
                                var_already_hygienized = true;
                            }
                        }

                        // Add hygiene prefix if needed
                        if (!var_is_global && !var_already_hygienized) {
                            char prefix[32];
                            snprintf(prefix, sizeof(prefix), "_m%d_", unique_id);
                            strbuf_append_str(&result, prefix);
                        }
                        strbuf_append(&result, var_start, var_len);
                    }
                }

                // Copy closing ]
                if (*p == ']') {
                    strbuf_append_char(&result, *p);
                    p++;
                }
            }

            // Copy {enumerate} or {loop} etc.
            if (*p == '{') {
                int brace_depth = 1;
                strbuf_append_char(&result, *p);
                p++;
                while (*p && brace_depth > 0) {
                    if (*p == '{')
                        brace_depth++;
                    else if (*p == '}')
                        brace_depth--;
                    strbuf_append_char(&result, *p);
                    p++;
                }
            }
        } else if (*p == '$' && *(p + 1) == '{') {
            // Handle ${name} - new CL syntax variable access
            strbuf_append_str(&result, "${");
            p += 2; // Skip '${'

            // Check for dereference prefix (just copy it through)
            if (*p == '*') {
                strbuf_append_char(&result, '*');
                p++;
            }

            // Parse the variable name
            const char *name_start = p;
            while (*p && *p != '}') p++;
            size_t name_len = p - name_start;

            if (name_len > 0) {
                // Check if global (ALL-CAPS) - no prefix needed
                bool is_global = true;
                for (const char *ch = name_start; ch < name_start + name_len; ch++) {
                    if (*ch >= 'a' && *ch <= 'z') {
                        is_global = false;
                        break;
                    }
                }

                // Check if already hygienized
                bool already_hygienized = false;
                if (name_len >= 4 && name_start[0] == '_' && name_start[1] == 'm') {
                    const char *ch = name_start + 2;
                    bool has_digits = false;
                    while (ch < name_start + name_len && *ch >= '0' && *ch <= '9') {
                        has_digits = true;
                        ch++;
                    }
                    if (has_digits && ch < name_start + name_len && *ch == '_') {
                        already_hygienized = true;
                    }
                }

                // Add hygiene prefix if needed
                if (!is_global && !already_hygienized) {
                    char prefix[32];
                    snprintf(prefix, sizeof(prefix), "_m%d_", unique_id);
                    strbuf_append_str(&result, prefix);
                }
                strbuf_append(&result, name_start, name_len);
            }

            if (*p == '}') {
                strbuf_append_char(&result, '}');
                p++;
            }
        } else {
            // Regular character
            strbuf_append_char(&result, *p);
            p++;
        }
    }

    return strbuf_detach(&result);
}

// Apply hygiene with ID 0 to top-level (non-macro) code
// This allows top-level lowercase variables to work with \ref
char *macro_apply_toplevel_hygiene(const char *input) {
    return add_hygiene_prefix(input, 0);
}

char *macro_expand_call(Macro *macro, const char *input, int *end_pos, char *error_msg,
                        int error_size) {
    if (!macro || !input) return NULL;

    const char *p = input;

    // Skip \name
    p++; // Skip '\'
    while (isalnum((unsigned char)*p) || *p == '_') p++;

    skip_ws(&p);

    // Allocate storage for argument values
    char **arg_values = calloc(macro->n_args, sizeof(char *));

    // Initialize with defaults
    for (int i = 0; i < macro->n_args; i++) {
        if (macro->args[i].default_value) {
            arg_values[i] = strdup(macro->args[i].default_value);
        } else if (macro->args[i].is_optional) {
            arg_values[i] = strdup(""); // Optional with no default = empty
        } else {
            arg_values[i] = NULL; // Required, must be provided
        }
    }

    // Parse optional arguments [...]
    if (*p == '[') {
        p++; // Skip '['

        while (*p && *p != ']') {
            skip_ws(&p);
            if (*p == ']') break;
            if (*p == ',') {
                p++;
                continue;
            }

            // Check for name:value or just value
            const char *start = p;

            // Look for ':' or '=' separator
            const char *sep = NULL;
            int brace_depth = 0;
            const char *scan = p;
            while (*scan && *scan != ']' && !(*scan == ',' && brace_depth == 0)) {
                if (*scan == '{')
                    brace_depth++;
                else if (*scan == '}')
                    brace_depth--;
                else if ((*scan == ':' || *scan == '=') && brace_depth == 0 && !sep) {
                    sep = scan;
                }
                scan++;
            }

            char *arg_name = NULL;
            char *arg_value = NULL;

            if (sep) {
                // name:value or name=value format
                size_t name_len = sep - start;
                // Trim whitespace from name
                while (name_len > 0 &&
                       (start[name_len - 1] == ' ' || start[name_len - 1] == '\t')) {
                    name_len--;
                }
                arg_name = malloc(name_len + 1);
                memcpy(arg_name, start, name_len);
                arg_name[name_len] = '\0';

                const char *val_start = sep + 1;
                while (*val_start == ' ' || *val_start == '\t') val_start++;

                size_t val_len = scan - val_start;
                while (val_len > 0 &&
                       (val_start[val_len - 1] == ' ' || val_start[val_len - 1] == '\t')) {
                    val_len--;
                }
                arg_value = malloc(val_len + 1);
                memcpy(arg_value, val_start, val_len);
                arg_value[val_len] = '\0';
            } else {
                // Just value, use first optional arg or single optional arg
                size_t val_len = scan - start;
                while (val_len > 0 && (start[val_len - 1] == ' ' || start[val_len - 1] == '\t')) {
                    val_len--;
                }
                arg_value = malloc(val_len + 1);
                memcpy(arg_value, start, val_len);
                arg_value[val_len] = '\0';

                // Find first unset optional arg
                if (macro->n_optional == 1) {
                    // Single optional arg - use it
                    for (int i = 0; i < macro->n_args; i++) {
                        if (macro->args[i].is_optional) {
                            arg_name = strdup(macro->args[i].name);
                            break;
                        }
                    }
                } else {
                    // Multiple optionals - use first one
                    for (int i = 0; i < macro->n_args; i++) {
                        if (macro->args[i].is_optional) {
                            arg_name = strdup(macro->args[i].name);
                            break;
                        }
                    }
                }
            }

            // Set the argument value
            if (arg_name) {
                for (int i = 0; i < macro->n_args; i++) {
                    if (strcmp(macro->args[i].name, arg_name) == 0) {
                        free(arg_values[i]);
                        arg_values[i] = arg_value;
                        arg_value = NULL;
                        break;
                    }
                }
                free(arg_name);
            }
            free(arg_value);

            p = scan;
            if (*p == ',') p++;
        }

        if (*p == ']') p++;
        skip_ws(&p);
    }

    // Parse required arguments {...}{...}
    int req_idx = 0;
    for (int i = 0; i < macro->n_args; i++) {
        if (!macro->args[i].is_optional) {
            if (*p != '{') {
                snprintf(error_msg, error_size, "Missing required argument {%s}",
                         macro->args[i].name);
                for (int j = 0; j < macro->n_args; j++) free(arg_values[j]);
                free(arg_values);
                return NULL;
            }
            p++;

            char *value = parse_brace_content(&p, error_msg, error_size);
            if (!value) {
                for (int j = 0; j < macro->n_args; j++) free(arg_values[j]);
                free(arg_values);
                return NULL;
            }

            free(arg_values[i]);
            arg_values[i] = value;
            req_idx++;

            // Only skip whitespace if there are more required arguments to parse
            // This preserves trailing whitespace after the last argument
            if (req_idx < macro->n_required) {
                skip_ws(&p);
            }
        }
    }

    // Log macro call if diagnostics enabled
    if (diag_is_enabled(DIAG_MACROS)) {
        diag_log(DIAG_MACROS, 0, "expanding \\%s", macro->name);
        for (int i = 0; i < macro->n_args; i++) {
            const char *val = arg_values[i] ? arg_values[i] : "(null)";
            diag_result(DIAG_MACROS, 1, "%s = %s", macro->args[i].name, val);
        }
    }

    // Now substitute ${arg} and ${arg.field} in body
    StrBuf result;
    strbuf_init(&result);

    const char *body = macro->body;
    while (*body) {
        // Check for ${...}
        if (body[0] == '$' && body[1] == '{') {
            const char *name_start = body + 2;
            const char *name_end = name_start;
            while (*name_end && *name_end != '}') name_end++;

            if (*name_end == '}') {
                size_t full_len = name_end - name_start;

                // Check for dot notation: ${param.field}
                const char *dot = NULL;
                for (const char *p = name_start; p < name_end; p++) {
                    if (*p == '.') {
                        dot = p;
                        break;
                    }
                }

                bool found = false;

                if (dot) {
                    // Compound field extraction: ${param.field}
                    size_t base_len = dot - name_start;
                    size_t field_len = name_end - dot - 1;

                    // Extract field name
                    char *field_name = malloc(field_len + 1);
                    memcpy(field_name, dot + 1, field_len);
                    field_name[field_len] = '\0';

                    // Find matching base argument
                    for (int i = 0; i < macro->n_args; i++) {
                        if (strlen(macro->args[i].name) == base_len &&
                            strncmp(macro->args[i].name, name_start, base_len) == 0) {
                            if (arg_values[i]) {
                                // Extract field from compound value
                                char *field_val = extract_compound_field(arg_values[i], field_name);
                                strbuf_append_str(&result, field_val);
                                free(field_val);
                            } else {
                                strbuf_append_str(&result, "0");
                            }
                            found = true;
                            break;
                        }
                    }
                    free(field_name);
                } else {
                    // Simple substitution: ${param}
                    for (int i = 0; i < macro->n_args; i++) {
                        if (strlen(macro->args[i].name) == full_len &&
                            strncmp(macro->args[i].name, name_start, full_len) == 0) {
                            if (arg_values[i]) {
                                strbuf_append_str(&result, arg_values[i]);
                            }
                            found = true;
                            break;
                        }
                    }
                }

                if (found) {
                    body = name_end + 1;
                    continue;
                }
            }
        }

        // Check for escaped braces
        if (body[0] == '\\' && body[1] == '{') {
            strbuf_append_char(&result, '{');
            body += 2;
            continue;
        }
        if (body[0] == '\\' && body[1] == '}') {
            strbuf_append_char(&result, '}');
            body += 2;
            continue;
        }

        // Regular character
        strbuf_append_char(&result, *body);
        body++;
    }

    // Get result before cleanup
    char *expanded = strbuf_detach(&result);

    // Apply macro hygiene: rewrite \measure and \recall with unique prefixes
    // This prevents name collisions when the same macro is called multiple times
    int unique_id = g_macro_hygiene_counter++;
    char *hygienic = add_hygiene_prefix(expanded, unique_id);
    free(expanded);

    // Log result if diagnostics enabled
    if (diag_is_enabled(DIAG_MACROS)) {
        diag_result(DIAG_MACROS, 1, "=> %s (with hygiene prefix _m%d_)", hygienic, unique_id);
    }

    // Cleanup
    for (int i = 0; i < macro->n_args; i++) {
        free(arg_values[i]);
    }
    free(arg_values);

    *end_pos = (int)(p - input);
    return hygienic;
}

// ============================================================================
// Full Document Processing
// ============================================================================

char *macro_process_document(const char *input, MacroRegistry *reg, char *error_msg,
                             int error_size) {
    if (!input || !reg) return NULL;

    StrBuf result;
    strbuf_init(&result);

    const char *p = input;
    int line = 1;
    int col = 1;

    while (*p) {
        // Check for \macro definition
        if (strncmp(p, "\\macro", 6) == 0 && p[6] == '<') {
            int start_line = line;
            int start_col = col;

            int end_pos = 0;
            Macro *macro = macro_parse_definition(p, &end_pos, error_msg, error_size);
            if (!macro) {
                strbuf_free(&result);
                return NULL;
            }

            // Calculate end position by scanning through the definition
            int end_line = start_line;
            int end_col = start_col;
            for (int i = 0; i < end_pos; i++) {
                if (p[i] == '\n') {
                    end_line++;
                    end_col = 1;
                } else {
                    end_col++;
                }
            }

            // Set position in macro
            macro->def_line = start_line;
            macro->def_col = start_col;
            macro->def_end_line = end_line;
            macro->def_end_col = end_col;

            macro_registry_add(reg, macro);
            free(macro); // Registry copied it

            // Update position tracking
            line = end_line;
            col = end_col;
            p += end_pos;
            continue;
        }

        // Regular character - track position
        if (*p == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
        strbuf_append_char(&result, *p);
        p++;
    }

    return strbuf_detach(&result);
}

#define MACRO_EXPAND_MAX_ITERATIONS 100

char *macro_expand_all(const char *input, MacroRegistry *reg, char *error_msg, int error_size) {
    if (!input || !reg) return NULL;

    char *current = strdup(input);

    if (diag_is_enabled(DIAG_MACROS) && reg->n_macros > 0) {
        diag_log(DIAG_MACROS, 0, "macro_expand_all: %d macro(s) registered", reg->n_macros);
    }

    for (int iter = 0; iter < MACRO_EXPAND_MAX_ITERATIONS; iter++) {
        StrBuf result;
        strbuf_init(&result);

        bool did_expand = false;
        const char *p = current;

        bool in_display_math = false;
        bool in_inline_math = false;

        while (*p) {
            // Track math mode boundaries — skip macro expansion inside math
            if (p[0] == '$' && p[1] == '$' && (p == current || p[-1] != '\\')) {
                in_display_math = !in_display_math;
                strbuf_append_char(&result, *p++);
                strbuf_append_char(&result, *p++);
                continue;
            }
            if (p[0] == '$' && p[1] != '$' && p[1] != '{' && (p == current || p[-1] != '\\') &&
                !in_display_math) {
                in_inline_math = !in_inline_math;
                strbuf_append_char(&result, *p++);
                continue;
            }

            // Try to match a macro call (only outside math mode)
            if (*p == '\\' && !in_display_math && !in_inline_math) {
                int name_end = 0;
                Macro *macro = macro_match_call(reg, p, &name_end);

                if (macro) {
                    int call_end = 0;
                    char *expanded = macro_expand_call(macro, p, &call_end, error_msg, error_size);

                    if (expanded) {
                        strbuf_append_str(&result, expanded);
                        free(expanded);
                        p += call_end;
                        did_expand = true;
                        continue;
                    }
                    // If expansion failed, fall through and copy as-is
                }
            }

            strbuf_append_char(&result, *p);
            p++;
        }

        free(current);
        current = strbuf_detach(&result);

        if (!did_expand) {
            break; // No more expansions
        }
    }

    return current;
}

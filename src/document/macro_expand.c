// macro_expand.c - Macro expansion for Hyades documents
//
// Handles expansion of:
// - System macros (\table, \quad, \qquad, \hskip, etc.)
// - Escape sequences (\{, \}, \#, \_, \&, \textbackslash, etc.)
// - User-defined macros (\macro<name>{...})
// - Julia computation (\julia, \call)

#include "macro_expand.h"
#include "calc.h"
#include "diagnostics/diagnostics.h"
#include "interop/julia_bridge.h"

// LSP infrastructure
#include "document/source_map.h"
#include "document/symbol_table.h"
#include "utils/error.h"
#ifndef HYADES_RENDER_ONLY
#include "macro/stdlib/stdlib_lambdas.h"
#include "macro/stdlib/stdlib_macros.h"
#endif
#include "macro/system/aligned_macro.h"
#include "macro/system/list_macro.h"
#include "macro/system/table_macro.h"
#include "math/renderer/render_opts.h"
#include "user_macro.h"
#include "utils/strbuf.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Constants
// ============================================================================

#define MACRO_EXPANSION_MAX_ITERATIONS 100
#define MACRO_EXPANSION_MAX_SIZE (50 * 1024 * 1024) // 50MB limit

// Maximum characters to scan in string parsing loops.
// If we scan more than this, something is wrong (corruption or missing terminator).
#define MAX_SCAN_CHARS (500 * 1024 * 1024) // 500MB - effectively unlimited

// NBSP in UTF-8
#define NBSP "\xC2\xA0"

// ============================================================================
// Global Macro Registry for Nested Calls
// ============================================================================
// When parse_document_as_vbox calls itself recursively (e.g., for \lineroutine
// content), nested calls should reuse the outer document's macro registry
// so that user-defined macros remain available.

static MacroRegistry *g_shared_macro_registry = NULL;
static int g_macro_expansion_nesting = 0;
static int g_macro_keep_alive = 0; // If > 0, don't free registry at end of expand_all_macros
static LspSymbolTable *g_shared_symbol_table = NULL; // For LSP symbol collection

// Continuous mode flag - set by hyades_main.c, checked by \main command
static bool g_continuous_mode = false;
static volatile int *g_exit_flag = NULL; // Points to hyades_main.c's exit flag

void macro_set_continuous_mode(bool enable, volatile int *exit_flag) {
    g_continuous_mode = enable;
    g_exit_flag = exit_flag;
}

bool macro_is_continuous_mode(void) {
    return g_continuous_mode;
}

volatile int *macro_get_exit_flag(void) {
    return g_exit_flag;
}

// Keep the macro registry alive across multiple expand_all_macros calls
void macro_registry_keep_alive(bool enable) {
    if (enable) {
        g_macro_keep_alive++;
    } else {
        g_macro_keep_alive--;
        if (g_macro_keep_alive <= 0) {
            if (g_shared_macro_registry) {
                macro_registry_free(g_shared_macro_registry);
                g_shared_macro_registry = NULL;
            }
            g_macro_keep_alive = 0;
        }
    }
}

// ============================================================================
// Forward Declarations
// ============================================================================

static bool has_expandable_macros(const char *input);
static char *expand_macros_once(const char *input, bool *did_expand, CalcContext *calc_ctx,
                                char *error_msg, int error_size);
static char *transform_begin_end(const char *input, bool *did_transform);

// ============================================================================
// Begin/End Environment Transformation
// ============================================================================
// Transforms \begin{NAME}...\end{NAME} to \NAME{...}
// Also handles:
//   \begin{NAME}[opts]...\end{NAME} to \NAME[opts]{...}
//   \begin[opts]{NAME}...\end{NAME} to \NAME[opts]{...}

static char *transform_begin_end(const char *input, bool *did_transform) {
    *did_transform = false;
    if (!input) return NULL;

    // Quick check - if no \begin{ or \begin[, nothing to do
    if (strstr(input, "\\begin{") == NULL && strstr(input, "\\begin[") == NULL) {
        return strdup(input);
    }

    StrBuf result;
    strbuf_init_with_capacity(&result, strlen(input) + 64);

    const char *p = input;

    while (*p) {
        // Look for \begin{ or \begin[
        if (strncmp(p, "\\begin", 6) == 0 && (p[6] == '{' || p[6] == '[')) {
            const char *begin_start = p;
            p += 6;

            char *env_name = NULL;
            char *opts = NULL;

            // Handle \begin[opts]{NAME} syntax (opts before name)
            if (*p == '[') {
                p++;
                const char *opts_start = p;
                int bracket_depth = 1;
                while (*p && bracket_depth > 0) {
                    if (*p == '[')
                        bracket_depth++;
                    else if (*p == ']')
                        bracket_depth--;
                    if (bracket_depth > 0) p++;
                }
                if (*p == ']') {
                    size_t opts_len = p - opts_start;
                    opts = malloc(opts_len + 1);
                    memcpy(opts, opts_start, opts_len);
                    opts[opts_len] = '\0';
                    p++; // skip ]
                } else {
                    // Malformed, copy as-is
                    strbuf_append_n(&result, begin_start, p - begin_start);
                    continue;
                }
            }

            // Now expect {NAME}
            if (*p != '{') {
                // Malformed, copy as-is
                strbuf_append_n(&result, begin_start, p - begin_start);
                free(opts);
                continue;
            }
            p++; // skip {

            // Extract environment name
            const char *name_start = p;
            while (*p && *p != '}' && *p != '\n') p++;
            if (*p != '}') {
                // Malformed, copy as-is
                strbuf_append_n(&result, begin_start, p - begin_start);
                free(opts);
                continue;
            }

            size_t name_len = p - name_start;
            if (name_len == 0) {
                // Empty name, copy as-is
                strbuf_append_n(&result, begin_start, p - begin_start + 1);
                p++;
                free(opts);
                continue;
            }

            env_name = malloc(name_len + 1);
            memcpy(env_name, name_start, name_len);
            env_name[name_len] = '\0';
            p++; // skip }

            // Skip hbox/vbox/loop/while - these are handled specially by the parser
            // hbox/vbox: handled by layout parser
            // loop/while: handled by expand_macros_once with shared CalcContext
            if (strcmp(env_name, "hbox") == 0 || strcmp(env_name, "vbox") == 0 ||
                strcmp(env_name, "loop") == 0 || strcmp(env_name, "while") == 0) {
                // Copy as-is and continue
                strbuf_append_n(&result, begin_start, p - begin_start);
                free(env_name);
                free(opts);
                continue;
            }

            // Check for optional [opts] AFTER name (LaTeX style: \begin{NAME}[opts])
            // Only if we didn't already get opts before name
            if (!opts && *p == '[') {
                p++;
                const char *opts_start = p;
                int bracket_depth = 1;
                while (*p && bracket_depth > 0) {
                    if (*p == '[')
                        bracket_depth++;
                    else if (*p == ']')
                        bracket_depth--;
                    if (bracket_depth > 0) p++;
                }
                if (*p == ']') {
                    size_t opts_len = p - opts_start;
                    opts = malloc(opts_len + 1);
                    memcpy(opts, opts_start, opts_len);
                    opts[opts_len] = '\0';
                    p++; // skip ]
                }
            }

            // Build the \end{NAME} pattern to search for
            char end_pattern[256];
            snprintf(end_pattern, sizeof(end_pattern), "\\end{%s}", env_name);
            size_t end_pattern_len = strlen(end_pattern);

            // Collect content until matching \end{NAME}
            // Handle nesting: count \begin{NAME}, \begin[...]{NAME} and \end{NAME}
            const char *content_start = p;
            int depth = 1;

            while (*p && depth > 0) {
                // Skip verbatim content \verb|...|
                if (strncmp(p, "\\verb", 5) == 0) {
                    p += 5;
                    if (*p && *p != '{' && *p != '[') {
                        char delim = *p++;
                        while (*p && *p != delim) p++;
                        if (*p == delim) p++;
                    }
                    continue;
                }

                // Check for nested \begin{NAME} or \begin[...]{NAME}
                if (strncmp(p, "\\begin", 6) == 0 && (p[6] == '{' || p[6] == '[')) {
                    const char *q = p + 6;
                    // Skip optional [...]
                    if (*q == '[') {
                        q++;
                        int bd = 1;
                        while (*q && bd > 0) {
                            if (*q == '[')
                                bd++;
                            else if (*q == ']')
                                bd--;
                            q++;
                        }
                    }
                    // Check if this is \begin...{NAME}
                    if (*q == '{') {
                        q++;
                        if (strncmp(q, env_name, name_len) == 0 && q[name_len] == '}') {
                            depth++;
                            p = q + name_len + 1; // skip past {NAME}
                            continue;
                        }
                    }
                }

                // Check for \end{NAME}
                if (strncmp(p, end_pattern, end_pattern_len) == 0) {
                    depth--;
                    if (depth == 0) {
                        // Found our closing \end
                        break;
                    }
                    p += end_pattern_len;
                    continue;
                }

                p++;
            }

            if (depth != 0) {
                // Unmatched \begin, copy as-is
                strbuf_append_n(&result, begin_start, p - begin_start);
                free(env_name);
                free(opts);
                continue;
            }

            // Extract content (everything between \begin{X}[opts] and \end{X})
            size_t content_len = p - content_start;

            // Output: \NAME[opts]{content}
            strbuf_putc(&result, '\\');
            strbuf_append(&result, env_name);
            if (opts) {
                strbuf_putc(&result, '[');
                strbuf_append(&result, opts);
                strbuf_putc(&result, ']');
            }
            strbuf_putc(&result, '{');
            strbuf_append_n(&result, content_start, content_len);
            strbuf_putc(&result, '}');

            // Skip past \end{NAME}
            p += end_pattern_len;

            *did_transform = true;
            free(env_name);
            free(opts);
        } else {
            strbuf_putc(&result, *p++);
        }
    }

    return strbuf_detach(&result);
}

// ============================================================================
// Julia Integration (Optional)
// ============================================================================

static bool g_julia_initialized = false;

static bool ensure_julia_init(void) {
#ifdef HYADES_JULIA_SUPPORT
    if (!g_julia_initialized) {
        g_julia_initialized = julia_init();
    }
    return g_julia_initialized;
#else
    return false;
#endif
}

// Parse \julia command
// Returns characters consumed, -1 on error
// If *result_out is set, caller must free it
static int parse_julia_command(const char *input, char **result_out, char *error_msg,
                               int error_size) {
    *result_out = NULL;

#ifndef HYADES_JULIA_SUPPORT
    // Julia not compiled in
    return 0;
#else
    if (strncmp(input, "\\julia", 6) != 0) return 0;

    const char *p = input + 6;

    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    // Determine form: inline \julia{code} or definition \julia<n>{code}
    bool is_inline = (*p == '{');
    char *name = NULL;
    char *params = NULL;

    if (!is_inline) {
        // Must have <name>
        if (*p != '<') return 0;
        p++;

        const char *name_start = p;
        while (*p && *p != '>' && *p != '\n') p++;
        if (*p != '>') return 0;

        size_t name_len = p - name_start;
        name = malloc(name_len + 1);
        memcpy(name, name_start, name_len);
        name[name_len] = '\0';
        p++;

        // Optional [params]
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '[') {
            p++;
            const char *params_start = p;
            while (*p && *p != ']' && *p != '\n') p++;
            if (*p != ']') {
                free(name);
                return 0;
            }
            size_t params_len = p - params_start;
            params = malloc(params_len + 1);
            memcpy(params, params_start, params_len);
            params[params_len] = '\0';
            p++;
        }
    }

    // Now expect {code}
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '{') {
        free(name);
        free(params);
        return 0;
    }
    p++;

    // Find matching }
    const char *code_start = p;
    int brace_depth = 1;
    while (*p && brace_depth > 0) {
        if (*p == '{')
            brace_depth++;
        else if (*p == '}')
            brace_depth--;
        if (brace_depth > 0) p++;
    }

    if (brace_depth != 0) {
        free(name);
        free(params);
        return 0;
    }

    size_t code_len = p - code_start;
    char *code = malloc(code_len + 1);
    memcpy(code, code_start, code_len);
    code[code_len] = '\0';
    p++;

    // Initialize Julia
    if (!ensure_julia_init()) {
        snprintf(error_msg, error_size, "\\julia: Julia not available");
        free(name);
        free(params);
        free(code);
        return -1;
    }

    if (is_inline) {
        // Inline eval
        JuliaResult *result = julia_eval(code);
        free(code);

        if (!result) {
            snprintf(error_msg, error_size, "\\julia: execution failed");
            return -1;
        }

        char *tex = julia_result_to_tex(result);
        julia_result_free(result);

        if (!tex) {
            snprintf(error_msg, error_size, "\\julia: failed to convert result");
            return -1;
        }

        *result_out = tex;
    } else {
        // Definition: register
        if (!julia_register(name, params, code)) {
            snprintf(error_msg, error_size, "\\julia<%s>: failed to register", name);
            free(name);
            free(params);
            free(code);
            return -1;
        }

        free(name);
        free(params);
        free(code);
    }

    return (int)(p - input);
#endif
}

// Parse \call[name]{args}
static char *expand_julia_call(const char *input, int *end_pos, char *error_msg, int error_size) {
#ifndef HYADES_JULIA_SUPPORT
    return NULL;
#else
    if (strncmp(input, "\\call", 5) != 0) return NULL;

    const char *p = input + 5;

    while (*p == ' ' || *p == '\t') p++;
    if (*p != '[') return NULL;
    p++;

    const char *name_start = p;
    while (*p && *p != ']' && *p != '\n') p++;
    if (*p != ']') {
        snprintf(error_msg, error_size, "\\call: missing ']'");
        return NULL;
    }

    size_t name_len = p - name_start;
    if (name_len == 0) {
        snprintf(error_msg, error_size, "\\call: empty name");
        return NULL;
    }

    char *name = malloc(name_len + 1);
    memcpy(name, name_start, name_len);
    name[name_len] = '\0';
    p++;

    // Optional {args}
    char *args = NULL;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '{') {
        p++;
        const char *args_start = p;
        int brace_depth = 1;
        while (*p && brace_depth > 0) {
            if (*p == '{')
                brace_depth++;
            else if (*p == '}')
                brace_depth--;
            if (brace_depth > 0) p++;
        }
        if (brace_depth != 0) {
            snprintf(error_msg, error_size, "\\call[%s]: unclosed '{'", name);
            free(name);
            return NULL;
        }
        size_t args_len = p - args_start;
        args = malloc(args_len + 1);
        memcpy(args, args_start, args_len);
        args[args_len] = '\0';
        p++;
    }

    *end_pos = (int)(p - input);

    if (!ensure_julia_init()) {
        snprintf(error_msg, error_size, "\\call[%s]: Julia not available", name);
        free(name);
        free(args);
        return NULL;
    }

    JuliaResult *result = julia_call(name, args);
    free(name);
    free(args);

    if (!result) {
        snprintf(error_msg, error_size, "\\call: execution failed");
        return NULL;
    }

    char *tex = julia_result_to_tex(result);
    julia_result_free(result);

    return tex;
#endif
}

// ============================================================================
// Macro Detection
// ============================================================================

static bool has_expandable_macros(const char *input) {
    // Note: \textbackslash, \textdollar, etc. are handled at render time
    // in compositor/paragraph.c, not during macro expansion
    return strstr(input, "\\table") != NULL || strstr(input, "\\aligned") != NULL ||
           strstr(input, "\\cases") != NULL || strstr(input, "\\list") != NULL ||
           strstr(input, "\\quad") != NULL || strstr(input, "\\qquad") != NULL ||
           strstr(input, "\\hskip") != NULL || strstr(input, "\\thinspace") != NULL ||
           strstr(input, "\\{") != NULL || strstr(input, "\\}") != NULL ||
           strstr(input, "\\#") != NULL || strstr(input, "\\_") != NULL ||
           strstr(input, "\\&") != NULL || strstr(input, "\\julia") != NULL ||
           strstr(input, "\\call") != NULL || strstr(input, "\\diagnostics") != NULL ||
           strstr(input, "\\begin<") != NULL ||       // Array enumerate
           strstr(input, "\\begin{loop}") != NULL ||  // Loop construct
           strstr(input, "\\begin{while}") != NULL || // While construct
           calc_has_commands(input);
}

// ============================================================================
// Single-Pass Macro Expansion
// ============================================================================

static char *expand_macros_once(const char *input, bool *did_expand, CalcContext *calc_ctx,
                                char *error_msg, int error_size) {
    *did_expand = false;
    error_msg[0] = '\0';

    if (!input) return NULL;

    // Quick check
    if (!has_expandable_macros(input)) {
        return strdup(input);
    }

    StrBuf result;
    strbuf_init_with_capacity(&result, strlen(input) * 2);

    const char *p = input;

    // Track math mode
    bool in_display_math = false;
    bool in_inline_math = false;
    size_t chars_scanned = 0;

    while (*p) {
        // Defensive: prevent infinite loop from memory corruption
        if (++chars_scanned > MAX_SCAN_CHARS) {
            fprintf(stderr, "macro_process_escapes: scan limit exceeded (corruption?)\n");
            break;
        }
        // Handle \verb blocks - copy verbatim without processing escapes
        // Format: \verb#...# where # is the delimiter character
        if (strncmp(p, "\\verb", 5) == 0 && p[5] != '\0') {
            strbuf_append_n(&result, p, 5); // \verb
            p += 5;
            char delim = *p;
            strbuf_putc(&result, delim);
            p++;
            // Copy until closing delimiter
            while (*p && *p != delim) {
                strbuf_putc(&result, *p);
                p++;
            }
            if (*p == delim) {
                strbuf_putc(&result, delim);
                p++;
            }
            continue;
        }

        // Handle special commands FIRST - enable settings before any expansion
        // Handle \diagnostics{categories}
        if (strncmp(p, "\\diagnostics", 12) == 0) {
            const char *q = p + 12;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '{') {
                q++;
                const char *cat_start = q;
                int brace_depth = 1;
                while (*q && brace_depth > 0) {
                    if (*q == '{')
                        brace_depth++;
                    else if (*q == '}')
                        brace_depth--;
                    if (brace_depth > 0) q++;
                }
                if (brace_depth == 0) {
                    // Parse the categories and enable diagnostics
                    size_t cat_len = q - cat_start;
                    char *cats_str = malloc(cat_len + 1);
                    memcpy(cats_str, cat_start, cat_len);
                    cats_str[cat_len] = '\0';

                    DiagCategory cats = diag_parse_categories(cats_str);
                    if (cats == DIAG_NONE) {
                        diag_disable();
                    } else {
                        diag_enable(cats);
                    }
                    free(cats_str);

                    // Skip past the command, keep it in output for the render phase
                    strbuf_append_n(&result, p, (q + 1) - p);
                    p = q + 1;
                    continue;
                }
            }
        }
        // Handle \setunicode{true/false} - update unicode mode during expansion
        // so that \unicode can reflect changes made by \setunicode
        if (strncmp(p, "\\setunicode", 11) == 0) {
            const char *q = p + 11;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '{') {
                q++;
                const char *val_start = q;
                int brace_depth = 1;
                while (*q && brace_depth > 0) {
                    if (*q == '{')
                        brace_depth++;
                    else if (*q == '}')
                        brace_depth--;
                    if (brace_depth > 0) q++;
                }
                if (brace_depth == 0) {
                    // Parse the value and set unicode mode
                    size_t val_len = q - val_start;
                    if (val_len == 4 && strncmp(val_start, "true", 4) == 0) {
                        set_unicode_mode(1);
                    } else if (val_len == 5 && strncmp(val_start, "false", 5) == 0) {
                        set_unicode_mode(0);
                    } else if (val_len == 1 && val_start[0] == '1') {
                        set_unicode_mode(1);
                    } else if (val_len == 1 && val_start[0] == '0') {
                        set_unicode_mode(0);
                    }
                    // Keep the command in output for the render phase
                    strbuf_append_n(&result, p, (q + 1) - p);
                    p = q + 1;
                    continue;
                }
            }
        }
        // Handle \linebreaker{greedy|knuth|raggedright} - expand any calc commands in the argument
        // (e.g., \recall<tmp>) and also update global state so \getlinebreaker works correctly
        if (strncmp(p, "\\linebreaker", 12) == 0) {
            const char *q = p + 12;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '{') {
                q++;
                const char *val_start = q;
                int brace_depth = 1;
                while (*q && brace_depth > 0) {
                    if (*q == '{')
                        brace_depth++;
                    else if (*q == '}')
                        brace_depth--;
                    if (brace_depth > 0) q++;
                }
                if (brace_depth == 0) {
                    // Extract the argument
                    size_t val_len = q - val_start;
                    char *arg = malloc(val_len + 1);
                    if (arg) {
                        memcpy(arg, val_start, val_len);
                        arg[val_len] = '\0';

                        // Check if argument might be a calc command (e.g., \recall<saved>)
                        // If so, expand it now so the render phase gets the literal value
                        const char *mode_str = arg;
                        char *expanded_arg = NULL;
                        if (calc_ctx && arg[0] == '\\') {
                            int calc_end = 0;
                            char calc_err[256] = {0};
                            expanded_arg = calc_try_expand(arg, &calc_end, calc_ctx, calc_err,
                                                           sizeof(calc_err));
                            if (expanded_arg) {
                                mode_str = expanded_arg;
                            }
                        }

                        // Update global state NOW so \getlinebreaker returns correct value
                        // This is needed for macros like \raggedright that save/restore state
                        set_linebreaker_mode(mode_str);

                        // Keep the command in output for the render phase
                        // IMPORTANT: Use the expanded value, not the original argument
                        // This ensures \linebreaker{\recall<tmp>} becomes \linebreaker{greedy}
                        strbuf_append(&result, "\\linebreaker{");
                        strbuf_append(&result, mode_str);
                        strbuf_append(&result, "}");

                        free(arg);
                        if (expanded_arg) free(expanded_arg);
                    }

                    p = q + 1;
                    continue;
                }
            }
        }
        // Try calc commands (arithmetic, counters, conditionals, measure)
        if (calc_ctx && *p == '\\') {
            // Update calc context with current math mode state
            calc_ctx->in_math_mode = (in_display_math || in_inline_math);
            calc_ctx->is_display_math = in_display_math;

            int calc_end = 0;
            char *calc_result = calc_try_expand(p, &calc_end, calc_ctx, error_msg, error_size);
            if (calc_result) {
                strbuf_append(&result, calc_result);
                free(calc_result);
                p += calc_end;
                *did_expand = true;
                continue;
            }
            if (error_msg[0] != '\0') {
                strbuf_free(&result);
                return NULL;
            }
        }

        // Handle ${name} variable access (new CL syntax for interpreter)
        if (calc_ctx && p[0] == '$' && p[1] == '{') {
            int dollar_end = 0;
            char *dollar_result = calc_try_expand_dollar(p, &dollar_end, calc_ctx);
            if (dollar_result) {
                strbuf_append(&result, dollar_result);
                free(dollar_result);
                p += dollar_end;
                *did_expand = true;
                continue;
            }
        }

        // Track $$ transitions
        if (p[0] == '$' && p[1] == '$' && (p == input || p[-1] != '\\')) {
            in_display_math = !in_display_math;
            strbuf_append_n(&result, p, 2);
            p += 2;
            continue;
        }

        // Track $ transitions (but not ${name} which is variable access)
        if (p[0] == '$' && p[1] != '$' && p[1] != '{' && (p == input || p[-1] != '\\') &&
            !in_display_math) {
            in_inline_math = !in_inline_math;
            strbuf_putc(&result, *p++);
            continue;
        }

        // \julia command
        if (strncmp(p, "\\julia", 6) == 0) {
            char *julia_result = NULL;
            int consumed = parse_julia_command(p, &julia_result, error_msg, error_size);
            if (consumed < 0) {
                strbuf_free(&result);
                return NULL;
            }
            if (consumed > 0) {
                if (julia_result) {
                    strbuf_append(&result, julia_result);
                    free(julia_result);
                }
                p += consumed;
                *did_expand = true;
                continue;
            }
        }

        // \call command
        if (strncmp(p, "\\call", 5) == 0) {
            int end_pos = 0;
            char *expanded = expand_julia_call(p, &end_pos, error_msg, error_size);
            if (expanded) {
                strbuf_append(&result, expanded);
                free(expanded);
                p += end_pos;
                *did_expand = true;
                continue;
            }
            if (error_msg[0] != '\0') {
                strbuf_free(&result);
                return NULL;
            }
        }

        // \table macro
        if (strncmp(p, "\\table", 6) == 0) {
            char after = p[6];
            if (after == '[' || after == '{' || after == ' ' || after == '\t' || after == '\n' ||
                after == '\0') {
                if (diag_is_enabled(DIAG_SYSTEM)) {
                    diag_log(DIAG_SYSTEM, 0, "expanding \\table");
                }
                int end_pos = 0;
                char *expanded = table_macro_expand(p, &end_pos, calc_ctx, error_msg, error_size);
                if (!expanded) {
                    strbuf_free(&result);
                    return NULL;
                }
                if (diag_is_enabled(DIAG_SYSTEM)) {
                    diag_result(DIAG_SYSTEM, 1, "=> %s", expanded);
                }
                strbuf_append(&result, expanded);
                free(expanded);
                p += end_pos;
                *did_expand = true;
                continue;
            }
        }

        // \list macro
        if (strncmp(p, "\\list", 5) == 0) {
            char after = p[5];
            if (after == '[' || after == '{' || after == ' ' || after == '\t' || after == '\n' ||
                after == '\0') {
                if (diag_is_enabled(DIAG_SYSTEM)) {
                    diag_log(DIAG_SYSTEM, 0, "expanding \\list");
                }
                int end_pos = 0;
                char *expanded = list_macro_expand(p, &end_pos, error_msg, error_size);
                if (!expanded) {
                    strbuf_free(&result);
                    return NULL;
                }
                if (diag_is_enabled(DIAG_SYSTEM)) {
                    diag_result(DIAG_SYSTEM, 1, "=> %s", expanded);
                }
                strbuf_append(&result, expanded);
                free(expanded);
                p += end_pos;
                *did_expand = true;
                continue;
            }
        }

        // \aligned macro - skip if inside math mode (will be handled by math parser)
        if (strncmp(p, "\\aligned", 8) == 0 && !in_display_math && !in_inline_math) {
            char after = p[8];
            if (after == '{' || after == '[' || after == ' ' || after == '\t' || after == '\n' ||
                after == '\0') {
                if (diag_is_enabled(DIAG_SYSTEM)) {
                    diag_log(DIAG_SYSTEM, 0, "expanding \\aligned");
                }
                int end_pos = 0;
                char *expanded = aligned_macro_expand(p, &end_pos, error_msg, error_size);
                if (!expanded) {
                    strbuf_free(&result);
                    return NULL;
                }
                if (diag_is_enabled(DIAG_SYSTEM)) {
                    diag_result(DIAG_SYSTEM, 1, "=> %s", expanded);
                }
                strbuf_append(&result, expanded);
                free(expanded);
                p += end_pos;
                *did_expand = true;
                continue;
            }
        }

        // \cases macro - skip if inside math mode (will be handled by math parser)
        if (strncmp(p, "\\cases", 6) == 0 && !in_display_math && !in_inline_math) {
            char after = p[6];
            if (after == '{' || after == '[' || after == ' ' || after == '\t' || after == '\n' ||
                after == '\0') {
                if (diag_is_enabled(DIAG_SYSTEM)) {
                    diag_log(DIAG_SYSTEM, 0, "expanding \\cases");
                }
                int end_pos = 0;
                char *expanded = cases_macro_expand(p, &end_pos, error_msg, error_size);
                if (!expanded) {
                    strbuf_free(&result);
                    return NULL;
                }
                if (diag_is_enabled(DIAG_SYSTEM)) {
                    diag_result(DIAG_SYSTEM, 1, "=> %s", expanded);
                }
                strbuf_append(&result, expanded);
                free(expanded);
                p += end_pos;
                *did_expand = true;
                continue;
            }
        }

        // \begin{loop}...\end{loop} or \begin{while}...\end{while}
        // Iterative expansion with shared CalcContext
        bool is_loop = strncmp(p, "\\begin{loop}", 12) == 0;
        bool is_while = strncmp(p, "\\begin{while}", 13) == 0;
        if (is_loop || is_while) {
            const char *env_name = is_loop ? "loop" : "while";
            int begin_len = is_loop ? 12 : 13;
            int end_len = is_loop ? 10 : 11;

            // Find matching \end{loop} or \end{while}
            const char *body_start = p + begin_len;
            const char *body_end = NULL;
            int loop_depth = 1;
            const char *scan = body_start;

            char begin_pattern[32], end_pattern[32];
            snprintf(begin_pattern, sizeof(begin_pattern), "\\begin{%s}", env_name);
            snprintf(end_pattern, sizeof(end_pattern), "\\end{%s}", env_name);

            while (*scan && loop_depth > 0) {
                if (strncmp(scan, begin_pattern, begin_len) == 0) {
                    loop_depth++;
                    scan += begin_len;
                } else if (strncmp(scan, end_pattern, end_len) == 0) {
                    loop_depth--;
                    if (loop_depth == 0) {
                        body_end = scan;
                    } else {
                        scan += end_len;
                    }
                } else {
                    scan++;
                }
            }

            if (body_end) {
                // Extract the body
                size_t body_len = body_end - body_start;
                char *body = malloc(body_len + 1);
                memcpy(body, body_start, body_len);
                body[body_len] = '\0';

                if (diag_is_enabled(DIAG_SYSTEM)) {
                    diag_log(DIAG_SYSTEM, 0, "\\begin{%s}: body length %zu", env_name, body_len);
                }

                // Maximum 1 million iterations
                const int MAX_LOOP_ITERATIONS = 1000000;

                // Save the outer scope for lambda storage
                Scope *loop_outer_scope = calc_ctx->current_scope;

                for (int iter = 0; iter < MAX_LOOP_ITERATIONS; iter++) {
                    // Clear exit flag at start of each iteration
                    calc_ctx->exit_loop_requested = false;

                    // Create per-iteration scope for proper closure behavior
                    Scope *iter_scope = scope_new(calc_ctx->current_scope);
                    Scope *saved_scope = calc_ctx->current_scope;
                    calc_ctx->current_scope = iter_scope;

                    if (diag_is_enabled(DIAG_CALC)) {
                        diag_log(DIAG_CALC, 1, "loop iteration %d: new scope created", iter);
                    }

                    // Set lambda_storage_scope so lambdas defined in the loop
                    // are stored in the outer scope but capture the iteration scope
                    calc_ctx->lambda_storage_scope = loop_outer_scope;

                    // Expand calc commands in the body using the shared calc_ctx
                    char *calc_expanded = expand_calc(body, calc_ctx);

                    // Check for exit marker
                    char *exit_marker = strstr(calc_expanded, "@@EXIT_LOOP@@");
                    if (exit_marker) {
                        // Found exit marker - terminate loop
                        // But first, keep the content BEFORE the marker
                        *exit_marker = '\0'; // Truncate at marker

                        if (diag_is_enabled(DIAG_SYSTEM)) {
                            diag_log(DIAG_SYSTEM, 1, "\\begin{%s}: exit at iteration %d", env_name,
                                     iter);
                        }
                        if (diag_is_enabled(DIAG_CALC)) {
                            diag_log(DIAG_CALC, 1, "loop exit marker found at iteration %d", iter);
                        }

                        // Expand macros in the content before exit marker
                        bool inner_did_expand;
                        char *expanded_body = expand_macros_once(calc_expanded, &inner_did_expand,
                                                                 calc_ctx, error_msg, error_size);
                        strbuf_append(&result, expanded_body ? expanded_body : calc_expanded);
                        free(expanded_body);
                        free(calc_expanded);
                        calc_ctx->lambda_storage_scope = NULL;
                        calc_ctx->current_scope = saved_scope;
                        scope_decref(iter_scope);
                        break;
                    }

                    // Then expand macros (including nested enumerate/loop)
                    bool inner_did_expand;
                    char *expanded_body = expand_macros_once(calc_expanded, &inner_did_expand,
                                                             calc_ctx, error_msg, error_size);

                    // Clear lambda_storage_scope
                    calc_ctx->lambda_storage_scope = NULL;

                    // Restore scope and cleanup
                    calc_ctx->current_scope = saved_scope;
                    scope_decref(iter_scope); // May survive if lambda captured it

                    // Append the expanded body to result
                    strbuf_append(&result, expanded_body ? expanded_body : calc_expanded);
                    free(expanded_body);
                    free(calc_expanded);
                }

                free(body);
                p = body_end + end_len; // Skip past \end{loop} or \end{while}
                *did_expand = true;
                continue;
            }
        }

        // NOTE: \begin<arr>[i,item]{enumerate} is now handled in calc.c
        // for correct expansion order (variables bound before body expansion)

        // \quad (2 NBSP)
        if (strncmp(p, "\\quad", 5) == 0 && !isalpha((unsigned char)p[5]) && !in_display_math &&
            !in_inline_math) {
            strbuf_append(&result, NBSP NBSP);
            p += 5;
            *did_expand = true;
            continue;
        }

        // \qquad (4 NBSP)
        if (strncmp(p, "\\qquad", 6) == 0 && !isalpha((unsigned char)p[6]) && !in_display_math &&
            !in_inline_math) {
            strbuf_append(&result, NBSP NBSP NBSP NBSP);
            p += 6;
            *did_expand = true;
            continue;
        }

        // \hskip{n}
        if (strncmp(p, "\\hskip", 6) == 0) {
            const char *q = p + 6;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '{') {
                q++;
                int n = 0;
                while (isdigit((unsigned char)*q)) {
                    n = n * 10 + (*q - '0');
                    q++;
                }
                if (*q == '}') {
                    q++;
                    if (n < 0) n = 0;
                    if (n > 100) n = 100;
                    for (int i = 0; i < n; i++) {
                        strbuf_append(&result, NBSP);
                    }
                    p = q;
                    *did_expand = true;
                    continue;
                }
            }
        }

        // \thinspace
        if (strncmp(p, "\\thinspace", 10) == 0 && !isalpha((unsigned char)p[10])) {
            strbuf_append(&result, NBSP);
            p += 10;
            *did_expand = true;
            continue;
        }

        // Note: Long-form text escapes like \textbackslash, \textdollar, etc.
        // are now handled at render time in compositor/paragraph.c
        // This avoids the problem where the expanded character gets
        // re-interpreted as a command (e.g., \textbackslash -> \ -> \cmd)

        // Note: \{ and \} are now handled at render time in compositor/paragraph.c
        // This ensures they stay as \{ and \} until the final rendering, avoiding
        // the issue where \{\} would become {} and get filtered as an empty group.
        if (p[0] == '\\' && p[1] == '#') {
            strbuf_putc(&result, '#');
            p += 2;
            *did_expand = true;
            continue;
        }
        if (p[0] == '\\' && p[1] == '_') {
            strbuf_putc(&result, '_');
            p += 2;
            *did_expand = true;
            continue;
        }
        if (p[0] == '\\' && p[1] == '&') {
            strbuf_putc(&result, '&');
            p += 2;
            *did_expand = true;
            continue;
        }

        // Regular character
        strbuf_putc(&result, *p++);
    }

    return strbuf_detach(&result);
}

// ============================================================================
// Public API: Full Macro Expansion
// ============================================================================

char *expand_all_macros(const char *input, int width, char *error_msg, int error_size) {
    if (!input) return NULL;

    g_macro_expansion_nesting++;

    // Reuse the shared macro registry if:
    // - We're in a nested call (during macro expansion), OR
    // - The registry is being kept alive (for lineroutine rendering)
    bool reuse_registry = (g_macro_expansion_nesting > 1 || g_macro_keep_alive > 0) &&
                          g_shared_macro_registry != NULL;
    MacroRegistry *user_macros;

    if (reuse_registry) {
        // Reuse existing registry from outer document
        user_macros = g_shared_macro_registry;
    } else {
        // Create new registry for top-level call
        user_macros = macro_registry_new();
        if (!user_macros) {
            g_macro_expansion_nesting--;
            snprintf(error_msg, error_size, "Out of memory");
            return NULL;
        }
        g_shared_macro_registry = user_macros;
    }

    // Create calc context with counter, content, array registries, and scope
    CalcContext calc_ctx = {0};
    calc_context_init(&calc_ctx);
    calc_ctx.user_macros = user_macros;       // For expanding content in \measure
    calc_ctx.width = width > 0 ? width : 80;  // Default to 80 if not specified
    calc_ctx.symbols = g_shared_symbol_table; // For LSP symbol collection

    if (!calc_ctx.counters || !calc_ctx.contents || !calc_ctx.arrays || !calc_ctx.global_scope) {
        snprintf(error_msg, error_size, "Out of memory");
        if (!reuse_registry) {
            macro_registry_free(user_macros);
            g_shared_macro_registry = NULL;
        }
        calc_context_free(&calc_ctx);
        g_macro_expansion_nesting--;
        return NULL;
    }

    // Register standard library macros (only for top-level)
#ifndef HYADES_RENDER_ONLY
    if (!reuse_registry) {
        stdlib_register_macros(user_macros);
        stdlib_register_lambdas(&calc_ctx); // Register STD:: functions
    }
#endif

    // First: extract user macro definitions (only for top-level)
    char *after_defs;
    if (!reuse_registry) {
        after_defs = macro_process_document(input, user_macros, error_msg, error_size);
        if (!after_defs) {
            macro_registry_free(user_macros);
            g_shared_macro_registry = NULL;
            calc_context_free(&calc_ctx);
            g_macro_expansion_nesting--;
            return NULL;
        }

        // Apply top-level hygiene (ID 0) to non-macro code
        // This allows top-level lowercase variables to work with \ref
        char *with_hygiene = macro_apply_toplevel_hygiene(after_defs);
        free(after_defs);
        after_defs = with_hygiene;
    } else {
        // For nested calls, macro definitions were already extracted
        after_defs = strdup(input);
    }

    char *current = after_defs;

    // Iteratively expand until fixed point
    for (int iteration = 0; iteration < MACRO_EXPANSION_MAX_ITERATIONS; iteration++) {
        bool did_expand = false;

        // Transform \begin{X}...\end{X} to \X{...} FIRST
        // This allows environments to work with both user macros and system macros
        bool did_transform = false;
        char *after_transform = transform_begin_end(current, &did_transform);
        if (!after_transform) {
            free(current);
            if (!reuse_registry) {
                macro_registry_free(user_macros);
                g_shared_macro_registry = NULL;
            }
            calc_context_free(&calc_ctx);
            g_macro_expansion_nesting--;
            return NULL;
        }
        if (did_transform) {
            did_expand = true;
        }
        free(current);
        current = after_transform;

        // Expand user macros FIRST so they can generate content for system macros
        // e.g., \grade inside \table expands to \row{...} before \table processes
        if (user_macros->n_macros > 0) {
            char *after_user = macro_expand_all(current, user_macros, error_msg, error_size);
            if (!after_user) {
                free(current);
                if (!reuse_registry) {
                    macro_registry_free(user_macros);
                    g_shared_macro_registry = NULL;
                }
                calc_context_free(&calc_ctx);
                g_macro_expansion_nesting--;
                return NULL;
            }

            if (strcmp(current, after_user) != 0) {
                did_expand = true;
            }

            free(current);
            current = after_user;
        }

        // Then expand system macros (including calc, table, etc.)
        char *after_system =
            expand_macros_once(current, &did_expand, &calc_ctx, error_msg, error_size);
        if (!after_system) {
            free(current);
            if (!reuse_registry) {
                macro_registry_free(user_macros);
                g_shared_macro_registry = NULL;
            }
            calc_context_free(&calc_ctx);
            g_macro_expansion_nesting--;
            return NULL;
        }

        free(current);
        current = after_system;

        if (!did_expand) break;
    }

    // Only free the registry at the top level and if not kept alive
    if (!reuse_registry && g_macro_keep_alive <= 0) {
        macro_registry_free(user_macros);
        g_shared_macro_registry = NULL;
    }
    calc_context_free(&calc_ctx);
    g_macro_expansion_nesting--;
    return current;
}

// ============================================================================
// LSP-Aware Macro Expansion
// ============================================================================

char *expand_all_macros_lsp(const char *input, int width, ParseErrorList *errors,
                            SourceMap *source_map, LspSymbolTable *symbols) {
    // Keep registry alive so we can extract symbols after expansion
    macro_registry_keep_alive(true);

    // Set shared symbol table for calc context to use
    g_shared_symbol_table = symbols;

    char error_msg[256] = {0};
    char *result = expand_all_macros(input, width, error_msg, sizeof(error_msg));

    // Clear shared symbol table
    g_shared_symbol_table = NULL;

    // Report error if expansion failed
    if (!result && error_msg[0] && errors) {
        parse_error_list_add(errors, PARSE_ERR_SYNTAX, 0, 0, 0, 0, "macro", "%s", error_msg);
    }

    // Extract symbols from the macro registry
    if (symbols && g_shared_macro_registry) {
        MacroRegistry *reg = g_shared_macro_registry;
        for (int i = 0; i < reg->n_macros; i++) {
            Macro *m = &reg->macros[i];
            if (!m->name) continue;

            // Skip stdlib macros (they start with a lowercase letter or don't have backslash)
            // User macros typically start with a backslash-escaped name

            // Build signature from arguments
            char sig[256] = {0};
            int sig_pos = 0;
            for (int j = 0; j < m->n_args && sig_pos < 250; j++) {
                if (m->args[j].is_optional) {
                    sig_pos += snprintf(sig + sig_pos, sizeof(sig) - sig_pos, "[%s%s%s]",
                                        m->args[j].name, m->args[j].default_value ? "=" : "",
                                        m->args[j].default_value ? m->args[j].default_value : "");
                } else {
                    sig_pos +=
                        snprintf(sig + sig_pos, sizeof(sig) - sig_pos, "{%s}", m->args[j].name);
                }
            }

            // Add symbol with position from macro definition
            Symbol *sym = lsp_symbol_table_add(symbols, m->name, SYMKIND_MACRO, m->def_line,
                                               m->def_col, m->def_end_line, m->def_end_col);
            if (sym) {
                symbol_set_signature(sym, sig);
                // Truncate body preview if too long
                if (m->body && strlen(m->body) > 100) {
                    char preview[104];
                    strncpy(preview, m->body, 100);
                    strcpy(preview + 100, "...");
                    symbol_set_body_preview(sym, preview);
                } else if (m->body) {
                    symbol_set_body_preview(sym, m->body);
                }
            }
        }
    }

    // Release registry keep-alive
    macro_registry_keep_alive(false);

    // Fix symbol positions by scanning original source
    // The positions collected during expansion are relative to transformed text,
    // so we need to find actual positions in the original source.
    // Macro names are stored WITHOUT leading backslash (e.g., "mymacro" not "\mymacro")
    if (symbols && source_map && source_map->original_source) {
        const char *orig = source_map->original_source;
        int n_syms = lsp_symbol_table_count(symbols);

        for (int i = 0; i < n_syms; i++) {
            Symbol *sym = (Symbol *)lsp_symbol_table_get(symbols, i);
            if (!sym || sym->kind != SYMKIND_MACRO || !sym->name) continue;

            // Build search pattern: \macro<\NAME where NAME is the macro name
            // The macro syntax is: \macro<\NAME[opts]{body}
            char pattern[128];
            snprintf(pattern, sizeof(pattern), "\\macro<\\%s", sym->name);

            // Search original source for this pattern, skipping rendered output lines
            // Rendered output lines contain NBSP (non-breaking space, UTF-8: 0xC2 0xA0)
            const char *search_start = orig;
            const char *found = NULL;

            while ((found = strstr(search_start, pattern)) != NULL) {
                // Check word boundary - macro name must be followed by '[', '{', or '>'
                // to avoid matching "indent" when looking for "indented" etc.
                size_t pattern_len = strlen(pattern);
                char next_char = found[pattern_len];
                if (next_char != '[' && next_char != '{' && next_char != '>') {
                    // Not a full match, continue searching
                    search_start = found + 1;
                    found = NULL;
                    continue;
                }

                // Find line start
                const char *line_start = found;
                while (line_start > orig && line_start[-1] != '\n') {
                    line_start--;
                }

                // Find line end
                const char *line_end = found;
                while (*line_end && *line_end != '\n') {
                    line_end++;
                }

                // Check if line contains NBSP (0xC2 0xA0)
                bool has_nbsp = false;
                for (const char *p = line_start; p < line_end - 1; p++) {
                    if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0xA0) {
                        has_nbsp = true;
                        break;
                    }
                }

                if (!has_nbsp) {
                    // Found a valid match not in rendered output
                    break;
                }

                // Continue searching after this match
                search_start = found + 1;
                found = NULL;
            }

            // If not found, this is likely a stdlib macro - leave position as-is
            if (found) {
                // Calculate line and column
                int line = 1, col = 1;
                for (const char *p = orig; p < found; p++) {
                    if (*p == '\n') {
                        line++;
                        col = 1;
                    } else {
                        col++;
                    }
                }

                // Look backwards for %%% doc comment block
                // Find the start of the line containing 'found'
                const char *line_start = found;
                while (line_start > orig && line_start[-1] != '\n') {
                    line_start--;
                }

                // Now look backwards for consecutive %%% lines
                const char *doc_end = line_start; // End of doc block (exclusive)
                const char *doc_start = line_start;

                // Go to previous line
                if (doc_start > orig) {
                    doc_start--; // Skip the newline before current line
                    while (doc_start > orig && doc_start[-1] != '\n') {
                        doc_start--;
                    }

                    // Check if this line starts with %%% (after optional whitespace)
                    const char *check = doc_start;
                    while (*check == ' ' || *check == '\t') check++;

                    if (strncmp(check, "%%%", 3) == 0) {
                        // Found a doc comment line, keep looking backwards
                        const char *first_doc_line = doc_start;

                        while (first_doc_line > orig) {
                            // Go to previous line
                            const char *prev_line = first_doc_line - 1;
                            while (prev_line > orig && prev_line[-1] != '\n') {
                                prev_line--;
                            }

                            // Check if it starts with %%%
                            check = prev_line;
                            while (*check == ' ' || *check == '\t') check++;

                            if (strncmp(check, "%%%", 3) == 0) {
                                first_doc_line = prev_line;
                            } else {
                                break; // Not a doc line, stop
                            }
                        }

                        doc_start = first_doc_line;

                        // Extract doc comment text
                        StrBuf doc_buf;
                        strbuf_init(&doc_buf);

                        const char *p = doc_start;
                        while (p < doc_end) {
                            // Skip leading whitespace
                            while (*p == ' ' || *p == '\t') p++;

                            // Skip %%%
                            if (strncmp(p, "%%%", 3) == 0) {
                                p += 3;
                                // Skip one space after %%% if present
                                if (*p == ' ') p++;

                                // Copy rest of line
                                while (*p && *p != '\n') {
                                    strbuf_putc(&doc_buf, *p++);
                                }
                                strbuf_putc(&doc_buf, '\n');
                            }

                            // Skip to next line
                            while (*p && *p != '\n') p++;
                            if (*p == '\n') p++;
                        }

                        // Remove trailing newline
                        char *doc_text = strbuf_detach(&doc_buf);
                        if (doc_text) {
                            size_t len = strlen(doc_text);
                            while (len > 0 &&
                                   (doc_text[len - 1] == '\n' || doc_text[len - 1] == ' ')) {
                                doc_text[--len] = '\0';
                            }
                            if (len > 0) {
                                symbol_set_doc(sym, doc_text);
                            }
                            free(doc_text);
                        }
                    }
                }

                // Find end of macro definition (closing brace at same depth)
                const char *end = found;
                int brace_depth = 0;
                bool in_def = false;
                while (*end) {
                    if (*end == '{') {
                        if (!in_def) in_def = true;
                        brace_depth++;
                    } else if (*end == '}') {
                        brace_depth--;
                        if (in_def && brace_depth == 0) {
                            end++;
                            break;
                        }
                    }
                    end++;
                }

                // Calculate end position
                int end_line = line, end_col = col;
                for (const char *p = found; p < end; p++) {
                    if (*p == '\n') {
                        end_line++;
                        end_col = 1;
                    } else {
                        end_col++;
                    }
                }

                // Update symbol position
                sym->def_line = line;
                sym->def_col = col;
                sym->def_end_line = end_line;
                sym->def_end_col = end_col;
            }
        }
    }

    // Scan original source for macro/lambda references and add them to the symbol table
    // This enables hover on macro calls, \begin{macro}, \recall<name>, \valueof<name>, etc.
    if (symbols && source_map && source_map->original_source) {
        const char *orig = source_map->original_source;
        const char *p = orig;
        int line = 1, col = 1;

        while (*p) {
            // Skip lines with NBSP (rendered output)
            if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0xA0) {
                // Skip to end of line
                while (*p && *p != '\n') p++;
                if (*p == '\n') {
                    line++;
                    col = 1;
                    p++;
                }
                continue;
            }

            // Skip \macro< definitions (we want calls, not definitions)
            if (strncmp(p, "\\macro<", 7) == 0) {
                // Skip to end of macro definition
                int brace_depth = 0;
                bool in_def = false;
                while (*p) {
                    if (*p == '\n') {
                        line++;
                        col = 1;
                    } else {
                        col++;
                    }
                    if (*p == '{') {
                        in_def = true;
                        brace_depth++;
                    } else if (*p == '}') {
                        brace_depth--;
                        if (in_def && brace_depth == 0) {
                            p++;
                            break;
                        }
                    }
                    p++;
                }
                continue;
            }

            // Match \begin[opts]{NAME} or \begin{NAME}
            if (strncmp(p, "\\begin", 6) == 0) {
                int start_line = line, start_col = col;
                p += 6;
                col += 6;

                // Skip optional [opts]
                if (*p == '[') {
                    int bracket_depth = 1;
                    p++;
                    col++;
                    while (*p && bracket_depth > 0) {
                        if (*p == '\n') {
                            line++;
                            col = 1;
                        } else {
                            col++;
                        }
                        if (*p == '[')
                            bracket_depth++;
                        else if (*p == ']')
                            bracket_depth--;
                        p++;
                    }
                }

                // Extract {NAME}
                if (*p == '{') {
                    p++;
                    col++;
                    const char *name_start = p;
                    while (*p && *p != '}' && *p != '\n') {
                        p++;
                        col++;
                    }
                    if (*p == '}') {
                        size_t name_len = p - name_start;
                        if (name_len > 0 && name_len < 64) {
                            char name[64];
                            memcpy(name, name_start, name_len);
                            name[name_len] = '\0';

                            // Check if this is a known symbol
                            if (lsp_symbol_table_find(symbols, name)) {
                                lsp_symbol_table_add_reference(symbols, name, start_line, start_col,
                                                               line, col + 1, false);
                            }
                        }
                        p++;
                        col++;
                    }
                }
                continue;
            }

            // Match \recall<NAME> or \valueof<NAME> or \call<NAME>
            bool is_recall = strncmp(p, "\\recall<", 8) == 0;
            bool is_valueof = strncmp(p, "\\valueof<", 9) == 0;
            bool is_call = strncmp(p, "\\call<", 6) == 0;
            if (is_recall || is_valueof || is_call) {
                int start_col = col;
                int cmd_len = is_recall ? 8 : (is_valueof ? 9 : 6);
                p += cmd_len;
                col += cmd_len;

                const char *name_start = p;
                while (*p && *p != '>' && *p != '\n') {
                    p++;
                    col++;
                }
                if (*p == '>') {
                    size_t name_len = p - name_start;
                    if (name_len > 0 && name_len < 64) {
                        char name[64];
                        memcpy(name, name_start, name_len);
                        name[name_len] = '\0';

                        // Try to find symbol directly (handles STD::NAME and regular names)
                        const char *lookup_name = name;

                        // If name is all uppercase without STD::, try adding STD:: prefix
                        if (strncmp(name, "STD::", 5) != 0) {
                            bool all_upper = true;
                            for (const char *c = name; *c; c++) {
                                if (*c >= 'a' && *c <= 'z') {
                                    all_upper = false;
                                    break;
                                }
                            }
                            if (all_upper) {
                                char prefixed_name[80];
                                snprintf(prefixed_name, sizeof(prefixed_name), "STD::%s", name);
                                if (lsp_symbol_table_find(symbols, prefixed_name)) {
                                    lookup_name = prefixed_name;
                                }
                            }
                        }

                        if (lsp_symbol_table_find(symbols, lookup_name)) {
                            lsp_symbol_table_add_reference(symbols, lookup_name, line, start_col,
                                                           line, col + 1, false);
                        }
                    }
                    p++;
                    col++;
                }
                continue;
            }

            // Match \NAME macro calls (backslash followed by alphanumeric)
            if (*p == '\\' && (isalpha((unsigned char)p[1]) || p[1] == '_')) {
                int start_col = col;
                p++;
                col++;

                const char *name_start = p;
                while (*p && (isalnum((unsigned char)*p) || *p == '_')) {
                    p++;
                    col++;
                }
                size_t name_len = p - name_start;

                if (name_len > 0 && name_len < 64) {
                    char name[64];
                    memcpy(name, name_start, name_len);
                    name[name_len] = '\0';

                    // Skip common non-macro commands
                    if (strcmp(name, "begin") != 0 && strcmp(name, "end") != 0 &&
                        strcmp(name, "macro") != 0 && strcmp(name, "let") != 0 &&
                        strcmp(name, "inc") != 0 && strcmp(name, "dec") != 0 &&
                        strcmp(name, "assign") != 0 && strcmp(name, "measure") != 0 &&
                        strcmp(name, "recall") != 0 && strcmp(name, "valueof") != 0 &&
                        strcmp(name, "lambda") != 0 && strcmp(name, "call") != 0 &&
                        strcmp(name, "if") != 0 && strcmp(name, "else") != 0) {

                        // Check if this is a known symbol (macro)
                        if (lsp_symbol_table_find(symbols, name)) {
                            lsp_symbol_table_add_reference(symbols, name, line, start_col, line,
                                                           col, false);
                        }
                    }
                }
                continue;
            }

            // Advance to next character
            if (*p == '\n') {
                line++;
                col = 1;
            } else {
                col++;
            }
            p++;
        }
    }

    // Fix lambda positions and extract doc comments (similar to macros)
    if (symbols && source_map && source_map->original_source) {
        const char *orig = source_map->original_source;
        int n_syms = lsp_symbol_table_count(symbols);

        for (int i = 0; i < n_syms; i++) {
            Symbol *sym = (Symbol *)lsp_symbol_table_get(symbols, i);
            if (!sym || sym->kind != SYMKIND_LAMBDA || !sym->name) continue;

            // Skip stdlib lambdas (STD:: prefix) - they don't have source positions
            if (strncmp(sym->name, "STD::", 5) == 0) continue;

            // Build search pattern: \lambda<NAME where NAME is the lambda name
            char pattern[128];
            snprintf(pattern, sizeof(pattern), "\\lambda<%s", sym->name);

            // Search original source for this pattern
            const char *search_start = orig;
            const char *found = NULL;

            while ((found = strstr(search_start, pattern)) != NULL) {
                // Check word boundary
                size_t pattern_len = strlen(pattern);
                char next_char = found[pattern_len];
                if (next_char != '[' && next_char != '>' && next_char != ']') {
                    search_start = found + 1;
                    found = NULL;
                    continue;
                }

                // Check if line contains NBSP (skip rendered output)
                const char *line_start = found;
                while (line_start > orig && line_start[-1] != '\n') line_start--;
                const char *line_end = found;
                while (*line_end && *line_end != '\n') line_end++;

                bool has_nbsp = false;
                for (const char *p = line_start; p < line_end - 1; p++) {
                    if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0xA0) {
                        has_nbsp = true;
                        break;
                    }
                }

                if (!has_nbsp) break;
                search_start = found + 1;
                found = NULL;
            }

            if (found) {
                // Calculate line and column
                int line = 1, col = 1;
                for (const char *p = orig; p < found; p++) {
                    if (*p == '\n') {
                        line++;
                        col = 1;
                    } else {
                        col++;
                    }
                }

                // Look for %%% doc comments before the lambda
                const char *doc_end = found;
                while (doc_end > orig && doc_end[-1] != '\n') doc_end--;

                if (doc_end > orig) {
                    const char *check_line = doc_end - 1;
                    while (check_line > orig && check_line[-1] != '\n') check_line--;

                    const char *check = check_line;
                    while (*check == ' ' || *check == '\t') check++;

                    if (strncmp(check, "%%%", 3) == 0) {
                        // Found doc comment, collect all consecutive %%% lines
                        const char *first_doc_line = check_line;
                        while (first_doc_line > orig) {
                            const char *prev = first_doc_line - 1;
                            while (prev > orig && prev[-1] != '\n') prev--;
                            const char *pcheck = prev;
                            while (*pcheck == ' ' || *pcheck == '\t') pcheck++;
                            if (strncmp(pcheck, "%%%", 3) == 0) {
                                first_doc_line = prev;
                            } else {
                                break;
                            }
                        }

                        // Extract doc text
                        StrBuf doc_buf;
                        strbuf_init(&doc_buf);
                        const char *p = first_doc_line;
                        while (p < doc_end) {
                            while (*p == ' ' || *p == '\t') p++;
                            if (strncmp(p, "%%%", 3) == 0) {
                                p += 3;
                                if (*p == ' ') p++;
                                while (*p && *p != '\n') strbuf_putc(&doc_buf, *p++);
                                strbuf_putc(&doc_buf, '\n');
                            }
                            while (*p && *p != '\n') p++;
                            if (*p == '\n') p++;
                        }

                        char *doc_text = strbuf_detach(&doc_buf);
                        if (doc_text) {
                            size_t len = strlen(doc_text);
                            while (len > 0 &&
                                   (doc_text[len - 1] == '\n' || doc_text[len - 1] == ' '))
                                doc_text[--len] = '\0';
                            if (len > 0) symbol_set_doc(sym, doc_text);
                            free(doc_text);
                        }
                    }
                }

                // Update symbol position
                sym->def_line = line;
                sym->def_col = col;
            }
        }
    }

    // Add documentation for stdlib lambdas and macros
    if (symbols) {
        static const struct {
            const char *name;
            const char *doc;
        } stdlib_docs[] = {
            // Array generators
            {"STD::RANGE",
             "Generate array of integers from start to end (inclusive).\nUsage: "
             "\\recall<STD::RANGE>[start,end]\nExample: \\recall<STD::RANGE>[1,5] → [1,2,3,4,5]"},
            {"STD::IOTA", "Generate array with custom step.\nUsage: "
                          "\\recall<STD::IOTA>[start,end,step]\nExample: "
                          "\\recall<STD::IOTA>[0,10,2] → [0,2,4,6,8,10]"},
            {"STD::REPEAT",
             "Create array with value repeated n times.\nUsage: "
             "\\recall<STD::REPEAT>[value,n]\nExample: \\recall<STD::REPEAT>[x,3] → [x,x,x]"},
            {"STD::FILL", "Create array of n zeros.\nUsage: \\recall<STD::FILL>[n]"},

            // Array operations
            {"STD::COPY", "Create a copy of an array.\nUsage: \\recall<STD::COPY>[array]"},
            {"STD::CONTAINS",
             "Check if array contains a value.\nUsage: "
             "\\recall<STD::CONTAINS>[array,value]\nReturns: 1 if found, 0 otherwise"},
            {"STD::INDEX_OF",
             "Find index of value in array.\nUsage: \\recall<STD::INDEX_OF>[array,value]\nReturns: "
             "index or -1 if not found"},
            {"STD::LEN", "Get length of an array.\nUsage: \\recall<STD::LEN>[array]"},
            {"STD::REVERSE", "Reverse an array.\nUsage: \\recall<STD::REVERSE>[array]"},
            {"STD::SLICE",
             "Extract a portion of an array.\nUsage: \\recall<STD::SLICE>[array,start,end]"},
            {"STD::TAKE", "Take first n elements.\nUsage: \\recall<STD::TAKE>[array,n]"},
            {"STD::DROP", "Drop first n elements.\nUsage: \\recall<STD::DROP>[array,n]"},
            {"STD::JOIN",
             "Join array elements with separator.\nUsage: \\recall<STD::JOIN>[array,sep]"},

            // Math operations
            {"STD::SUM", "Sum all elements in array.\nUsage: \\recall<STD::SUM>[array]"},
            {"STD::PRODUCT",
             "Multiply all elements in array.\nUsage: \\recall<STD::PRODUCT>[array]"},
            {"STD::MIN", "Find minimum value in array.\nUsage: \\recall<STD::MIN>[array]"},
            {"STD::MAX", "Find maximum value in array.\nUsage: \\recall<STD::MAX>[array]"},
            {"STD::AVG", "Calculate average of array elements.\nUsage: \\recall<STD::AVG>[array]"},
            {"STD::ABS", "Absolute value.\nUsage: \\recall<STD::ABS>[n]"},
            {"STD::SIGN", "Sign of number (-1, 0, or 1).\nUsage: \\recall<STD::SIGN>[n]"},
            {"STD::CLAMP", "Clamp value to range.\nUsage: \\recall<STD::CLAMP>[value,min,max]"},
            {"STD::POW", "Power function.\nUsage: \\recall<STD::POW>[base,exp]"},
            {"STD::FACTORIAL", "Factorial (n!).\nUsage: \\recall<STD::FACTORIAL>[n]"},
            {"STD::GCD", "Greatest common divisor.\nUsage: \\recall<STD::GCD>[a,b]"},
            {"STD::LCM", "Least common multiple.\nUsage: \\recall<STD::LCM>[a,b]"},
            {"STD::FIB", "Fibonacci number.\nUsage: \\recall<STD::FIB>[n]"},

            // Higher-order functions
            {"STD::MAP", "Apply function to each element.\nUsage: \\recall<STD::MAP>[array,func]"},
            {"STD::FILTER",
             "Filter elements by predicate.\nUsage: \\recall<STD::FILTER>[array,pred]"},
            {"STD::COUNT",
             "Count elements matching predicate.\nUsage: \\recall<STD::COUNT>[array,pred]"},
            {"STD::SORT", "Sort array ascending.\nUsage: \\recall<STD::SORT>[array]"},
            {"STD::SORT_DESC", "Sort array descending.\nUsage: \\recall<STD::SORT_DESC>[array]"},

            // Predicates
            {"STD::IS_POSITIVE",
             "Check if number is positive.\nUsage: \\recall<STD::IS_POSITIVE>[n]"},
            {"STD::IS_NEGATIVE",
             "Check if number is negative.\nUsage: \\recall<STD::IS_NEGATIVE>[n]"},
            {"STD::IS_ZERO", "Check if number is zero.\nUsage: \\recall<STD::IS_ZERO>[n]"},
            {"STD::IS_EVEN", "Check if number is even.\nUsage: \\recall<STD::IS_EVEN>[n]"},
            {"STD::IS_ODD", "Check if number is odd.\nUsage: \\recall<STD::IS_ODD>[n]"},

            // Formatting
            {"STD::ORDINAL",
             "Convert number to ordinal (1st, 2nd, 3rd...).\nUsage: \\recall<STD::ORDINAL>[n]"},
            {"STD::ROMAN", "Convert number to Roman numerals.\nUsage: \\recall<STD::ROMAN>[n]"},

            // Common stdlib macros
            {"center", "Center content horizontally.\nUsage: \\center{content}"},
            {"right", "Right-align content.\nUsage: \\right{content}"},
            {"indent", "Indent content.\nUsage: \\indent[n]{content}"},
            {"frame", "Draw a frame around content.\nUsage: \\frame[style]{content}"},
            {"Tightframe", "Compact frame around content.\nUsage: \\Tightframe{content}"},
            {"Boxed", "Box with optional subtitle.\nUsage: \\Boxed[desc]{content}"},
            {"title", "Document title.\nUsage: \\title{text}"},
            {"section", "Section heading.\nUsage: \\section{text}"},
            {"subsection", "Subsection heading.\nUsage: \\subsection{text}"},

            {NULL, NULL}};

        for (int i = 0; stdlib_docs[i].name; i++) {
            Symbol *sym = lsp_symbol_table_find(symbols, stdlib_docs[i].name);
            if (sym && !sym->doc_comment) {
                symbol_set_doc(sym, stdlib_docs[i].doc);
            }
        }
    }

    return result;
}

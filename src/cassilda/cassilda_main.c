// cassilda_main.c - Cassilda command-line interface

#include "cassilda.h"
#include "label_library.h"
#include "math/renderer/render_opts.h"
#include "math/renderer/symbols.h"
#include "public_api/hyades_parse_api.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// File I/O Helpers
// ============================================================================

// Strip carriage returns in-place (CRLF → LF)
static void strip_cr(char *s) {
    char *w = s;
    for (const char *r = s; *r; r++) {
        if (*r != '\r') *w++ = *r;
    }
    *w = '\0';
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(content, 1, size, f);
    content[read] = '\0';
    fclose(f);

    strip_cr(content);
    return content;
}

static bool write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);

    return written == len;
}

static char *read_stdin(void) {
    size_t capacity = 4096;
    size_t len = 0;
    char *content = malloc(capacity);

    int c;
    while ((c = getchar()) != EOF) {
        if (len + 1 >= capacity) {
            capacity *= 2;
            content = realloc(content, capacity);
        }
        content[len++] = (char)c;
    }
    content[len] = '\0';

    strip_cr(content);
    return content;
}

// ============================================================================
// Usage
// ============================================================================

static void print_usage(const char *program) {
    fprintf(
        stderr,
        "Cassilda - Hyades Document Embedder\n"
        "\n"
        "Usage:\n"
        "  %s [options] process <file>       Process file and update in place\n"
        "  %s [options] process -            Read from stdin, write to stdout\n"
        "  %s clean <file>                   Remove all rendered output from file\n"
        "  %s check <file>                   Check if file is up-to-date (for CI)\n"
        "  %s render <file> <seg>            Render a single segment to stdout\n"
        "  %s list <file>                    List all segments in file\n"
        "  %s lsp-debug [-n N] <file|->      Run LSP parse path (for debugging)\n"
        "  %s help                           Show this help message\n"
        "\n"
        "Options (for process command):\n"
        "  --config <path>        Use specific .cassilda.json config file\n"
        "  --find-config          Search parent directories for .cassilda.json\n"
        "  --filename <name>      Hint filename for stdin (infers comment char, target prefix)\n"
        "  --rebuild-cache        Force rebuild of label cache\n"
        "  --no-cache             Disable caching (always scan files)\n"
        "  --verbose              Print discovery and processing information\n"
        "\n"
        "Exit codes:\n"
        "  0  Success\n"
        "  1  Error (parse error, segment not found, etc.)\n"
        "  2  Check failed (file is out of date)\n"
        "\n"
        "Label Libraries:\n"
        "  Cassilda can find labels in external .cld files using:\n"
        "    1. Config file (.cassilda.json) specifying library paths\n"
        "    2. Automatic scanning of .cld files in same directory\n"
        "    3. Caching for fast lookup in large projects\n"
        "\n"
        "Example .cassilda.json:\n"
        "  {\n"
        "    \"library_dirs\": [\"docs/formulas\", \"lib\"],\n"
        "    \"library_files\": [\"common/header.cld\"],\n"
        "    \"include_subdirs\": false,\n"
        "    \"cache_path\": \".cassilda-cache\"\n"
        "  }\n"
        "\n",
        program, program, program, program, program, program, program, program);
}

// ============================================================================
// Commands
// ============================================================================

// Get default comment character for file extension
static const char *get_default_comment_char_for_file(const char *filename) {
    if (!filename || strcmp(filename, "-") == 0) return NULL;

    const char *ext = strrchr(filename, '.');
    if (!ext) return NULL;

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

    // SQL/Lua comments
    if (strcmp(ext, ".sql") == 0 || strcmp(ext, ".lua") == 0) {
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

    return NULL;
}

// Helper to prepend default comment_char if not present
static char *maybe_add_default_comment_char(const char *input, const char *filename) {
    if (!filename || strcmp(filename, "-") == 0) return (char *)input;
    if (strstr(input, "#comment_char") != NULL) return (char *)input;

    const char *default_cc = get_default_comment_char_for_file(filename);
    if (!default_cc) return (char *)input;

    size_t directive_len = strlen("#comment_char \"") + strlen(default_cc) + strlen("\"\n");
    size_t input_len = strlen(input);
    size_t total_len = directive_len + input_len + 1;

    char *result = malloc(total_len);
    snprintf(result, total_len, "#comment_char \"%s\"\n%s", default_cc, input);
    return result;
}

static int cmd_process(const char *path, const LibraryOptions *lib_opts,
                       const char *filename_hint) {
    char *input;
    bool from_stdin = (strcmp(path, "-") == 0);

    if (from_stdin) {
        input = read_stdin();
    } else {
        input = read_file(path);
    }

    if (!input) {
        fprintf(stderr, "Error: Cannot read %s\n", from_stdin ? "stdin" : path);
        return 1;
    }

    // When reading from stdin, use filename_hint for extension-based inference
    const char *effective_filename = from_stdin ? filename_hint : path;

    // Check for #!clear directive - if present, just clean and return
    if (strstr(input, "#!clear") != NULL) {
        char *output = cassilda_clean(input, NULL);
        free(input);

        if (!output) {
            fprintf(stderr, "Error: Failed to clean\n");
            return 1;
        }

        if (from_stdin) {
            printf("%s", output);
        } else {
            if (!write_file(path, output)) {
                fprintf(stderr, "Error: Cannot write to %s\n", path);
                free(output);
                return 1;
            }
        }

        free(output);
        return 0;
    }

    // Initialize library context if needed
    LibraryContext lib_ctx;
    bool using_library = false;

    if (lib_opts) {
        library_context_init(&lib_ctx, lib_opts);

        char error[512];
        if (!library_discover(&lib_ctx, error, sizeof(error))) {
            fprintf(stderr, "Warning: Library discovery failed: %s\n", error);
            fprintf(stderr, "Continuing with labels in current file only...\n");
            // Continue anyway - we can still process local labels
        } else {
            using_library = true;
        }
    }

    // Look up target_prefix from config before parsing (for rendered output detection)
    const char *target_prefix = NULL;
    if (using_library && effective_filename) {
        target_prefix = library_lookup_target_prefix(&lib_ctx.config, effective_filename);
    }

    // Parse document (with filename for default comment_char inference)
    char error[512] = {0};
    CassildaDocument *doc =
        cassilda_parse(input, effective_filename, target_prefix, error, sizeof(error));
    free(input);

    if (!doc) {
        if (lib_opts) {
            library_context_free(&lib_ctx);
        }
        fprintf(stderr, "Error: %s\n", error);
        return 1;
    }

    // Process document
    char *output;
    if (using_library) {
        output = cassilda_process_with_library(doc, &lib_ctx, error, sizeof(error));
    } else {
        output = cassilda_process(doc, error, sizeof(error));
    }

    cassilda_free(doc);

    // Clean up library context
    if (lib_opts) {
        library_context_free(&lib_ctx);
    }

    if (!output) {
        fprintf(stderr, "Error: %s\n", error);
        return 1;
    }

    if (from_stdin) {
        printf("%s", output);
    } else {
        if (!write_file(path, output)) {
            fprintf(stderr, "Error: Cannot write to %s\n", path);
            free(output);
            return 1;
        }
    }

    free(output);
    return 0;
}

static int cmd_check(const char *path) {
    char *input = read_file(path);
    if (!input) {
        fprintf(stderr, "Error: Cannot read %s\n", path);
        return 1;
    }

    char error[512] = {0};
    bool up_to_date = cassilda_check(input, error, sizeof(error));
    free(input);

    if (error[0]) {
        fprintf(stderr, "Error: %s\n", error);
        return 1;
    }

    if (up_to_date) {
        printf("%s is up to date\n", path);
        return 0;
    } else {
        printf("%s is OUT OF DATE - run 'cassilda process %s' to update\n", path, path);
        return 2;
    }
}

static int cmd_clean(const char *path) {
    char *input = read_file(path);
    if (!input) {
        fprintf(stderr, "Error: Cannot read %s\n", path);
        return 1;
    }

    // Clean rendered output (remove lines with NBSP)
    char *output = cassilda_clean(input, NULL);
    free(input);

    if (!output) {
        fprintf(stderr, "Error: Failed to clean %s\n", path);
        return 1;
    }

    if (!write_file(path, output)) {
        fprintf(stderr, "Error: Cannot write to %s\n", path);
        free(output);
        return 1;
    }

    free(output);
    printf("Cleaned rendered output from %s\n", path);
    return 0;
}

static int cmd_render(const char *path, const char *segment_name) {
    char *input = read_file(path);
    if (!input) {
        fprintf(stderr, "Error: Cannot read %s\n", path);
        return 1;
    }

    char error[512] = {0};
    CassildaDocument *doc = cassilda_parse(input, path, NULL, error, sizeof(error));
    free(input);

    if (!doc) {
        fprintf(stderr, "Error: %s\n", error);
        return 1;
    }

    char *rendered = cassilda_render_segment(doc, segment_name, error, sizeof(error));
    cassilda_free(doc);

    if (!rendered) {
        fprintf(stderr, "Error: %s\n", error);
        return 1;
    }

    printf("%s", rendered);
    free(rendered);
    return 0;
}

static int cmd_list(const char *path) {
    char *input = read_file(path);
    if (!input) {
        fprintf(stderr, "Error: Cannot read %s\n", path);
        return 1;
    }

    char error[512] = {0};
    CassildaDocument *doc = cassilda_parse(input, path, NULL, error, sizeof(error));
    free(input);

    if (!doc) {
        fprintf(stderr, "Error: %s\n", error);
        return 1;
    }

    printf("Segments in %s:\n", path);
    for (int i = 0; i < doc->n_segments; i++) {
        printf("  @label %s\n", doc->segments[i].name);
    }

    if (doc->n_segments == 0) {
        printf("  (none)\n");
    }

    printf("\nReferences:\n");
    for (int i = 0; i < doc->n_references; i++) {
        printf("  @cassilda:");
        for (int j = 0; j < doc->references[i].n_labels; j++) {
            printf(" %s", doc->references[i].labels[j]);
            if (j < doc->references[i].n_labels - 1) printf(",");
        }
        printf(" (line %d)\n", doc->references[i].line_number + 1);
    }

    if (doc->n_references == 0) {
        printf("  (none)\n");
    }

    cassilda_free(doc);
    return 0;
}

// ============================================================================
// LSP Debug Mode
// ============================================================================

static int cmd_lsp_debug(const char *path, int iterations) {
    char *input;
    bool from_stdin = (strcmp(path, "-") == 0);

    if (from_stdin) {
        input = read_stdin();
    } else {
        input = read_file(path);
    }

    if (!input) {
        fprintf(stderr, "Error: Cannot read %s\n", from_stdin ? "stdin" : path);
        return 1;
    }

    fprintf(stderr, "LSP Debug Mode: %zu bytes, %d iterations\n", strlen(input), iterations);

    for (int i = 0; i < iterations; i++) {
        if (iterations > 1 && i % 100 == 0) {
            fprintf(stderr, "Iteration %d...\n", i);
        }

        // This exercises the exact same code path as the LSP
        HyadesParseResult *result = hyades_parse_for_lsp(input);

        if (!result) {
            fprintf(stderr, "CRASH: Parse returned NULL at iteration %d\n", i);
            free(input);
            return 1;
        }

        // Exercise JSON output paths (where the bug might be)
        char *diag_json = hyades_errors_to_json(result);
        if (diag_json) {
            if (i == 0) {
                fprintf(stderr, "Errors: %d, Warnings: %d\n", hyades_error_count(result),
                        hyades_warning_count(result));
                fprintf(stderr, "Diagnostics: %s\n", diag_json);
            }
            free(diag_json);
        }

        char *sym_json = hyades_symbols_to_json(result);
        if (sym_json) {
            if (i == 0) {
                int n = hyades_symbol_count(result);
                fprintf(stderr, "Symbols: %d\n", n);
                // Print all symbols for debugging
                for (int j = 0; j < n; j++) {
                    const Symbol *sym = hyades_symbol_at(result, j);
                    if (sym) {
                        fprintf(stderr, "  [%d] %s (%s) at %d:%d-%d:%d\n", j, sym->name,
                                symbol_kind_name(sym->kind), sym->def_line, sym->def_col,
                                sym->def_end_line, sym->def_end_col);
                    }
                }

                // Test hover at every line
                fprintf(stderr, "\nHover at every line (col 5):\n");
                for (int line = 1; line <= 40; line++) {
                    char *hover = hyades_hover_to_json(result, line, 5);
                    if (hover && strcmp(hover, "null") != 0) {
                        // Extract just the macro name from the JSON
                        char *name_start = strstr(hover, "**");
                        if (name_start) {
                            name_start += 2;
                            char *name_end = strstr(name_start, "**");
                            if (name_end) {
                                int name_len = (int)(name_end - name_start);
                                fprintf(stderr, "  Line %d: %.*s\n", line, name_len, name_start);
                                // Print full hover JSON for first match to verify doc comments
                                static int first = 1;
                                if (first) {
                                    first = 0;
                                    fprintf(stderr, "  Full hover JSON:\n%s\n", hover);
                                }
                            }
                        }
                    }
                    if (hover) free(hover);
                }
            }
            free(sym_json);
        }

        hyades_parse_result_free(result);
    }

    fprintf(stderr, "Done - no crash detected\n");
    free(input);
    return 0;
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char **argv) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    // Initialize symbol tables (required for rendering)
    symbols_init();
    set_unicode_mode(1);
    set_render_mode(MODE_UNICODE);
    set_math_cursive_mode(1);

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Parse options
    LibraryOptions lib_opts;
    library_options_init(&lib_opts);
    const char *filename_hint = NULL;

    int arg_idx = 1;
    while (arg_idx < argc && argv[arg_idx][0] == '-' && argv[arg_idx][1] == '-') {
        const char *opt = argv[arg_idx];

        if (strcmp(opt, "--config") == 0) {
            if (arg_idx + 1 >= argc) {
                fprintf(stderr, "Error: --config requires a path argument\n");
                return 1;
            }
            lib_opts.config_path = argv[++arg_idx];
        } else if (strcmp(opt, "--find-config") == 0) {
            lib_opts.find_config = true;
        } else if (strcmp(opt, "--filename") == 0) {
            if (arg_idx + 1 >= argc) {
                fprintf(stderr, "Error: --filename requires a name argument\n");
                return 1;
            }
            filename_hint = argv[++arg_idx];
        } else if (strcmp(opt, "--rebuild-cache") == 0) {
            lib_opts.rebuild_cache = true;
        } else if (strcmp(opt, "--no-cache") == 0) {
            lib_opts.no_cache = true;
        } else if (strcmp(opt, "--verbose") == 0) {
            lib_opts.verbose = true;
        } else if (strcmp(opt, "--help") == 0 || strcmp(opt, "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", opt);
            return 1;
        }

        arg_idx++;
    }

    if (arg_idx >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[arg_idx++];

    // Handle commands that don't take options
    if (strcmp(cmd, "help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(cmd, "lsp-debug") == 0) {
        int iterations = 1;
        // Check for -n flag
        if (arg_idx < argc && strcmp(argv[arg_idx], "-n") == 0) {
            arg_idx++;
            if (arg_idx >= argc) {
                fprintf(stderr, "Error: -n requires a number\n");
                return 1;
            }
            iterations = atoi(argv[arg_idx++]);
        }
        if (arg_idx >= argc) {
            fprintf(stderr, "Error: lsp-debug requires a file argument\n");
            return 1;
        }
        return cmd_lsp_debug(argv[arg_idx], iterations);
    }

    if (strcmp(cmd, "process") == 0) {
        if (arg_idx >= argc) {
            fprintf(stderr, "Error: process requires a file argument\n");
            return 1;
        }
        const char *file = argv[arg_idx];
        lib_opts.processed_file = file;
        return cmd_process(file, &lib_opts, filename_hint);
    }

    if (strcmp(cmd, "check") == 0) {
        if (arg_idx >= argc) {
            fprintf(stderr, "Error: check requires a file argument\n");
            return 1;
        }
        return cmd_check(argv[arg_idx]);
    }

    if (strcmp(cmd, "clean") == 0) {
        if (arg_idx >= argc) {
            fprintf(stderr, "Error: clean requires a file argument\n");
            return 1;
        }
        return cmd_clean(argv[arg_idx]);
    }

    if (strcmp(cmd, "render") == 0) {
        if (arg_idx + 1 >= argc) {
            fprintf(stderr, "Error: render requires file and segment arguments\n");
            return 1;
        }
        return cmd_render(argv[arg_idx], argv[arg_idx + 1]);
    }

    if (strcmp(cmd, "list") == 0) {
        if (arg_idx >= argc) {
            fprintf(stderr, "Error: list requires a file argument\n");
            return 1;
        }
        return cmd_list(argv[arg_idx]);
    }

    fprintf(stderr, "Error: Unknown command '%s'\n", cmd);
    print_usage(argv[0]);
    return 1;
}
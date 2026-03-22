// test_lsp_parse.c - Native test binary for LSP parsing path
// Build: cc -g -fsanitize=address src/test_lsp_parse.c -o test_lsp_parse -I. -Lbuild -lhyades_core -lm
// Or add to CMakeLists.txt

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "public_api/hyades_parse_api.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static char *read_stdin(void) {
    size_t capacity = 4096;
    size_t len = 0;
    char *buf = malloc(capacity);
    if (!buf) return NULL;

    int c;
    while ((c = getchar()) != EOF) {
        if (len + 1 >= capacity) {
            capacity *= 2;
            char *new_buf = realloc(buf, capacity);
            if (!new_buf) {
                free(buf);
                return NULL;
            }
            buf = new_buf;
        }
        buf[len++] = (char)c;
    }
    buf[len] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    char *source = NULL;
    int iterations = 1;

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            iterations = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-") == 0) {
            source = read_stdin();
        } else if (argv[i][0] != '-') {
            source = read_file(argv[i]);
        }
    }

    if (!source) {
        fprintf(stderr, "Usage: %s [-n iterations] <file.cld | ->\n", argv[0]);
        fprintf(stderr, "  -n N   Run N iterations (for stress testing)\n");
        fprintf(stderr, "  -      Read from stdin\n");
        return 1;
    }

    printf("Input: %zu bytes\n", strlen(source));
    printf("Running %d iteration(s)...\n", iterations);

    for (int i = 0; i < iterations; i++) {
        if (iterations > 1 && i % 100 == 0) {
            printf("Iteration %d...\n", i);
        }

        // This is the same path the LSP takes
        HyadesParseResult *result = hyades_parse_for_lsp(source);

        if (!result) {
            fprintf(stderr, "Parse returned NULL at iteration %d\n", i);
            free(source);
            return 1;
        }

        // Get diagnostics (exercises JSON output path)
        char *diag_json = hyades_errors_to_json(result);
        if (diag_json) {
            if (i == 0) {
                printf("Errors: %d\n", hyades_error_count(result));
                printf("Diagnostics JSON: %.200s%s\n", diag_json,
                       strlen(diag_json) > 200 ? "..." : "");
            }
            free(diag_json);
        }

        // Get symbols (exercises another JSON path)
        char *sym_json = hyades_symbols_to_json(result);
        if (sym_json) {
            if (i == 0) {
                printf("Symbols: %d\n", hyades_symbol_count(result));
            }
            free(sym_json);
        }

        hyades_parse_result_free(result);
    }

    printf("Done.\n");
    free(source);
    return 0;
}

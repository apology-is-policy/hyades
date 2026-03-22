// gen_stdlib_lambdas.c - Generate C code from stdlib_lambdas.cld definitions
//
// Usage: gen_stdlib_lambdas <input.cld> <output.c>
//
// Reads lambda definitions from a .cld file and generates C code that
// registers them into the global scope at runtime.

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LAMBDA_SIZE (64 * 1024)
#define MAX_LAMBDAS 256

typedef struct {
    char *text;
    size_t len;
} LambdaDef;

static LambdaDef g_lambdas[MAX_LAMBDAS];
static int g_n_lambdas = 0;

// Read entire file into memory
static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open '%s'\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, len, f);
    buf[read] = '\0';
    fclose(f);

    // Strip \r (CRLF -> LF) so Windows line endings don't leak into lambda bodies
    char *w = buf;
    for (const char *r = buf; *r; r++) {
        if (*r != '\r') *w++ = *r;
    }
    *w = '\0';
    read = w - buf;

    if (out_len) *out_len = read;
    return buf;
}

// Skip whitespace and comments (lines starting with %)
static const char *skip_ws_comments(const char *p) {
    while (*p) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

        // Skip comment lines
        if (*p == '%') {
            while (*p && *p != '\n') p++;
            continue;
        }

        break;
    }
    return p;
}

// Parse a single \lambda<name>[params]{body} definition
// Returns pointer past the definition, or NULL on error
static const char *parse_lambda_def(const char *input, LambdaDef *out) {
    const char *p = input;

    // Expect \lambda
    if (strncmp(p, "\\lambda", 7) != 0) {
        return NULL;
    }

    const char *start = p;
    p += 7;

    // Parse <name>
    if (*p != '<') return NULL;
    int angle_depth = 1;
    p++;
    while (*p && angle_depth > 0) {
        if (*p == '<')
            angle_depth++;
        else if (*p == '>')
            angle_depth--;
        p++;
    }
    if (angle_depth != 0) return NULL;

    // Optional [params]
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '[') {
        p++;
        int bracket_depth = 1;
        while (*p && bracket_depth > 0) {
            if (*p == '[')
                bracket_depth++;
            else if (*p == ']')
                bracket_depth--;
            p++;
        }
        if (bracket_depth != 0) return NULL;
    }

    // Skip whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    // Expect {body} or #{body} (compact mode)
    bool compact_mode = false;
    if (*p == '#' && *(p + 1) == '{') {
        compact_mode = true;
        p++; // Skip '#'
    }
    (void)compact_mode; // Currently unused, but may be used for future processing

    if (*p != '{') return NULL;
    p++;

    int brace_depth = 1;
    while (*p && brace_depth > 0) {
        if (*p == '{')
            brace_depth++;
        else if (*p == '}')
            brace_depth--;
        if (brace_depth > 0) p++;
    }
    if (brace_depth != 0) return NULL;
    p++; // Skip final }

    // Copy the lambda definition
    size_t len = p - start;
    out->text = malloc(len + 1);
    memcpy(out->text, start, len);
    out->text[len] = '\0';
    out->len = len;

    return p;
}

// Extract all lambda definitions from input
static int extract_lambdas(const char *input) {
    const char *p = input;

    while (*p) {
        p = skip_ws_comments(p);
        if (!*p) break;

        // Try to parse a lambda definition
        if (strncmp(p, "\\lambda", 7) == 0) {
            LambdaDef def = {0};
            const char *next = parse_lambda_def(p, &def);
            if (next && def.text) {
                if (g_n_lambdas < MAX_LAMBDAS) {
                    g_lambdas[g_n_lambdas++] = def;
                } else {
                    fprintf(stderr, "Warning: Too many lambdas, max %d\n", MAX_LAMBDAS);
                    free(def.text);
                }
                p = next;
            } else {
                fprintf(stderr, "Warning: Failed to parse lambda at: %.40s...\n", p);
                p++;
            }
        } else {
            // Skip unknown content
            p++;
        }
    }

    return g_n_lambdas;
}

// Escape a string for C
static void write_c_string(FILE *f, const char *s) {
    fputc('"', f);
    while (*s) {
        switch (*s) {
        case '\n': fputs("\\n", f); break;
        case '\t': fputs("\\t", f); break;
        case '\\': fputs("\\\\", f); break;
        case '"': fputs("\\\"", f); break;
        default:
            if ((unsigned char)*s < 32) {
                fprintf(f, "\\x%02x", (unsigned char)*s);
            } else {
                fputc(*s, f);
            }
        }
        s++;
    }
    fputc('"', f);
}

// Generate the C output file
static int generate_c(const char *output_path) {
    FILE *f = fopen(output_path, "w");
    if (!f) {
        fprintf(stderr, "Error: Cannot create '%s'\n", output_path);
        return -1;
    }

    fprintf(f, "// stdlib_lambdas.c - Generated from stdlib/stdlib_lambdas.cld\n");
    fprintf(f, "// DO NOT EDIT - This file is auto-generated by gen_stdlib_lambdas\n\n");
    fprintf(f, "#include \"document/calc.h\"\n");
    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <stdlib.h>\n");
    fprintf(f, "#include <string.h>\n\n");

    fprintf(f, "// Number of stdlib lambdas\n");
    fprintf(f, "#define STDLIB_N_LAMBDAS %d\n\n", g_n_lambdas);

    fprintf(f, "// Lambda definition strings\n");
    fprintf(f, "static const char *stdlib_lambda_defs[STDLIB_N_LAMBDAS] = {\n");

    for (int i = 0; i < g_n_lambdas; i++) {
        fprintf(f, "    ");
        write_c_string(f, g_lambdas[i].text);
        if (i < g_n_lambdas - 1) fputc(',', f);
        fputc('\n', f);
    }

    fprintf(f, "};\n\n");

    fprintf(f, "// Register all stdlib lambdas into a CalcContext's global scope\n");
    fprintf(f, "void stdlib_register_lambdas(CalcContext *ctx) {\n");
    fprintf(f, "    if (!ctx || !ctx->global_scope) return;\n");
    fprintf(f, "    \n");
    fprintf(f, "    // Process each lambda definition\n");
    fprintf(f, "    for (int i = 0; i < STDLIB_N_LAMBDAS; i++) {\n");
    fprintf(f, "        // Parse and expand the lambda definition\n");
    fprintf(f, "        // This will register it in the current (global) scope\n");
    fprintf(f, "        char *result = expand_calc(stdlib_lambda_defs[i], ctx);\n");
    fprintf(f, "        if (result) {\n");
    fprintf(f, "            free(result);\n");
    fprintf(f, "        } else {\n");
    fprintf(f, "            fprintf(stderr, \"stdlib: Failed to register lambda %%d\\n\", i);\n");
    fprintf(f, "        }\n");
    fprintf(f, "    }\n");
    fprintf(f, "}\n");

    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input.cld> <output.c>\n", argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];

    // Read input file
    size_t input_len;
    char *input = read_file(input_path, &input_len);
    if (!input) {
        return 1;
    }

    // Extract lambdas
    int n = extract_lambdas(input);
    fprintf(stderr, "gen_stdlib_lambdas: Extracted %d lambdas from %s\n", n, input_path);

    // Generate C code
    if (generate_c(output_path) != 0) {
        free(input);
        return 1;
    }

    fprintf(stderr, "gen_stdlib_lambdas: Generated %s\n", output_path);

    // Cleanup
    for (int i = 0; i < g_n_lambdas; i++) {
        free(g_lambdas[i].text);
    }
    free(input);

    return 0;
}

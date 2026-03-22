// gen_stdlib.c - Generate C code from stdlib.cld macro definitions
//
// Usage: gen_stdlib <input.cld> <output.c>
//
// Reads macro definitions from a .cld file and generates C code that
// registers them into a MacroRegistry at runtime.

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MACRO_SIZE (64 * 1024)
#define MAX_MACROS 256

typedef struct {
    char *text;
    size_t len;
} MacroDef;

static MacroDef g_macros[MAX_MACROS];
static int g_n_macros = 0;

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

    // Strip \r (CRLF -> LF) so Windows line endings don't leak into macro bodies
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

// Skip over a \verb block - returns pointer past the closing delimiter
// p should point to the 'v' in \verb
static const char *skip_verb_block(const char *p) {
    // Skip "verb"
    if (strncmp(p, "verb", 4) != 0) return p;
    p += 4;

    // The next character is the delimiter
    if (!*p) return p;
    char delim = *p;
    p++;

    // Find the closing delimiter
    while (*p && *p != delim) p++;
    if (*p == delim) p++;

    return p;
}

// Parse a single \macro<...>{...} definition
// Returns pointer past the definition, or NULL on error
static const char *parse_macro_def(const char *input, MacroDef *out) {
    const char *p = input;

    // Expect \macro
    if (strncmp(p, "\\macro", 6) != 0) {
        return NULL;
    }

    const char *start = p;

    // Find the < > part
    while (*p && *p != '<') p++;
    if (*p != '<') return NULL;

    // Skip to the { for the body
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

    // Skip whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    // Expect {
    if (*p != '{') return NULL;
    p++;

    // Find matching }, handling \verb blocks specially
    int brace_depth = 1;
    while (*p && brace_depth > 0) {
        // Check for \verb - skip over it without counting braces
        if (*p == '\\' && strncmp(p + 1, "verb", 4) == 0) {
            p = skip_verb_block(p + 1);
            continue;
        }

        if (*p == '{')
            brace_depth++;
        else if (*p == '}')
            brace_depth--;
        if (brace_depth > 0) p++;
    }
    if (brace_depth != 0) return NULL;
    p++; // Skip final }

    // Copy the macro definition
    size_t len = p - start;
    out->text = malloc(len + 1);
    memcpy(out->text, start, len);
    out->text[len] = '\0';
    out->len = len;

    return p;
}

// Extract all macro definitions from input
static int extract_macros(const char *input) {
    const char *p = input;

    while (*p) {
        p = skip_ws_comments(p);
        if (!*p) break;

        // Try to parse a macro definition
        if (strncmp(p, "\\macro", 6) == 0) {
            MacroDef def = {0};
            const char *next = parse_macro_def(p, &def);
            if (next && def.text) {
                if (g_n_macros < MAX_MACROS) {
                    g_macros[g_n_macros++] = def;
                } else {
                    fprintf(stderr, "Warning: Too many macros, max %d\n", MAX_MACROS);
                    free(def.text);
                }
                p = next;
            } else {
                fprintf(stderr, "Warning: Failed to parse macro at: %.40s...\n", p);
                p++;
            }
        } else {
            // Skip unknown content
            p++;
        }
    }

    return g_n_macros;
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

    fprintf(f, "// stdlib_macros.c - Generated from stdlib/stdlib.cld\n");
    fprintf(f, "// DO NOT EDIT - This file is auto-generated by gen_stdlib\n\n");
    fprintf(f, "#include \"macro/user/macro.h\"\n");
    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <stdlib.h>\n\n");

    fprintf(f, "// Number of stdlib macros\n");
    fprintf(f, "#define STDLIB_N_MACROS %d\n\n", g_n_macros);

    fprintf(f, "// Macro definition strings\n");
    fprintf(f, "static const char *stdlib_macro_defs[STDLIB_N_MACROS] = {\n");

    for (int i = 0; i < g_n_macros; i++) {
        fprintf(f, "    ");
        write_c_string(f, g_macros[i].text);
        if (i < g_n_macros - 1) fputc(',', f);
        fputc('\n', f);
    }

    fprintf(f, "};\n\n");

    fprintf(f, "// Register all stdlib macros into a registry\n");
    fprintf(f, "void stdlib_register_macros(MacroRegistry *reg) {\n");
    fprintf(f, "    char error[256];\n");
    fprintf(f, "    for (int i = 0; i < STDLIB_N_MACROS; i++) {\n");
    fprintf(f, "        int end_pos = 0;\n");
    fprintf(f, "        Macro *m = macro_parse_definition(stdlib_macro_defs[i], &end_pos, error, "
               "sizeof(error));\n");
    fprintf(f, "        if (m) {\n");
    fprintf(f, "            macro_registry_add(reg, m);\n");
    fprintf(f, "            free(m);\n");
    fprintf(f, "        } else {\n");
    fprintf(
        f,
        "            fprintf(stderr, \"stdlib: Failed to parse macro %%d: %%s\\n\", i, error);\n");
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

    // Extract macros
    int n = extract_macros(input);
    fprintf(stderr, "gen_stdlib: Extracted %d macros from %s\n", n, input_path);

    // Generate C code
    if (generate_c(output_path) != 0) {
        free(input);
        return 1;
    }

    fprintf(stderr, "gen_stdlib: Generated %s\n", output_path);

    // Cleanup
    for (int i = 0; i < g_n_macros; i++) {
        free(g_macros[i].text);
    }
    free(input);

    return 0;
}

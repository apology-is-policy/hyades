// gen_examples.c - Generate examples-library.js from .cld files
//
// Reads all .cld files from wasm/examples/ directory and generates
// a JavaScript file with the EXAMPLES_LIBRARY object.
//
// Each .cld file should have metadata comments at the top:
//   % @category: Getting Started
//   % @name: Hello World
//   % @description: Your first Cassilda document
//
// Files are sorted alphabetically, so use numeric prefixes for ordering:
//   001-hello-world.cld, 002-basic-structure.cld, etc.

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_EXAMPLES 256
#define MAX_CATEGORIES 32
#define MAX_LINE 4096
#define MAX_CONTENT 65536

typedef struct {
    char category[256];
    char name[256];
    char description[512];
    char content[MAX_CONTENT];
    char filename[256];
} Example;

typedef struct {
    char name[256];
    Example *examples[MAX_EXAMPLES];
    int count;
} Category;

static Example examples[MAX_EXAMPLES];
static int example_count = 0;

static Category categories[MAX_CATEGORIES];
static int category_count = 0;

// Find or create a category
static Category *get_category(const char *name) {
    for (int i = 0; i < category_count; i++) {
        if (strcmp(categories[i].name, name) == 0) {
            return &categories[i];
        }
    }
    if (category_count >= MAX_CATEGORIES) {
        fprintf(stderr, "Too many categories\n");
        exit(1);
    }
    Category *cat = &categories[category_count++];
    strncpy(cat->name, name, sizeof(cat->name) - 1);
    cat->count = 0;
    return cat;
}

// Parse metadata from a line like "% @name: Hello World"
static int parse_metadata(const char *line, const char *key, char *value, size_t value_size) {
    // Skip leading whitespace
    while (*line == ' ' || *line == '\t') line++;

    // Must start with %
    if (*line != '%') return 0;
    line++;

    // Skip whitespace after %
    while (*line == ' ' || *line == '\t') line++;

    // Must start with @
    if (*line != '@') return 0;
    line++;

    // Check if key matches
    size_t key_len = strlen(key);
    if (strncmp(line, key, key_len) != 0) return 0;
    line += key_len;

    // Must be followed by :
    if (*line != ':') return 0;
    line++;

    // Skip whitespace after :
    while (*line == ' ' || *line == '\t') line++;

    // Copy value (trim trailing whitespace/newline)
    strncpy(value, line, value_size - 1);
    value[value_size - 1] = '\0';

    // Trim trailing whitespace
    char *end = value + strlen(value) - 1;
    while (end > value && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
        *end-- = '\0';
    }

    return 1;
}

// Check if line is a metadata line (starts with "% @")
static int is_metadata_line(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line != '%') return 0;
    line++;
    while (*line == ' ' || *line == '\t') line++;
    return *line == '@';
}

// Read and parse a single .cld file
static int read_example(const char *dir, const char *filename) {
    if (example_count >= MAX_EXAMPLES) {
        fprintf(stderr, "Too many examples\n");
        return 0;
    }

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, filename);

    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", path);
        return 0;
    }

    Example *ex = &examples[example_count];
    memset(ex, 0, sizeof(*ex));
    strncpy(ex->filename, filename, sizeof(ex->filename) - 1);

    char line[MAX_LINE];
    int in_metadata = 1;
    size_t content_len = 0;

    while (fgets(line, sizeof(line), f)) {
        // Parse metadata from header
        if (in_metadata) {
            if (parse_metadata(line, "category", ex->category, sizeof(ex->category))) continue;
            if (parse_metadata(line, "name", ex->name, sizeof(ex->name))) continue;
            if (parse_metadata(line, "description", ex->description, sizeof(ex->description)))
                continue;

            // Still in metadata section if line is a metadata line
            if (is_metadata_line(line)) continue;

            // First non-metadata line ends the metadata section
            // Skip blank lines immediately after metadata
            int is_blank = 1;
            for (const char *p = line; *p; p++) {
                if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
                    is_blank = 0;
                    break;
                }
            }
            if (is_blank) continue;

            in_metadata = 0;
        }

        // Append to content
        size_t line_len = strlen(line);
        if (content_len + line_len < MAX_CONTENT - 1) {
            strcpy(ex->content + content_len, line);
            content_len += line_len;
        }
    }

    fclose(f);

    // Validate required fields
    if (!ex->category[0] || !ex->name[0]) {
        fprintf(stderr, "Warning: %s missing @category or @name, skipping\n", filename);
        return 0;
    }

    example_count++;
    return 1;
}

// Compare function for sorting filenames
static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

// Escape a string for JavaScript
static void print_js_string(FILE *out, const char *s) {
    fputc('`', out);
    while (*s) {
        switch (*s) {
        case '\\': fputs("\\\\", out); break;
        case '`': fputs("\\`", out); break;
        case '$': fputs("\\$", out); break;
        default: fputc(*s, out); break;
        }
        s++;
    }
    fputc('`', out);
}

int main(int argc, char **argv) {
    const char *input_dir = "wasm/examples";
    const char *output_file = "wasm/examples-library.js";

    if (argc >= 2) input_dir = argv[1];
    if (argc >= 3) output_file = argv[2];

    // Read directory
    DIR *dir = opendir(input_dir);
    if (!dir) {
        fprintf(stderr, "Cannot open directory: %s\n", input_dir);
        return 1;
    }

    // Collect .cld filenames
    char *filenames[MAX_EXAMPLES];
    int file_count = 0;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_REG || ent->d_type == DT_UNKNOWN) {
            size_t len = strlen(ent->d_name);
            if (len > 4 && strcmp(ent->d_name + len - 4, ".cld") == 0) {
                filenames[file_count] = strdup(ent->d_name);
                file_count++;
            }
        }
    }
    closedir(dir);

    if (file_count == 0) {
        fprintf(stderr, "No .cld files found in %s\n", input_dir);
        return 1;
    }

    // Sort filenames alphabetically
    qsort(filenames, file_count, sizeof(char *), cmp_str);

    // Read all examples
    for (int i = 0; i < file_count; i++) {
        read_example(input_dir, filenames[i]);
        free(filenames[i]);
    }

    // Group examples by category (preserving order of first appearance)
    for (int i = 0; i < example_count; i++) {
        Category *cat = get_category(examples[i].category);
        cat->examples[cat->count++] = &examples[i];
    }

    // Generate JavaScript output
    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "Cannot write to %s\n", output_file);
        return 1;
    }

    fprintf(out, "/**\n");
    fprintf(out, " * Cassilda Examples Library\n");
    fprintf(out, " * \n");
    fprintf(out, " * AUTO-GENERATED from wasm/examples/*.cld files\n");
    fprintf(out, " * Do not edit this file directly!\n");
    fprintf(out, " * \n");
    fprintf(out, " * To add/edit examples, modify the .cld files in wasm/examples/\n");
    fprintf(out, " * then run: cmake --build build --target gen_examples\n");
    fprintf(out, " */\n\n");

    fprintf(out, "const EXAMPLES_LIBRARY = {\n");

    for (int c = 0; c < category_count; c++) {
        Category *cat = &categories[c];

        if (c > 0) fprintf(out, ",\n");
        fprintf(out, "    \"%s\": [\n", cat->name);

        for (int e = 0; e < cat->count; e++) {
            Example *ex = cat->examples[e];

            if (e > 0) fprintf(out, ",\n");
            fprintf(out, "        {\n");
            fprintf(out, "            name: \"%s\",\n", ex->name);
            fprintf(out, "            description: \"%s\",\n", ex->description);
            fprintf(out, "            content: ");
            print_js_string(out, ex->content);
            fprintf(out, "\n        }");
        }

        fprintf(out, "\n    ]");
    }

    fprintf(out, "\n};\n\n");
    fprintf(out, "// Make it available globally\n");
    fprintf(out, "if (typeof window !== 'undefined') {\n");
    fprintf(out, "    window.EXAMPLES_LIBRARY = EXAMPLES_LIBRARY;\n");
    fprintf(out, "}\n");

    fclose(out);

    printf("Generated %s with %d examples in %d categories\n", output_file, example_count,
           category_count);

    return 0;
}

// commands.c - Inline command handling for compositor

#include "compositor_internal.h"
#include "render/figlet.h"
#include "utils/utf8.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Inline Command Management
// ============================================================================

void inline_command_free(InlineCommand *cmd) {
    if (!cmd) return;
    free(cmd->name);
    for (int i = 0; i < cmd->n_args; i++) free(cmd->args[i]);
    free(cmd->args);
}

InlineCommand *parse_inline_command(const char *input, int *pos) {
    const char *p = input + *pos;
    if (*p != '\\') return NULL;
    p++;

    const char *name_start = p;
    while (isalpha((unsigned char)*p) || isdigit((unsigned char)*p) || *p == '_') p++;
    if (p == name_start) return NULL;

    InlineCommand *cmd = malloc(sizeof(InlineCommand));
    size_t name_len = p - name_start;
    cmd->name = malloc(name_len + 1);
    memcpy(cmd->name, name_start, name_len);
    cmd->name[name_len] = '\0';
    cmd->args = NULL;
    cmd->n_args = 0;

    // Save position before skipping whitespace - if no args found, we'll restore
    const char *before_ws = p;
    while (*p == ' ' || *p == '\t') p++;

    // Parse optional [arg]
    if (*p == '[') {
        p++;
        const char *opt_start = p;
        while (*p && *p != ']') p++;
        if (*p == ']') {
            size_t opt_len = p - opt_start;
            cmd->args = malloc(sizeof(char *));
            cmd->args[0] = malloc(opt_len + 1);
            memcpy(cmd->args[0], opt_start, opt_len);
            cmd->args[0][opt_len] = '\0';
            cmd->n_args = 1;
            p++;
        }
    }

    while (*p == ' ' || *p == '\t') p++;

    // Parse {arg} arguments
    while (*p == '{') {
        p++;
        const char *arg_start = p;
        int brace_depth = 1;
        while (*p && brace_depth > 0) {
            // Skip escaped braces
            if (*p == '\\' && (p[1] == '{' || p[1] == '}')) {
                p += 2;
                continue;
            }
            if (*p == '{')
                brace_depth++;
            else if (*p == '}')
                brace_depth--;
            if (brace_depth > 0) p++;
        }
        if (brace_depth != 0) {
            inline_command_free(cmd);
            free(cmd);
            return NULL;
        }

        size_t arg_len = p - arg_start;
        cmd->args = realloc(cmd->args, (cmd->n_args + 1) * sizeof(char *));
        cmd->args[cmd->n_args] = malloc(arg_len + 1);
        memcpy(cmd->args[cmd->n_args], arg_start, arg_len);
        cmd->args[cmd->n_args][arg_len] = '\0';
        cmd->n_args++;
        p++;
    }

    // If no arguments were found, restore position to preserve trailing whitespace
    // This allows "\textbackslash x" to keep the space, while "\textbackslash{}x" doesn't
    if (cmd->n_args == 0) {
        p = before_ws;
    }

    *pos = (int)(p - input);
    return cmd;
}

bool is_output_command(const char *name) {
    return (strcmp(name, "figlet") == 0 || strcmp(name, "hrule") == 0 ||
            strcmp(name, "vspace") == 0 || strcmp(name, "hspace") == 0);
}

// ============================================================================
// String to Box Conversion
// ============================================================================

static Box cmd_string_to_box(const char *str) {
    if (!str || !*str) return make_box(0, 0, 0);

    int height = 0, max_width = 0, current_width = 0;
    size_t pos = 0, len = strlen(str);

    while (pos < len) {
        uint32_t cp = utf8_next(str, len, &pos);
        if (cp == '\n') {
            height++;
            if (current_width > max_width) max_width = current_width;
            current_width = 0;
        } else {
            current_width++;
        }
    }

    if (current_width > 0) {
        height++;
        if (current_width > max_width) max_width = current_width;
    }

    if (height == 0 || max_width == 0) return make_box(0, 0, 0);

    Box box = make_box(max_width, height, height - 1);
    if (!box.cells) return box; // Allocation failed

    int row = 0, col = 0;
    pos = 0;

    while (pos < len) {
        uint32_t cp = utf8_next(str, len, &pos);
        if (cp == '\n') {
            while (col < max_width) {
                box.cells[row * max_width + col] = ' ';
                col++;
            }
            row++;
            col = 0;
        } else if (row < height && col < max_width) {
            box.cells[row * max_width + col] = cp;
            col++;
        }
    }

    while (row < height && col < max_width) {
        box.cells[row * max_width + col] = ' ';
        col++;
    }

    return box;
}

// ============================================================================
// Command Rendering
// ============================================================================

Box render_command_box(InlineCommand *cmd, const CompOptions *opt) {
    (void)opt;
    if (!cmd || !cmd->name) return make_box(0, 0, 0);

    if (strcmp(cmd->name, "figlet") == 0) {
        if (cmd->n_args < 1) return make_box(0, 0, 0);

        int font = FIGLET_TINY;
        const char *text = cmd->args[0];

        if (cmd->n_args >= 2) {
            const char *font_name = cmd->args[0];
            if (strcmp(font_name, "standard") == 0)
                font = FIGLET_STANDARD;
            else if (strcmp(font_name, "small") == 0)
                font = FIGLET_SMALL;
            else if (strcmp(font_name, "tiny") == 0)
                font = FIGLET_TINY;
            text = cmd->args[1];
        }

        char *art = figlet_render(text, font);
        if (!art) return make_box(0, 0, 0);

        Box box = cmd_string_to_box(art);
        free(art);
        return box;
    }

    if (strcmp(cmd->name, "hrule") == 0) {
        int width = (cmd->n_args >= 1) ? atoi(cmd->args[0]) : 40;
        if (width < 1) width = 40;

        Box box = make_box(width, 1, 0);
        for (int i = 0; i < width; i++) box.cells[i] = '-';
        return box;
    }

    if (strcmp(cmd->name, "hspace") == 0) {
        int width = (cmd->n_args >= 1) ? atoi(cmd->args[0]) : 4;
        if (width < 1) width = 1;

        Box box = make_box(width, 1, 0);
        for (int i = 0; i < width; i++) box.cells[i] = ' ';
        return box;
    }

    return make_box(0, 0, 0);
}

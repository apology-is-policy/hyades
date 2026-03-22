// layout_render.c - BoxLayout rendering for Hyades
//
// Renders a BoxLayout tree to a Box by recursively processing nodes,
// executing commands, and merging child boxes.

#include "compositor/compositor.h"
#include "diagnostics/diagnostics.h"
#include "document/document.h"
#include "document/macro_expand.h"
#include "layout.h"
#include "layout_internal.h"
#include "layout_types.h"
#include "math/ast.h"
#include "math/parser/parser.h"
#include "math/renderer/symbols.h"
#include "render/box_drawing.h"
#include "utils/utf8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External render mode functions (from render_opts.c or symbols.c)
extern void set_unicode_mode(int enabled);
extern void set_math_cursive_mode(int enabled);
extern const char *get_linebreaker_mode(void);
extern void set_linebreaker_mode(const char *mode);
// set_render_mode is declared in symbols.h with RenderMode type

// ============================================================================
// Line Content Registry (for lineroutine markers)
// ============================================================================
// Stores pre-rendered line Boxes that can be retrieved by ID during rendering.
// This allows lineroutine to pass line content through macro expansion as
// opaque markers, then substitute the actual Boxes during final rendering.

typedef struct {
    char *id;
    Box *box;
} LineContentEntry;

static LineContentEntry *g_line_registry = NULL;
static int g_line_registry_count = 0;
static int g_line_registry_capacity = 0;

// Register a line Box with a unique ID
static void line_registry_add(const char *id, Box *box) {
    if (g_line_registry_count >= g_line_registry_capacity) {
        int new_cap = g_line_registry_capacity == 0 ? 16 : g_line_registry_capacity * 2;
        g_line_registry = realloc(g_line_registry, new_cap * sizeof(LineContentEntry));
        g_line_registry_capacity = new_cap;
    }
    g_line_registry[g_line_registry_count].id = strdup(id);
    g_line_registry[g_line_registry_count].box = box;
    g_line_registry_count++;
}

// Look up a line Box by ID (returns NULL if not found)
static Box *line_registry_get(const char *id) {
    for (int i = 0; i < g_line_registry_count; i++) {
        if (strcmp(g_line_registry[i].id, id) == 0) {
            return g_line_registry[i].box;
        }
    }
    return NULL;
}

// Clear the registry (frees IDs but NOT Boxes - caller manages Box lifetime)
static void line_registry_clear(void) {
    for (int i = 0; i < g_line_registry_count; i++) {
        free(g_line_registry[i].id);
        // Note: don't free box here - it's owned by the lineroutine result
    }
    g_line_registry_count = 0;
}

// Counter for generating line IDs
static int g_line_id_counter = 0;

// Generate a unique ID for a line
static char *generate_line_id(void) {
    char *id = malloc(32);
    snprintf(id, 32, "__lr_%d__", g_line_id_counter++);
    return id;
}

// Public: Reset all line registry state (for WASM re-renders)
void line_registry_reset(void) {
    line_registry_clear();
    g_line_id_counter = 0;
}

// Extract text content from a Box (single row)
// Caller must free the returned string
static char *box_to_text(Box *box) {
    if (!box || box->w == 0 || box->h == 0 || !box->cells) return strdup("");

    // Allocate buffer (4 bytes per char for UTF-8)
    size_t max_len = box->w * 4 + 1;
    char *result = malloc(max_len);
    char *p = result;

    // Extract first row only (lineroutine boxes are 1 row)
    for (int i = 0; i < box->w; i++) {
        uint32_t cp = box->cells[i];
        if (cp == 0 || cp == ' ') {
            // Trailing spaces - stop here for trimmed output
            // But we need to include internal spaces, so just add space
            *p++ = ' ';
        } else if (cp < 128) {
            *p++ = (char)cp;
        } else {
            // UTF-8 encode
            if (cp < 0x80) {
                *p++ = (char)cp;
            } else if (cp < 0x800) {
                *p++ = (char)(0xC0 | (cp >> 6));
                *p++ = (char)(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                *p++ = (char)(0xE0 | (cp >> 12));
                *p++ = (char)(0x80 | ((cp >> 6) & 0x3F));
                *p++ = (char)(0x80 | (cp & 0x3F));
            } else {
                *p++ = (char)(0xF0 | (cp >> 18));
                *p++ = (char)(0x80 | ((cp >> 12) & 0x3F));
                *p++ = (char)(0x80 | ((cp >> 6) & 0x3F));
                *p++ = (char)(0x80 | (cp & 0x3F));
            }
        }
    }
    *p = '\0';

    // Trim trailing spaces
    while (p > result && *(p - 1) == ' ') {
        *(--p) = '\0';
    }

    return result;
}

// Public API: Get text content of a line by marker ID
// Returns NULL if not found, caller must free the returned string
char *line_registry_get_text(const char *id) {
    Box *box = line_registry_get(id);
    if (!box) return NULL;
    return box_to_text(box);
}

// ============================================================================
// Forward Declarations
// ============================================================================

static Box *render_hbox(BoxLayout *layout, CompOptions *opt, ParseError *err);
static Box *render_vbox(BoxLayout *layout, CompOptions *opt, ParseError *err);
static void cleanup_child_boxes(Box **boxes, int n);
static Box *adjust_box_width(Box *child_box, int target_width, Alignment h_align);

// ============================================================================
// Command Execution
// ============================================================================

static void execute_box_command(BoxLayout *cmd, CompOptions *opt) {
    if (!cmd || !cmd->command_name || !opt) return;

    const char *name = cmd->command_name;
    char **args = cmd->command_args;
    int n_args = cmd->n_command_args;

    // \setunicode{true|false}
    if (strcmp(name, "setunicode") == 0 && n_args == 1) {
        if (strcmp(args[0], "true") == 0) {
            set_unicode_mode(1);
            set_render_mode(MODE_UNICODE);
        } else if (strcmp(args[0], "false") == 0) {
            set_unicode_mode(0);
            set_render_mode(MODE_ASCII);
        }
    }

    // \setwidth{N}
    else if (strcmp(name, "setwidth") == 0 && n_args == 1) {
        int w = atoi(args[0]);
        if (w >= 10) {
            opt->width = w;
        }
    }

    // \sethyphenate{true|false}
    else if (strcmp(name, "sethyphenate") == 0 && n_args == 1) {
        opt->hyphenate = (strcmp(args[0], "true") == 0);
        if (opt->hyphenate) {
            // Ensure sane defaults when hyphenation is enabled with zeroed options
            if (opt->hyphen_min_left < 2) opt->hyphen_min_left = 2;
            if (opt->hyphen_min_right < 3) opt->hyphen_min_right = 3;
        }
    }

    // \sethyphenminleft{N}
    else if (strcmp(name, "sethyphenminleft") == 0 && n_args == 1) {
        int n = atoi(args[0]);
        if (n >= 1) opt->hyphen_min_left = n;
    }

    // \sethyphenminright{N}
    else if (strcmp(name, "sethyphenminright") == 0 && n_args == 1) {
        int n = atoi(args[0]);
        if (n >= 1) opt->hyphen_min_right = n;
    }

    // \linebreaker{greedy|knuth|raggedright}
    else if (strcmp(name, "linebreaker") == 0 && n_args == 1) {
        if (strcmp(args[0], "greedy") == 0) {
            opt->linebreaker = LINEBREAK_GREEDY;
            opt->alignment = TEXT_ALIGN_JUSTIFIED;
            opt->hyphenate = true;
            if (opt->hyphen_min_left < 2) opt->hyphen_min_left = 2;
            if (opt->hyphen_min_right < 3) opt->hyphen_min_right = 3;
            set_linebreaker_mode("greedy");
        } else if (strcmp(args[0], "knuth") == 0) {
            opt->linebreaker = LINEBREAK_KNUTH_PLASS;
            opt->alignment = TEXT_ALIGN_JUSTIFIED;
            opt->hyphenate = true;
            if (opt->hyphen_min_left < 2) opt->hyphen_min_left = 2;
            if (opt->hyphen_min_right < 3) opt->hyphen_min_right = 3;
            set_linebreaker_mode("knuth");
        } else if (strcmp(args[0], "raggedright") == 0) {
            opt->linebreaker = LINEBREAK_GREEDY;
            opt->alignment = TEXT_ALIGN_RAGGED_RIGHT;
            opt->hyphenate = false; // Disable hyphenation for ragged right
            set_linebreaker_mode("raggedright");
        }
    }

    // \setlinepenalty{N}
    else if (strcmp(name, "setlinepenalty") == 0 && n_args == 1) {
        double val = atof(args[0]);
        if (val >= 0) opt->kp_line_penalty = val;
    }

    // \sethyphenpenalty{N}
    else if (strcmp(name, "sethyphenpenalty") == 0 && n_args == 1) {
        double val = atof(args[0]);
        if (val >= 0) opt->kp_hyphen_penalty = val;
    }

    // \setconsechyphenpenalty{N}
    else if (strcmp(name, "setconsechyphenpenalty") == 0 && n_args == 1) {
        double val = atof(args[0]);
        if (val >= 0) opt->kp_consec_hyphen_penalty = val;
    }

    // \settolerance{N}
    else if (strcmp(name, "settolerance") == 0 && n_args == 1) {
        double val = atof(args[0]);
        if (val > 0) opt->kp_tolerance = val;
    }

    // Greedy linebreaker parameters
    // \setshortthreshold{N} - min word length for relaxed scoring (default 3)
    else if (strcmp(name, "setshortthreshold") == 0 && n_args == 1) {
        int val = atoi(args[0]);
        if (val >= 1) opt->gp_short_threshold = val;
    }

    // \setlinkthreshold{N} - max token width for symmetric spacing (default 2)
    else if (strcmp(name, "setlinkthreshold") == 0 && n_args == 1) {
        int val = atoi(args[0]);
        if (val >= 1) opt->gp_link_threshold = val;
    }

    // \setspreaddistance{N} - gaps within this distance get penalized (default 2)
    else if (strcmp(name, "setspreaddistance") == 0 && n_args == 1) {
        int val = atoi(args[0]);
        if (val >= 0) opt->gp_spread_distance = val;
    }

    // \setneighbordivisor{N} - penalty for immediate neighbors: 0=disable, else divide (default 0)
    else if (strcmp(name, "setneighbordivisor") == 0 && n_args == 1) {
        int val = atoi(args[0]);
        if (val >= 0) opt->gp_neighbor_divisor = val;
    }

    // \setspreaddivisor{N} - divisor for gaps at distance 2..spread_dist (default 2)
    else if (strcmp(name, "setspreaddivisor") == 0 && n_args == 1) {
        int val = atoi(args[0]);
        if (val >= 1) opt->gp_spread_divisor = val;
    }

    // \setminscore{N} - minimum score to receive extra space (default 0)
    else if (strcmp(name, "setminscore") == 0 && n_args == 1) {
        int val = atoi(args[0]);
        if (val >= 0) opt->gp_min_score = val;
    }

    // \setmathitalic{true|false}
    else if (strcmp(name, "setmathitalic") == 0 && n_args == 1) {
        set_math_cursive_mode(strcmp(args[0], "true") == 0 ? 1 : 0);
    }

    // \setmode{ascii|unicode}
    else if (strcmp(name, "setmode") == 0 && n_args == 1) {
        if (strcmp(args[0], "ascii") == 0) {
            set_render_mode(MODE_ASCII);
        } else if (strcmp(args[0], "unicode") == 0) {
            set_render_mode(MODE_UNICODE);
        }
    }

    // \setsym{SYMBOL_NAME}{value}
    else if (strcmp(name, "setsym") == 0 && n_args == 2) {
        SymbolID id = symbol_name_to_id(args[0]);
        if (id != SYM_INVALID) {
            set_symbol(id, args[1]);
        }
    }

    // \setparskip{N}
    else if (strcmp(name, "setparskip") == 0 && n_args == 1) {
        int n = atoi(args[0]);
        if (n >= 0) opt->parskip = n;
    }

    // \setmathabove{N}
    else if (strcmp(name, "setmathabove") == 0 && n_args == 1) {
        int n = atoi(args[0]);
        if (n >= 0) opt->math_above_skip = n;
    }

    // \setmathbelow{N}
    else if (strcmp(name, "setmathbelow") == 0 && n_args == 1) {
        int n = atoi(args[0]);
        if (n >= 0) opt->math_below_skip = n;
    }

    // \diagnostics{categories}
    else if (strcmp(name, "diagnostics") == 0 && n_args == 1) {
        DiagCategory cats = diag_parse_categories(args[0]);
        if (cats == DIAG_NONE) {
            diag_disable();
        } else {
            diag_enable(cats);
        }
    }
}

// ============================================================================
// Rule Rendering Helpers
// ============================================================================

// Render a vrule box with specific dimensions
// center: optional center character (for curly brace waist), used at height/2
static Box *render_vrule_box(int width, int height, const char *top, const char *fill,
                             const char *center, const char *bottom) {
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;

    Box *box = malloc(sizeof(Box));
    *box = make_box(width, height, 0);

    // Decode fill character
    uint32_t fill_cp = '|';
    if (fill && *fill) {
        size_t pos = 0;
        fill_cp = utf8_next(fill, strlen(fill), &pos);
    }

    uint32_t top_cp = fill_cp;
    if (top && *top) {
        size_t pos = 0;
        top_cp = utf8_next(top, strlen(top), &pos);
    }

    uint32_t bottom_cp = fill_cp;
    if (bottom && *bottom) {
        size_t pos = 0;
        bottom_cp = utf8_next(bottom, strlen(bottom), &pos);
    }

    uint32_t center_cp = fill_cp;
    if (center && *center) {
        size_t pos = 0;
        center_cp = utf8_next(center, strlen(center), &pos);
    }

    int center_row = height / 2;

    for (int y = 0; y < height; y++) {
        uint32_t cp;
        if (y == 0 && top && *top)
            cp = top_cp;
        else if (y == height - 1 && bottom && *bottom)
            cp = bottom_cp;
        else if (y == center_row && center && *center)
            cp = center_cp;
        else
            cp = fill_cp;

        for (int x = 0; x < width; x++) {
            box->cells[y * width + x] = cp;
        }
    }

    return box;
}

// Render an hrule box with specific dimensions
static Box *render_hrule_box(int width, const char *left, const char *fill, const char *right) {
    if (width <= 0) width = 1;

    Box *box = malloc(sizeof(Box));
    *box = make_box(width, 1, 0);

    // Decode fill character
    uint32_t fill_cp = '-';
    if (fill && *fill) {
        size_t pos = 0;
        fill_cp = utf8_next(fill, strlen(fill), &pos);
    }

    uint32_t left_cp = fill_cp;
    if (left && *left) {
        size_t pos = 0;
        left_cp = utf8_next(left, strlen(left), &pos);
    }

    uint32_t right_cp = fill_cp;
    if (right && *right) {
        size_t pos = 0;
        right_cp = utf8_next(right, strlen(right), &pos);
    }

    for (int x = 0; x < width; x++) {
        uint32_t cp;
        if (x == 0 && left && *left)
            cp = left_cp;
        else if (x == width - 1 && right && *right)
            cp = right_cp;
        else
            cp = fill_cp;

        box->cells[x] = cp;
    }

    return box;
}

// ============================================================================
// Content Rendering Helpers
// ============================================================================

// Convert preformatted content directly to Box without composition
static Box *content_to_box_direct(const char *text, int width) {
    if (!text || !*text) {
        Box *empty = malloc(sizeof(Box));
        *empty = make_box(width > 0 ? width : 1, 1, 0);
        for (int i = 0; i < empty->w; i++) {
            empty->cells[i] = (uint32_t)' ';
        }
        return empty;
    }

    // Count lines and max width
    int lines = 0;
    int max_width = 0;
    int current_width = 0;

    size_t pos = 0;
    size_t len = strlen(text);

    while (pos < len) {
        uint32_t cp = utf8_next(text, len, &pos);
        if (cp == '\n') {
            lines++;
            if (current_width > max_width) max_width = current_width;
            current_width = 0;
        } else {
            current_width++;
        }
    }

    if (current_width > 0 || lines == 0) {
        lines++;
        if (current_width > max_width) max_width = current_width;
    }

    // Use specified width or content width, whichever is larger
    int box_width = (max_width > width) ? max_width : width;

    // Create box
    Box *box = malloc(sizeof(Box));
    *box = make_box(box_width, lines, 0);

    // Fill with content
    int row = 0, col = 0;
    pos = 0;

    while (pos < len) {
        uint32_t cp = utf8_next(text, len, &pos);

        if (cp == '\n') {
            // Pad rest of line with spaces
            while (col < box_width) {
                box->cells[row * box_width + col] = (uint32_t)' ';
                col++;
            }
            row++;
            col = 0;
        } else {
            if (row < lines && col < box_width) {
                box->cells[row * box_width + col] = cp;
                col++;
            }
        }
    }

    // Pad remaining cells
    while (row < lines) {
        while (col < box_width) {
            box->cells[row * box_width + col] = (uint32_t)' ';
            col++;
        }
        row++;
        col = 0;
    }

    return box;
}

// ============================================================================
// Main Rendering Function
// ============================================================================

Box *box_layout_render(BoxLayout *layout, CompOptions *opt, ParseError *err) {
    if (!layout) return NULL;

    // Resolve widths first
    // Skip for intrinsic-width nodes (computed_width=0 or WIDTH_INTRINSIC is intentional)
    // Also skip when computed_width is WIDTH_INTRINSIC (-3) - this indicates the node
    // is inside a WIDTH_INTRINSIC parent and should preserve its inherited undefined width
    if (layout->computed_width <= 0 && layout->width != WIDTH_INTRINSIC &&
        layout->computed_width != WIDTH_INTRINSIC) {
        layout->computed_width = layout->width > 0 ? layout->width : (opt ? opt->width : 80);
    }

    switch (layout->type) {
    case BOX_TYPE_COMMAND: {
        // Execute command - modifies opt!
        execute_box_command(layout, opt);

        // Special cases: diagnostic commands that produce output
        const char *cmd_name = layout->command_name;
        if (cmd_name && strcmp(cmd_name, "printdiagnostics") == 0) {
            // Output accumulated diagnostic logs as preformatted text
            char *diag_output = diag_get_output();
            if (diag_output) {
                Box *box = string_to_box(diag_output);
                free(diag_output);
                return box;
            }
        } else if (cmd_name && strcmp(cmd_name, "showdiag") == 0 && layout->n_command_args >= 1 &&
                   layout->command_args[0]) {
            // Render content and append diagnostics inline
            // 1. Clear any existing diagnostics to get just this content's logs
            diag_clear();

            // 2. Parse and render the content
            ParseError content_err = {0};
            int target_width = opt ? opt->width : 80;
            BoxLayout *content_layout =
                parse_document_as_vbox(layout->command_args[0], target_width, &content_err);

            Box *content_box = NULL;
            if (content_layout) {
                content_box = box_layout_render(content_layout, opt, &content_err);
                box_layout_free(content_layout);
            }

            // 3. Get the diagnostics for this content
            char *diag_output = diag_get_output();

            // 4. Combine content and diagnostics vertically
            if (content_box && diag_output) {
                Box *diag_box = string_to_box(diag_output);
                free(diag_output);

                if (diag_box) {
                    // Merge vertically: content on top, diagnostics below
                    Box *boxes[2] = {content_box, diag_box};
                    Box *merged = vbox_merge(boxes, 2, target_width);

                    box_free(content_box);
                    free(content_box);
                    box_free(diag_box);
                    free(diag_box);

                    return merged;
                }
            }

            if (diag_output) free(diag_output);
            return content_box;
        } else if (cmd_name && strcmp(cmd_name, "diag_expand") == 0 &&
                   layout->n_command_args >= 1 && layout->command_args[0]) {
            // Log macro expansion result to diagnostics
            // Note: By the time we get here, macros have already been expanded
            // during document parsing. This shows the final expanded form.
            const char *expanded = layout->command_args[0];

            diag_log(DIAG_EXPANSION, 0, "diag_expand: macro expansion result");

            // Log the expanded content, splitting on newlines for readability
            const char *p = expanded;
            int line_num = 1;
            while (*p) {
                // Find end of line
                const char *line_end = p;
                while (*line_end && *line_end != '\n') line_end++;

                // Log this line (up to 120 chars)
                int line_len = (int)(line_end - p);
                if (line_len > 0) {
                    char line_buf[128];
                    int copy_len = line_len < 120 ? line_len : 120;
                    memcpy(line_buf, p, copy_len);
                    line_buf[copy_len] = '\0';
                    if (line_len > 120) {
                        strcat(line_buf, "...");
                    }
                    diag_log(DIAG_EXPANSION, 1, "%s", line_buf);
                }

                // Move to next line
                p = line_end;
                if (*p == '\n') p++;
                line_num++;

                // Limit output
                if (line_num > 1000) {
                    diag_log(DIAG_EXPANSION, 1, "... (%d more lines)",
                             (int)(strlen(p) > 0 ? 1 : 0));
                    break;
                }
            }

            // Return NULL - diag_expand produces no rendered output
            return NULL;
        }

        // Normal commands produce no output
        return NULL;
    }

    case BOX_TYPE_INTERSECT_RULES: {
        // Render the single child, then apply junction fixup
        if (layout->n_children < 1) {
            return NULL;
        }

        Box *child_box = box_layout_render(layout->children[0], opt, err);
        if (!child_box) {
            return NULL;
        }

        // Apply junction fixup to merge intersecting box-drawing chars
        box_fixup_junctions(child_box);

        return child_box;
    }

    case BOX_TYPE_DISPLAY_MATH: {
        // Render display math
        if (!layout->math_src) return NULL;

        Ast *ast = parse_math(layout->math_src, err);
        if (!ast) {
            if (err && err->code == PARSE_OK) {
                err->code = PARSE_ERR_MATH_SYNTAX;
                snprintf(err->message, sizeof(err->message), "Failed to parse display math");
            }
            return NULL;
        }

        Box box = render_ast(ast);
        ast_free(ast);

        Box *result = malloc(sizeof(Box));
        *result = box;
        return result;
    }

    case BOX_TYPE_CONTENT: {
        // Render text content (may contain inline $...$)
        if (!layout->content || !*layout->content) {
            Box *empty = malloc(sizeof(Box));
            *empty = make_box(1, 1, 0);
            return empty;
        }

        if (layout->preformatted) {
            // Preformatted - convert string to box directly
            return string_to_box(layout->content);
        }

        // Use compositor - handles inline math, \figlet, etc.
        CompOptions local_opt = opt ? *opt : default_options();

        // Determine the width for text wrapping:
        // - For document-level content (width=-1 and computed_width == document_width),
        //   use opt->width which can be modified by \setwidth
        // - For content inside nested containers (explicit width), use computed_width
        if (layout->computed_width > 0) {
            if (layout->width == -1 && // Only inherited-width content
                opt && opt->document_width > 0 && layout->computed_width == opt->document_width &&
                opt->width != opt->document_width) {
                // Document-level content and \setwidth was used - use opt->width
                local_opt.width = opt->width;
            } else {
                // Nested content or no \setwidth - use computed_width
                local_opt.width = layout->computed_width;
            }
        }

        // Use compose_text_with_meta to get both string and cell metadata
        uint8_t *meta = NULL;
        int meta_len = 0;
        compositor_reset_baseline(); // Reset before composition
        char *result_str =
            compose_text_with_meta(layout->content, &local_opt, &meta, &meta_len, err);
        if (!result_str) {
            free(meta);
            if (err && err->code != PARSE_OK) return NULL;
            Box *empty = malloc(sizeof(Box));
            *empty = make_box(1, 1, 0);
            return empty;
        }

        // Convert to box with metadata preserved
        Box *box = string_to_box_with_meta(result_str, meta, meta_len);
        free(result_str);
        free(meta);

        // Apply baseline from composition (for inline math alignment)
        if (box) {
            box->baseline = compositor_get_baseline();
        }
        return box;
    }

    case BOX_TYPE_HBOX: {
        // Two-pass rendering to support auto-height vrules
        return render_hbox(layout, opt, err);
    }

    case BOX_TYPE_VBOX: {
        // Two-pass rendering to support auto-width hrules
        return render_vbox(layout, opt, err);
    }

    case BOX_TYPE_VRULE: {
        // Vrule appearing outside hbox - use rule_height if set, else default
        int height = (layout->rule_height > 0) ? layout->rule_height : 1;
        Box *result = render_vrule_box(layout->rule_width, height, layout->rule_start,
                                       layout->rule_fill, layout->rule_center, layout->rule_end);
        if (result && layout->inherit_style != 0) {
            box_ensure_style(result);
            if (result->style) {
                for (int idx = 0; idx < result->w * result->h; idx++)
                    if (result->style[idx] == 0) result->style[idx] = layout->inherit_style;
            }
        }
        return result;
    }

    case BOX_TYPE_HRULE: {
        // Hrule appearing outside vbox - use computed_width or default
        int width = (layout->rule_width > 0)       ? layout->rule_width
                    : (layout->computed_width > 0) ? layout->computed_width
                                                   : 40;
        Box *result =
            render_hrule_box(width, layout->rule_start, layout->rule_fill, layout->rule_end);
        if (result && layout->inherit_style != 0) {
            box_ensure_style(result);
            if (result->style) {
                for (int idx = 0; idx < result->w * result->h; idx++)
                    if (result->style[idx] == 0) result->style[idx] = layout->inherit_style;
            }
        }
        return result;
    }

    case BOX_TYPE_LINE_BREAK: {
        // Line break renders to nothing - it only affects spacing chain
        // Returning NULL means it won't be included in vbox/hbox merge
        return NULL;
    }

    case BOX_TYPE_LINEROUTINE: {
        // Line routine: render content, then apply routine macro to each line
        // Uses marker-based approach to avoid re-parsing rendered content:
        // 1. Extract each row as a Box
        // 2. Register each row Box with a unique marker ID
        // 3. Build macro call with marker: \routine{__lr_ID__}{line_num}
        // 4. Expand macros (markers pass through unchanged)
        // 5. Parse/render, with LINEINSERT nodes looking up stored Boxes

        if (layout->n_children < 1 || !layout->routine_name) {
            if (diag_is_enabled(DIAG_LAYOUT)) {
                diag_log(DIAG_LAYOUT, 0, "LINEROUTINE: missing children or routine_name");
            }
            return NULL;
        }

        if (diag_is_enabled(DIAG_LAYOUT)) {
            diag_log(DIAG_LAYOUT, 0, "LINEROUTINE: processing with routine '%s'",
                     layout->routine_name);
        }

        // 1. Render the child content to a Box
        Box *content_box = box_layout_render(layout->children[0], opt, err);
        if (!content_box || content_box->h == 0 || !content_box->cells) {
            if (diag_is_enabled(DIAG_LAYOUT)) {
                diag_log(DIAG_LAYOUT, 1, "child content empty or failed to render");
            }
            return content_box; // Nothing to process
        }

        if (diag_is_enabled(DIAG_LAYOUT)) {
            diag_log(DIAG_LAYOUT, 1, "rendered child content: %dx%d", content_box->w,
                     content_box->h);
        }

        int target_width = opt ? opt->width : 80;
        Box **line_boxes = malloc(content_box->h * sizeof(Box *));
        int n_lines = 0;

        // Clear any previous line registry entries
        line_registry_clear();

        // 2. For each row, create a row Box and register with marker
        for (int row = 0; row < content_box->h; row++) {
            // Find the actual content width (trim trailing spaces)
            int row_start = row * content_box->w;
            int actual_width = content_box->w;
            while (actual_width > 0 && (content_box->cells[row_start + actual_width - 1] == ' ' ||
                                        content_box->cells[row_start + actual_width - 1] == 0)) {
                actual_width--;
            }
            if (actual_width == 0) actual_width = 1; // At least 1 column

            // Create a trimmed 1-row Box for this line
            Box *row_box = calloc(1, sizeof(Box));
            row_box->w = actual_width;
            row_box->h = 1;
            row_box->baseline = 0;
            row_box->cells = malloc(actual_width * sizeof(uint32_t));
            if (!row_box->cells) {
                free(row_box);
                continue; // Skip this line on allocation failure
            }
            memcpy(row_box->cells, content_box->cells + row_start, actual_width * sizeof(uint32_t));
            // Copy metadata if present
            if (content_box->meta) {
                box_ensure_meta(row_box);
                if (row_box->meta)
                    memcpy(row_box->meta, content_box->meta + row_start,
                           actual_width * sizeof(uint8_t));
            }
            // Copy style if present
            if (content_box->style) {
                box_ensure_style(row_box);
                if (row_box->style)
                    memcpy(row_box->style, content_box->style + row_start,
                           actual_width * sizeof(uint16_t));
            }

            // Generate unique ID and register
            char *line_id = generate_line_id();
            line_registry_add(line_id, row_box);

            if (diag_is_enabled(DIAG_LAYOUT)) {
                diag_log(DIAG_LAYOUT, 1, "line %d: registered as %s (width=%d)", row + 1, line_id,
                         actual_width);
            }

            // Build macro call with marker: \routine{__lr_ID__}{line_num}
            int line_num = row + 1;
            size_t call_size = strlen(layout->routine_name) + strlen(line_id) + 32;
            char *macro_call = malloc(call_size);
            snprintf(macro_call, call_size, "\\%s{%s}{%d}", layout->routine_name, line_id,
                     line_num);
            free(line_id);

            if (diag_is_enabled(DIAG_LAYOUT)) {
                diag_log(DIAG_LAYOUT, 2, "macro call: %s", macro_call);
            }

            // Expand macros (markers pass through unchanged)
            char macro_error[256] = {0};
            char *expanded =
                expand_all_macros(macro_call, target_width, macro_error, sizeof(macro_error));
            free(macro_call);

            if (!expanded) {
                // Expansion failed, skip this line
                if (diag_is_enabled(DIAG_LAYOUT)) {
                    diag_log(DIAG_LAYOUT, 2, "macro expansion failed: %s", macro_error);
                }
                continue;
            }

            if (diag_is_enabled(DIAG_LAYOUT)) {
                diag_result(DIAG_LAYOUT, 2, "expanded to: %s", expanded);
            }

            // Strip leading/trailing whitespace from expanded result
            // This allows macro bodies to be formatted with newlines/indentation
            char *start = expanded;
            while (*start &&
                   (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r')) {
                start++;
            }
            char *end = start + strlen(start);
            while (end > start &&
                   (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) {
                end--;
            }
            size_t trimmed_len = end - start;
            char *trimmed = malloc(trimmed_len + 1);
            memcpy(trimmed, start, trimmed_len);
            trimmed[trimmed_len] = '\0';
            free(expanded);
            expanded = trimmed;

            // Parse and render the expanded result
            ParseError line_err = {0};
            BoxLayout *line_layout = parse_document_as_vbox(expanded, target_width, &line_err);
            free(expanded);

            if (line_layout) {
                Box *line_box = box_layout_render(line_layout, opt, &line_err);
                box_layout_free(line_layout);

                if (line_box) {
                    line_boxes[n_lines++] = line_box;
                }
            } else {
            }
        }

        // Clean up content box
        box_free(content_box);
        free(content_box);

        // Clear registry (we're done with markers)
        line_registry_clear();

        // 3. Merge all line boxes vertically
        if (n_lines == 0) {
            if (diag_is_enabled(DIAG_LAYOUT)) {
                diag_log(DIAG_LAYOUT, 1, "no lines produced");
            }
            free(line_boxes);
            return NULL;
        }

        Box *merged = vbox_merge(line_boxes, n_lines, target_width);

        if (diag_is_enabled(DIAG_LAYOUT)) {
            diag_log(DIAG_LAYOUT, 0, "LINEROUTINE: complete, %d lines merged to %dx%d", n_lines,
                     merged ? merged->w : 0, merged ? merged->h : 0);
        }

        // Cleanup line boxes
        for (int i = 0; i < n_lines; i++) {
            box_free(line_boxes[i]);
            free(line_boxes[i]);
        }
        free(line_boxes);

        return merged;
    }

    case BOX_TYPE_LINEINSERT: {
        // Look up pre-rendered line Box by ID and return a copy
        if (!layout->lineinsert_id) {
            if (diag_is_enabled(DIAG_LAYOUT)) {
                diag_log(DIAG_LAYOUT, 2, "LINEINSERT: no ID specified");
            }
            return NULL;
        }

        Box *stored = line_registry_get(layout->lineinsert_id);
        if (!stored) {
            if (diag_is_enabled(DIAG_LAYOUT)) {
                diag_log(DIAG_LAYOUT, 2, "LINEINSERT: '%s' not found in registry",
                         layout->lineinsert_id);
            }
            return NULL;
        }

        if (diag_is_enabled(DIAG_LAYOUT)) {
            diag_log(DIAG_LAYOUT, 2, "LINEINSERT: '%s' resolved to %dx%d box",
                     layout->lineinsert_id, stored->w, stored->h);
        }

        // Return a copy of the stored box
        Box *copy = calloc(1, sizeof(Box));
        if (!copy) return NULL;
        copy->w = stored->w;
        copy->h = stored->h;
        copy->baseline = stored->baseline;
        copy->cells = malloc(stored->w * stored->h * sizeof(uint32_t));
        if (!copy->cells) {
            free(copy);
            return NULL;
        }
        memcpy(copy->cells, stored->cells, stored->w * stored->h * sizeof(uint32_t));
        // Copy metadata if present
        if (stored->meta) {
            box_ensure_meta(copy);
            if (copy->meta)
                memcpy(copy->meta, stored->meta, stored->w * stored->h * sizeof(uint8_t));
        }
        // Copy style if present
        if (stored->style) {
            box_ensure_style(copy);
            if (copy->style)
                memcpy(copy->style, stored->style, stored->w * stored->h * sizeof(uint16_t));
        }

        return copy;
    }

    case BOX_TYPE_VSKIP:
    case BOX_TYPE_HSKIP:
        // Skip nodes are handled by render_vbox/render_hbox, not directly rendered
        return NULL;

    case BOX_TYPE_ANSI:
        // ANSI nodes are handled by render_hbox, not directly rendered
        return NULL;
    }

    return NULL;
}

// ============================================================================
// HBOX Rendering (Two-pass for auto-height vrules)
// ============================================================================

static Box *render_hbox(BoxLayout *layout, CompOptions *opt, ParseError *err) {
    // Check if this is document-level hbox where \setwidth changed the width
    // If so, we need to re-resolve child widths based on the new width
    if (layout->width == -1 && opt && opt->document_width > 0 &&
        layout->computed_width == opt->document_width && opt->width != opt->document_width) {
        // Re-resolve widths for this hbox and its children using the new width
        layout->computed_width = opt->width;
        box_layout_resolve_widths(layout, opt->width);
    }

    // === PASS 1: Render non-auto children, find max height ===
    Box **child_boxes = malloc(layout->n_children * sizeof(Box *));
    bool *is_auto = calloc(layout->n_children, sizeof(bool));
    int max_height = 0;

    for (int i = 0; i < layout->n_children; i++) {
        BoxLayout *child = layout->children[i];

        // Check for auto-height vrule
        if (child->type == BOX_TYPE_VRULE && child->rule_height == RULE_SIZE_AUTO) {
            is_auto[i] = true;
            child_boxes[i] = NULL; // Placeholder
            continue;
        }

        Box *child_box = box_layout_render(child, opt, err);

        if (child_box) {
            // Apply width adjustment (padding/clipping)
            if (child->computed_width > 0 && child->computed_width != child_box->w) {
                child_box = adjust_box_width(child_box, child->computed_width, child->h_align);
            }

            if (child_box->h > max_height) {
                max_height = child_box->h;
            }

            // Apply bg_fill — replaces background component of every cell's style
            if (child->bg_fill != 0 && child_box) {
                box_ensure_style(child_box);
                if (child_box->style) {
                    uint16_t fill_bg = child->bg_fill;
                    for (int idx = 0; idx < child_box->w * child_box->h; idx++) {
                        uint8_t fg = child_box->style[idx] & 0xFF;
                        child_box->style[idx] = fill_bg | fg;
                    }
                }
            }

            child_boxes[i] = child_box;
        } else {
            child_boxes[i] = NULL;
        }

        if (err && err->code != PARSE_OK) {
            cleanup_child_boxes(child_boxes, i + 1);
            free(child_boxes);
            free(is_auto);
            return NULL;
        }
    }

    // Ensure minimum height
    if (max_height < 1) max_height = 1;

    // === PASS 2: Render auto-height vrules ===
    for (int i = 0; i < layout->n_children; i++) {
        if (!is_auto[i]) continue;

        BoxLayout *child = layout->children[i];

        // Render vrule with computed height
        Box *vrule_box = render_vrule_box(child->rule_width, max_height, child->rule_start,
                                          child->rule_fill, child->rule_center, child->rule_end);

        // Apply width adjustment if needed
        if (child->computed_width > 0 && child->computed_width != vrule_box->w) {
            vrule_box = adjust_box_width(vrule_box, child->computed_width, ALIGN_LEFT);
        }

        child_boxes[i] = vrule_box;

        // Apply inherited ANSI style from \term_color wrapping
        if (child->inherit_style != 0) {
            box_ensure_style(vrule_box);
            if (vrule_box->style) {
                for (int idx = 0; idx < vrule_box->w * vrule_box->h; idx++) {
                    if (vrule_box->style[idx] == 0) vrule_box->style[idx] = child->inherit_style;
                }
            }
        }
    }

    free(is_auto);

    // === Apply per-child vertical alignment ===
    for (int i = 0; i < layout->n_children; i++) {
        if (!child_boxes[i]) continue;
        BoxLayout *child = layout->children[i];
        if (child->v_align != ALIGN_TOP && child_boxes[i]->h < max_height) {
            Box *aligned = apply_v_alignment(child_boxes[i], max_height, child->v_align);
            if (aligned != child_boxes[i]) {
                box_free(child_boxes[i]);
                free(child_boxes[i]);
                child_boxes[i] = aligned;
            }
        }
    }

    // Collect non-null boxes for merging, tracking skip offsets and ANSI escapes
    Box **merge_boxes = malloc(layout->n_children * sizeof(Box *));
    int *x_offsets = calloc(layout->n_children, sizeof(int));
    int n_merge = 0;
    int pending_skip = 0; // Accumulates skip amounts for the next non-skip child
    bool has_skips = false;

    // Track current ANSI fg/bg state for per-cell style application
    uint8_t current_fg = 0;
    uint8_t current_bg = 0;

    for (int i = 0; i < layout->n_children; i++) {
        BoxLayout *child = layout->children[i];

        // Handle HSKIP nodes - they adjust offset for the next box
        if (child->type == BOX_TYPE_HSKIP) {
            pending_skip += child->skip_amount;
            if (child->skip_amount != 0) has_skips = true;
            continue; // Don't add skip nodes to merge_boxes
        }

        // Handle ANSI nodes - parse codes to update current fg/bg state
        if (child->type == BOX_TYPE_ANSI && child->ansi_codes) {
            const char *p = child->ansi_codes;
            while (*p) {
                int code = (int)strtol(p, (char **)&p, 10);
                if (code == 0) {
                    current_fg = 0;
                    current_bg = 0;
                } else if (code == 39) {
                    current_fg = 0;
                } else if (code == 49) {
                    current_bg = 0;
                } else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97))
                    current_fg = (uint8_t)code;
                else if ((code >= 40 && code <= 47) || (code >= 100 && code <= 107))
                    current_bg = (uint8_t)code;
                if (*p == ';') p++;
            }
            continue;
        }

        if (child_boxes[i]) {
            // Apply current ANSI style to all unstyled cells in this box
            uint16_t style = ((uint16_t)current_bg << 8) | current_fg;
            if (style != 0) {
                Box *box = child_boxes[i];
                box_ensure_style(box);
                if (box->style) {
                    for (int idx = 0; idx < box->w * box->h; idx++) {
                        if (box->style[idx] == 0) box->style[idx] = style;
                    }
                }
            }

            x_offsets[n_merge] = pending_skip;
            pending_skip = 0; // Reset after applying to a box
            merge_boxes[n_merge++] = child_boxes[i];
        }
    }

    Box *merged = NULL;
    if (n_merge > 0) {
        if (has_skips) {
            merged = hbox_merge_with_skips(merge_boxes, x_offsets, n_merge);
        } else {
            // Pass v_align to control baseline vs top alignment
            merged = hbox_merge(merge_boxes, n_merge, layout->v_align);
        }
    } else {
        merged = malloc(sizeof(Box));
        *merged = make_box(1, 1, 0);
    }

    free(x_offsets);

    // Cleanup
    cleanup_child_boxes(child_boxes, layout->n_children);
    free(child_boxes);
    free(merge_boxes);

    return merged;
}

// ============================================================================
// ANSI-only hbox detection (for vbox ANSI state tracking)
// ============================================================================

// Check if a layout node is an hbox containing ONLY BOX_TYPE_ANSI children
// (and possibly whitespace-only CONTENT children from macro expansion).
// These are created by flush_text when ANSI markers (from \term_color) wrap
// a layout command like \Boxed. In a vbox context, they should be transparent
// (zero height) and their ANSI state should transfer to adjacent content.
static bool is_ansi_only_hbox(BoxLayout *node) {
    if (!node || node->type != BOX_TYPE_HBOX || node->n_children == 0) return false;
    bool has_ansi = false;
    for (int i = 0; i < node->n_children; i++) {
        BoxLayout *child = node->children[i];
        if (child->type == BOX_TYPE_ANSI) {
            has_ansi = true;
        } else if (child->type == BOX_TYPE_CONTENT && child->content) {
            // Allow whitespace-only content (from macro expansion trailing whitespace)
            const char *p = child->content;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            if (*p != '\0') return false; // Non-whitespace content
        } else {
            return false; // Other child type
        }
    }
    return has_ansi;
}

// Parse ANSI codes from an ANSI-only hbox and update fg/bg state.
static void extract_ansi_from_hbox(BoxLayout *node, uint8_t *fg, uint8_t *bg) {
    for (int i = 0; i < node->n_children; i++) {
        BoxLayout *child = node->children[i];
        if (child->type == BOX_TYPE_ANSI && child->ansi_codes) {
            const char *p = child->ansi_codes;
            while (*p) {
                int code = (int)strtol(p, (char **)&p, 10);
                if (code == 0) {
                    *fg = 0;
                    *bg = 0;
                } else if (code == 39) {
                    *fg = 0;
                } else if (code == 49) {
                    *bg = 0;
                } else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97))
                    *fg = (uint8_t)code;
                else if ((code >= 40 && code <= 47) || (code >= 100 && code <= 107))
                    *bg = (uint8_t)code;
                if (*p == ';') p++;
            }
        }
    }
}

// ============================================================================
// VBOX Rendering (Two-pass for auto-width hrules)
// ============================================================================

static Box *render_vbox(BoxLayout *layout, CompOptions *opt, ParseError *err) {
    // Check if this is document-level vbox where \setwidth changed the width
    // If so, we need to re-resolve child widths based on the new width
    if (layout->width == -1 && opt && opt->document_width > 0 &&
        layout->computed_width == opt->document_width && opt->width != opt->document_width) {
        // Re-resolve widths for this vbox and its children using the new width
        layout->computed_width = opt->width;
        box_layout_resolve_widths(layout, opt->width);
    }

    // Handle WIDTH_INTRINSIC: two-pass rendering to measure natural content width
    // Only do measurement if computed_width is still WIDTH_INTRINSIC (not yet measured)
    if (layout->width == WIDTH_INTRINSIC && layout->computed_width == WIDTH_INTRINSIC) {
        if (diag_is_enabled(DIAG_LAYOUT)) {
            diag_log(
                DIAG_LAYOUT, 0,
                "WIDTH_INTRINSIC: starting (opt->width=%d, opt->measuring_mode=%d, n_children=%d)",
                opt ? opt->width : -1, opt ? opt->measuring_mode : 0, layout->n_children);
        }

        // First pass: render at large width in measuring mode
        CompOptions measure_opt = opt ? *opt : default_options();
        measure_opt.width = 1000;
        measure_opt.document_width = 1000; // Match width to avoid false \setwidth detection
        measure_opt.measuring_mode = true;

        // Temporarily set computed_width to avoid infinite recursion
        layout->computed_width = 1000;

        Box *measure_box = render_vbox(layout, &measure_opt, err);
        if (!measure_box) {
            layout->computed_width = WIDTH_INTRINSIC; // Reset on error
            return NULL;
        }

        // Measure actual content width (leftmost to rightmost content)
        int measured_width = measure_content_span(measure_box);
        if (measured_width < 1) measured_width = 1;

        if (diag_is_enabled(DIAG_LAYOUT)) {
            diag_log(DIAG_LAYOUT, 0,
                     "WIDTH_INTRINSIC: measured content width = %d (box w=%d, h=%d)",
                     measured_width, measure_box->w, measure_box->h);
        }

        // Set computed_width to measured value for THIS layout only
        // IMPORTANT: Do NOT re-resolve children's widths - this would break centering
        // for content that should center relative to the document width, not the
        // intrinsic width. Children already have their widths resolved from the
        // initial width resolution pass.
        layout->computed_width = measured_width;

        // If we're being measured by an outer \measure call (measuring_mode=true),
        // keep measuring_mode=true for the second pass to avoid alignment
        // that could cause content clipping during measurement
        bool keep_measuring = (opt && opt->measuring_mode);

        if (diag_is_enabled(DIAG_LAYOUT)) {
            diag_log(DIAG_LAYOUT, 1, "second pass: measured_width=%d, keep_measuring=%d",
                     measured_width, keep_measuring);
        }

        // Free measurement box - we'll do a properly-sized pass
        box_free(measure_box);
        free(measure_box);

        // Second pass: render at measured width
        // Keep measuring_mode if we're inside an outer measurement
        // DON'T override opt->width - let computed_width control the width
        CompOptions final_opt = opt ? *opt : default_options();
        final_opt.measuring_mode = keep_measuring;

        if (diag_is_enabled(DIAG_LAYOUT)) {
            diag_log(DIAG_LAYOUT, 1, "rendering final box (computed_width=%d, opt->width=%d)",
                     layout->computed_width, final_opt.width);
        }

        Box *final_box = render_vbox(layout, &final_opt, err);

        if (diag_is_enabled(DIAG_LAYOUT) && final_box) {
            diag_result(DIAG_LAYOUT, 1, "final box: w=%d, h=%d", final_box->w, final_box->h);
        }

        return final_box;
    }

    // Determine the target width for alignment and auto hrules
    // Priority: computed_width (from flex) > explicit width > opt->width > fallback 80
    int target_width;
    if (layout->computed_width > 0) {
        // Check if this is document-level content where \setwidth changed the width
        // This applies to vboxes with inherited width (-1) at document level
        bool is_doc_level_inherited = layout->width == -1 && // With inherited width
                                      opt && opt->document_width > 0 &&
                                      layout->computed_width == opt->document_width &&
                                      opt->width != opt->document_width;
        if (is_doc_level_inherited) {
            // Document-level vbox and \setwidth was used - use opt->width
            target_width = opt->width;
        } else {
            // Use computed_width from flex distribution or explicit setting
            target_width = layout->computed_width;
        }
    } else if (layout->width > 0) {
        // Explicit width set on this vbox - respect it
        target_width = layout->width;
    } else if (opt && opt->width > 0) {
        // Use opt->width which can be set by \setwidth command
        target_width = opt->width;
    } else {
        target_width = 80;
    }

    if (diag_is_enabled(DIAG_LAYOUT)) {
        diag_log(DIAG_LAYOUT, 0,
                 "render_vbox: layout->width=%d, computed_width=%d, target_width=%d, "
                 "opt->width=%d, measuring=%d, n_children=%d",
                 layout->width, layout->computed_width, target_width, opt ? opt->width : -1,
                 opt ? opt->measuring_mode : 0, layout->n_children);
    }

    // === PASS 1: Render non-auto children ===
    Box **child_boxes = malloc(layout->n_children * sizeof(Box *));
    bool *is_auto = calloc(layout->n_children, sizeof(bool));

    for (int i = 0; i < layout->n_children; i++) {
        BoxLayout *child = layout->children[i];

        // Check for auto-width hrule
        if (child->type == BOX_TYPE_HRULE && child->rule_width == RULE_SIZE_AUTO) {
            is_auto[i] = true;
            child_boxes[i] = NULL; // Placeholder
            continue;
        }

        Box *child_box = box_layout_render(child, opt, err);

        // Apply horizontal alignment if child is narrower than target width
        // Use child's alignment, or inherit from parent VBOX if child has default
        Alignment effective_align =
            (child->h_align != ALIGN_LEFT) ? child->h_align : layout->h_align;

        // Determine alignment width:
        // - For document-level CONTENT/DISPLAY_MATH (width=-1 and computed_width == document_width),
        //   use opt->width which can be modified by \setwidth
        // - For nested boxes (VBOX, HBOX) and content with explicit width, use computed_width
        // - When computed_width is WIDTH_INTRINSIC (-3), use document_width for centering
        //   This ensures proper centering relative to the document, not the measurement width
        int align_width;
        if (child->computed_width == WIDTH_INTRINSIC) {
            // Child inherited WIDTH_INTRINSIC from parent
            // Use document_width for centering (not opt->width which may be 1000 during measurement)
            // If no document_width, use actual content width (no centering)
            if (opt && opt->document_width > 0) {
                align_width = opt->document_width;
            } else {
                align_width = child_box ? child_box->w : target_width;
            }
        } else if (child->computed_width > 0) {
            // Check if this is document-level content where \setwidth changed the width
            // This applies to all box types with inherited width (-1)
            bool is_doc_level_inherited = child->width == -1 && // With inherited width
                                          opt && opt->document_width > 0 &&
                                          child->computed_width == opt->document_width &&
                                          opt->width != opt->document_width;
            if (is_doc_level_inherited) {
                // Document-level content and \setwidth was used - use opt->width
                align_width = opt->width;
            } else {
                // Nested boxes or content with explicit width - use computed_width
                align_width = child->computed_width;
            }
        } else if (child->width > 0) {
            align_width = child->width;
        } else if (opt && opt->width > 0) {
            align_width = opt->width;
        } else {
            align_width = target_width;
        }

        // Apply horizontal alignment if needed
        if (child_box && effective_align != ALIGN_LEFT) {
            if (child_box->w < align_width) {
                // Normal case: box narrower than container
                Box *aligned = apply_h_alignment(child_box, align_width, effective_align);
                if (aligned != child_box) {
                    box_free(child_box);
                    free(child_box);
                    child_box = aligned;
                }
            } else if (effective_align == ALIGN_CENTER && child_box->w == align_width &&
                       child_box->cells) {
                // Box exactly fills container with CENTER alignment.
                // Check if actual content is narrower (trailing whitespace from
                // e.g. lineroutine padding to target_width). If so, trim and
                // center based on content span.
                // Only do this if content starts at column 0 — content that's
                // already indented (e.g. list items) should not be re-centered.
                int min_left = child_box->w;
                int max_right = 0;
                for (int y = 0; y < child_box->h; y++) {
                    for (int x = 0; x < child_box->w; x++) {
                        uint32_t c = child_box->cells[y * child_box->w + x];
                        if (c != ' ' && c != 0 && c != 0xA0) {
                            if (x < min_left) min_left = x;
                            break;
                        }
                    }
                    for (int x = child_box->w - 1; x >= 0; x--) {
                        uint32_t c = child_box->cells[y * child_box->w + x];
                        if (c != ' ' && c != 0 && c != 0xA0) {
                            if (x + 1 > max_right) max_right = x + 1;
                            break;
                        }
                    }
                }
                if (min_left == 0 && max_right > 0 && max_right < align_width) {
                    // Trim to content width, then center.
                    // Per-cell style propagates correctly through apply_h_alignment.
                    Box *trimmed = calloc(1, sizeof(Box));
                    *trimmed = make_box(max_right, child_box->h, child_box->baseline);
                    for (int y = 0; y < child_box->h; y++) {
                        for (int x = 0; x < max_right; x++) {
                            trimmed->cells[y * max_right + x] =
                                child_box->cells[y * child_box->w + x];
                            if (child_box->meta) {
                                uint8_t m = child_box->meta[y * child_box->w + x];
                                if (m != CELL_META_NONE) box_set_meta(trimmed, x, y, m);
                            }
                            if (child_box->style) {
                                uint16_t s = child_box->style[y * child_box->w + x];
                                if (s) box_set_cell_style(trimmed, x, y, s);
                            }
                        }
                    }
                    Box *aligned = apply_h_alignment(trimmed, align_width, ALIGN_CENTER);
                    if (aligned != trimmed) {
                        box_free(trimmed);
                        free(trimmed);
                    }

                    box_free(child_box);
                    free(child_box);
                    child_box = aligned;
                }
            }
            // else: w > align_width — skip (handled by adjust_box_width in hbox)
        }

        // Apply bg_fill — replaces background component of every cell's style
        // Pad to target_width first so fill covers the full column, not just content
        if (child->bg_fill != 0 && child_box) {
            if (child_box->w < target_width) {
                Alignment fill_align =
                    (child->h_align != ALIGN_LEFT) ? child->h_align : layout->h_align;
                child_box = adjust_box_width(child_box, target_width, fill_align);
            }
            box_ensure_style(child_box);
            if (child_box->style) {
                uint16_t fill_bg = child->bg_fill;
                for (int idx = 0; idx < child_box->w * child_box->h; idx++) {
                    uint8_t fg = child_box->style[idx] & 0xFF;
                    child_box->style[idx] = fill_bg | fg;
                }
            }
        }

        child_boxes[i] = child_box;

        if (err && err->code != PARSE_OK) {
            cleanup_child_boxes(child_boxes, i + 1);
            free(child_boxes);
            free(is_auto);
            return NULL;
        }
    }

    // === PASS 2: Render auto-width hrules ===
    // Auto hrules use target_width (computed from flex or explicit width)
    int hrule_width = target_width;

    for (int i = 0; i < layout->n_children; i++) {
        if (!is_auto[i]) continue;

        BoxLayout *child = layout->children[i];

        // Render hrule with current width
        Box *hrule_box =
            render_hrule_box(hrule_width, child->rule_start, child->rule_fill, child->rule_end);

        child_boxes[i] = hrule_box;

        // Apply inherited ANSI style from \term_color wrapping
        if (child->inherit_style != 0) {
            box_ensure_style(hrule_box);
            if (hrule_box->style) {
                for (int idx = 0; idx < hrule_box->w * hrule_box->h; idx++) {
                    if (hrule_box->style[idx] == 0) hrule_box->style[idx] = child->inherit_style;
                }
            }
        }
    }

    free(is_auto);

    // Collect non-null boxes for merging, tracking skip offsets
    // Also track ANSI state from ANSI-only hboxes (created when \term_color wraps
    // layout commands like \Boxed). These are transparent and their style transfers
    // to adjacent content children.
    Box **merge_boxes = malloc(layout->n_children * sizeof(Box *));
    int *y_offsets = calloc(layout->n_children, sizeof(int));
    int n_merge = 0;
    int max_child_width = 0;
    int pending_skip = 0; // Accumulates skip amounts for the next non-skip child
    bool has_skips = false;

    // Track ANSI fg/bg state for per-cell style application across vbox children
    uint8_t vbox_fg = 0;
    uint8_t vbox_bg = 0;

    for (int i = 0; i < layout->n_children; i++) {
        BoxLayout *child = layout->children[i];

        // Handle VSKIP nodes - they adjust offset for the next box
        if (child->type == BOX_TYPE_VSKIP) {
            pending_skip += child->skip_amount;
            if (child->skip_amount != 0) has_skips = true;
            continue; // Don't add skip nodes to merge_boxes
        }

        // Handle bare ANSI nodes - extract fg/bg state, skip from merge
        // These are created by flush_text when leading/trailing ANSI markers
        // are stripped from paragraph text and added as bare vbox children
        if (child->type == BOX_TYPE_ANSI && child->ansi_codes) {
            const char *ap = child->ansi_codes;
            while (*ap) {
                int code = (int)strtol(ap, (char **)&ap, 10);
                if (code == 0) {
                    vbox_fg = 0;
                    vbox_bg = 0;
                } else if (code == 39) {
                    vbox_fg = 0;
                } else if (code == 49) {
                    vbox_bg = 0;
                } else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97))
                    vbox_fg = (uint8_t)code;
                else if ((code >= 40 && code <= 47) || (code >= 100 && code <= 107))
                    vbox_bg = (uint8_t)code;
                if (*ap == ';') ap++;
            }
            // child_boxes[i] is already NULL (ANSI returns NULL from box_layout_render)
            continue;
        }

        // Handle ANSI-only hboxes - extract fg/bg state, skip from merge
        // These are created by flush_text when \term_color wraps a layout command
        if (is_ansi_only_hbox(child)) {
            extract_ansi_from_hbox(child, &vbox_fg, &vbox_bg);
            // Free the rendered 1x1 box since we're skipping it
            if (child_boxes[i]) {
                box_free(child_boxes[i]);
                free(child_boxes[i]);
                child_boxes[i] = NULL;
            }
            continue;
        }

        if (child_boxes[i]) {
            // Apply current ANSI style to all unstyled cells in this box
            uint16_t style = ((uint16_t)vbox_bg << 8) | vbox_fg;
            if (style != 0) {
                Box *box = child_boxes[i];
                box_ensure_style(box);
                if (box->style) {
                    for (int idx = 0; idx < box->w * box->h; idx++) {
                        if (box->style[idx] == 0) box->style[idx] = style;
                    }
                }
            }

            y_offsets[n_merge] = pending_skip;
            pending_skip = 0; // Reset after applying to a box
            merge_boxes[n_merge++] = child_boxes[i];
            if (child_boxes[i]->w > max_child_width) {
                max_child_width = child_boxes[i]->w;
            }
        }
    }

    // Use max of target_width and max_child_width to avoid truncating wide children
    // This ensures tables and other wide content aren't cut off
    int final_width = (max_child_width > target_width) ? max_child_width : target_width;

    Box *merged = NULL;
    if (n_merge > 0) {
        if (has_skips) {
            merged = vbox_merge_with_skips(merge_boxes, y_offsets, n_merge, final_width);
        } else {
            merged = vbox_merge(merge_boxes, n_merge, final_width);
        }
    } else {
        merged = malloc(sizeof(Box));
        *merged = make_box(target_width, 1, 0);
    }

    free(y_offsets);

    // Cleanup
    cleanup_child_boxes(child_boxes, layout->n_children);
    free(child_boxes);
    free(merge_boxes);

    return merged;
}

// ============================================================================
// Helper Functions
// ============================================================================

// Cleanup child boxes array
static void cleanup_child_boxes(Box **boxes, int n) {
    for (int i = 0; i < n; i++) {
        if (boxes[i]) {
            box_free(boxes[i]);
            free(boxes[i]);
        }
    }
}

// Adjust box width with padding or clipping
static Box *adjust_box_width(Box *child_box, int target_width, Alignment h_align) {
    if (!child_box || !child_box->cells) return NULL;
    int content_width = child_box->w;

    Box *adjusted = malloc(sizeof(Box));
    *adjusted = make_box(target_width, child_box->h, child_box->baseline);

    // Fill with spaces
    for (int y = 0; y < adjusted->h; y++) {
        for (int x = 0; x < adjusted->w; x++) {
            adjusted->cells[y * adjusted->w + x] = ' ';
        }
    }

    if (content_width <= target_width) {
        // Padding
        int pad_left = 0;
        int padding_total = target_width - content_width;
        switch (h_align) {
        case ALIGN_CENTER: pad_left = padding_total / 2; break;
        case ALIGN_RIGHT: pad_left = padding_total; break;
        default: pad_left = 0; break;
        }
        for (int y = 0; y < child_box->h; y++) {
            for (int x = 0; x < content_width; x++) {
                if (pad_left + x < target_width) {
                    int dst_idx = y * adjusted->w + (pad_left + x);
                    int src_idx = y * content_width + x;
                    adjusted->cells[dst_idx] = child_box->cells[src_idx];
                    // Copy metadata if present
                    if (child_box->meta) {
                        uint8_t m = child_box->meta[src_idx];
                        if (m != CELL_META_NONE) {
                            box_set_meta(adjusted, pad_left + x, y, m);
                        }
                    }
                    // Copy style if present
                    if (child_box->style) {
                        uint16_t s = child_box->style[src_idx];
                        if (s) box_set_cell_style(adjusted, pad_left + x, y, s);
                    }
                }
            }
        }
    } else {
        // Clipping
        int clip_left = 0;
        switch (h_align) {
        case ALIGN_CENTER: clip_left = (content_width - target_width) / 2; break;
        case ALIGN_RIGHT: clip_left = content_width - target_width; break;
        default: clip_left = 0; break;
        }
        for (int y = 0; y < child_box->h; y++) {
            for (int x = 0; x < target_width; x++) {
                int src_x = clip_left + x;
                if (src_x >= 0 && src_x < content_width) {
                    int dst_idx = y * adjusted->w + x;
                    int src_idx = y * content_width + src_x;
                    adjusted->cells[dst_idx] = child_box->cells[src_idx];
                    // Copy metadata if present
                    if (child_box->meta) {
                        uint8_t m = child_box->meta[src_idx];
                        if (m != CELL_META_NONE) {
                            box_set_meta(adjusted, x, y, m);
                        }
                    }
                    // Copy style if present
                    if (child_box->style) {
                        uint16_t s = child_box->style[src_idx];
                        if (s) box_set_cell_style(adjusted, x, y, s);
                    }
                }
            }
        }
    }

    box_free(child_box);
    free(child_box);
    return adjusted;
}

// ============================================================================
// Utility: Compose Text to Fixed Width Box
// ============================================================================

Box *compose_text_fixed_width(const char *text, int width) {
    if (!text || !*text) {
        Box *empty = malloc(sizeof(Box));
        *empty = make_box(1, 1, 0);
        return empty;
    }

    CompOptions opt = default_options();
    opt.width = width;

    ParseError err = {0};
    char *result = compose_text(text, &opt, &err);

    if (!result) {
        Box *empty = malloc(sizeof(Box));
        *empty = make_box(1, 1, 0);
        return empty;
    }

    Box *box = string_to_box(result);
    free(result);
    return box;
}

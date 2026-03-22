#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostics/diagnostics.h"
#include "document/document.h"
#include "hyades_kernel_internal.h"

BoxLayout *parse_hrule_command(const char *input, int *pos);
BoxLayout *parse_vrule_command(const char *input, int *pos);

// ============================================================================
// Parser for Box Layout Syntax
// ============================================================================

// Width values:
//   -1 = not specified (inherit/auto depending on context)
//   -2 = explicitly "auto" (same as -1, but explicit)
//   >0 = fixed width
#define WIDTH_NOT_SPECIFIED -1
#define WIDTH_AUTO -2

// Parse optional [width] or [auto] parameter
// Returns width, WIDTH_NOT_SPECIFIED if no bracket, or WIDTH_AUTO for [auto]
static int parse_width_param(const char *input, int *pos) {
    const char *p = input + *pos;

    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    if (*p != '[') return WIDTH_NOT_SPECIFIED; // No bracket at all

    // Peek inside the bracket to see if it's a width or alignment
    const char *inside = p + 1;
    while (*inside == ' ' || *inside == '\t') inside++;

    // Check for "auto"
    if (strncmp(inside, "auto", 4) == 0) {
        const char *after = inside + 4;
        if (*after == ']' || *after == ' ' || *after == '\t') {
            // It's [auto] - consume it
            p = after;
            while (*p == ' ' || *p == '\t') p++;
            if (*p != ']') return WIDTH_NOT_SPECIFIED; // Malformed
            p++;                                       // Skip ']'
            *pos = (int)(p - input);
            return WIDTH_AUTO;
        }
    }

    // Check for "intrinsic" - render to natural size
    if (strncmp(inside, "intrinsic", 9) == 0) {
        const char *after = inside + 9;
        if (*after == ']' || *after == ' ' || *after == '\t') {
            // It's [intrinsic] - consume it
            p = after;
            while (*p == ' ' || *p == '\t') p++;
            if (*p != ']') return WIDTH_NOT_SPECIFIED; // Malformed
            p++;                                       // Skip ']'
            *pos = (int)(p - input);
            return WIDTH_INTRINSIC;
        }
    }

    // Check for number
    if (isdigit((unsigned char)*inside)) {
        // It's a number - consume the bracket and parse
        p = inside;
        int width = 0;
        while (isdigit((unsigned char)*p)) {
            width = width * 10 + (*p - '0');
            p++;
        }

        // Skip whitespace
        while (*p == ' ' || *p == '\t') p++;

        if (*p != ']') return WIDTH_NOT_SPECIFIED; // Malformed
        p++;                                       // Skip ']'

        *pos = (int)(p - input);
        return width;
    }

    // It's something else (like [center]) - don't consume, let align parser handle it
    return WIDTH_NOT_SPECIFIED;
}

// Parse alignment parameter (optional [...])
static Alignment parse_align_param(const char *input, int *pos, Alignment default_align) {
    const char *p = input + *pos;

    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    if (*p != '[') return default_align; // No alignment

    p++; // Skip '['

    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    // Read alignment name
    const char *start = p;
    while (isalpha((unsigned char)*p)) p++;

    size_t len = p - start;
    char name[32];
    if (len >= sizeof(name)) return default_align;
    memcpy(name, start, len);
    name[len] = '\0';

    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    if (*p != ']') return default_align;
    p++; // Skip ']'

    *pos = (int)(p - input);

    // Parse alignment
    if (strcmp(name, "left") == 0) return ALIGN_LEFT;
    if (strcmp(name, "center") == 0) return ALIGN_CENTER;
    if (strcmp(name, "right") == 0) return ALIGN_RIGHT;
    if (strcmp(name, "justify") == 0) return ALIGN_JUSTIFY;
    if (strcmp(name, "top") == 0) return ALIGN_TOP;
    if (strcmp(name, "middle") == 0) return ALIGN_MIDDLE;
    if (strcmp(name, "bottom") == 0) return ALIGN_BOTTOM;

    return default_align;
}

// Map color name to background SGR code (0 = unknown)
static uint8_t parse_bg_color_name(const char *name) {
    if (strcmp(name, "black") == 0) return 40;
    if (strcmp(name, "red") == 0) return 41;
    if (strcmp(name, "green") == 0) return 42;
    if (strcmp(name, "yellow") == 0) return 43;
    if (strcmp(name, "blue") == 0) return 44;
    if (strcmp(name, "magenta") == 0) return 45;
    if (strcmp(name, "cyan") == 0) return 46;
    if (strcmp(name, "white") == 0) return 47;
    if (strcmp(name, "bright_black") == 0) return 100;
    if (strcmp(name, "bright_red") == 0) return 101;
    if (strcmp(name, "bright_green") == 0) return 102;
    if (strcmp(name, "bright_yellow") == 0) return 103;
    if (strcmp(name, "bright_blue") == 0) return 104;
    if (strcmp(name, "bright_magenta") == 0) return 105;
    if (strcmp(name, "bright_cyan") == 0) return 106;
    if (strcmp(name, "bright_white") == 0) return 107;
    return 0;
}

// Parse optional [bg=colorname] parameter
// Returns (bg_sgr << 8) or 0 if not found/invalid
static uint16_t parse_bg_option(const char *input, int *pos) {
    const char *p = input + *pos;

    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    if (*p != '[') return 0;

    // Peek: expect "bg="
    const char *inside = p + 1;
    while (*inside == ' ' || *inside == '\t') inside++;

    if (strncmp(inside, "bg=", 3) != 0) return 0;

    inside += 3; // skip "bg="

    // Read color name
    const char *name_start = inside;
    while (isalpha((unsigned char)*inside) || *inside == '_') inside++;

    size_t name_len = inside - name_start;
    if (name_len == 0 || name_len >= 32) return 0;

    char name[32];
    memcpy(name, name_start, name_len);
    name[name_len] = '\0';

    // Skip whitespace before ']'
    while (*inside == ' ' || *inside == '\t') inside++;
    if (*inside != ']') return 0;
    inside++; // skip ']'

    uint8_t bg_sgr = parse_bg_color_name(name);
    if (bg_sgr == 0) return 0;

    *pos = (int)(inside - input);
    return (uint16_t)bg_sgr << 8;
}

// Forward declaration
static BoxLayout *parse_box_layout_content(const char *input, int *pos, const char *end_marker,
                                           int target_width);

// Parse \xvskip{n} or \xhskip{n} - returns NULL if not found
// n can be positive or negative
static BoxLayout *parse_xskip(const char *input, int *pos) {
    const char *p = input + *pos;

    // Check for \xvskip or \xhskip
    bool is_vskip = (strncmp(p, "\\xvskip{", 8) == 0);
    bool is_hskip = (strncmp(p, "\\xhskip{", 8) == 0);

    if (!is_vskip && !is_hskip) return NULL;

    p += 8; // Skip "\xvskip{" or "\xhskip{"

    // Parse the number (can be negative)
    bool negative = false;
    if (*p == '-') {
        negative = true;
        p++;
    }

    if (!isdigit((unsigned char)*p)) return NULL; // Must have at least one digit

    int amount = 0;
    while (isdigit((unsigned char)*p)) {
        amount = amount * 10 + (*p - '0');
        p++;
    }

    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    // Expect '}'
    if (*p != '}') return NULL;
    p++;

    if (negative) amount = -amount;

    *pos = (int)(p - input);

    // Create skip node
    BoxLayout *skip = box_layout_new(is_vskip ? BOX_TYPE_VSKIP : BOX_TYPE_HSKIP, -1);
    skip->skip_amount = amount;

    if (diag_is_enabled(DIAG_LAYOUT)) {
        diag_log(DIAG_LAYOUT, 1, "\\x%skip{%d}", is_vskip ? "v" : "h", amount);
    }

    return skip;
}

// Parse \child[width][align]{content}
// Width can be: number, "auto", or omitted
// If omitted, alignment can still be specified: \child[center]{...}
// target_width: the document/parent width to use for children without explicit width
static BoxLayout *parse_child(const char *input, int *pos, int target_width) {
    const char *p = input + *pos;

    // Expect "\child"
    if (strncmp(p, "\\child", 6) != 0) return NULL;
    p += 6;

    int temp_pos = (int)(p - input);
    int width = parse_width_param(input, &temp_pos);
    p = input + temp_pos;

    // Parse alignment (second bracket, or first if width wasn't specified)
    Alignment align = parse_align_param(input, &temp_pos, ALIGN_LEFT);
    p = input + temp_pos;

    // Parse optional [bg=color] parameter
    uint16_t bg_fill = parse_bg_option(input, &temp_pos);
    p = input + temp_pos;

    // Parse optional vertical alignment [top|middle|bottom]
    int v_align_pos = temp_pos;
    Alignment v_align = parse_align_param(input, &temp_pos, ALIGN_TOP);
    bool v_align_explicit = (temp_pos != v_align_pos);
    p = input + temp_pos;

    // Skip whitespace (including newlines for resilient partial-input parsing)
    while (*p == ' ' || *p == '\t' || *p == '\n') p++;

    // Expect '{'
    if (*p != '{') return NULL;
    p++;

    // Find matching '}'
    const char *content_start = p;
    int brace_depth = 1;
    while (*p && brace_depth > 0) {
        if (*p == '{')
            brace_depth++;
        else if (*p == '}')
            brace_depth--;
        if (brace_depth > 0) p++;
    }

    if (brace_depth != 0) return NULL; // Unclosed brace

    size_t content_len = p - content_start;
    char *content = malloc(content_len + 1);
    memcpy(content, content_start, content_len);
    content[content_len] = '\0';

    p++; // Skip '}'

    *pos = (int)(p - input);

    // Normalize width: both -1 and -2 mean "auto" for layout purposes
    // We keep -1 as the internal representation
    int effective_width = (width == WIDTH_AUTO) ? -1 : width;

    if (diag_is_enabled(DIAG_LAYOUT)) {
        const char *align_name = (align == ALIGN_CENTER)    ? "center"
                                 : (align == ALIGN_RIGHT)   ? "right"
                                 : (align == ALIGN_JUSTIFY) ? "justify"
                                                            : "left";
        diag_log(DIAG_LAYOUT, 1, "\\child width=%d align=%s", effective_width, align_name);
        diag_result(DIAG_LAYOUT, 2, "content: %.50s%s", content, strlen(content) > 50 ? "..." : "");
    }

    // Check if content starts with \begin (direct nested box layout)
    // Note: we use strncmp, not strstr, to avoid matching \begin inside
    // other constructs like \intersect_rules{\begin...}
    const char *content_trimmed = content;
    while (*content_trimmed == ' ' || *content_trimmed == '\t' || *content_trimmed == '\n') {
        content_trimmed++;
    }
    if (strncmp(content_trimmed, "\\begin", 6) == 0) {
        // Parse nested layout
        int inner_pos = (int)(content_trimmed - content);
        BoxLayout *nested = parse_box_layout_content(content, &inner_pos, NULL, target_width);
        if (nested) {
            free(content);
            if (effective_width != -1) {
                nested->width = effective_width;
                nested->computed_width = effective_width;
            }
            nested->h_align = align;
            // Only override nested layout's v_align if \child explicitly specified one.
            // Otherwise preserve the nested layout's own setting (e.g., \begin[middle]{hbox}).
            if (v_align_explicit) {
                nested->v_align = v_align;
            }
            nested->bg_fill = bg_fill;
            return nested;
        }
        // If parsing failed, fall through to document parser below
    }

    {
        // Check if content is a rule command
        if (strncmp(content, "\\hrule", 6) == 0) {
            int rule_pos = 0;
            BoxLayout *rule = parse_hrule_command(content, &rule_pos);
            free(content);
            if (rule && effective_width != -1) {
                rule->width = effective_width;
                rule->computed_width = effective_width;
            }
            if (rule) rule->h_align = align;
            if (rule) rule->v_align = v_align;
            if (rule) rule->bg_fill = bg_fill;
            return rule;
        } else if (strncmp(content, "\\vrule", 6) == 0) {
            int rule_pos = 0;
            BoxLayout *rule = parse_vrule_command(content, &rule_pos);
            free(content);
            if (rule && effective_width != -1) {
                rule->width = effective_width;
                rule->computed_width = effective_width;
            }
            if (rule) rule->h_align = align;
            if (rule) rule->v_align = v_align;
            if (rule) rule->bg_fill = bg_fill;
            return rule;
        }

        // Parse all content through document parser to handle \\, \vskip, etc.
        // This is more robust than scanning for specific patterns.
        ParseError err = {0};
        int content_width = effective_width > 0 ? effective_width : target_width;
        BoxLayout *parsed = parse_document_as_vbox(content, content_width, &err);
        free(content);

        // Unwrap vrule/hrule from ANSI-wrapping vbox.
        // When \term_color wraps a \vrule or \hrule, parse_document_as_vbox
        // produces: vbox [ ANSI-only-hbox, vrule/hrule, ANSI-only-hbox ].
        // Unwrap the rule so it's a direct child of the parent hbox,
        // preserving auto-scaling. Carry ANSI style via inherit_style.
        if (parsed && parsed->type == BOX_TYPE_VBOX) {
            BoxLayout *rule = NULL;
            int rule_idx = -1;
            bool all_ansi_or_rule = true;

            for (int ci = 0; ci < parsed->n_children; ci++) {
                BoxLayout *ch = parsed->children[ci];
                if (ch->type == BOX_TYPE_VRULE || ch->type == BOX_TYPE_HRULE) {
                    if (rule) {
                        all_ansi_or_rule = false;
                        break;
                    } // >1 rule
                    rule = ch;
                    rule_idx = ci;
                } else if (ch->type == BOX_TYPE_ANSI) {
                    // Bare ANSI node (from flush_text stripping leading/trailing markers)
                    // This is fine — it's just a style marker
                } else if (ch->type == BOX_TYPE_HBOX) {
                    // Check if ANSI-only hbox
                    bool is_ansi = false;
                    for (int j = 0; j < ch->n_children; j++) {
                        BoxLayout *sub = ch->children[j];
                        if (sub->type == BOX_TYPE_ANSI) {
                            is_ansi = true;
                        } else if (sub->type == BOX_TYPE_CONTENT && sub->content) {
                            const char *s = sub->content;
                            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
                            if (*s != '\0') {
                                all_ansi_or_rule = false;
                                break;
                            }
                        } else {
                            all_ansi_or_rule = false;
                            break;
                        }
                    }
                    if (!is_ansi) all_ansi_or_rule = false;
                } else {
                    all_ansi_or_rule = false;
                }
                if (!all_ansi_or_rule) break;
            }

            if (all_ansi_or_rule && rule) {
                // Extract ANSI style from ANSI children BEFORE the rule
                // (children after the rule are the reset sequence)
                uint8_t fg = 0, bg = 0;
                for (int ci = 0; ci < rule_idx; ci++) {
                    BoxLayout *ch = parsed->children[ci];
                    if (ch->type == BOX_TYPE_ANSI && ch->ansi_codes) {
                        // Bare ANSI node (from flush_text leading marker stripping)
                        const char *s = ch->ansi_codes;
                        while (*s) {
                            int code = (int)strtol(s, (char **)&s, 10);
                            if (code == 0) {
                                fg = 0;
                                bg = 0;
                            } else if (code == 39)
                                fg = 0;
                            else if (code == 49)
                                bg = 0;
                            else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97))
                                fg = (uint8_t)code;
                            else if ((code >= 40 && code <= 47) || (code >= 100 && code <= 107))
                                bg = (uint8_t)code;
                            if (*s == ';') s++;
                        }
                    } else if (ch->type == BOX_TYPE_HBOX) {
                        for (int j = 0; j < ch->n_children; j++) {
                            BoxLayout *sub = ch->children[j];
                            if (sub->type == BOX_TYPE_ANSI && sub->ansi_codes) {
                                const char *s = sub->ansi_codes;
                                while (*s) {
                                    int code = (int)strtol(s, (char **)&s, 10);
                                    if (code == 0) {
                                        fg = 0;
                                        bg = 0;
                                    } else if (code == 39)
                                        fg = 0;
                                    else if (code == 49)
                                        bg = 0;
                                    else if ((code >= 30 && code <= 37) ||
                                             (code >= 90 && code <= 97))
                                        fg = (uint8_t)code;
                                    else if ((code >= 40 && code <= 47) ||
                                             (code >= 100 && code <= 107))
                                        bg = (uint8_t)code;
                                    if (*s == ';') s++;
                                }
                            }
                        }
                    }
                }

                // Detach rule from vbox, free the rest
                parsed->children[rule_idx] = NULL;
                box_layout_free(parsed);

                rule->inherit_style = ((uint16_t)bg << 8) | fg;
                if (effective_width != -1) {
                    rule->width = effective_width;
                    rule->computed_width = effective_width;
                }
                rule->h_align = align;
                rule->v_align = v_align;
                rule->bg_fill = bg_fill;
                return rule;
            }
        }

        if (parsed) {
            // If the result is a VBOX with a single CONTENT child (simple text),
            // unwrap it to avoid unnecessary nesting that could affect rendering
            if (parsed->type == BOX_TYPE_VBOX && parsed->n_children == 1 &&
                parsed->children[0]->type == BOX_TYPE_CONTENT) {
                BoxLayout *unwrapped = parsed->children[0];
                parsed->children[0] = NULL; // Prevent child from being freed
                parsed->n_children = 0;
                box_layout_free(parsed);

                unwrapped->h_align = align;
                unwrapped->v_align = v_align;
                unwrapped->bg_fill = bg_fill;
                if (effective_width > 0) {
                    unwrapped->width = effective_width;
                    unwrapped->computed_width = effective_width;
                }
                return unwrapped;
            }

            // Otherwise return the parsed VBOX with proper width/alignment
            // IMPORTANT: Always set width, even for flex (-1), so HBOX flex
            // distribution works correctly with multi-line content
            parsed->width = effective_width;
            if (effective_width > 0) {
                parsed->computed_width = effective_width;
            }
            parsed->h_align = align;
            parsed->v_align = v_align;
            parsed->bg_fill = bg_fill;
            return parsed;
        }

        // Fallback: create empty content node
        BoxLayout *child = box_layout_new(BOX_TYPE_CONTENT, effective_width);
        child->h_align = align;
        child->v_align = v_align;
        child->bg_fill = bg_fill;
        return child;
    }
}

// Parse \begin[width]{vbox|hbox}...\end{vbox|hbox}
// target_width: the document-level width to use for children without explicit width
static BoxLayout *parse_box_layout_content(const char *input, int *pos, const char *end_marker,
                                           int target_width) {
    const char *p = input + *pos;

    // Skip whitespace
    while (*p == ' ' || *p == '\t' || *p == '\n') p++;

    // Expect "\begin"
    if (strncmp(p, "\\begin", 6) != 0) return NULL;
    p += 6;

    // Parse [width] or [alignment] or both
    int temp_pos = (int)(p - input);
    int width = parse_width_param(input, &temp_pos);
    // For \begin, treat -2 (explicit auto) same as -1
    if (width == WIDTH_AUTO) width = -1;
    p = input + temp_pos;

    // Try parsing alignment (e.g., [top])
    temp_pos = (int)(p - input);
    Alignment v_align =
        parse_align_param(input, &temp_pos, ALIGN_TOP); // Default: no baseline align
    p = input + temp_pos;

    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    // Expect '{type}'
    if (*p != '{') return NULL;
    p++;

    const char *type_start = p;
    while (*p && *p != '}') p++;
    if (*p != '}') return NULL;

    size_t type_len = p - type_start;
    char type_name[16];
    if (type_len >= sizeof(type_name)) return NULL;
    memcpy(type_name, type_start, type_len);
    type_name[type_len] = '\0';

    p++; // Skip '}'

    // Determine box type
    BoxLayoutType type;
    if (strcmp(type_name, "vbox") == 0) {
        type = BOX_TYPE_VBOX;
    } else if (strcmp(type_name, "hbox") == 0) {
        type = BOX_TYPE_HBOX;
    } else {
        return NULL; // Unknown type
    }

    // Create layout
    BoxLayout *layout = box_layout_new(type, width);
    layout->v_align =
        v_align; // Set vertical alignment (ALIGN_TOP = no baseline, ALIGN_MIDDLE = baseline)

    if (diag_is_enabled(DIAG_LAYOUT)) {
        diag_log(DIAG_LAYOUT, 0, "\\begin{%s} width=%d v_align=%d", type_name, width, v_align);
    }

    // Parse children until \end{type}
    char end_tag[32];
    snprintf(end_tag, sizeof(end_tag), "\\end{%s}", type_name);

    // Safety limit to prevent runaway parsing of malformed input
    int skip_count = 0;
    const int MAX_SKIPS = 10000;

    while (*p) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;

        // Check if we reached end of input after skipping whitespace
        if (!*p) break;

        // Check for end tag
        if (strncmp(p, end_tag, strlen(end_tag)) == 0) {
            p += strlen(end_tag);
            *pos = (int)(p - input);
            if (diag_is_enabled(DIAG_LAYOUT)) {
                diag_result(DIAG_LAYOUT, 0, "parsed %d children", layout->n_children);
            }
            return layout;
        }

        // Try to parse xskip first (for \xvskip and \xhskip in box layouts)
        int skip_pos = (int)(p - input);
        BoxLayout *skip = parse_xskip(input, &skip_pos);

        if (skip) {
            box_layout_add_child(layout, skip);
            p = input + skip_pos;
            skip_count = 0; // Reset on successful parse
            continue;
        }

        // Parse child
        int child_pos = (int)(p - input);
        BoxLayout *child = parse_child(input, &child_pos, target_width);

        if (child) {
            box_layout_add_child(layout, child);
            p = input + child_pos;
            skip_count = 0; // Reset on successful parse
        } else {
            // Not a child, skip character
            p++;
            skip_count++;
            if (skip_count > MAX_SKIPS) {
                // Too many skipped characters - malformed input, bail out
                box_layout_free(layout);
                return NULL;
            }
        }
    }

    // Unclosed \begin
    box_layout_free(layout);
    return NULL;
}

// Internal: Parse box layout with explicit target width
// Used by document parser to pass document width to box layouts
BoxLayout *parse_box_layout_with_width(const char *input, int *end_pos, int target_width) {
    if (!input) return NULL;
    int pos = 0;
    BoxLayout *result = parse_box_layout_content(input, &pos, NULL, target_width);
    if (end_pos) *end_pos = pos;
    return result;
}

// Public API: Parse box layout from string
// Uses default width of 80 for standalone box layout parsing
BoxLayout *parse_box_layout(const char *input, int *end_pos) {
    return parse_box_layout_with_width(input, end_pos, 80);
}

// Test if string contains box layout syntax
bool is_box_layout(const char *input) {
    if (!input) return false;
    return (strstr(input, "\\begin{vbox}") != NULL || strstr(input, "\\begin{hbox}") != NULL ||
            strstr(input, "\\begin[") != NULL);
}

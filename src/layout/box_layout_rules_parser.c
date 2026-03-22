// box_layout_rules_parser.c - Parser for \hrule and \vrule with auto support

#include "diagnostics/diagnostics.h"
#include "hyades_kernel_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Parse \hrule and \vrule Commands
// ============================================================================

// Sentinel value for "no bracket present" (distinct from RULE_SIZE_AUTO)
#define RULE_SIZE_NOT_SPECIFIED -2

// Parse a {...} argument, returns allocated string
static char *parse_braced_arg(const char *input, int *pos) {
    const char *p = input + *pos;

    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    if (*p != '{') return NULL;
    p++; // Skip '{'

    const char *start = p;
    int depth = 1;

    // Find matching '}', handling escaped braces \{ and \}
    while (*p && depth > 0) {
        if (*p == '\\' && (p[1] == '{' || p[1] == '}')) {
            // Escaped brace - skip both characters, doesn't affect depth
            p += 2;
        } else if (*p == '{') {
            depth++;
            p++;
        } else if (*p == '}') {
            depth--;
            if (depth > 0) p++;
        } else {
            p++;
        }
    }

    if (depth != 0) return NULL; // Unclosed brace

    size_t len = p - start;
    char *arg = malloc(len + 1);
    memcpy(arg, start, len);
    arg[len] = '\0';

    p++; // Skip '}'
    *pos = (int)(p - input);

    return arg;
}

// Parse [number] or [auto] parameter
// Returns: RULE_SIZE_NOT_SPECIFIED if no bracket, RULE_SIZE_AUTO for [auto], or the numeric value
static int parse_size_bracket(const char *input, int *pos) {
    const char *p = input + *pos;

    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    if (*p != '[') return RULE_SIZE_NOT_SPECIFIED; // No bracket present
    p++;                                           // Skip '['

    // Skip whitespace inside bracket
    while (*p == ' ' || *p == '\t') p++;

    // Check for "auto"
    if (strncmp(p, "auto", 4) == 0) {
        const char *after = p + 4;
        // Make sure "auto" is followed by ] or whitespace
        if (*after == ']' || *after == ' ' || *after == '\t') {
            p = after;
            while (*p == ' ' || *p == '\t') p++;
            if (*p != ']') return RULE_SIZE_NOT_SPECIFIED; // Malformed
            p++;                                           // Skip ']'
            *pos = (int)(p - input);
            return RULE_SIZE_AUTO;
        }
    }

    // Parse number
    if (!isdigit((unsigned char)*p)) return RULE_SIZE_NOT_SPECIFIED;

    int value = 0;
    while (isdigit((unsigned char)*p)) {
        value = value * 10 + (*p - '0');
        p++;
    }

    // Skip whitespace
    while (*p == ' ' || *p == '\t') p++;

    if (*p != ']') return RULE_SIZE_NOT_SPECIFIED;
    p++; // Skip ']'

    *pos = (int)(p - input);
    return value;
}

// ============================================================================
// \hrule[width]{left}{fill}{right}
// width can be a number or "auto"
// ============================================================================

BoxLayout *parse_hrule_command(const char *input, int *pos) {
    const char *p = input + *pos;

    // Expect "\hrule"
    if (strncmp(p, "\\hrule", 6) != 0) return NULL;
    p += 6;

    int temp_pos = (int)(p - input);

    // Parse optional [width] - may be "auto"
    int width = parse_size_bracket(input, &temp_pos);
    if (width == RULE_SIZE_NOT_SPECIFIED) width = RULE_SIZE_AUTO; // Default to auto width
    // Note: RULE_SIZE_AUTO (-1) passes through for auto-sizing
    p = input + temp_pos;

    // Parse optional {left}{fill}{right} - all three or none
    temp_pos = (int)(p - input);
    char *left = parse_braced_arg(input, &temp_pos);
    char *fill = NULL;
    char *right = NULL;

    if (left) {
        p = input + temp_pos;

        // Parse {fill}
        temp_pos = (int)(p - input);
        fill = parse_braced_arg(input, &temp_pos);
        if (!fill) {
            free(left);
            return NULL;
        }
        p = input + temp_pos;

        // Parse {right}
        temp_pos = (int)(p - input);
        right = parse_braced_arg(input, &temp_pos);
        if (!right) {
            free(left);
            free(fill);
            return NULL;
        }
        p = input + temp_pos;
    } else {
        // No braced args - use defaults (empty ends, dash fill)
        left = strdup("");
        fill = strdup("─");
        right = strdup("");
    }

    *pos = (int)(p - input);

    // Log hrule parsing
    if (diag_is_enabled(DIAG_LAYOUT)) {
        if (width == RULE_SIZE_AUTO) {
            diag_log(DIAG_LAYOUT, 0, "\\hrule[auto] left=\"%s\" fill=\"%s\" right=\"%s\"",
                     left ? left : "", fill ? fill : "", right ? right : "");
        } else {
            diag_log(DIAG_LAYOUT, 0, "\\hrule[%d] left=\"%s\" fill=\"%s\" right=\"%s\"", width,
                     left ? left : "", fill ? fill : "", right ? right : "");
        }
    }

    // Use auto constructor which handles both auto and fixed cases
    BoxLayout *rule = box_layout_hrule_auto(width, left, fill, right);

    free(left);
    free(fill);
    free(right);

    return rule;
}

// ============================================================================
// \vrule[width][height]{top}{fill}{bottom}
// height can be a number or "auto"
// ============================================================================

BoxLayout *parse_vrule_command(const char *input, int *pos) {
    const char *p = input + *pos;

    // Expect "\vrule"
    if (strncmp(p, "\\vrule", 6) != 0) return NULL;
    p += 6;

    int temp_pos = (int)(p - input);

    // Parse first optional bracket [width] or [auto]
    int first_bracket = parse_size_bracket(input, &temp_pos);

    // Parse second optional bracket [height]
    int second_temp_pos = temp_pos;
    int second_bracket = parse_size_bracket(input, &second_temp_pos);

    int width, height;

    if (first_bracket == RULE_SIZE_NOT_SPECIFIED) {
        // No brackets at all: \vrule{}{║}{}
        width = 1;
        height = 3; // Default
        // temp_pos already at correct position (after \vrule)
    } else if (first_bracket == RULE_SIZE_AUTO && second_bracket == RULE_SIZE_NOT_SPECIFIED) {
        // Single [auto]: \vrule[auto]{}{║}{} - means auto HEIGHT (width=1)
        width = 1;
        height = RULE_SIZE_AUTO;
        // temp_pos already updated by first parse_size_bracket
    } else if (second_bracket == RULE_SIZE_NOT_SPECIFIED) {
        // Single numeric bracket: \vrule[N]{}{║}{} - ambiguous, treat as width
        // (for backward compatibility)
        width = first_bracket;
        height = 3; // Default
        // temp_pos already updated by first parse_size_bracket
    } else {
        // Two brackets: \vrule[W][H]{}{║}{}
        width = (first_bracket == RULE_SIZE_AUTO) ? 1 : first_bracket;
        height = second_bracket;
        temp_pos = second_temp_pos; // Update to position after second bracket
    }

    p = input + temp_pos;

    // Parse {top}
    temp_pos = (int)(p - input);
    char *top = parse_braced_arg(input, &temp_pos);
    if (!top) return NULL;
    p = input + temp_pos;

    // Parse {fill}
    temp_pos = (int)(p - input);
    char *fill = parse_braced_arg(input, &temp_pos);
    if (!fill) {
        free(top);
        return NULL;
    }
    p = input + temp_pos;

    // Parse {third} - could be center or bottom depending on if there's a 4th arg
    temp_pos = (int)(p - input);
    char *third = parse_braced_arg(input, &temp_pos);
    if (!third) {
        free(top);
        free(fill);
        return NULL;
    }
    p = input + temp_pos;

    // Try to parse optional 4th arg {bottom}
    int fourth_temp_pos = (int)(p - input);
    char *fourth = parse_braced_arg(input, &fourth_temp_pos);

    char *center = NULL;
    char *bottom = NULL;

    if (fourth) {
        // 4 args: {top}{fill}{center}{bottom}
        center = third;
        bottom = fourth;
        p = input + fourth_temp_pos;
    } else {
        // 3 args: {top}{fill}{bottom} - no center
        bottom = third;
    }

    *pos = (int)(p - input);

    // Log vrule parsing
    if (diag_is_enabled(DIAG_LAYOUT)) {
        if (height == RULE_SIZE_AUTO) {
            diag_log(DIAG_LAYOUT, 0,
                     "\\vrule[%d][auto] top=\"%s\" fill=\"%s\" center=\"%s\" bottom=\"%s\"", width,
                     top ? top : "", fill ? fill : "", center ? center : "(none)",
                     bottom ? bottom : "");
        } else {
            diag_log(DIAG_LAYOUT, 0,
                     "\\vrule[%d][%d] top=\"%s\" fill=\"%s\" center=\"%s\" bottom=\"%s\"", width,
                     height, top ? top : "", fill ? fill : "", center ? center : "(none)",
                     bottom ? bottom : "");
        }
        // Check for escaped braces that will be unescaped
        bool has_escaped = (top && strstr(top, "\\{")) || (top && strstr(top, "\\}")) ||
                           (fill && strstr(fill, "\\{")) || (fill && strstr(fill, "\\}")) ||
                           (center && strstr(center, "\\{")) || (center && strstr(center, "\\}")) ||
                           (bottom && strstr(bottom, "\\{")) || (bottom && strstr(bottom, "\\}"));
        if (has_escaped) {
            diag_result(DIAG_LAYOUT, 0, "contains escaped braces (\\{ or \\}) - will unescape");
        }
    }

    // Use auto constructor which handles both auto and fixed cases
    BoxLayout *rule = box_layout_vrule_auto(width, height, top, fill, center, bottom);

    free(top);
    free(fill);
    free(center);
    free(bottom);

    return rule;
}

// ============================================================================
// Helper Functions
// ============================================================================

// Test if string starts with \hrule or \vrule
bool is_rule_command(const char *input) {
    return (strncmp(input, "\\hrule", 6) == 0 || strncmp(input, "\\vrule", 6) == 0);
}

// Parse a rule command (either hrule or vrule)
// Returns BoxLayout or NULL
BoxLayout *parse_rule_command(const char *content, int *rule_pos) {
    BoxLayout *rule = NULL;

    if (strncmp(content, "\\hrule", 6) == 0) {
        rule = parse_hrule_command(content, rule_pos);
    } else if (strncmp(content, "\\vrule", 6) == 0) {
        rule = parse_vrule_command(content, rule_pos);
    }

    return rule;
}
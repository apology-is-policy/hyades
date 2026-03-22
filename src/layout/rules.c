// rules.c - Horizontal and vertical rule creation for Hyades layout
//
// Creates rule boxes (lines) with customizable characters.
// Supports both immediate (fixed size) and deferred (auto size) rules.

#include "layout_internal.h"
#include "layout_types.h"
#include "utils/utf8.h"

#include <stdlib.h>
#include <string.h>

// ============================================================================
// Horizontal Rule (hrule)
// ============================================================================

// Create horizontal rule with custom characters
// left, fill, right can be multi-char UTF-8 strings
BoxLayout *box_layout_hrule_custom(int width, const char *left, const char *fill,
                                   const char *right) {
    if (width <= 0) {
        return box_layout_new(BOX_TYPE_CONTENT, width);
    }

    // Calculate lengths in CHARACTERS (not bytes!)
    int left_len = left ? (int)utf8_display_width(left) : 0;
    int right_len = right ? (int)utf8_display_width(right) : 0;
    int fill_char_len = fill ? (int)utf8_display_width(fill) : 1;

    if (left_len + right_len > width) {
        // Not enough room, just use fill
        left_len = 0;
        right_len = 0;
    }

    int middle_width = width - left_len - right_len;

    // Calculate how much space we need (in bytes)
    // Each UTF-8 char can be up to 4 bytes, but we'll allocate generously
    size_t max_size = (left ? strlen(left) : 0) +
                      (fill ? strlen(fill) * (size_t)(middle_width + 1) : (size_t)middle_width) +
                      (right ? strlen(right) : 0) + 1;

    char *rule_str = malloc(max_size);
    if (!rule_str) {
        return box_layout_new(BOX_TYPE_CONTENT, width);
    }

    int pos = 0;

    // Left terminator (copy bytes directly)
    if (left) {
        size_t left_bytes = strlen(left);
        memcpy(rule_str + pos, left, left_bytes);
        pos += (int)left_bytes;
    }

    // Fill characters - repeat fill string enough times to reach middle_width characters
    if (fill) {
        size_t fill_bytes = strlen(fill);
        int chars_written = 0;
        while (chars_written < middle_width) {
            // Copy one instance of fill pattern
            memcpy(rule_str + pos, fill, fill_bytes);
            pos += (int)fill_bytes;
            chars_written += fill_char_len;
        }
    }

    // Right terminator (copy bytes directly)
    if (right) {
        size_t right_bytes = strlen(right);
        memcpy(rule_str + pos, right, right_bytes);
        pos += (int)right_bytes;
    }

    rule_str[pos] = '\0';

    BoxLayout *rule = box_layout_new(BOX_TYPE_CONTENT, width);
    rule->preformatted = true; // Don't apply compositor!
    box_layout_set_content(rule, rule_str);
    free(rule_str);

    return rule;
}

// ============================================================================
// Vertical Rule (vrule)
// ============================================================================

// Helper: unescape \{ and \} to { and }
static char *unescape_braces(const char *s) {
    if (!s || !*s) return s ? strdup(s) : NULL;
    size_t len = strlen(s);
    char *result = malloc(len + 1);
    char *out = result;
    const char *p = s;
    while (*p) {
        if (*p == '\\' && (p[1] == '{' || p[1] == '}')) {
            *out++ = p[1];
            p += 2;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
    return result;
}

// Create vertical rule with custom characters
// center is optional - if provided, used for the middle row (for curly brace waist)
BoxLayout *box_layout_vrule_custom(int width, int height, const char *top, const char *fill,
                                   const char *center, const char *bottom) {
    if (width <= 0 || height <= 0) {
        return box_layout_new(BOX_TYPE_CONTENT, width);
    }

    // Unescape \{ and \} since preformatted content skips compositor
    char *top_unesc = unescape_braces(top);
    char *fill_unesc = unescape_braces(fill);
    char *center_unesc = unescape_braces(center);
    char *bottom_unesc = unescape_braces(bottom);

    // Use unescaped versions
    top = top_unesc;
    fill = fill_unesc;
    center = center_unesc;
    bottom = bottom_unesc;

    // Calculate byte lengths for UTF-8 strings
    size_t top_bytes = top ? strlen(top) : 0;
    size_t fill_bytes = fill ? strlen(fill) : 1;
    size_t center_bytes = center ? strlen(center) : 0;
    size_t bottom_bytes = bottom ? strlen(bottom) : 0;

    // Calculate character widths
    int fill_char_width = fill ? (int)utf8_display_width(fill) : 1;
    int center_char_width = center ? (int)utf8_display_width(center) : fill_char_width;

    // Calculate center row (for odd heights, it's the exact middle)
    int center_row = height / 2;

    // Allocate enough space (generous estimate for UTF-8)
    size_t max_bytes_per_line = (top_bytes > bottom_bytes ? top_bytes : bottom_bytes);
    if (center_bytes > max_bytes_per_line) max_bytes_per_line = center_bytes;
    if (fill_bytes * (size_t)(width + 1) > max_bytes_per_line) {
        max_bytes_per_line = fill_bytes * (size_t)(width + 1);
    }
    size_t content_size = max_bytes_per_line * (size_t)height + (size_t)height + 1;

    char *content = malloc(content_size);
    if (!content) {
        free(top_unesc);
        free(fill_unesc);
        free(center_unesc);
        free(bottom_unesc);
        return box_layout_new(BOX_TYPE_CONTENT, width);
    }

    int pos = 0;

    for (int y = 0; y < height; y++) {
        const char *line_str;
        size_t line_bytes;
        int line_char_width;

        // Check if top/bottom are non-empty (not just non-NULL)
        if (y == 0 && top && *top) {
            line_str = top;
            line_bytes = top_bytes;
            line_char_width = (int)utf8_display_width(top);
        } else if (y == height - 1 && bottom && *bottom) {
            line_str = bottom;
            line_bytes = bottom_bytes;
            line_char_width = (int)utf8_display_width(bottom);
        } else if (y == center_row && center && *center) {
            // Use center character for the middle row
            line_str = center;
            line_bytes = center_bytes;
            line_char_width = center_char_width;
        } else {
            // Use fill, or fallback to default
            line_str = (fill && *fill) ? fill : "|";
            line_bytes = (fill && *fill) ? fill_bytes : 1;
            line_char_width = (fill && *fill) ? fill_char_width : 1;
        }

        // Repeat the character/string to fill width
        int chars_written = 0;
        while (chars_written < width) {
            memcpy(content + pos, line_str, line_bytes);
            pos += (int)line_bytes;
            chars_written += line_char_width;
        }

        content[pos++] = '\n';
    }
    content[pos] = '\0';

    BoxLayout *rule = box_layout_new(BOX_TYPE_CONTENT, width);
    rule->preformatted = true; // Don't apply compositor!
    box_layout_set_content(rule, content);
    free(content);

    free(top_unesc);
    free(fill_unesc);
    free(center_unesc);
    free(bottom_unesc);

    return rule;
}

// ============================================================================
// Auto-sizing Rules (Deferred Rendering)
// ============================================================================

// Create vrule with potentially auto height
// height = RULE_SIZE_AUTO for automatic height based on hbox siblings
// center is optional (NULL = use fill for all middle rows)
BoxLayout *box_layout_vrule_auto(int width, int height, const char *top, const char *fill,
                                 const char *center, const char *bottom) {
    // If height is not auto, use the immediate rendering path
    if (height != RULE_SIZE_AUTO) {
        return box_layout_vrule_custom(width, height, top, fill, center, bottom);
    }

    // Create deferred vrule
    BoxLayout *layout = box_layout_new(BOX_TYPE_VRULE, width);
    layout->rule_width = width > 0 ? width : 1;
    layout->rule_height = RULE_SIZE_AUTO;
    layout->rule_start = top ? strdup(top) : NULL;
    layout->rule_fill = fill ? strdup(fill) : NULL;
    layout->rule_center = center ? strdup(center) : NULL;
    layout->rule_end = bottom ? strdup(bottom) : NULL;
    return layout;
}

// Create hrule with potentially auto width
// width = RULE_SIZE_AUTO for automatic width based on vbox parent
BoxLayout *box_layout_hrule_auto(int width, const char *left, const char *fill, const char *right) {
    // If width is not auto, use the immediate rendering path
    if (width != RULE_SIZE_AUTO) {
        return box_layout_hrule_custom(width, left, fill, right);
    }

    // Create deferred hrule
    BoxLayout *layout = box_layout_new(BOX_TYPE_HRULE, RULE_SIZE_AUTO);
    layout->rule_width = RULE_SIZE_AUTO;
    layout->rule_height = 1;
    layout->rule_start = left ? strdup(left) : NULL;
    layout->rule_fill = fill ? strdup(fill) : NULL;
    layout->rule_end = right ? strdup(right) : NULL;
    return layout;
}

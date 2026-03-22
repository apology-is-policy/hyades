#include <stdlib.h>
#include <string.h>

#include "hyades_kernel_internal.h"

// ============================================================================
// Helper Commands
// ============================================================================

// Create \vspace{n} - n lines of vertical space
BoxLayout *box_layout_vspace(int lines) {
    BoxLayout *space = box_layout_new(BOX_TYPE_CONTENT, -1); // Inherit width

    // Create empty content with newlines
    char *content = malloc(lines + 1);
    for (int i = 0; i < lines; i++) {
        content[i] = '\n';
    }
    content[lines] = '\0';

    box_layout_set_content(space, content);
    free(content);

    return space;
}

// Create \hrule{width} - horizontal line
BoxLayout *box_layout_hrule(int width) {
    BoxLayout *rule = box_layout_new(BOX_TYPE_CONTENT, width);

    // Create line of dashes
    char *content = malloc(width + 1);
    for (int i = 0; i < width; i++) {
        content[i] = '-';
    }
    content[width] = '\0';

    box_layout_set_content(rule, content);
    free(content);

    return rule;
}

// Create \vrule{width}{height} - vertical line
BoxLayout *box_layout_vrule(int width, int height) {
    BoxLayout *rule = box_layout_new(BOX_TYPE_CONTENT, width);

    // Create vertical line using '|' characters
    int content_len = width * height + height; // chars + newlines
    char *content = malloc(content_len + 1);

    int pos = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            content[pos++] = '|';
        }
        content[pos++] = '\n';
    }
    content[pos] = '\0';

    box_layout_set_content(rule, content);
    free(content);

    return rule;
}

// ============================================================================
// Skip Commands (can be negative for overlap)
// ============================================================================

// Create \vskip{n} - vertical skip (positive = down, negative = overlap up)
BoxLayout *box_layout_vskip(int lines) {
    BoxLayout *skip = box_layout_new(BOX_TYPE_VSKIP, -1); // Inherit width
    skip->skip_amount = lines;
    return skip;
}

// Create \hskip{n} - horizontal skip (positive = right, negative = overlap left)
BoxLayout *box_layout_hskip(int columns) {
    BoxLayout *skip = box_layout_new(BOX_TYPE_HSKIP, -1); // Inherit width
    skip->skip_amount = columns;
    return skip;
}

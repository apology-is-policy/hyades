// box.c - Box primitive operations for Hyades layout system
//
// Box is the fundamental rendering unit - a 2D grid of Unicode codepoints.
// This file contains operations for creating, converting, and manipulating boxes.

#include "layout_types.h"
#include "utils/utf8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Box to String Conversion
// ============================================================================

// Combining mark lookup table (must match AccentKind enum in ast.h)
static const uint32_t g_combining_marks[] = {
    0x0302, // ACCENT_HAT - COMBINING CIRCUMFLEX
    0x0304, // ACCENT_BAR - COMBINING MACRON
    0x0303, // ACCENT_TILDE - COMBINING TILDE
    0x0307, // ACCENT_DOT - COMBINING DOT ABOVE
    0x0308, // ACCENT_DDOT - COMBINING DIAERESIS
    0x20D7, // ACCENT_VEC - COMBINING RIGHT ARROW ABOVE
    0x0301, // ACCENT_ACUTE - COMBINING ACUTE
    0x0300, // ACCENT_GRAVE - COMBINING GRAVE
    0x0306, // ACCENT_BREVE - COMBINING BREVE
    0x030C, // ACCENT_CHECK - COMBINING CARON
};

#define NUM_COMBINING_MARKS (sizeof(g_combining_marks) / sizeof(g_combining_marks[0]))

// Check if a codepoint is a combining diacritical mark (zero-width)
static int is_combining_mark(uint32_t cp) {
    // Combining Diacritical Marks: U+0300–U+036F
    if (cp >= 0x0300 && cp <= 0x036F) return 1;
    // Combining Diacritical Marks Extended: U+1AB0–U+1AFF
    if (cp >= 0x1AB0 && cp <= 0x1AFF) return 1;
    // Combining Diacritical Marks Supplement: U+1DC0–U+1DFF
    if (cp >= 0x1DC0 && cp <= 0x1DFF) return 1;
    // Combining Diacritical Marks for Symbols: U+20D0–U+20FF
    if (cp >= 0x20D0 && cp <= 0x20FF) return 1;
    return 0;
}

// Find accent index for a combining codepoint, or -1 if not found
static int find_accent_index(uint32_t cp) {
    for (size_t i = 0; i < NUM_COMBINING_MARKS; i++) {
        if (g_combining_marks[i] == cp) return (int)i;
    }
    return -1;
}

// Encode a base character and accent into a single cell value
static uint32_t encode_accented_cell(uint32_t base, int accent_idx) {
    // Bit 31 = flag, bits 21-30 = accent type, bits 0-20 = base char
    return 0x80000000 | ((uint32_t)accent_idx << 21) | (base & 0x1FFFFF);
}

// Encode a single cell value to UTF-8, handling combining mark encoded cells.
// Returns number of bytes written (up to 8 for base + combining mark).
// Buffer must have room for at least 8 bytes.
size_t encode_cell_utf8(uint32_t cell, char *out) {
    size_t pos = 0;

    // Check for encoded combining mark (bit 31 set)
    if (cell & 0x80000000) {
        // Decode: bits 21-30 = accent type, bits 0-20 = base char
        uint32_t accent_type = (cell >> 21) & 0x3FF;
        uint32_t base_char = cell & 0x1FFFFF;

        // Emit base character
        pos += utf8_encode(base_char, out + pos);

        // Emit combining mark if valid
        if (accent_type < NUM_COMBINING_MARKS) {
            uint32_t combining = g_combining_marks[accent_type];
            pos += utf8_encode(combining, out + pos);
        }
    } else {
        // Regular Unicode codepoint
        pos = utf8_encode(cell, out);
    }

    return pos;
}

// Emit ANSI escape for transitioning from old_style to new_style.
// Returns number of bytes written. Buffer must have room for at least 20 bytes.
static int emit_style_transition(char *buf, uint16_t old_style, uint16_t new_style) {
    int pos = 0;
    if (new_style == 0) {
        memcpy(buf, "\033[0m", 4);
        return 4;
    }
    if (old_style != 0) {
        memcpy(buf + pos, "\033[0m", 4);
        pos += 4;
    }
    uint8_t fg = new_style & 0xFF;
    uint8_t bg = (new_style >> 8) & 0xFF;
    if (fg && bg)
        pos += sprintf(buf + pos, "\033[%d;%dm", fg, bg);
    else if (fg)
        pos += sprintf(buf + pos, "\033[%dm", fg);
    else if (bg)
        pos += sprintf(buf + pos, "\033[%dm", bg);
    return pos;
}

char *box_to_string(Box *box) {
    if (!box || box->w == 0 || box->h == 0 || !box->cells) {
        return strdup("");
    }

    // UTF-8: up to 8 bytes/cell (base + combining), plus ANSI overhead per row
    int size = box->w * box->h * 8 + box->h * 100 + 1;

    char *str = malloc(size);
    if (!str) {
        return strdup("");
    }

    int pos = 0;
    for (int y = 0; y < box->h; y++) {
        // Find last content column: last cell where cell != ' ' && cell != 0,
        // OR style != 0 (styled spaces count as content for bg-color purposes)
        int last_content = -1;
        for (int x = box->w - 1; x >= 0; x--) {
            uint32_t c = box->cells[y * box->w + x];
            uint16_t s = box->style ? box->style[y * box->w + x] : 0;
            if ((c != ' ' && c != 0) || s != 0) {
                last_content = x;
                break;
            }
        }

        // Emit cells with inline ANSI transitions
        uint16_t current_style = 0;
        for (int x = 0; x <= last_content; x++) {
            uint32_t c = box->cells[y * box->w + x];

            // Skip null cells
            if (c == 0) continue;

            // Get cell style
            uint16_t cell_style = box->style ? box->style[y * box->w + x] : 0;

            // Emit style transition if needed
            if (cell_style != current_style) {
                pos += emit_style_transition(str + pos, current_style, cell_style);
                current_style = cell_style;
            }

            // Emit cell character (existing UTF-8 / combining mark logic)
            if (c & 0x80000000) {
                uint32_t accent_type = (c >> 21) & 0x3FF;
                uint32_t base_char = c & 0x1FFFFF;

                if (base_char < 128) {
                    str[pos++] = (char)base_char;
                } else {
                    char utf8_buf[4];
                    size_t len = utf8_encode(base_char, utf8_buf);
                    for (size_t i = 0; i < len; i++) {
                        str[pos++] = utf8_buf[i];
                    }
                }

                if (accent_type < NUM_COMBINING_MARKS) {
                    uint32_t combining = g_combining_marks[accent_type];
                    char utf8_buf[4];
                    size_t len = utf8_encode(combining, utf8_buf);
                    for (size_t i = 0; i < len; i++) {
                        str[pos++] = utf8_buf[i];
                    }
                }
            } else if (c < 128) {
                str[pos++] = (char)c;
            } else {
                char utf8_buf[4];
                size_t len = utf8_encode(c, utf8_buf);
                for (size_t i = 0; i < len; i++) {
                    str[pos++] = utf8_buf[i];
                }
            }
        }

        // End of row: reset if styled
        if (current_style != 0) {
            memcpy(str + pos, "\033[0m", 4);
            pos += 4;
        }

        str[pos++] = '\n';
    }
    str[pos] = '\0';
    return str;
}

// ============================================================================
// String to Box Conversion
// ============================================================================

Box *string_to_box(const char *str) {
    if (!str || !*str) {
        Box *empty = malloc(sizeof(Box));
        *empty = make_box(1, 1, 0);
        return empty;
    }

    // Count lines and max width IN CODEPOINTS (not bytes!)
    int lines = 0;
    int max_width = 0;
    int current_width = 0;

    size_t pos = 0;
    size_t len = strlen(str);

    while (pos < len) {
        uint32_t cp = utf8_next(str, len, &pos);
        if (cp == '\n') {
            lines++;
            if (current_width > max_width) max_width = current_width;
            current_width = 0;
        } else if (!is_combining_mark(cp)) {
            // Only count non-combining characters for width
            current_width++;
        }
    }
    if (current_width > 0) {
        lines++;
        if (current_width > max_width) max_width = current_width;
    }

    if (lines == 0) lines = 1;
    if (max_width == 0) max_width = 1;

    Box *box = malloc(sizeof(Box));
    *box = make_box(max_width, lines, 0);

    // Fill box with UTF-8 CODEPOINTS (not bytes!)
    int row = 0, col = 0;
    pos = 0;

    while (pos < len) {
        uint32_t cp = utf8_next(str, len, &pos);

        if (cp == '\n') {
            // Fill rest of row with spaces
            while (col < max_width) {
                box->cells[row * max_width + col] = ' ';
                col++;
            }
            row++;
            col = 0;
        } else if (is_combining_mark(cp)) {
            // Combining mark: encode with previous cell if possible
            int prev_col = col - 1;
            int prev_row = row;
            if (prev_col < 0 && prev_row > 0) {
                prev_row--;
                prev_col = max_width - 1;
            }
            if (prev_col >= 0 && prev_row >= 0) {
                int cell_idx = prev_row * max_width + prev_col;
                uint32_t base = box->cells[cell_idx];
                // Only encode if base is not already accented and we recognize this mark
                if (!(base & 0x80000000)) {
                    int accent_idx = find_accent_index(cp);
                    if (accent_idx >= 0) {
                        box->cells[cell_idx] = encode_accented_cell(base, accent_idx);
                    }
                    // If not recognized, just skip the combining mark
                }
            }
            // Don't advance col for combining marks
        } else {
            if (col < max_width && row < lines) {
                box->cells[row * max_width + col] = cp; // Store CODEPOINT!
                col++;
            }
        }
    }

    // Fill any remaining cells with spaces
    while (row < lines) {
        while (col < max_width) {
            box->cells[row * max_width + col] = ' ';
            col++;
        }
        row++;
        col = 0;
    }

    return box;
}

// Convert string to box with metadata
// The meta array should have one entry per codepoint (not per byte)
Box *string_to_box_with_meta(const char *str, const uint8_t *meta, int meta_len) {
    if (!str || !*str) {
        Box *empty = malloc(sizeof(Box));
        *empty = make_box(1, 1, 0);
        return empty;
    }

    // Count lines and max width IN CODEPOINTS
    int lines = 0;
    int max_width = 0;
    int current_width = 0;

    size_t pos = 0;
    size_t len = strlen(str);

    while (pos < len) {
        uint32_t cp = utf8_next(str, len, &pos);
        if (cp == '\n') {
            lines++;
            if (current_width > max_width) max_width = current_width;
            current_width = 0;
        } else if (!is_combining_mark(cp)) {
            current_width++;
        }
    }
    if (current_width > 0) {
        lines++;
        if (current_width > max_width) max_width = current_width;
    }

    if (lines == 0) lines = 1;
    if (max_width == 0) max_width = 1;

    Box *box = malloc(sizeof(Box));
    *box = make_box(max_width, lines, 0);

    // Allocate metadata if we have any non-NONE values
    bool has_meta = false;
    if (meta && meta_len > 0) {
        for (int i = 0; i < meta_len; i++) {
            if (meta[i] != CELL_META_NONE) {
                has_meta = true;
                break;
            }
        }
    }
    if (has_meta) {
        box_ensure_meta(box);
    }

    // Fill box with codepoints and metadata
    int row = 0, col = 0;
    int meta_idx = 0;
    pos = 0;

    while (pos < len) {
        uint32_t cp = utf8_next(str, len, &pos);

        if (cp == '\n') {
            // Fill rest of row with spaces
            while (col < max_width) {
                box->cells[row * max_width + col] = ' ';
                col++;
            }
            row++;
            col = 0;
            meta_idx++; // newline consumes one metadata entry
        } else if (is_combining_mark(cp)) {
            // Combining mark: encode with previous cell
            int prev_col = col - 1;
            int prev_row = row;
            if (prev_col < 0 && prev_row > 0) {
                prev_row--;
                prev_col = max_width - 1;
            }
            if (prev_col >= 0 && prev_row >= 0) {
                int cell_idx = prev_row * max_width + prev_col;
                uint32_t base = box->cells[cell_idx];
                if (!(base & 0x80000000)) {
                    int accent_idx = find_accent_index(cp);
                    if (accent_idx >= 0) {
                        box->cells[cell_idx] = encode_accented_cell(base, accent_idx);
                    }
                }
            }
            // Combining marks don't consume metadata entries (they're part of prev char)
        } else {
            if (col < max_width && row < lines) {
                box->cells[row * max_width + col] = cp;

                // Copy metadata if available
                if (box->meta && meta && meta_idx < meta_len) {
                    box->meta[row * max_width + col] = meta[meta_idx];
                }
                col++;
            }
            meta_idx++; // regular char consumes one metadata entry
        }
    }

    // Fill remaining cells with spaces
    while (row < lines) {
        while (col < max_width) {
            box->cells[row * max_width + col] = ' ';
            col++;
        }
        row++;
        col = 0;
    }

    return box;
}

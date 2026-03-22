// math_accents.c - Accent rendering
// Verbatim from original render.c

#include "math_internal.h"

// ============================================================================
// Accent rendering: place accent mark above the content
// ============================================================================

// Get combining diacritical mark for Unicode mode
uint32_t get_combining_accent(AccentKind ak) {
    switch (ak) {
    case ACCENT_HAT: return 0x0302;            // COMBINING CIRCUMFLEX ACCENT
    case ACCENT_BAR: return 0x0304;            // COMBINING MACRON
    case ACCENT_TILDE: return 0x0303;          // COMBINING TILDE
    case ACCENT_DOT: return 0x0307;            // COMBINING DOT ABOVE
    case ACCENT_DDOT: return 0x0308;           // COMBINING DIAERESIS
    case ACCENT_VEC: return 0x20D7;            // COMBINING RIGHT ARROW ABOVE
    case ACCENT_ACUTE: return 0x0301;          // COMBINING ACUTE ACCENT
    case ACCENT_GRAVE: return 0x0300;          // COMBINING GRAVE ACCENT
    case ACCENT_BREVE: return 0x0306;          // COMBINING BREVE
    case ACCENT_CHECK: return 0x030C;          // COMBINING CARON
    case ACCENT_UNDERLINE: return 0x0332;      // COMBINING LOW LINE
    case ACCENT_OVERRIGHTARROW: return 0x20D7; // COMBINING RIGHT ARROW ABOVE
    case ACCENT_OVERLEFTARROW: return 0x20D6;  // COMBINING LEFT ARROW ABOVE
    default: return 0x0302;
    }
}

// Get standalone (non-combining) accent for multi-char or ASCII fallback
uint32_t get_standalone_accent(AccentKind ak, int unicode_mode) {
    if (unicode_mode) {
        switch (ak) {
        case ACCENT_HAT: return U'ˆ';            // U+02C6 MODIFIER LETTER CIRCUMFLEX
        case ACCENT_BAR: return U'‾';            // U+203E OVERLINE
        case ACCENT_TILDE: return U'˜';          // U+02DC SMALL TILDE
        case ACCENT_DOT: return U'˙';            // U+02D9 DOT ABOVE
        case ACCENT_DDOT: return U'¨';           // U+00A8 DIAERESIS
        case ACCENT_VEC: return U'→';            // U+2192 RIGHTWARDS ARROW
        case ACCENT_ACUTE: return U'´';          // U+00B4 ACUTE ACCENT
        case ACCENT_GRAVE: return U'`';          // U+0060 GRAVE ACCENT
        case ACCENT_BREVE: return U'˘';          // U+02D8 BREVE
        case ACCENT_CHECK: return U'ˇ';          // U+02C7 CARON
        case ACCENT_UNDERLINE: return U'_';      // underscore
        case ACCENT_OVERRIGHTARROW: return U'→'; // right arrow
        case ACCENT_OVERLEFTARROW: return U'←';  // left arrow
        default: return U'ˆ';
        }
    } else {
        // ASCII mode
        switch (ak) {
        case ACCENT_HAT: return '^';
        case ACCENT_BAR: return '_';
        case ACCENT_TILDE: return '~';
        case ACCENT_DOT: return '.';
        case ACCENT_DDOT: return ':';
        case ACCENT_VEC: return '>';
        case ACCENT_ACUTE: return '\'';
        case ACCENT_GRAVE: return '`';
        case ACCENT_BREVE: return 'u';
        case ACCENT_CHECK: return 'v';
        case ACCENT_UNDERLINE: return '_';
        case ACCENT_OVERRIGHTARROW: return '>';
        case ACCENT_OVERLEFTARROW: return '<';
        default: return '^';
        }
    }
}

// Check if a box is a single printable character (for combining accent optimization)
int is_single_char_box(const Box *b, uint32_t *out_char) {
    if (b->h != 1 || b->w != 1) return 0;
    uint32_t c = b->cells[0];
    if (c == ' ' || c == 0) return 0;
    if (out_char) *out_char = c;
    return 1;
}

// Render an accented expression
// For single characters in Unicode mode: use combining diacriticals (x + ̂ → x̂)
// For multi-char or ASCII: place accent mark on line above
Box render_accent(AccentKind ak, const Box *content) {
    int uni = get_unicode_mode();

    // Check if we can use combining characters (single char in Unicode mode)
    uint32_t base_char;
    if (uni && is_single_char_box(content, &base_char)) {
        // Use combining diacritical mark - renders as single glyph
        // We create a 1-wide box but encode both base and combining mark
        // using a special encoding: high 11 bits = combining mark, low 21 bits = base
        // This works because valid Unicode is only 21 bits (0-0x10FFFF)
        // and combining marks are in range 0x0300-0x20FF (fits in 11 bits as offset)

        uint32_t combining = get_combining_accent(ak);

        // Encode: bits 21-31 = (combining - 0x0300 + 1), bits 0-20 = base
        // The +1 ensures non-zero so we can detect it
        // We'll use a simpler scheme: set bit 31 as flag, bits 21-30 = accent type
        uint32_t encoded = base_char | 0x80000000 | ((uint32_t)ak << 21);

        Box r = make_box(1, 1, 0);
        r.cells[0] = encoded;
        return r;
    }

    // Multi-character content or ASCII mode: put accent on line above (or below for underline)
    int w = content->w;

    // Special case: underline goes BELOW the content
    if (ak == ACCENT_UNDERLINE) {
        int h = content->h + 1;
        int baseline = content->baseline; // baseline stays same

        Box r = make_box(w, h, baseline);

        // Copy content at top
        for (int y = 0; y < content->h; y++) {
            for (int x = 0; x < content->w; x++) {
                put(&r, x, y, content->cells[y * content->w + x]);
            }
        }

        // Put underline at bottom
        for (int x = 0; x < w; x++) {
            put(&r, x, content->h, uni ? U'─' : '_');
        }

        return r;
    }

    // Standard case: accent goes ABOVE the content
    int h = content->h + 1;
    int baseline = content->baseline + 1;

    Box r = make_box(w, h, baseline);

    // Copy content
    for (int y = 0; y < content->h; y++) {
        for (int x = 0; x < content->w; x++) {
            put(&r, x, y + 1, content->cells[y * content->w + x]);
        }
    }

    // Place accent based on type
    // For multi-char, we use ASCII-style accents even in Unicode mode
    // because the small standalone Unicode accents look bad floating above

    switch (ak) {
    case ACCENT_BAR:
        // Overline: fill the full width
        for (int x = 0; x < w; x++) {
            put(&r, x, 0, uni ? U'⎽' : '_');
        }
        break;

    case ACCENT_VEC:
    case ACCENT_OVERRIGHTARROW:
        // Vector arrow: prolongable with tail
        // "──→" or "--->" style
        if (w >= 2) {
            for (int x = 0; x < w - 1; x++) {
                put(&r, x, 0, uni ? U'─' : '-');
            }
            put(&r, w - 1, 0, uni ? U'→' : '>');
        } else {
            put(&r, 0, 0, uni ? U'→' : '>');
        }
        break;

    case ACCENT_OVERLEFTARROW:
        // Left arrow: prolongable with tail
        // "←──" or "<---" style
        if (w >= 2) {
            put(&r, 0, 0, uni ? U'←' : '<');
            for (int x = 1; x < w; x++) {
                put(&r, x, 0, uni ? U'─' : '-');
            }
        } else {
            put(&r, 0, 0, uni ? U'←' : '<');
        }
        break;

    case ACCENT_TILDE:
        // Wide tilde: ╱∼∼╲ construction (or ~/~~ in ASCII)
        if (w == 1) {
            put(&r, 0, 0, '~');
        } else {
            const char *left = get_symbol(SYM_WIDEHAT_LEFT);
            const char *right = get_symbol(SYM_WIDEHAT_RIGHT);
            const char *fill = get_symbol(SYM_WIDETILDE_FILL);
            put_str(&r, 0, 0, left);
            for (int x = 1; x < w - 1; x++) {
                put_str(&r, x, 0, fill);
            }
            put_str(&r, w - 1, 0, right);
        }
        break;

    case ACCENT_HAT:
        // Wide hat: ╱‾‾╲ construction (or /--\ in ASCII)
        if (w == 1) {
            put(&r, 0, 0, uni ? U'∧' : '^');
        } else {
            const char *left = get_symbol(SYM_WIDEHAT_LEFT);
            const char *right = get_symbol(SYM_WIDEHAT_RIGHT);
            const char *fill = get_symbol(SYM_WIDEHAT_FILL);
            put_str(&r, 0, 0, left);
            for (int x = 1; x < w - 1; x++) {
                put_str(&r, x, 0, fill);
            }
            put_str(&r, w - 1, 0, right);
        }
        break;

    case ACCENT_DOT:
        // Dot: centered single dot, or dots over each char for wide
        if (w <= 2) {
            put(&r, w / 2, 0, '.');
        } else {
            for (int x = 0; x < w; x++) {
                put(&r, x, 0, '.');
            }
        }
        break;

    case ACCENT_DDOT:
        // Double dot: centered, or repeated
        if (w <= 2) {
            put(&r, w / 2, 0, ':');
        } else {
            for (int x = 0; x < w; x++) {
                put(&r, x, 0, ':');
            }
        }
        break;

    case ACCENT_ACUTE:
        // Acute: typically at the right edge or centered
        put(&r, w - 1, 0, ',');
        break;

    case ACCENT_GRAVE:
        // Grave: typically at the left edge or centered
        put(&r, 0, 0, ',');
        break;

    case ACCENT_BREVE:
        // Breve: centered
        put(&r, w / 2, 0, uni ? U'∪' : 'u');
        break;

    case ACCENT_CHECK:
        // Caron/háček: centered
        put(&r, w / 2, 0, uni ? U'v' : 'v');
        break;

    default: put(&r, w / 2, 0, '^'); break;
    }

    return r;
}

// Render \overbrace or \underbrace with optional label
// is_over=true:  label (above) + brace line + content (below)
// is_over=false: content (above) + brace line + label (below)
Box render_brace(bool is_over, const Box *content, const Box *label) {
    int uni = get_unicode_mode();
    bool has_label = (label->w > 0 && label->h > 0);

    // Brace must span the content width (minimum 3 for ╭┴╮)
    int brace_w = imax(content->w, 3);
    // Total width must also accommodate the label
    int total_w = imax(brace_w, has_label ? label->w : 0);
    // Re-center the brace within total width
    brace_w = total_w;

    // Build the brace line (1 row)
    Box brace_line = make_box(brace_w, 1, 0);
    int center = brace_w / 2;

    if (uni) {
        for (int x = 0; x < brace_w; x++) {
            if (x == 0)
                put(&brace_line, x, 0, is_over ? U'╭' : U'╰');
            else if (x == brace_w - 1)
                put(&brace_line, x, 0, is_over ? U'╮' : U'╯');
            else if (x == center && has_label)
                put(&brace_line, x, 0, is_over ? U'┴' : U'┬');
            else
                put(&brace_line, x, 0, U'─');
        }
    } else {
        // ASCII fallback
        for (int x = 0; x < brace_w; x++) {
            if (x == 0)
                put(&brace_line, x, 0, is_over ? '(' : '\\');
            else if (x == brace_w - 1)
                put(&brace_line, x, 0, is_over ? ')' : '/');
            else if (x == center && has_label)
                put(&brace_line, x, 0, is_over ? '^' : 'v');
            else
                put(&brace_line, x, 0, '-');
        }
    }

    // Calculate total height and compose
    int label_h = has_label ? label->h : 0;
    int total_h = content->h + 1 + label_h; // content + brace + label

    // Baseline: the content's baseline row in the final box
    int content_y, brace_y, label_y;
    if (is_over) {
        // label on top, brace in middle, content at bottom
        label_y = 0;
        brace_y = label_h;
        content_y = label_h + 1;
    } else {
        // content on top, brace in middle, label at bottom
        content_y = 0;
        brace_y = content->h;
        label_y = content->h + 1;
    }

    int baseline = content_y + content->baseline;
    Box r = make_box(total_w, total_h, baseline);

    // Place label (centered)
    if (has_label) {
        int lx = (total_w - label->w) / 2;
        for (int y = 0; y < label->h; y++)
            for (int x = 0; x < label->w; x++) put_cell(&r, lx + x, label_y + y, label, x, y);
    }

    // Place brace line (centered)
    int bx = (total_w - brace_w) / 2;
    for (int x = 0; x < brace_w; x++) put_cell(&r, bx + x, brace_y, &brace_line, x, 0);

    // Place content (centered)
    int cx = (total_w - content->w) / 2;
    for (int y = 0; y < content->h; y++)
        for (int x = 0; x < content->w; x++) put_cell(&r, cx + x, content_y + y, content, x, y);

    box_free(&brace_line);
    return r;
}

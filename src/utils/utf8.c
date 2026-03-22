#include "utf8.h"

static int in_range(uint32_t x, uint32_t a, uint32_t b) {
    return x >= a && x <= b;
}

uint32_t utf8_next(const char *s, size_t len, size_t *p) {
    if (*p >= len) return 0;
    unsigned char c0 = (unsigned char)s[*p];
    if (c0 < 0x80) {
        (*p)++;
        return c0;
    }
    if ((c0 >> 5) == 0x6 && *p + 1 < len) {
        unsigned char c1 = (unsigned char)s[*p + 1];
        if ((c1 & 0xC0) != 0x80) {
            (*p)++;
            return 0xFFFD;
        }
        uint32_t cp = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
        (*p) += 2;
        return cp;
    }
    if ((c0 >> 4) == 0xE && *p + 2 < len) {
        unsigned char c1 = (unsigned char)s[*p + 1];
        unsigned char c2 = (unsigned char)s[*p + 2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80) {
            (*p)++;
            return 0xFFFD;
        }
        uint32_t cp = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
        (*p) += 3;
        return cp;
    }
    if ((c0 >> 3) == 0x1E && *p + 3 < len) {
        unsigned char c1 = (unsigned char)s[*p + 1];
        unsigned char c2 = (unsigned char)s[*p + 2];
        unsigned char c3 = (unsigned char)s[*p + 3];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) {
            (*p)++;
            return 0xFFFD;
        }
        uint32_t cp = ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        (*p) += 4;
        return cp;
    }
    (*p)++;
    return 0xFFFD;
}

size_t utf8_encode(uint32_t cp, char out[4]) {
    if (cp <= 0x7F) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp <= 0x7FF) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp <= 0xFFFF) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

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

int utf8_skip_ansi_escape(const char *s, size_t len, size_t *p) {
    if (*p + 1 < len && (unsigned char)s[*p] == 0x1B && s[*p + 1] == '[') {
        *p += 2; // skip ESC [
        // Skip parameter bytes (digits, semicolons) and intermediate bytes (0x20-0x3F)
        while (*p < len && (unsigned char)s[*p] >= 0x20 && (unsigned char)s[*p] <= 0x3F) {
            (*p)++;
        }
        // Skip the final byte (0x40-0x7E, typically 'm')
        if (*p < len && (unsigned char)s[*p] >= 0x40 && (unsigned char)s[*p] <= 0x7E) {
            (*p)++;
        }
        return 1;
    }
    return 0;
}

size_t utf8_display_width(const char *s) {
    size_t p = 0, len = 0, L = 0;
    while (s[len]) len++;
    while (p < len) {
        if (utf8_skip_ansi_escape(s, len, &p)) continue;
        uint32_t cp = utf8_next(s, len, &p);
        // Skip combining diacritical marks (they don't take up space)
        if (!is_combining_mark(cp)) {
            ++L; // 1 column per non-combining codepoint
        }
    }
    return L;
}
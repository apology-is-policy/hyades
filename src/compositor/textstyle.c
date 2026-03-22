// textstyle.c - Unicode text style conversion for compositor

#include "compositor_internal.h"
#include "utils/utf8.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// ASCII to Unicode Style Conversion
// ============================================================================

uint32_t char_to_bold(uint32_t c) {
    if (c >= 'A' && c <= 'Z') return 0x1D400 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 0x1D41A + (c - 'a');
    if (c >= '0' && c <= '9') return 0x1D7CE + (c - '0');
    return c;
}

uint32_t char_to_bold_italic(uint32_t c) {
    if (c >= 'A' && c <= 'Z') return 0x1D468 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 0x1D482 + (c - 'a');
    if (c >= '0' && c <= '9') return 0x1D7CE + (c - '0'); // bold digits (no bold-italic digits)
    return c;
}

uint32_t char_to_italic(uint32_t c) {
    if (c >= 'A' && c <= 'Z') return 0x1D434 + (c - 'A');
    if (c >= 'a' && c <= 'z') {
        if (c == 'h') return 0x210E;
        return 0x1D44E + (c - 'a');
    }
    return c;
}

uint32_t styled_to_ascii(uint32_t c) {
    if (c >= 0x1D400 && c <= 0x1D419) return 'A' + (c - 0x1D400);
    if (c >= 0x1D41A && c <= 0x1D433) return 'a' + (c - 0x1D41A);
    if (c >= 0x1D7CE && c <= 0x1D7D7) return '0' + (c - 0x1D7CE);
    if (c >= 0x1D434 && c <= 0x1D44D) return 'A' + (c - 0x1D434);
    if (c >= 0x1D44E && c <= 0x1D467) return 'a' + (c - 0x1D44E);
    if (c == 0x210E) return 'h';
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return c;
    return 0;
}

bool is_hyphenatable_letter(uint32_t c) {
    return styled_to_ascii(c) != 0;
}

char *convert_text_style(const char *text, char style) {
    if (!text) return NULL;

    size_t len = strlen(text);
    char *out = malloc(len * 4 + 1);
    if (!out) return NULL;

    char *p = out;
    const unsigned char *src = (const unsigned char *)text;

    while (*src) {
        uint32_t c;
        int bytes;

        if ((*src & 0x80) == 0) {
            c = *src;
            bytes = 1;
        } else if ((*src & 0xE0) == 0xC0) {
            c = (*src & 0x1F) << 6 | (src[1] & 0x3F);
            bytes = 2;
        } else if ((*src & 0xF0) == 0xE0) {
            c = (*src & 0x0F) << 12 | (src[1] & 0x3F) << 6 | (src[2] & 0x3F);
            bytes = 3;
        } else if ((*src & 0xF8) == 0xF0) {
            c = (*src & 0x07) << 18 | (src[1] & 0x3F) << 12 | (src[2] & 0x3F) << 6 |
                (src[3] & 0x3F);
            bytes = 4;
        } else {
            *p++ = *src++;
            continue;
        }

        src += bytes;

        uint32_t converted = (style == 'b')   ? char_to_bold(c)
                             : (style == 'i') ? char_to_italic(c)
                             : (style == 'B') ? char_to_bold_italic(c)
                                              : c;

        p += utf8_encode(converted, p);
    }

    *p = '\0';
    return out;
}

char *normalize_for_hyphenation(const char *utf8_word, int **offsets, int *num_codepoints) {
    if (!utf8_word) return NULL;

    size_t len = strlen(utf8_word);
    char *ascii = malloc(len + 1);
    int *offs = malloc((len + 1) * sizeof(int));
    if (!ascii || !offs) {
        free(ascii);
        free(offs);
        return NULL;
    }

    int ascii_len = 0;
    size_t pos = 0;

    while (pos < len) {
        size_t start_pos = pos;
        uint32_t cp = utf8_next(utf8_word, len, &pos);
        uint32_t ascii_char = styled_to_ascii(cp);
        if (ascii_char == 0) {
            free(ascii);
            free(offs);
            return NULL;
        }
        offs[ascii_len] = (int)start_pos;
        ascii[ascii_len++] = (char)ascii_char;
    }

    ascii[ascii_len] = '\0';
    offs[ascii_len] = (int)pos;

    if (offsets)
        *offsets = offs;
    else
        free(offs);
    if (num_codepoints) *num_codepoints = ascii_len;

    return ascii;
}

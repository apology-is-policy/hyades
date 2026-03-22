// typography.c - Smart typography transforms for compositor

#include "compositor_internal.h"
#include <ctype.h>

// UTF-8 encoded typography characters
static const char EM_DASH[] = "\xe2\x80\x94";      // —
static const char EN_DASH[] = "\xe2\x80\x93";      // –
static const char ELLIPSIS[] = "\xe2\x80\xa6";     // …
static const char OPEN_SINGLE[] = "\xe2\x80\x98";  // '
static const char CLOSE_SINGLE[] = "\xe2\x80\x99"; // '
static const char OPEN_DOUBLE[] = "\xe2\x80\x9c";  // "
static const char CLOSE_DOUBLE[] = "\xe2\x80\x9d"; // "

void smart_typography_with_map(const char *in, Str *out, I32Buf *col2in) {
    str_init(out);
    i32_init(col2in);
    int open_sq = 1;

    for (int i = 0; in[i];) {
        char c = in[i];

        // em dash: ---
        if (in[i] == '-' && in[i + 1] == '-' && in[i + 2] == '-') {
            str_puts(out, EM_DASH);
            i32_push(col2in, i);
            i += 3;
            continue;
        }

        // en dash: --
        if (in[i] == '-' && in[i + 1] == '-') {
            str_puts(out, EN_DASH);
            i32_push(col2in, i);
            i += 2;
            continue;
        }

        // ellipsis: ...
        if (in[i] == '.' && in[i + 1] == '.' && in[i + 2] == '.') {
            str_puts(out, ELLIPSIS);
            i32_push(col2in, i);
            i += 3;
            continue;
        }

        // apostrophe
        if (c == '\'') {
            char pch = (i > 0) ? in[i - 1] : ' ';
            char nch = in[i + 1] ? in[i + 1] : ' ';

            // Inside a word -> curly apostrophe
            if (isalnum((unsigned char)pch) && isalnum((unsigned char)nch)) {
                str_puts(out, CLOSE_SINGLE);
                i32_push(col2in, i);
                i++;
                continue;
            }

            // Quote
            if (open_sq)
                str_puts(out, OPEN_SINGLE);
            else
                str_puts(out, CLOSE_SINGLE);
            i32_push(col2in, i);
            open_sq = !open_sq;
            i++;
            continue;
        }

        // double quotes
        if (c == '"') {
            char pch = (i > 0) ? in[i - 1] : ' ';
            int opening = (i == 0) || isspace((unsigned char)pch) || ispunct((unsigned char)pch);

            if (opening)
                str_puts(out, OPEN_DOUBLE);
            else
                str_puts(out, CLOSE_DOUBLE);
            i32_push(col2in, i);
            i++;
            continue;
        }

        // regular byte
        str_putc(out, c);
        i32_push(col2in, i);
        i++;
    }
}

void passthrough_with_map(const char *in, Str *out, I32Buf *col2in) {
    str_init(out);
    i32_init(col2in);

    for (int i = 0; in[i];) {
        unsigned char c = (unsigned char)in[i];
        int start = i;
        int adv = ((c & 0x80) == 0x00)   ? 1
                  : ((c & 0xE0) == 0xC0) ? 2
                  : ((c & 0xF0) == 0xE0) ? 3
                  : ((c & 0xF8) == 0xF0) ? 4
                                         : 1;

        for (int k = 0; k < adv && in[i + k]; k++) str_putc(out, in[i + k]);
        i32_push(col2in, start);
        i += adv;
    }
}

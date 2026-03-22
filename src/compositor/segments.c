// segments.c - Segment buffer and block splitting for compositor

#include "compositor_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Segment Buffer
// ============================================================================

void seg_push(SegBuf *sb, Segment s) {
    if (sb->n >= sb->cap) {
        sb->cap = sb->cap ? sb->cap * 2 : 32;
        sb->v = realloc(sb->v, sb->cap * sizeof(Segment));
        if (!sb->v) {
            fprintf(stderr, "OOM\n");
            exit(1);
        }
    }
    sb->v[sb->n++] = s;
}

void seg_free(SegBuf *sb) {
    for (int i = 0; i < sb->n; i++) {
        if (sb->v[i].kind == SEG_TEXT) free(sb->v[i].text);
        if (sb->v[i].kind == SEG_INLINE_MATH || sb->v[i].kind == SEG_DISPLAY_MATH ||
            sb->v[i].kind == SEG_INLINE_BOX) {
            box_free(&sb->v[i].box);
        }
    }
    free(sb->v);
    sb->v = NULL;
    sb->n = sb->cap = 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

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

int u8_cols_str(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    int w = 0;
    while (*p) {
        unsigned c = *p;
        int adv;
        uint32_t cp;
        if ((c & 0x80) == 0x00) {
            adv = 1;
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            adv = 2;
            cp = ((c & 0x1F) << 6) | (p[1] & 0x3F);
        } else if ((c & 0xF0) == 0xE0) {
            adv = 3;
            cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        } else if ((c & 0xF8) == 0xF0) {
            adv = 4;
            cp = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        } else {
            adv = 1;
            cp = c;
        }
        p += adv;
        // Skip combining diacritical marks (they don't take up space)
        if (!is_combining_mark(cp)) {
            w += 1;
        }
    }
    return w;
}

int heuristic_baseline(const Box *b) {
    if (!b || !b->cells) return 0;
    int best_y = 0, best_score = -1;
    for (int y = 0; y < b->h; y++) {
        int score = 0;
        for (int x = 0; x < b->w; x++)
            if (b->cells[y * b->w + x] != U' ') score++;
        if (score > best_score || (score == best_score && y > best_y)) {
            best_score = score;
            best_y = y;
        }
    }
    if (best_y < 0) best_y = 0;
    if (best_y >= b->h) best_y = b->h - 1;
    return best_y;
}

int box_baseline(const Box *b) {
    if (!b) return 0;
    return b->baseline;
}

// ============================================================================
// Comment Stripping
// ============================================================================

char *strip_tex_comments(const char *in) {
    size_t len = strlen(in);
    char *out = malloc(len + 1);
    if (!out) return NULL;

    char *o = out;
    const char *p = in;

    while (*p) {
        if (*p == '\\' && p[1] == '%') {
            *o++ = *p++;
            *o++ = *p++;
        } else if (*p == '%') {
            while (*p && *p != '\n') p++;
        } else {
            *o++ = *p++;
        }
    }
    *o = '\0';
    return out;
}

// Find closing $$ that's not inside a comment
static const char *find_closing_display_math(const char *start) {
    const char *p = start;
    while (*p) {
        if (*p == '%') {
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }
        if (p[0] == '$' && p[1] == '$') return p;
        p++;
    }
    return NULL;
}

// ============================================================================
// Block Splitting
// ============================================================================

BlockList split_blocks_display_math_with_pos(const char *in) {
    BlockList bl = {0};
    const char *p = in;

    while (*p) {
        const char *start = strstr(p, "$$");
        if (!start) {
            size_t len = strlen(p);
            bl.blocks = realloc(bl.blocks, (bl.n + 1) * sizeof(char *));
            bl.is_math = realloc(bl.is_math, (bl.n + 1) * sizeof(bool));
            bl.start = realloc(bl.start, (bl.n + 1) * sizeof(int));
            bl.blocks[bl.n] = malloc(len + 1);
            memcpy(bl.blocks[bl.n], p, len + 1);
            bl.is_math[bl.n] = false;
            bl.start[bl.n] = (int)(p - in);
            bl.n++;
            break;
        }

        if (start > p) {
            size_t len = (size_t)(start - p);
            bl.blocks = realloc(bl.blocks, (bl.n + 1) * sizeof(char *));
            bl.is_math = realloc(bl.is_math, (bl.n + 1) * sizeof(bool));
            bl.start = realloc(bl.start, (bl.n + 1) * sizeof(int));
            bl.blocks[bl.n] = malloc(len + 1);
            memcpy(bl.blocks[bl.n], p, len);
            bl.blocks[bl.n][len] = 0;
            bl.is_math[bl.n] = false;
            bl.start[bl.n] = (int)(p - in);
            bl.n++;
        }

        const char *end = find_closing_display_math(start + 2);
        if (!end) {
            size_t len = strlen(start);
            bl.blocks = realloc(bl.blocks, (bl.n + 1) * sizeof(char *));
            bl.is_math = realloc(bl.is_math, (bl.n + 1) * sizeof(bool));
            bl.start = realloc(bl.start, (bl.n + 1) * sizeof(int));
            bl.blocks[bl.n] = malloc(len + 1);
            memcpy(bl.blocks[bl.n], start, len + 1);
            bl.is_math[bl.n] = false;
            bl.start[bl.n] = (int)(start - in);
            bl.n++;
            break;
        }

        size_t len = (size_t)(end - (start + 2));
        bl.blocks = realloc(bl.blocks, (bl.n + 1) * sizeof(char *));
        bl.is_math = realloc(bl.is_math, (bl.n + 1) * sizeof(bool));
        bl.start = realloc(bl.start, (bl.n + 1) * sizeof(int));
        bl.blocks[bl.n] = malloc(len + 1);
        memcpy(bl.blocks[bl.n], start + 2, len);
        bl.blocks[bl.n][len] = 0;
        bl.is_math[bl.n] = true;
        bl.start[bl.n] = (int)((start + 2) - in);
        bl.n++;

        p = end + 2;
        if (*p == '\n') p++;
    }
    return bl;
}

void blocks_free(BlockList *bl) {
    for (int i = 0; i < bl->n; i++) free(bl->blocks[i]);
    free(bl->blocks);
    free(bl->is_math);
    free(bl->start);
    bl->blocks = NULL;
    bl->is_math = NULL;
    bl->start = NULL;
    bl->n = 0;
}

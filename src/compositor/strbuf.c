// strbuf.c - String and integer buffer utilities for compositor

#include "compositor_internal.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// String Buffer (Str)
// ============================================================================

void str_init(Str *b) {
    b->s = NULL;
    b->n = b->cap = 0;
}

void str_reserve(Str *b, size_t add) {
    size_t need = b->n + add + 1;
    if (need > b->cap) {
        size_t cap2 = b->cap ? (b->cap * 2) : (need + 64);
        if (cap2 < need) cap2 = need;
        b->s = realloc(b->s, cap2);
        if (!b->s) {
            fprintf(stderr, "OOM\n");
            exit(1);
        }
        b->cap = cap2;
    }
}

void str_putc(Str *b, char c) {
    str_reserve(b, 1);
    b->s[b->n++] = c;
    b->s[b->n] = 0;
}

void str_puts(Str *b, const char *s) {
    size_t m = strlen(s);
    str_reserve(b, m);
    memcpy(b->s + b->n, s, m);
    b->n += m;
    b->s[b->n] = 0;
}

char *str_detach(Str *b) {
    char *r = b->s;
    b->s = NULL;
    b->n = b->cap = 0;
    return r;
}

void str_free(Str *b) {
    free(b->s);
    b->s = NULL;
    b->n = b->cap = 0;
}

// ============================================================================
// Integer Buffer (I32Buf)
// ============================================================================

void i32_init(I32Buf *b) {
    b->v = NULL;
    b->n = b->cap = 0;
}

static void i32_reserve(I32Buf *b, int add) {
    int need = b->n + add;
    if (need > b->cap) {
        int cap2 = b->cap ? b->cap * 2 : (need + 64);
        if (cap2 < need) cap2 = need;
        b->v = realloc(b->v, (size_t)cap2 * sizeof(int));
        if (!b->v) {
            fprintf(stderr, "OOM\n");
            exit(1);
        }
        b->cap = cap2;
    }
}

void i32_push(I32Buf *b, int x) {
    i32_reserve(b, 1);
    b->v[b->n++] = x;
}

int *i32_detach(I32Buf *b) {
    int *r = b->v;
    b->v = NULL;
    b->n = b->cap = 0;
    return r;
}

void i32_free(I32Buf *b) {
    free(b->v);
    b->v = NULL;
    b->n = b->cap = 0;
}

// ============================================================================
// Uint8 Buffer (U8Buf) - for cell metadata
// ============================================================================

void u8_init(U8Buf *b) {
    b->v = NULL;
    b->n = b->cap = 0;
}

static void u8_reserve(U8Buf *b, int add) {
    int need = b->n + add;
    if (need > b->cap) {
        int cap2 = b->cap ? b->cap * 2 : (need + 64);
        if (cap2 < need) cap2 = need;
        b->v = realloc(b->v, (size_t)cap2 * sizeof(uint8_t));
        if (!b->v) {
            fprintf(stderr, "OOM\n");
            exit(1);
        }
        b->cap = cap2;
    }
}

void u8_push(U8Buf *b, uint8_t x) {
    u8_reserve(b, 1);
    b->v[b->n++] = x;
}

void u8_push_n(U8Buf *b, uint8_t x, int count) {
    u8_reserve(b, count);
    for (int i = 0; i < count; i++) {
        b->v[b->n++] = x;
    }
}

uint8_t *u8_detach(U8Buf *b) {
    uint8_t *r = b->v;
    b->v = NULL;
    b->n = b->cap = 0;
    return r;
}

void u8_free(U8Buf *b) {
    free(b->v);
    b->v = NULL;
    b->n = b->cap = 0;
}

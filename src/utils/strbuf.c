// strbuf.c - Growable string buffer utility for Hyades

#include "strbuf.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Default initial capacity
#define STRBUF_INITIAL_CAPACITY 64

// ============================================================================
// Lifecycle
// ============================================================================

void strbuf_init(StrBuf *sb) {
    sb->data = NULL;
    sb->len = 0;
    sb->capacity = 0;
}

void strbuf_init_with_capacity(StrBuf *sb, size_t capacity) {
    if (capacity == 0) {
        strbuf_init(sb);
        return;
    }

    sb->data = malloc(capacity);
    if (sb->data) {
        sb->data[0] = '\0';
        sb->capacity = capacity;
    } else {
        sb->capacity = 0;
    }
    sb->len = 0;
}

void strbuf_free(StrBuf *sb) {
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->capacity = 0;
}

void strbuf_clear(StrBuf *sb) {
    sb->len = 0;
    if (sb->data) {
        sb->data[0] = '\0';
    }
}

char *strbuf_detach(StrBuf *sb) {
    char *result = sb->data;

    // If empty, return an allocated empty string for consistency
    if (!result) {
        result = malloc(1);
        if (result) result[0] = '\0';
    }

    sb->data = NULL;
    sb->len = 0;
    sb->capacity = 0;

    return result;
}

// ============================================================================
// Internal: Capacity Management
// ============================================================================

// Ensure we have room for `additional` more bytes plus null terminator
static void ensure_capacity(StrBuf *sb, size_t additional) {
    size_t needed = sb->len + additional + 1; // +1 for null terminator

    if (needed <= sb->capacity) {
        return; // Already have enough space
    }

    // Growth strategy: double or use needed, whichever is larger
    size_t new_capacity = sb->capacity ? sb->capacity * 2 : STRBUF_INITIAL_CAPACITY;
    if (new_capacity < needed) {
        new_capacity = needed;
    }

    char *new_data = realloc(sb->data, new_capacity);
    if (!new_data) {
        // OOM - for a utility library, we'll just not grow
        // Callers should check if operations succeeded by comparing lengths
        // or we could add an error flag. For now, fail silently.
        return;
    }

    sb->data = new_data;
    sb->capacity = new_capacity;

    // Ensure null-terminated even if this is first allocation
    if (sb->len == 0) {
        sb->data[0] = '\0';
    }
}

// ============================================================================
// Appending
// ============================================================================

void strbuf_append(StrBuf *sb, const char *str) {
    if (!str) return;
    strbuf_append_n(sb, str, strlen(str));
}

void strbuf_append_n(StrBuf *sb, const char *str, size_t n) {
    if (!str || n == 0) return;

    ensure_capacity(sb, n);

    // Check if we actually got the capacity
    if (sb->len + n + 1 > sb->capacity) {
        return; // OOM, silently fail
    }

    memcpy(sb->data + sb->len, str, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void strbuf_putc(StrBuf *sb, char c) {
    ensure_capacity(sb, 1);

    if (sb->len + 2 > sb->capacity) {
        return; // OOM
    }

    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

void strbuf_printf(StrBuf *sb, const char *fmt, ...) {
    va_list ap;

    // First pass: determine required size
    va_start(ap, fmt);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0) {
        return; // Format error
    }

    ensure_capacity(sb, (size_t)needed);

    if (sb->len + (size_t)needed + 1 > sb->capacity) {
        return; // OOM
    }

    // Second pass: actually format
    va_start(ap, fmt);
    vsnprintf(sb->data + sb->len, (size_t)needed + 1, fmt, ap);
    va_end(ap);

    sb->len += (size_t)needed;
}

void strbuf_repeat(StrBuf *sb, char c, size_t n) {
    if (n == 0) return;

    ensure_capacity(sb, n);

    if (sb->len + n + 1 > sb->capacity) {
        return; // OOM
    }

    memset(sb->data + sb->len, c, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

// ============================================================================
// Capacity Management
// ============================================================================

void strbuf_reserve(StrBuf *sb, size_t additional) {
    ensure_capacity(sb, additional);
}

void strbuf_shrink_to_fit(StrBuf *sb) {
    if (sb->len == 0) {
        free(sb->data);
        sb->data = NULL;
        sb->capacity = 0;
        return;
    }

    size_t needed = sb->len + 1;
    if (needed < sb->capacity) {
        char *new_data = realloc(sb->data, needed);
        if (new_data) {
            sb->data = new_data;
            sb->capacity = needed;
        }
        // If realloc fails, just keep the larger buffer
    }
}

// ============================================================================
// Convenience
// ============================================================================

StrBuf strbuf_from(const char *str) {
    StrBuf sb;
    strbuf_init(&sb);
    if (str) {
        strbuf_append(&sb, str);
    }
    return sb;
}

#if defined(_MSC_VER)
#define HYADES_PRINTF_LIKE(fmt_index, first_arg)
#else
#define HYADES_PRINTF_LIKE(fmt_index, first_arg)                                                   \
    __attribute__((format(printf, fmt_index, first_arg)))
#endif
// strbuf.h - Growable string buffer utility for Hyades
//
// A simple, efficient growable string buffer. Use this instead of
// rolling your own in each module.
//
// Usage:
//   StrBuf sb;
//   strbuf_init(&sb);
//   strbuf_append(&sb, "Hello ");
//   strbuf_append(&sb, "World");
//   strbuf_putc(&sb, '!');
//   char *result = strbuf_detach(&sb);  // Caller owns result
//   // ... use result ...
//   free(result);
//
// Or with automatic cleanup:
//   StrBuf sb;
//   strbuf_init(&sb);
//   strbuf_append(&sb, "Hello");
//   printf("%s\n", sb.data);
//   strbuf_free(&sb);

#ifndef STRBUF_H
#define STRBUF_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *data;      // Null-terminated string data
    size_t len;      // Current length (excluding null terminator)
    size_t capacity; // Allocated capacity (including null terminator)
} StrBuf;

// ============================================================================
// Lifecycle
// ============================================================================

// Initialize a string buffer (starts empty)
void strbuf_init(StrBuf *sb);

// Initialize with pre-allocated capacity
void strbuf_init_with_capacity(StrBuf *sb, size_t capacity);

// Free resources (buffer becomes empty but reusable)
void strbuf_free(StrBuf *sb);

// Reset to empty without freeing memory (for reuse)
void strbuf_clear(StrBuf *sb);

// Detach and return the string, leaving buffer empty
// Caller must free() the returned string
char *strbuf_detach(StrBuf *sb);

// ============================================================================
// Appending
// ============================================================================

// Append a null-terminated string
void strbuf_append(StrBuf *sb, const char *str);

// Append exactly n bytes (str need not be null-terminated)
void strbuf_append_n(StrBuf *sb, const char *str, size_t n);

// Append a single character
void strbuf_putc(StrBuf *sb, char c);

// Append formatted string (printf-style)
void strbuf_printf(StrBuf *sb, const char *fmt, ...) HYADES_PRINTF_LIKE(2, 3);

// Append a character repeated n times
void strbuf_repeat(StrBuf *sb, char c, size_t n);

// ============================================================================
// Capacity Management
// ============================================================================

// Ensure at least `additional` bytes can be appended without reallocation
void strbuf_reserve(StrBuf *sb, size_t additional);

// Shrink capacity to fit current content
void strbuf_shrink_to_fit(StrBuf *sb);

// ============================================================================
// Convenience
// ============================================================================

// Create a new buffer initialized with a copy of str
// Returns initialized StrBuf (not a pointer)
StrBuf strbuf_from(const char *str);

// Check if buffer is empty
static inline bool strbuf_is_empty(const StrBuf *sb) {
    return sb->len == 0;
}

// Get current length
static inline size_t strbuf_len(const StrBuf *sb) {
    return sb->len;
}

// Get the string (read-only access)
static inline const char *strbuf_str(const StrBuf *sb) {
    return sb->data ? sb->data : "";
}

#endif // STRBUF_H

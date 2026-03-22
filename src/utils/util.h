#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_LEN(x) ((int)(sizeof(x) / sizeof((x)[0])))

// strndup is POSIX but not available on Windows
#ifdef _WIN32
static inline char *hyades_strndup(const char *s, size_t n) {
    size_t len = strlen(s);
    if (n < len) len = n;
    char *r = (char *)malloc(len + 1);
    if (r) {
        memcpy(r, s, len);
        r[len] = '\0';
    }
    return r;
}
#define strndup hyades_strndup
#endif

static inline int imax(int a, int b) {
    return a > b ? a : b;
}
static inline int imin(int a, int b) {
    return a < b ? a : b;
}

// Simple arena-like bump allocator (optional later). For now we just use malloc/free.
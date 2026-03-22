// warnings.c - Non-fatal warning accumulator for render passes
//
// Collects warnings (e.g., unknown commands rendered as literal text) during
// a render call so they can be reported to callers after rendering succeeds.

#include "warnings.h"

#include <stdarg.h>
#include <stdio.h>

static int g_warning_count = 0;
static char g_warnings[HYADES_MAX_WARNINGS][WARNING_MSG_SIZE];

void warnings_clear(void) {
    g_warning_count = 0;
}

void hyades_add_warning(const char *fmt, ...) {
    if (g_warning_count >= HYADES_MAX_WARNINGS) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_warnings[g_warning_count], WARNING_MSG_SIZE, fmt, args);
    va_end(args);
    g_warning_count++;
}

int hyades_render_warning_count(void) {
    return g_warning_count;
}

const char *hyades_render_warning_message(int index) {
    if (index < 0 || index >= g_warning_count) return NULL;
    return g_warnings[index];
}

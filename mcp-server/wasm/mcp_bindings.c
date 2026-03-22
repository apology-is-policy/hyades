// mcp_bindings.c - Minimal WASM bindings for MCP server
//
// Thin wrapper around hyades_render() for the Cloudflare Worker MCP server.
// Only three exported functions: init, render, free.

#include "hyades.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

// Forward declarations for internal reset functions
extern void set_unicode_mode(int on);
extern void set_math_cursive_mode(int on);
extern void hyades_reset_state(void);
extern void set_render_mode(int mode);

static bool g_initialized = false;

WASM_EXPORT
void hyades_mcp_init(void) {
    if (!g_initialized) {
        hyades_init();
        g_initialized = true;
    }
}

WASM_EXPORT
char *hyades_mcp_render(const char *input, int width, int ascii) {
    hyades_mcp_init();

    // Reset state between calls
    hyades_reset_state();
    set_unicode_mode(ascii ? 0 : 1);
    set_math_cursive_mode(ascii ? 0 : 1);
    set_render_mode(ascii ? 0 : 1);

    if (!input) {
        return strdup("ERROR: Input is NULL");
    }

    char *input_copy = strdup(input);
    if (!input_copy) {
        return strdup("ERROR: Out of memory");
    }

    HyadesOptions opts = hyades_default_options();
    if (width > 0) {
        opts.width = width;
    }
    opts.unicode = !ascii;
    opts.math_italic = !ascii;

    HyadesError error;
    hyades_error_init(&error);

    char *result = hyades_render(input_copy, &opts, &error);
    free(input_copy);

    if (!result) {
        size_t msg_len = strlen(error.message);
        char *err_str = malloc(msg_len + 16);
        if (err_str) {
            snprintf(err_str, msg_len + 16, "ERROR: %s", error.message);
        } else {
            err_str = strdup("ERROR: Unknown error");
        }
        return err_str;
    }

    return result;
}

WASM_EXPORT
void hyades_mcp_free(char *ptr) {
    if (ptr) {
        free(ptr);
    }
}

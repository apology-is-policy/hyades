// wasm_bindings.c - WebAssembly bindings for Hyades
//
// This provides a thin C API optimized for JavaScript interop via Emscripten.
// All functions use simple types (char*, int) for easy cwrap/ccall usage.

#include "hyades.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#define DEBUG_LOG(...) emscripten_log(EM_LOG_CONSOLE, __VA_ARGS__)
#else
#define WASM_EXPORT
#define DEBUG_LOG(...) fprintf(stderr, __VA_ARGS__)
#endif

// Forward declarations for internal reset functions
extern void set_unicode_mode(int on);
extern void set_math_cursive_mode(int on);
extern void hyades_reset_state(void);
extern void set_render_mode(int mode); // MODE_ASCII=0, MODE_UNICODE=1

// ============================================================================
// Initialization
// ============================================================================

static bool g_wasm_initialized = false;

// Initialize the WASM module (call once before using other functions)
WASM_EXPORT
void hyades_wasm_init(void) {
    if (!g_wasm_initialized) {
        // Note: Don't log here - it corrupts LSP stdio protocol
        hyades_init();
        g_wasm_initialized = true;
    }
}

// Reset state between calls to ensure clean processing
static void reset_render_state(void) {
    // Clear all accumulated global state (macros, line registry, verbatim store)
    hyades_reset_state();

    // Reset to default options
    set_unicode_mode(1);
    set_math_cursive_mode(1);
    set_render_mode(1); // MODE_UNICODE = 1 (must match set_unicode_mode)
}

// ============================================================================
// Memory Management
// ============================================================================

// Free a string returned by any hyades_wasm_* function
// JavaScript should call this after consuming the result
WASM_EXPORT
void hyades_wasm_free(char *ptr) {
    if (ptr) {
        free(ptr);
    }
}

// Safe strdup that handles NULL
static char *safe_strdup(const char *s) {
    if (!s) return strdup("");
    return strdup(s);
}

// ============================================================================
// Core Rendering API
// ============================================================================

// Render a complete Hyades document
//
// Parameters:
//   input - Hyades source (UTF-8 string)
//   width - Output width in characters (0 for default 80)
//
// Returns:
//   Newly allocated string with rendered output, or error message prefixed with "ERROR: "
//   Caller must free with hyades_wasm_free()
//
WASM_EXPORT
char *hyades_wasm_render(const char *input, int width) {
    DEBUG_LOG("WASM: hyades_wasm_render called, width=%d\n", width);

    hyades_wasm_init();
    reset_render_state();

    if (!input) {
        DEBUG_LOG("WASM: Input is NULL\n");
        return strdup("ERROR: Input is NULL");
    }

    DEBUG_LOG("WASM: Input length = %zu\n", strlen(input));

    // Make a copy of the input to ensure it's not modified
    char *input_copy = safe_strdup(input);
    if (!input_copy) {
        return strdup("ERROR: Out of memory");
    }

    HyadesOptions opts = hyades_default_options();
    if (width > 0) {
        opts.width = width;
    }

    HyadesError error;
    hyades_error_init(&error);

    DEBUG_LOG("WASM: Calling hyades_render...\n");
    char *result = hyades_render(input_copy, &opts, &error);
    DEBUG_LOG("WASM: hyades_render returned, result=%p\n", (void *)result);

    free(input_copy);

    if (!result) {
        // Return error as string prefixed with "ERROR: "
        size_t msg_len = strlen(error.message);
        char *err_str = malloc(msg_len + 16);
        if (err_str) {
            snprintf(err_str, msg_len + 16, "ERROR: %s", error.message);
        } else {
            err_str = strdup("ERROR: Unknown error");
        }
        DEBUG_LOG("WASM: Error: %s\n", err_str);
        return err_str;
    }

    DEBUG_LOG("WASM: Success, output length = %zu\n", strlen(result));
    return result;
}

// Render just a math expression (convenience wrapper)
//
// Parameters:
//   math_src - TeX math source (without $$ delimiters)
//   width    - Output width (0 for default 80)
//
// Returns:
//   Rendered math as string, or error message prefixed with "ERROR: "
//
WASM_EXPORT
char *hyades_wasm_render_math(const char *math_src, int width) {
    DEBUG_LOG("WASM: hyades_wasm_render_math called\n");

    hyades_wasm_init();

    if (!math_src) {
        return strdup("ERROR: Math source is NULL");
    }

    // Wrap in $$ delimiters
    size_t len = strlen(math_src);
    char *wrapped = malloc(len + 16); // Extra padding for safety
    if (!wrapped) {
        return strdup("ERROR: Out of memory");
    }
    snprintf(wrapped, len + 16, "$$%s$$\n", math_src);

    char *result = hyades_wasm_render(wrapped, width);
    free(wrapped);

    return result;
}

// ============================================================================
// Cassilda API
// ============================================================================

// Process a Cassilda document
//
// Parameters:
//   input - Cassilda document source (UTF-8 string)
//
// Returns:
//   Processed document with rendered segments inserted, or error message
//
WASM_EXPORT
char *hyades_wasm_cassilda_process(const char *input) {
    DEBUG_LOG("WASM: hyades_wasm_cassilda_process called\n");

    hyades_wasm_init();
    reset_render_state();

    if (!input) {
        DEBUG_LOG("WASM: Input is NULL\n");
        return strdup("ERROR: Input is NULL");
    }

    DEBUG_LOG("WASM: Input length = %zu\n", strlen(input));

    // Make a copy of the input to ensure it's not modified
    char *input_copy = safe_strdup(input);
    if (!input_copy) {
        return strdup("ERROR: Out of memory");
    }

    HyadesError error;
    hyades_error_init(&error);

    DEBUG_LOG("WASM: Calling hyades_cassilda_process...\n");
    char *result = hyades_cassilda_process(input_copy, &error);
    DEBUG_LOG("WASM: hyades_cassilda_process returned, result=%p\n", (void *)result);

    free(input_copy);

    if (!result) {
        size_t msg_len = strlen(error.message);
        char *err_str = malloc(msg_len + 16);
        if (err_str) {
            snprintf(err_str, msg_len + 16, "ERROR: %s", error.message);
        } else {
            err_str = strdup("ERROR: Unknown error");
        }
        DEBUG_LOG("WASM: Error: %s\n", err_str);
        return err_str;
    }

    DEBUG_LOG("WASM: Success, output length = %zu\n", strlen(result));
    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

// Get version string
WASM_EXPORT
const char *hyades_wasm_version(void) {
    return HYADES_VERSION_STRING;
}

// Check if result is an error (starts with "ERROR: ")
// Returns 1 if error, 0 if success
WASM_EXPORT
int hyades_wasm_is_error(const char *result) {
    if (!result) return 1;
    return strncmp(result, "ERROR: ", 7) == 0 ? 1 : 0;
}
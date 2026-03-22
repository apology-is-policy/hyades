// julia_bridge_wasm.c — Stub implementation for WASM builds
// Julia is not available in the browser environment
//
// This provides the same API as julia_bridge.c but all functions
// return appropriate "not available" responses.

#include "interop/julia_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Configuration (no-ops, but keep state for API compatibility)
// ============================================================================

static int g_float_precision = 6;
static bool g_scientific_notation = true;

void julia_set_float_precision(int digits) {
    if (digits >= 1 && digits <= 17) g_float_precision = digits;
}

void julia_set_scientific_notation(bool enabled) {
    g_scientific_notation = enabled;
}

// ============================================================================
// Lifecycle - Julia is never available in WASM
// ============================================================================

bool julia_init(void) {
    // Julia cannot be loaded in WASM environment
    return false;
}

void julia_shutdown(void) {
    // Nothing to do
}

bool julia_available(void) {
    return false;
}

// ============================================================================
// Registry - store registrations but they won't work
// ============================================================================

bool julia_register(const char *name, const char *params, const char *code) {
    (void)name;
    (void)params;
    (void)code;
    return false; // Cannot register without Julia
}

bool julia_is_registered(const char *name) {
    (void)name;
    return false;
}

bool julia_unregister(const char *name) {
    (void)name;
    return false;
}

void julia_clear_registry(void) {
    // Nothing to clear
}

// ============================================================================
// Execution - always returns error
// ============================================================================

static JuliaResult *make_error(const char *msg) {
    JuliaResult *r = calloc(1, sizeof(JuliaResult));
    if (r) {
        r->type = JULIA_RESULT_ERROR;
        r->string_val = strdup(msg);
    }
    return r;
}

JuliaResult *julia_call(const char *name, const char *args) {
    (void)name;
    (void)args;
    return make_error("Julia is not available in browser environment");
}

JuliaResult *julia_eval(const char *code) {
    (void)code;
    return make_error("Julia is not available in browser environment");
}

// ============================================================================
// Result handling
// ============================================================================

void julia_result_free(JuliaResult *result) {
    if (!result) return;
    switch (result->type) {
    case JULIA_RESULT_ERROR:
    case JULIA_RESULT_STRING:
    case JULIA_RESULT_TEX: free(result->string_val); break;
    case JULIA_RESULT_VECTOR: free(result->vector.data); break;
    case JULIA_RESULT_MATRIX: free(result->matrix.data); break;
    default: break;
    }
    free(result);
}

const char *julia_result_type_name(JuliaResultType type) {
    switch (type) {
    case JULIA_RESULT_ERROR: return "error";
    case JULIA_RESULT_NIL: return "nil";
    case JULIA_RESULT_INT: return "int";
    case JULIA_RESULT_FLOAT: return "float";
    case JULIA_RESULT_STRING: return "string";
    case JULIA_RESULT_VECTOR: return "vector";
    case JULIA_RESULT_MATRIX: return "matrix";
    case JULIA_RESULT_TEX: return "tex";
    default: return "unknown";
    }
}

char *julia_result_to_tex(const JuliaResult *result) {
    if (!result) return strdup("");

    switch (result->type) {
    case JULIA_RESULT_NIL: return strdup("");
    case JULIA_RESULT_ERROR: {
        const char *msg = result->string_val ? result->string_val : "unknown";
        size_t len = strlen(msg) + 20;
        char *tex = malloc(len);
        if (tex) snprintf(tex, len, "\\text{Error: %s}", msg);
        return tex ? tex : strdup("");
    }
    case JULIA_RESULT_STRING:
    case JULIA_RESULT_TEX: return strdup(result->string_val ? result->string_val : "");
    default: return strdup("");
    }
}
// hyades_public_api.c - Public API implementation for Hyades library

#include "compositor/compositor.h"
#include "document/document.h"     // For verbatim_store_clear
#include "document/macro_expand.h" // For macro_registry_keep_alive
#include "hyades_api.h"
#include "layout/layout_internal.h" // For line_registry_reset
#include "math/renderer/render_opts.h"
#include "math/renderer/symbols.h"
#include "utils/error.h"
#include "utils/warnings.h"
#include <hyades.h>

#ifndef HYADES_RENDER_ONLY
// Include cassilda internal header with renamed references
// The internal functions use cassilda_* naming, we wrap them here
#define CASSILDA_INTERNAL
#include "cassilda/cassilda.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Initialization State
// ============================================================================

static bool g_initialized = false;

void hyades_init(void) {
    if (g_initialized) return;
    symbols_init();
    g_initialized = true;
}

void hyades_shutdown(void) {
    // Currently no cleanup needed - symbols are static
    g_initialized = false;
}

void hyades_reset_state(void) {
    // Clear user macro registry
    // Calling keep_alive(false) when count is 0 will free the registry if it exists
    macro_registry_keep_alive(false);

    // Clear line registry (for lineroutine)
    line_registry_reset();

    // Clear verbatim store
    verbatim_store_clear();

    // Reset linebreaker mode to default
    set_linebreaker_mode("greedy");
}

// Ensure initialized before any operation
static void ensure_init(void) {
    if (!g_initialized) {
        hyades_init();
    }
}

// ============================================================================
// Error Handling
// ============================================================================

void hyades_error_init(HyadesError *err) {
    if (!err) return;
    err->code = HYADES_OK;
    err->line = 0;
    err->column = 0;
    err->message[0] = '\0';
}

// Convert internal ParseError to public HyadesError
static void convert_parse_error(const ParseError *src, HyadesError *dst) {
    if (!dst) return;

    if (!src || src->code == PARSE_OK) {
        hyades_error_init(dst);
        return;
    }

    switch (src->code) {
    case PARSE_ERR_SYNTAX: dst->code = HYADES_ERR_SYNTAX; break;
    case PARSE_ERR_MATH_SYNTAX: dst->code = HYADES_ERR_MATH_SYNTAX; break;
    case PARSE_ERR_UNSUPPORTED: dst->code = HYADES_ERR_UNSUPPORTED; break;
    case PARSE_ERR_OOM: dst->code = HYADES_ERR_MEMORY; break;
    case PARSE_ERR_UNKNOWN_COMMAND: dst->code = HYADES_ERR_MATH_SYNTAX; break;
    case PARSE_ERR_INTERNAL:
    case PARSE_ERR_OTHER:
    default: dst->code = HYADES_ERR_INTERNAL; break;
    }

    dst->line = src->row;
    dst->column = src->col;
    if (src->row > 0) {
        snprintf(dst->message, sizeof(dst->message), "line %d, col %d: %s", src->row, src->col,
                 src->message);
    } else {
        strncpy(dst->message, src->message, sizeof(dst->message) - 1);
        dst->message[sizeof(dst->message) - 1] = '\0';
    }
}

// Set error from string message
static void set_error(HyadesError *err, HyadesErrorCode code, const char *msg) {
    if (!err) return;
    err->code = code;
    err->line = 0;
    err->column = 0;
    strncpy(err->message, msg, sizeof(err->message) - 1);
    err->message[sizeof(err->message) - 1] = '\0';
}

// ============================================================================
// Options
// ============================================================================

HyadesOptions hyades_default_options(void) {
    HyadesOptions opt = {.width = 80, .unicode = true, .math_italic = true};
    return opt;
}

// Apply HyadesOptions to internal state
static CompOptions apply_options(const HyadesOptions *opts) {
    CompOptions comp = default_options();

    if (opts) {
        comp.width = opts->width;
        set_unicode_mode(opts->unicode ? 1 : 0);
        set_math_cursive_mode(opts->math_italic ? 1 : 0);
        set_render_mode(opts->unicode ? MODE_UNICODE : MODE_ASCII);
    } else {
        // Apply defaults
        set_unicode_mode(1);
        set_math_cursive_mode(1);
        set_render_mode(MODE_UNICODE);
    }

    return comp;
}

// ============================================================================
// Hyades Core API
// ============================================================================

char *hyades_render(const char *input, const HyadesOptions *options, HyadesError *error) {
    ensure_init();
    warnings_clear();

    if (!input) {
        set_error(error, HYADES_ERR_SYNTAX, "Input is NULL");
        return NULL;
    }

    // Apply options
    CompOptions comp_opts = apply_options(options);

    // Render using internal API
    ParseError parse_err = {0};
    char *result = compose_document(input, &comp_opts, &parse_err);

    if (!result) {
        convert_parse_error(&parse_err, error);
        return NULL;
    }

    if (error) {
        hyades_error_init(error);
    }

    return result;
}

// ============================================================================
// Cassilda API - Public wrappers for internal functions
// ============================================================================

#ifndef HYADES_RENDER_ONLY

// Note: The internal cassilda.h defines:
//   cassilda_run(input, err_msg, err_size) -> char*
//   cassilda_check(input, err_msg, err_size) -> bool
//   cassilda_render_segment(doc, name, err_msg, err_size) -> char*
//
// We wrap these with the public HyadesError interface.

char *hyades_cassilda_process(const char *input, HyadesError *error) {
    ensure_init();

    if (!input) {
        set_error(error, HYADES_ERR_SYNTAX, "Input is NULL");
        return NULL;
    }

    char err_msg[256] = {0};
    char *result = cassilda_run(input, err_msg, sizeof(err_msg));

    if (!result) {
        set_error(error, HYADES_ERR_SYNTAX, err_msg);
        return NULL;
    }

    if (error) {
        hyades_error_init(error);
    }

    return result;
}

bool hyades_cassilda_check(const char *input, HyadesError *error) {
    ensure_init();

    if (!input) {
        set_error(error, HYADES_ERR_SYNTAX, "Input is NULL");
        return false;
    }

    char err_msg[256] = {0};
    bool result = cassilda_check(input, err_msg, sizeof(err_msg));

    if (err_msg[0] != '\0') {
        set_error(error, HYADES_ERR_SYNTAX, err_msg);
        return false;
    }

    if (error) {
        hyades_error_init(error);
    }

    return result;
}

char *hyades_cassilda_render_segment(const char *input, const char *segment_name,
                                     HyadesError *error) {
    ensure_init();

    if (!input) {
        set_error(error, HYADES_ERR_SYNTAX, "Input is NULL");
        return NULL;
    }

    if (!segment_name) {
        set_error(error, HYADES_ERR_SYNTAX, "Segment name is NULL");
        return NULL;
    }

    char err_msg[256] = {0};

    // Parse the document (no filename available from public API)
    CassildaDocument *doc = cassilda_parse(input, NULL, NULL, err_msg, sizeof(err_msg));
    if (!doc) {
        set_error(error, HYADES_ERR_SYNTAX, err_msg);
        return NULL;
    }

    // Render the segment
    char *result = cassilda_render_segment(doc, segment_name, err_msg, sizeof(err_msg));
    cassilda_free(doc);

    if (!result) {
        set_error(error, HYADES_ERR_NOT_FOUND, err_msg);
        return NULL;
    }

    if (error) {
        hyades_error_init(error);
    }

    return result;
}

#endif // HYADES_RENDER_ONLY